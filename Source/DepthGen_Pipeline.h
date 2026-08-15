#pragma once

// Pixel-level pipeline shared by the AE render path and the pipeline
// benchmark. All sampling is aligned-corners bilinear, matching
// ResizeBilinearAligned in DepthGen_Image.cpp.

#include "AE_Effect.h"
#include "DepthGen_Image.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace depthgen {

constexpr float kAlphaEpsilon = 1.0e-6f;
constexpr size_t kTransferLutSize = 4096;
using TransferLut = std::array<float, kTransferLutSize>;

inline TransferLut BuildLinearToSrgbLut() {
	TransferLut lut;
	for (size_t i = 0; i < lut.size(); ++i) {
		lut[i] = LinearToSrgb(static_cast<float>(i) / static_cast<float>(lut.size() - 1));
	}
	return lut;
}

template <typename Pixel>
Pixel* PixelAt(PF_EffectWorld* world, A_long x, A_long y) {
	return reinterpret_cast<Pixel*>(reinterpret_cast<char*>(world->data) +
		static_cast<ptrdiff_t>(y) * world->rowbytes) + x;
}

template <typename Pixel>
const Pixel* PixelAt(const PF_EffectWorld* world, A_long x, A_long y) {
	return reinterpret_cast<const Pixel*>(reinterpret_cast<const char*>(world->data) +
		static_cast<ptrdiff_t>(y) * world->rowbytes) + x;
}

template <typename Pixel>
void ReadPixel(const Pixel& pixel, float* red, float* green, float* blue, float* alpha);

template <>
inline void ReadPixel(const PF_Pixel& pixel, float* red, float* green, float* blue, float* alpha) {
	*red = pixel.red / 255.0f;
	*green = pixel.green / 255.0f;
	*blue = pixel.blue / 255.0f;
	*alpha = pixel.alpha / 255.0f;
}

template <>
inline void ReadPixel(const PF_Pixel16& pixel, float* red, float* green, float* blue, float* alpha) {
	*red = pixel.red / 32768.0f;
	*green = pixel.green / 32768.0f;
	*blue = pixel.blue / 32768.0f;
	*alpha = pixel.alpha / 32768.0f;
}

template <>
inline void ReadPixel(const PF_PixelFloat& pixel, float* red, float* green, float* blue,
	float* alpha) {
	*red = pixel.red;
	*green = pixel.green;
	*blue = pixel.blue;
	*alpha = pixel.alpha;
}

template <typename Pixel>
void WritePixel(Pixel* pixel, float depth, float alpha);

template <>
inline void WritePixel(PF_Pixel* pixel, float depth, float alpha) {
	// Clamped inputs make (v * max + 0.5f) truncation identical to lround.
	const auto to_byte = [](float value) {
		return static_cast<A_u_char>(std::max(0.0f, std::min(1.0f, value)) * 255.0f + 0.5f);
	};
	const A_u_char grey = to_byte(depth);
	pixel->red = grey;
	pixel->green = grey;
	pixel->blue = grey;
	pixel->alpha = to_byte(alpha);
}

template <>
inline void WritePixel(PF_Pixel16* pixel, float depth, float alpha) {
	const auto to_word = [](float value) {
		return static_cast<A_u_short>(std::max(0.0f, std::min(1.0f, value)) * 32768.0f + 0.5f);
	};
	const A_u_short grey = to_word(depth);
	pixel->red = grey;
	pixel->green = grey;
	pixel->blue = grey;
	pixel->alpha = to_word(alpha);
}

template <>
inline void WritePixel(PF_PixelFloat* pixel, float depth, float alpha) {
	const auto clamp = [](float value) { return std::max(0.0f, std::min(1.0f, value)); };
	pixel->red = clamp(depth);
	pixel->green = clamp(depth);
	pixel->blue = clamp(depth);
	pixel->alpha = clamp(alpha);
}

template <typename Pixel>
inline float AlphaValue(const Pixel& pixel);

template <>
inline float AlphaValue(const PF_Pixel& pixel) {
	return pixel.alpha / 255.0f;
}

template <>
inline float AlphaValue(const PF_Pixel16& pixel) {
	return pixel.alpha / 32768.0f;
}

template <>
inline float AlphaValue(const PF_PixelFloat& pixel) {
	return pixel.alpha;
}

// Full-resolution alpha, used by level mapping and output alpha preservation.
template <typename Pixel>
std::vector<float> ReadWorldAlpha(const PF_EffectWorld* world) {
	const size_t pixel_count = static_cast<size_t>(world->width) * static_cast<size_t>(world->height);
	std::vector<float> alpha(pixel_count);
	for (A_long y = 0; y < world->height; ++y) {
		const Pixel* row = PixelAt<Pixel>(world, 0, y);
		float* out = alpha.data() + static_cast<size_t>(y) * world->width;
		for (A_long x = 0; x < world->width; ++x) {
			out[x] = std::max(0.0f, std::min(1.0f, AlphaValue(row[x])));
		}
	}
	return alpha;
}

