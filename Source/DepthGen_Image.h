#pragma once

#include <cstddef>
#include <vector>

namespace depthgen {

struct FloatImage {
	int width = 0;
	int height = 0;
	std::vector<float> values;

	bool valid() const noexcept {
		return width > 0 && height > 0 &&
			values.size() == static_cast<size_t>(width) * static_cast<size_t>(height);
	}
};

// ImageNet normalisation applied to Depth Anything inputs.
inline constexpr float kImageNetMean[3] = {0.485f, 0.456f, 0.406f};
inline constexpr float kImageNetDeviation[3] = {0.229f, 0.224f, 0.225f};

int RoundUpToPatchMultiple(int value, int patch = 14) noexcept;
void ComputeInferenceSize(int source_width, int source_height, int short_edge,
	int* out_width, int* out_height) noexcept;
int ScaleShortEdgeToRender(int full_width, int full_height, int render_width,
	int render_height, int full_res_short_edge) noexcept;
FloatImage ResizeBilinearAligned(const FloatImage& input, int out_width, int out_height);
float LinearToSrgb(float value) noexcept;

// Linear-interpolated lookup into a uniform [0,1] table.
inline float SampleLut(const float* lut, size_t lut_size, float value) noexcept {
	if (!lut || lut_size < 2) {
		return value;
	}
	const float clamped = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
	const float position = clamped * static_cast<float>(lut_size - 1);
	const size_t lower = static_cast<size_t>(position);
	const size_t upper = lower + 1 < lut_size ? lower + 1 : lut_size - 1;
	const float fraction = position - static_cast<float>(lower);
	return lut[lower] + (lut[upper] - lut[lower]) * fraction;
}

void ImageNetNormaliseInterleavedRgb(std::vector<float>* rgb);
void MapRelativeDepthToUnit(
	std::vector<float>* depth,
	const std::vector<float>& alpha,
	float alpha_threshold,
	bool use_alpha_for_levels,
	float far_percentile,
	float near_percentile,
	float contrast,
	bool invert);

} // namespace depthgen
