#include "DepthGen_Image.h"
#include "DepthGen_Pipeline.h"
#include "DepthGen_Temporal.h"

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
	Require(depthgen::RoundUpToPatchMultiple(512) == 512, "512 must already be a 32-pixel multiple");
	Require(depthgen::RoundUpToPatchMultiple(513) == 544, "patch rounding must round up");

	int width = 0;
	int height = 0;
	depthgen::ComputeInferenceSize(1920, 1080, 384, &width, &height);
	Require(width == 704 && height == 384, "landscape inference size must preserve aspect and patch alignment");
	depthgen::ComputeInferenceSize(1080, 1920, 384, &width, &height);
	Require(width == 384 && height == 704, "portrait inference size must preserve aspect and patch alignment");

	depthgen::FloatImage edge;
	edge.width = 2;
	edge.height = 2;
	edge.values = {0.0f, 1.0f, 1.0f, 0.0f};
	const depthgen::FloatImage upsampled = depthgen::ResizeBilinearAligned(edge, 3, 3);
	Require(upsampled.valid(), "bilinear result must be valid");
	Require(Near(upsampled.values.front(), 0.0f), "aligned interpolation must retain first corner");
	Require(Near(upsampled.values.back(), 0.0f), "aligned interpolation must retain final corner");
	Require(Near(upsampled.values[4], 0.5f), "aligned interpolation must centre-average corners");

	Require(Near(255.0f * depthgen::kByteToUnit, 1.0f),
		"byte normalisation must map 255 to one");

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

	Require(depthgen::ScaleShortEdgeToRender(1920, 1080, 1920, 1080, 1080) == 1080,
		"full-resolution short edge must be unchanged");
	Require(depthgen::ScaleShortEdgeToRender(1920, 1080, 960, 540, 1080) == 540,
		"half-resolution preview must scale the labelled short edge");
	depthgen::ComputeInferenceSize(0, 1080, 1080, &width, &height);
	Require(width == 0 && height == 0, "invalid source must yield a zero inference size");
	depthgen::ComputeInferenceSize(960, 540, 540, &width, &height);
	Require(width == 960 && height == 544, "half-resolution High size must stay patch-aligned");
	depthgen::ComputeInferenceSize(3840, 2160, 2160, &width, &height);
	Require(width == 3840 && height == 2176, "UHD Custom maximum must retain full pixel detail");
	depthgen::ComputeInferenceSize(1920, 1080, 518, &width, &height, depthgen::kDav2Patch);
	Require(width == 924 && height == 518, "DAV2 Balanced landscape must use a 14-pixel patch");

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
		Require(Near(tensor[0], 0.0f),
			"tensor corner must sample the first source corner");
		Require(Near(tensor[1], red_far),
			"tensor must sample the far source corner at reduced size");
		Require(Near(tensor[5], 0.0f),
			"green plane must be planar after the red plane");
		Require(Near(tensor[7], red_far),
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
		Require(Near(tensor[0], 1.0f, 1.0e-3f),
			"premultiplied half-alpha red must unpremultiply to full red");
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
		Require(Near(tensor[0], 0.0f),
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
		Require(Near(tensor[0], expected, 3.0e-3f),
			"linear input must pass through the sRGB transfer LUT");
	}

	{
		depthgen::DepthLevels current{0.10f, 1.18f * 0.98f + 0.04f, true};
		depthgen::DepthLevels previous{0.10f, 0.98f, true};
		const depthgen::DepthLevels mixed = depthgen::SmoothMappingRange(current, previous, 0.0f);
		Require(Near(mixed.low, current.low) && Near(mixed.high, current.high),
			"stability 0 must keep the current mapping range");
		const depthgen::DepthLevels locked = depthgen::SmoothMappingRange(current, previous, 1.0f);
		Require(Near(locked.low, previous.low) && Near(locked.high, previous.high),
			"full stability must inherit the previous Far/Near endpoints");
	}

	{
		std::vector<float> current = {
			0.10f, 0.12f, 0.18f, 0.22f, 0.28f, 0.35f, 0.42f, 0.48f,
			0.55f, 0.62f, 0.70f, 0.78f, 0.85f, 0.90f, 0.94f, 0.98f};
		const std::vector<float> original = current;
		for (float& value : current) {
			value = value * 1.18f + 0.04f;
		}
		const std::vector<float> pulsed = current;
		const std::vector<float> opaque(current.size(), 1.0f);
		const depthgen::DepthLevels dummy{0.0f, 1.0f, true};
		const depthgen::TemporalRange previous =
			depthgen::MeasureUnitRange(original, opaque, 0.0f, dummy);
		depthgen::AlignUnitQuantiles(&current, opaque, 0.0f, previous.quantiles, 1.0f);
		const float original_t = (original[8] - original[4]) / (original[12] - original[4]);
		const float aligned_t = (current[8] - current[4]) / (current[12] - current[4]);
		Require(std::abs(aligned_t - original_t) < 1.0e-3f,
			"quantile alignment must preserve relative spatial structure");
		Require(std::abs(current[4] - original[4]) < std::abs(pulsed[4] - original[4]),
			"quantile alignment must pull a range pulse toward the previous histogram");
	}

	{
		std::vector<float> original(32);
		for (size_t i = 0; i < original.size(); ++i) {
			original[i] = 0.04f + 0.03f * static_cast<float>(i);
		}
		std::vector<float> current = original;
		for (size_t i = 0; i < current.size(); ++i) {
			if (original[i] > 0.35f && original[i] < 0.70f) {
				current[i] += 0.12f;
			}
		}
		const std::vector<float> pulsed = current;
		const std::vector<float> opaque(current.size(), 1.0f);
		const depthgen::DepthLevels dummy{0.0f, 1.0f, true};
		float original_q[depthgen::kTemporalQuantileCount] = {};
		float pulsed_q[depthgen::kTemporalQuantileCount] = {};
		float aligned_q[depthgen::kTemporalQuantileCount] = {};
		Require(depthgen::MeasureUnitQuantiles(original, opaque, 0.0f, original_q),
			"original midtone ramp must yield quantiles");
		Require(depthgen::MeasureUnitQuantiles(pulsed, opaque, 0.0f, pulsed_q),
			"pulsed midtones must yield quantiles");
		depthgen::AlignUnitQuantiles(&current, opaque, 0.0f, original_q, 1.0f);
		Require(depthgen::MeasureUnitQuantiles(current, opaque, 0.0f, aligned_q),
			"aligned midtones must yield quantiles");
		Require(std::abs(aligned_q[3] - original_q[3]) < std::abs(pulsed_q[3] - original_q[3]),
			"quantile matching must pull a midtone histogram pulse toward the previous median");
	}

	{
		std::vector<float> clean(16);
		for (size_t i = 0; i < clean.size(); ++i) {
			clean[i] = 0.05f + 0.06f * static_cast<float>(i);
		}
		std::vector<float> current = clean;
		current[8] = 0.99f;
		const float neighbour_ratio = (current[11] - current[10]) / (current[12] - current[10]);
		const std::vector<float> opaque(current.size(), 1.0f);
		const depthgen::DepthLevels dummy{0.0f, 1.0f, true};
		const depthgen::TemporalRange previous =
			depthgen::MeasureUnitRange(clean, opaque, 0.0f, dummy);
		depthgen::AlignUnitQuantiles(&current, opaque, 0.0f, previous.quantiles, 1.0f);
		Require(current[8] > current[7] && current[8] > current[9],
			"a single-pixel jump must remain a local peak after histogram matching");
		const float aligned_ratio = (current[11] - current[10]) / (current[12] - current[10]);
		Require(std::abs(aligned_ratio - neighbour_ratio) < 1.0e-3f,
			"neighbours must keep their current-frame spatial relationship");
	}

	{
		const depthgen::DepthLevels current{0.20f, 0.90f, true};
		const depthgen::DepthLevels previous{0.05f, 0.15f, true};
		const depthgen::DepthLevels cut = depthgen::SmoothMappingRange(current, previous, 1.0f);
		Require(Near(cut.low, current.low) && Near(cut.high, current.high),
			"a large mapping-range jump must reset instead of mixing endpoints");
	}

	{
		depthgen::TemporalHistory history;
		const depthgen::TemporalLayout layout{4, 4, 1, 768};
		depthgen::TemporalRange range;
		range.mapping = {0.1f, 0.9f, true};
		range.quantiles[0] = 0.10f;
		range.quantiles[1] = 0.22f;
		range.quantiles[2] = 0.35f;
		range.quantiles[3] = 0.50f;
		range.quantiles[4] = 0.65f;
		range.quantiles[5] = 0.78f;
		range.quantiles[6] = 0.90f;
		range.valid = true;
		history.Store(0, layout, range);
		depthgen::TemporalRange previous;
		Require(history.CopyPrevious(1, 1, layout, &previous),
			"sequential access must recover the previous range by time");
		Require(Near(previous.mapping.low, 0.1f) && Near(previous.quantiles[6], 0.90f),
			"cached range statistics must round-trip");
		Require(!history.CopyPrevious(100, 1, layout, &previous),
			"random access without a cached neighbour must miss rather than reuse a distant frame");
		Require(history.CopyPrevious(2, 1, layout, &previous),
			"a one-frame gap must still recover the nearest earlier sample");
		const depthgen::TemporalLayout other{4, 4, 2, 768};
		Require(!history.CopyPrevious(1, 1, other, &previous),
			"a model or size change must not reuse an incompatible cache entry");
	}

	std::cout << "DepthGen image-pipeline tests passed\n";
	return 0;
}
