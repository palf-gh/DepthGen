// Times the per-frame host pipeline (alpha read, fused inference tensor
// sampling, depth upsample, level mapping, output write) without running the
// model. Not registered with CTest; run it manually before/after changes.

#include "DepthGen_Image.h"
#include "DepthGen_Pipeline.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

template <typename Pixel>
struct WorldStorage {
	std::vector<Pixel> pixels;
	PF_EffectWorld world{};

	WorldStorage(A_long width, A_long height) {
		pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
		world.data = reinterpret_cast<PF_PixelPtr>(pixels.data());
		world.rowbytes = static_cast<A_long>(width * sizeof(Pixel));
		world.width = width;
		world.height = height;
		world.origin_x = 0;
		world.origin_y = 0;
	}
};

template <typename Pixel, typename Fn>
double BestOfFive(Fn&& stage) {
	double best_ms = 1.0e30;
	for (int run = 0; run < 5; ++run) {
		const auto start = std::chrono::steady_clock::now();
		stage();
		best_ms = std::min(best_ms,
			std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
	}
	return best_ms;
}

template <typename Pixel>
void RunSuite(A_long width, A_long height, int inference_width, int inference_height) {
	WorldStorage<Pixel> input(width, height);
	WorldStorage<Pixel> output(width, height);
	for (A_long y = 0; y < height; ++y) {
		for (A_long x = 0; x < width; ++x) {
			const float fx = static_cast<float>(x) / static_cast<float>(std::max<A_long>(width - 1, 1));
			const float fy = static_cast<float>(y) / static_cast<float>(std::max<A_long>(height - 1, 1));
			depthgen::WritePixel(depthgen::PixelAt<Pixel>(&input.world, x, y),
				0.5f + 0.5f * std::sin((fx + fy) * 6.28318530718f), 1.0f);
		}
	}
	depthgen::FloatImage raw;
	raw.width = inference_width;
	raw.height = inference_height;
	raw.values.resize(static_cast<size_t>(inference_width) * static_cast<size_t>(inference_height));
	for (size_t i = 0; i < raw.values.size(); ++i) {
		raw.values[i] = static_cast<float>(std::sin(static_cast<double>(i) * 0.001) * 0.5 + 0.5);
	}
	std::vector<float> alpha;
	std::vector<float> tensor;
	depthgen::FloatImage full_depth;

	std::cout << "# suite " << width << 'x' << height << " inference " << inference_width << 'x'
		<< inference_height << " pixel_bytes " << sizeof(Pixel) << '\n';
	std::cout << "stage,ms_best_of_5\n" << std::fixed << std::setprecision(3);
	std::cout << "read_alpha," << BestOfFive<Pixel>([&] {
		alpha = depthgen::ReadWorldAlpha<Pixel>(&input.world);
	}) << '\n';
	std::cout << "inference_tensor," << BestOfFive<Pixel>([&] {
		tensor = depthgen::ReadWorldToInferenceTensor<Pixel>(&input.world, inference_width,
			inference_height, false);
	}) << '\n';
	std::cout << "depth_upsample," << BestOfFive<Pixel>([&] {
		full_depth = depthgen::ResizeBilinearAligned(raw, static_cast<int>(width), static_cast<int>(height));
	}) << '\n';
	std::cout << "map_levels," << BestOfFive<Pixel>([&] {
		std::vector<float> work = full_depth.values;
		depthgen::MapRelativeDepthToUnit(&work, alpha, 0.0f, true, 2.0f, 98.0f, 1.0f, false);
	}) << '\n';
	std::cout << "map_levels_gamma2," << BestOfFive<Pixel>([&] {
		std::vector<float> work = full_depth.values;
		depthgen::MapRelativeDepthToUnit(&work, alpha, 0.0f, true, 2.0f, 98.0f, 2.0f, false);
	}) << '\n';
	std::cout << "write_output," << BestOfFive<Pixel>([&] {
		depthgen::WriteDepthWorld<Pixel>(&output.world, &input.world, full_depth.values, alpha, true);
	}) << '\n';
	std::cout << "tensor_checksum," << (tensor.empty() ? 0.0f : tensor.front()) << '\n';
}

} // namespace

int main() {
	int inference_width = 0;
	int inference_height = 0;
	depthgen::ComputeInferenceSize(1920, 1080, 518, &inference_width, &inference_height);
	RunSuite<PF_Pixel>(1920, 1080, inference_width, inference_height);
	RunSuite<PF_PixelFloat>(1920, 1080, inference_width, inference_height);
	depthgen::ComputeInferenceSize(3840, 2160, 518, &inference_width, &inference_height);
	RunSuite<PF_Pixel>(3840, 2160, inference_width, inference_height);
	return 0;
}
