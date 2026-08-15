#include "DepthGen_Image.h"
#include "DepthGen_Pipeline.h"

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

PF_EffectWorld MakeWorld8(std::vector<PF_Pixel>* storage, A_long width, A_long height) {
	storage->resize(static_cast<size_t>(width) * static_cast<size_t>(height));
	PF_EffectWorld world{};
	world.data = storage->data();
	world.rowbytes = static_cast<A_long>(width * sizeof(PF_Pixel));
	world.width = width;
	world.height = height;
	world.origin_x = 0;
	world.origin_y = 0;
	return world;
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

	// Percentile interpolation must match the order statistics of a full sort.
	{
		std::vector<float> eleven(11);
		const std::vector<float> opaque(11, 1.0f);
		for (size_t i = 0; i < eleven.size(); ++i) {
			eleven[i] = static_cast<float>(i);
		}
		depthgen::MapRelativeDepthToUnit(&eleven, opaque, 0.0f, true, 25.0f, 75.0f, 1.0f, false);
		Require(Near(eleven[5], 0.5f), "interpolated 25/75 percentiles must centre the mid sample");
		Require(Near(eleven[2], 0.0f) && Near(eleven[8], 1.0f),
			"interpolated percentiles must clip at the fractional bounds");
	}
	// Contrast uses a LUT; verify sqrt behaviour at a mid value within LUT error.
	{
		std::vector<float> ramp = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
		depthgen::MapRelativeDepthToUnit(&ramp, alpha, 0.0f, true, 0.0f, 100.0f, 2.0f, false);
		Require(Near(ramp[1], 0.5f, 2.0e-3f) && Near(ramp[2], std::sqrt(0.5f), 2.0e-3f) &&
			Near(ramp[3], std::sqrt(0.75f), 2.0e-3f),
			"contrast LUT must approximate the gamma curve");
	}

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

	// Fused inference sampler: aligned bilinear reduction straight to NCHW.
	{
		std::vector<PF_Pixel> pixels;
		PF_EffectWorld world = MakeWorld8(&pixels, 4, 4);
		for (A_long y = 0; y < 4; ++y) {
			for (A_long x = 0; x < 4; ++x) {
				PF_Pixel* pixel = depthgen::PixelAt<PF_Pixel>(&world, x, y);
				pixel->alpha = 255;
				pixel->red = static_cast<A_u_char>(x * 80);
				pixel->green = static_cast<A_u_char>(y * 80);
				pixel->blue = 128;
			}
		}
		const std::vector<float> tensor =
			depthgen::ReadWorldToInferenceTensor<PF_Pixel>(&world, 2, 2, false);
		Require(tensor.size() == 12, "2x2 RGB tensor must hold 12 values");
		const float red_far = 240.0f / 255.0f;
		Require(Near(tensor[0], (0.0f - depthgen::kImageNetMean[0]) / depthgen::kImageNetDeviation[0]),
			"tensor corner must sample the first source corner");
		Require(Near(tensor[1], (red_far - depthgen::kImageNetMean[0]) / depthgen::kImageNetDeviation[0]),
			"tensor must sample the far source corner at reduced size");
		Require(Near(tensor[5], (0.0f - depthgen::kImageNetMean[1]) / depthgen::kImageNetDeviation[1]),
			"green plane must be planar after the red plane");
		Require(Near(tensor[7], (red_far - depthgen::kImageNetMean[1]) / depthgen::kImageNetDeviation[1]),
			"green plane must track vertical position");
	}
	// Premultiplied sources must unpremultiply after the alpha-weighted reduction.
	{
		std::vector<PF_Pixel> pixels;
		PF_EffectWorld world = MakeWorld8(&pixels, 4, 4);
		for (PF_Pixel& pixel : pixels) {
			pixel.alpha = 128;
			pixel.red = 128;
			pixel.green = 0;
			pixel.blue = 0;
		}
		const std::vector<float> tensor =
			depthgen::ReadWorldToInferenceTensor<PF_Pixel>(&world, 2, 2, false);
		Require(Near(tensor[0], (1.0f - depthgen::kImageNetMean[0]) / depthgen::kImageNetDeviation[0],
			1.0e-3f), "premultiplied half-alpha red must unpremultiply to full red");
	}
	// Fully transparent pixels contribute zero colour, matching the old path.
	{
		std::vector<PF_Pixel> pixels;
		PF_EffectWorld world = MakeWorld8(&pixels, 4, 4);
		for (PF_Pixel& pixel : pixels) {
			pixel.alpha = 0;
			pixel.red = 255;
		}
		const std::vector<float> tensor =
			depthgen::ReadWorldToInferenceTensor<PF_Pixel>(&world, 2, 2, false);
		Require(Near(tensor[0], (0.0f - depthgen::kImageNetMean[0]) / depthgen::kImageNetDeviation[0]),
			"transparent pixels must sample as black");
	}
	// Linear-to-sRGB conversion goes through the LUT with sub-quantisation error.
	{
		std::vector<PF_Pixel> pixels;
		PF_EffectWorld world = MakeWorld8(&pixels, 2, 2);
		for (PF_Pixel& pixel : pixels) {
			pixel.alpha = 255;
			pixel.red = 128;
			pixel.green = 128;
			pixel.blue = 128;
		}
		const std::vector<float> tensor =
			depthgen::ReadWorldToInferenceTensor<PF_Pixel>(&world, 2, 2, true);
		const float expected = depthgen::LinearToSrgb(128.0f / 255.0f);
		Require(Near(tensor[0], (expected - depthgen::kImageNetMean[0]) / depthgen::kImageNetDeviation[0],
			3.0e-3f), "linear input must pass through the sRGB transfer LUT");
	}

	std::cout << "DepthGen image-pipeline tests passed\n";
	return 0;
}
