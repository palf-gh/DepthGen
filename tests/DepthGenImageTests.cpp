#include "DepthGen_Image.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

bool Near(float lhs, float rhs, float tolerance = 1.0e-5f) {
	return std::abs(lhs - rhs) <= tolerance;
}

} // namespace

int main() {
	Require(depthgen::RoundUpToPatchMultiple(518) == 518, "518 must already be a 14-pixel multiple");
	Require(depthgen::RoundUpToPatchMultiple(519) == 532, "patch rounding must round up");

	int width = 0;
	int height = 0;
	depthgen::ComputeInferenceSize(1920, 1080, 518, &width, &height);
	Require(width == 924 && height == 518, "landscape inference size must preserve aspect and patch alignment");
	depthgen::ComputeInferenceSize(1080, 1920, 518, &width, &height);
	Require(width == 518 && height == 924, "portrait inference size must preserve aspect and patch alignment");

	depthgen::FloatImage edge;
	edge.width = 2;
	edge.height = 2;
	edge.values = {0.0f, 1.0f, 1.0f, 0.0f};
	const depthgen::FloatImage upsampled = depthgen::ResizeBilinearAligned(edge, 3, 3);
	Require(upsampled.valid(), "bilinear result must be valid");
	Require(Near(upsampled.values.front(), 0.0f), "aligned interpolation must retain first corner");
	Require(Near(upsampled.values.back(), 0.0f), "aligned interpolation must retain final corner");
	Require(Near(upsampled.values[4], 0.5f), "aligned interpolation must centre-average corners");

	std::vector<float> rgb = {0.485f, 0.456f, 0.406f};
	depthgen::ImageNetNormaliseInterleavedRgb(&rgb);
	Require(Near(rgb[0], 0.0f) && Near(rgb[1], 0.0f) && Near(rgb[2], 0.0f),
		"ImageNet means must normalise to zero");

	std::vector<float> depth = {0.0f, 0.25f, 0.50f, 0.75f, 1.0f};
	const std::vector<float> alpha = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	depthgen::MapRelativeDepthToUnit(&depth, alpha, 0.0f, true, 0.0f, 100.0f, 1.0f, false);
	Require(Near(depth[0], 0.0f) && Near(depth[2], 0.5f) && Near(depth[4], 1.0f),
		"full percentile range must retain a unit depth ramp");

	depthgen::MapRelativeDepthToUnit(&depth, alpha, 0.0f, true, 0.0f, 100.0f, 1.0f, true);
	Require(Near(depth[0], 1.0f) && Near(depth[4], 0.0f), "inversion must reverse depth polarity");

	std::vector<float> transparent_depth = {0.10f, 0.90f};
	const std::vector<float> transparent_alpha = {1.0f, 0.0f};
	depthgen::MapRelativeDepthToUnit(&transparent_depth, transparent_alpha, 0.01f, true,
		0.0f, 100.0f, 1.0f, false);
	Require(Near(transparent_depth[0], 0.0f), "alpha-aware levels must exclude transparent outliers");

	Require(Near(depthgen::LinearToSrgb(0.0f), 0.0f) && Near(depthgen::LinearToSrgb(1.0f), 1.0f),
		"sRGB transfer must retain endpoints");

	Require(depthgen::ScaleShortEdgeToRender(1920, 1080, 1920, 1080, 518) == 518,
		"full-resolution short edge must be unchanged");
	Require(depthgen::ScaleShortEdgeToRender(1920, 1080, 960, 540, 518) == 259,
		"half-resolution preview must scale the labelled short edge");
	depthgen::ComputeInferenceSize(0, 1080, 518, &width, &height);
	Require(width == 0 && height == 0, "invalid source must yield a zero inference size");
	depthgen::ComputeInferenceSize(960, 540, 259, &width, &height);
	Require(width == 462 && height == 266, "half-resolution Balanced size must stay patch-aligned");

	std::cout << "DepthGen image-pipeline tests passed\n";
	return 0;
}