// Samples the (premultiplied) source world straight into the model's NCHW
// tensor at inference size: alpha-weighted bilinear reduction, unpremultiply,
// optional linear-to-sRGB, and ImageNet normalisation, all in one pass.
template <typename Pixel>
std::vector<float> ReadWorldToInferenceTensor(const PF_EffectWorld* world,
	int inference_width, int inference_height, bool linear_to_srgb) {
	std::vector<float> tensor;
	if (!world || world->width <= 0 || world->height <= 0 ||
		inference_width <= 0 || inference_height <= 0) {
		return tensor;
	}
	const size_t plane = static_cast<size_t>(inference_width) * static_cast<size_t>(inference_height);
	tensor.resize(plane * 3U);
	TransferLut transfer_lut{};
	if (linear_to_srgb) {
		transfer_lut = BuildLinearToSrgbLut();
	}
	const float x_scale = inference_width > 1
		? static_cast<float>(world->width - 1) / static_cast<float>(inference_width - 1) : 0.0f;
	const float y_scale = inference_height > 1
		? static_cast<float>(world->height - 1) / static_cast<float>(inference_height - 1) : 0.0f;
	for (int y = 0; y < inference_height; ++y) {
		const float source_y = y * y_scale;
		const A_long y0 = static_cast<A_long>(std::floor(source_y));
		const A_long y1 = std::min<A_long>(y0 + 1, world->height - 1);
		const float fy = source_y - static_cast<float>(y0);
		for (int x = 0; x < inference_width; ++x) {
			const float source_x = x * x_scale;
			const A_long x0 = static_cast<A_long>(std::floor(source_x));
			const A_long x1 = std::min<A_long>(x0 + 1, world->width - 1);
			const float fx = source_x - static_cast<float>(x0);
			float red = 0.0f;
			float green = 0.0f;
			float blue = 0.0f;
			float alpha = 0.0f;
			const float weights[4] = {
				(1.0f - fx) * (1.0f - fy), fx * (1.0f - fy),
				(1.0f - fx) * fy, fx * fy};
			const Pixel* taps[4] = {
				PixelAt<Pixel>(world, x0, y0), PixelAt<Pixel>(world, x1, y0),
				PixelAt<Pixel>(world, x0, y1), PixelAt<Pixel>(world, x1, y1)};
			for (int tap = 0; tap < 4; ++tap) {
				float tap_red = 0.0f;
				float tap_green = 0.0f;
				float tap_blue = 0.0f;
				float tap_alpha = 0.0f;
				ReadPixel(*taps[tap], &tap_red, &tap_green, &tap_blue, &tap_alpha);
				red += tap_red * weights[tap];
				green += tap_green * weights[tap];
				blue += tap_blue * weights[tap];
				alpha += tap_alpha * weights[tap];
			}
			const size_t index = static_cast<size_t>(y) * inference_width + x;
			float colour[3] = {0.0f, 0.0f, 0.0f};
			if (alpha > kAlphaEpsilon) {
				colour[0] = std::max(0.0f, std::min(1.0f, red / alpha));
				colour[1] = std::max(0.0f, std::min(1.0f, green / alpha));
				colour[2] = std::max(0.0f, std::min(1.0f, blue / alpha));
				if (linear_to_srgb) {
					for (float& channel : colour) {
						channel = SampleLut(transfer_lut.data(), transfer_lut.size(), channel);
					}
				}
			}
			for (size_t channel = 0; channel < 3; ++channel) {
				tensor[channel * plane + index] =
					(colour[channel] - kImageNetMean[channel]) / kImageNetDeviation[channel];
			}
		}
	}
	return tensor;
}

// Writes mapped depth into the (possibly tiled) output world, translating
// between output and input origins.
template <typename Pixel>
void WriteDepthWorld(
	PF_EffectWorld* output_world,
	const PF_EffectWorld* input_world,
	const std::vector<float>& depth,
	const std::vector<float>& source_alpha,
	bool preserve_alpha) {
	if (!output_world || !input_world || input_world->width <= 0 || input_world->height <= 0) {
		return;
	}
	// Fast path: the output covers the checked-out input 1:1 (the usual
	// full-frame SmartFX case), so no per-pixel bounds logic is needed.
	if (output_world->width == input_world->width && output_world->height == input_world->height &&
		output_world->origin_x == input_world->origin_x &&
		output_world->origin_y == input_world->origin_y &&
		depth.size() >= static_cast<size_t>(input_world->width) * static_cast<size_t>(input_world->height) &&
		source_alpha.size() >= depth.size()) {
		for (A_long y = 0; y < output_world->height; ++y) {
			Pixel* out = PixelAt<Pixel>(output_world, 0, y);
			const size_t row = static_cast<size_t>(y) * static_cast<size_t>(input_world->width);
			for (A_long x = 0; x < output_world->width; ++x) {
				const size_t index = row + static_cast<size_t>(x);
				const float a = source_alpha[index];
				const float value = a <= kAlphaEpsilon ? 0.0f : depth[index];
				WritePixel(out + x, value, preserve_alpha ? a : 1.0f);
			}
		}
		return;
	}
	for (A_long y = 0; y < output_world->height; ++y) {
		for (A_long x = 0; x < output_world->width; ++x) {
			const A_long src_x = x + output_world->origin_x - input_world->origin_x;
			const A_long src_y = y + output_world->origin_y - input_world->origin_y;
			float value = 0.0f;
			float alpha = preserve_alpha ? 0.0f : 1.0f;
			if (src_x >= 0 && src_x < input_world->width &&
				src_y >= 0 && src_y < input_world->height) {
				const size_t index = static_cast<size_t>(src_y) * static_cast<size_t>(input_world->width) +
					static_cast<size_t>(src_x);
				if (index < depth.size() && index < source_alpha.size()) {
					alpha = preserve_alpha ? source_alpha[index] : 1.0f;
					value = source_alpha[index] <= kAlphaEpsilon ? 0.0f : depth[index];
				}
			}
			WritePixel(PixelAt<Pixel>(output_world, x, y), value, alpha);
		}
	}
}

} // namespace depthgen
