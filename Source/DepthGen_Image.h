#pragma once

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

int RoundUpToPatchMultiple(int value, int patch = 14) noexcept;
void ComputeInferenceSize(int source_width, int source_height, int short_edge,
	int* out_width, int* out_height) noexcept;
FloatImage ResizeCubic(const FloatImage& input, int out_width, int out_height);
FloatImage ResizeBilinearAligned(const FloatImage& input, int out_width, int out_height);
float LinearToSrgb(float value) noexcept;
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
