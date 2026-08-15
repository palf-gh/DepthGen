#include "DepthGen_Image.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace depthgen {
namespace {

float Clamp01(float value) noexcept {
	return std::max(0.0f, std::min(1.0f, value));
}

float CubicWeight(float value) noexcept {
	// OpenCV's INTER_CUBIC kernel uses a = -0.75.
	constexpr float a = -0.75f;
	value = std::abs(value);
	if (value <= 1.0f) {
		return (a + 2.0f) * value * value * value - (a + 3.0f) * value * value + 1.0f;
	}
	if (value < 2.0f) {
		return a * value * value * value - 5.0f * a * value * value + 8.0f * a * value - 4.0f * a;
	}
	return 0.0f;
}

int ClampIndex(int value, int limit) noexcept {
	return std::max(0, std::min(limit - 1, value));
}

float Percentile(std::vector<float>* sorted, float percentile) {
	if (sorted->empty()) {
		return 0.0f;
	}
	const float p = Clamp01(percentile / 100.0f);
	const float position = p * static_cast<float>(sorted->size() - 1);
	const size_t lower = static_cast<size_t>(std::floor(position));
	const size_t upper = static_cast<size_t>(std::ceil(position));
	const float fraction = position - static_cast<float>(lower);
	return (*sorted)[lower] + ((*sorted)[upper] - (*sorted)[lower]) * fraction;
}

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
		return;
	}
	const float scale = static_cast<float>(std::max(short_edge, 14)) /
		static_cast<float>(std::min(source_width, source_height));
	*out_width = RoundUpToPatchMultiple(static_cast<int>(std::lround(source_width * scale)));
	*out_height = RoundUpToPatchMultiple(static_cast<int>(std::lround(source_height * scale)));
}

FloatImage ResizeCubic(const FloatImage& input, int out_width, int out_height) {
	FloatImage output;
	if (!input.valid() || out_width <= 0 || out_height <= 0) {
		return output;
	}
	output.width = out_width;
	output.height = out_height;
	output.values.resize(static_cast<size_t>(out_width) * static_cast<size_t>(out_height));
	const float scale_x = static_cast<float>(input.width) / static_cast<float>(out_width);
	const float scale_y = static_cast<float>(input.height) / static_cast<float>(out_height);
	for (int y = 0; y < out_height; ++y) {
		const float source_y = (static_cast<float>(y) + 0.5f) * scale_y - 0.5f;
		const int y_base = static_cast<int>(std::floor(source_y));
		for (int x = 0; x < out_width; ++x) {
			const float source_x = (static_cast<float>(x) + 0.5f) * scale_x - 0.5f;
			const int x_base = static_cast<int>(std::floor(source_x));
			float sum = 0.0f;
			float weight_sum = 0.0f;
			for (int ky = -1; ky <= 2; ++ky) {
				const float wy = CubicWeight(source_y - static_cast<float>(y_base + ky));
				const int sample_y = ClampIndex(y_base + ky, input.height);
				for (int kx = -1; kx <= 2; ++kx) {
					const float weight = wy * CubicWeight(source_x - static_cast<float>(x_base + kx));
					const int sample_x = ClampIndex(x_base + kx, input.width);
					sum += input.values[static_cast<size_t>(sample_y) * input.width + sample_x] * weight;
					weight_sum += weight;
				}
			}
			output.values[static_cast<size_t>(y) * out_width + x] =
				weight_sum != 0.0f ? sum / weight_sum : 0.0f;
		}
	}
	return output;
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
	constexpr float mean[] = {0.485f, 0.456f, 0.406f};
	constexpr float deviation[] = {0.229f, 0.224f, 0.225f};
	for (size_t pixel = 0; pixel + 2 < rgb->size(); pixel += 3) {
		(*rgb)[pixel] = ((*rgb)[pixel] - mean[0]) / deviation[0];
		(*rgb)[pixel + 1] = ((*rgb)[pixel + 1] - mean[1]) / deviation[1];
		(*rgb)[pixel + 2] = ((*rgb)[pixel + 2] - mean[2]) / deviation[2];
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
	std::sort(samples.begin(), samples.end());
	const float low = Percentile(&samples, std::min(far_percentile, near_percentile));
	const float high = Percentile(&samples, std::max(far_percentile, near_percentile));
	const float span = std::max(high - low, std::numeric_limits<float>::epsilon());
	const float gamma = std::max(contrast, 0.01f);
	for (float& value : *depth) {
		value = Clamp01((value - low) / span);
		value = std::pow(value, 1.0f / gamma);
		if (invert) {
			value = 1.0f - value;
		}
	}
}

} // namespace depthgen
