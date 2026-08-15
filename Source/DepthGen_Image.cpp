#include "DepthGen_Image.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace depthgen {
namespace {

float Clamp01(float value) noexcept {
	return std::max(0.0f, std::min(1.0f, value));
}

// Exact order-statistic percentile (identical to sorting, but O(n)).
// `values` may be reordered by the call.
float PercentileFromUnsorted(std::vector<float>* values, float percentile) {
	const size_t count = values->size();
	const float position = Clamp01(percentile / 100.0f) * static_cast<float>(count - 1);
	const size_t lower = static_cast<size_t>(std::floor(position));
	const size_t upper = static_cast<size_t>(std::ceil(position));
	std::nth_element(values->begin(), values->begin() + upper, values->end());
	const float high = (*values)[upper];
	if (lower == upper) {
		return high;
	}
	// After nth_element at `upper`, every value before it is <= high, so the
	// lower order statistic is the maximum of the prefix.
	const float low = *std::max_element(values->begin(), values->begin() + upper);
	return low + (high - low) * (position - static_cast<float>(lower));
}

constexpr size_t kGammaLutSize = 4096;

} // namespace

int RoundUpToPatchMultiple(int value, int patch) noexcept {
	if (patch <= 0) {
		return std::max(value, 1);
	}
	return std::max(patch, ((std::max(value, 1) + patch - 1) / patch) * patch);
}

void ComputeInferenceSize(int source_width, int source_height, int short_edge,
	int* out_width, int* out_height) noexcept {
	if (!out_width || !out_height || source_width <= 0 || source_height <= 0) {
		if (out_width) *out_width = 0;
		if (out_height) *out_height = 0;
		return;
	}
	const float scale = static_cast<float>(std::max(short_edge, 14)) /
		static_cast<float>(std::min(source_width, source_height));
	*out_width = RoundUpToPatchMultiple(static_cast<int>(std::lround(source_width * scale)));
	*out_height = RoundUpToPatchMultiple(static_cast<int>(std::lround(source_height * scale)));
}

int ScaleShortEdgeToRender(int full_width, int full_height, int render_width,
	int render_height, int full_res_short_edge) noexcept {
	const int full_short = std::min(std::max(full_width, 1), std::max(full_height, 1));
	const int render_short = std::min(std::max(render_width, 1), std::max(render_height, 1));
	const int scaled = static_cast<int>(
		(static_cast<long long>(std::max(full_res_short_edge, 14)) * render_short +
			full_short / 2) / full_short);
	return std::max(14, scaled);
}

FloatImage ResizeBilinearAligned(const FloatImage& input, int out_width, int out_height) {
	FloatImage output;
	if (!input.valid() || out_width <= 0 || out_height <= 0) {
		return output;
	}
	output.width = out_width;
	output.height = out_height;
	output.values.resize(static_cast<size_t>(out_width) * static_cast<size_t>(out_height));
	const float x_scale = out_width > 1
		? static_cast<float>(input.width - 1) / static_cast<float>(out_width - 1) : 0.0f;
	const float y_scale = out_height > 1
		? static_cast<float>(input.height - 1) / static_cast<float>(out_height - 1) : 0.0f;
	for (int y = 0; y < out_height; ++y) {
		const float source_y = y * y_scale;
		const int y0 = static_cast<int>(std::floor(source_y));
		const int y1 = std::min(y0 + 1, input.height - 1);
		const float fy = source_y - y0;
		for (int x = 0; x < out_width; ++x) {
			const float source_x = x * x_scale;
			const int x0 = static_cast<int>(std::floor(source_x));
			const int x1 = std::min(x0 + 1, input.width - 1);
			const float fx = source_x - x0;
			const float top = input.values[static_cast<size_t>(y0) * input.width + x0] * (1.0f - fx) +
				input.values[static_cast<size_t>(y0) * input.width + x1] * fx;
			const float bottom = input.values[static_cast<size_t>(y1) * input.width + x0] * (1.0f - fx) +
				input.values[static_cast<size_t>(y1) * input.width + x1] * fx;
			output.values[static_cast<size_t>(y) * out_width + x] = top * (1.0f - fy) + bottom * fy;
		}
	}
	return output;
}

float LinearToSrgb(float value) noexcept {
	value = Clamp01(value);
	return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
}

void ImageNetNormaliseInterleavedRgb(std::vector<float>* rgb) {
	if (!rgb) {
		return;
	}
	for (size_t pixel = 0; pixel + 2 < rgb->size(); pixel += 3) {
		for (size_t channel = 0; channel < 3; ++channel) {
			(*rgb)[pixel + channel] =
				((*rgb)[pixel + channel] - kImageNetMean[channel]) / kImageNetDeviation[channel];
		}
	}
}

void MapRelativeDepthToUnit(
	std::vector<float>* depth,
	const std::vector<float>& alpha,
	float alpha_threshold,
	bool use_alpha_for_levels,
	float far_percentile,
	float near_percentile,
	float contrast,
	bool invert) {
	if (!depth || depth->empty() || depth->size() != alpha.size()) {
		return;
	}
	std::vector<float> samples;
	samples.reserve(depth->size());
	for (size_t i = 0; i < depth->size(); ++i) {
		if ((!use_alpha_for_levels || alpha[i] > alpha_threshold) && std::isfinite((*depth)[i])) {
			samples.push_back((*depth)[i]);
		}
	}
	if (samples.empty()) {
		std::fill(depth->begin(), depth->end(), 0.0f);
		return;
	}
	const float low = PercentileFromUnsorted(&samples, std::min(far_percentile, near_percentile));
	const float high = PercentileFromUnsorted(&samples, std::max(far_percentile, near_percentile));
	const float span = std::max(high - low, std::numeric_limits<float>::epsilon());
	const float inverse_span = 1.0f / span;
	const float gamma = std::max(contrast, 0.01f);
	if (std::abs(gamma - 1.0f) <= 1.0e-6f) {
		if (invert) {
			for (float& value : *depth) {
				value = 1.0f - Clamp01((value - low) * inverse_span);
			}
		} else {
			for (float& value : *depth) {
				value = Clamp01((value - low) * inverse_span);
			}
		}
		return;
	}
	// pow() per pixel at full resolution is expensive; a 4096-entry LUT with
	// linear interpolation stays far below display quantisation error.
	std::array<float, kGammaLutSize> lut{};
	const float exponent = 1.0f / gamma;
	for (size_t i = 0; i < lut.size(); ++i) {
		lut[i] = std::pow(static_cast<float>(i) / static_cast<float>(lut.size() - 1), exponent);
	}
	if (invert) {
		for (float& value : *depth) {
			value = 1.0f - SampleLut(lut.data(), lut.size(), (value - low) * inverse_span);
		}
	} else {
		for (float& value : *depth) {
			value = SampleLut(lut.data(), lut.size(), (value - low) * inverse_span);
		}
	}
}

} // namespace depthgen
