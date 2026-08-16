#include "DepthGen_Inference.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool Run(int width, int height, depthgen::InferencePreference preference,
	depthgen::InferenceResult* result, depthgen::InferenceProvider* provider) {
	const size_t plane = static_cast<size_t>(width) * height;
	std::vector<float> input(plane * 3U);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const size_t index = static_cast<size_t>(y) * width + x;
			input[index] = static_cast<float>(x) / static_cast<float>(width - 1);
			input[plane + index] = static_cast<float>(y) / static_cast<float>(height - 1);
			input[plane * 2U + index] = 0.5f;
		}
	}
	std::string error;
	if (!depthgen::InferZipDepth(input, width, height, result, provider, &error, preference)) {
		std::cerr << error << '\n';
		return false;
	}
	return true;
}

void Normalise(std::vector<float>* values) {
	const auto [minimum_it, maximum_it] = std::minmax_element(values->begin(), values->end());
	const float minimum = *minimum_it;
	const float span = std::max(*maximum_it - minimum, 1.0e-6f);
	for (float& value : *values) {
		value = (value - minimum) / span;
	}
}

bool ValidateShape(int width, int height) {
	depthgen::InferenceResult accelerated;
	depthgen::InferenceResult cpu;
	depthgen::InferenceProvider accelerated_provider = depthgen::InferenceProvider::Unavailable;
	depthgen::InferenceProvider cpu_provider = depthgen::InferenceProvider::Unavailable;
	if (!Run(width, height, depthgen::InferencePreference::Accelerated,
			&accelerated, &accelerated_provider) ||
		!Run(width, height, depthgen::InferencePreference::Cpu, &cpu, &cpu_provider)) {
		return false;
	}
	if (accelerated_provider == depthgen::InferenceProvider::Cpu || cpu_provider != depthgen::InferenceProvider::Cpu ||
		accelerated.width != cpu.width || accelerated.height != cpu.height ||
		accelerated.depth.size() != cpu.depth.size()) {
		std::cerr << "Expected an accelerator result and a matching CPU result at "
			<< width << 'x' << height << ".\n";
		return false;
	}
	const auto [accelerated_minimum_it, accelerated_maximum_it] =
		std::minmax_element(accelerated.depth.begin(), accelerated.depth.end());
	const auto [cpu_minimum_it, cpu_maximum_it] =
		std::minmax_element(cpu.depth.begin(), cpu.depth.end());
	const float accelerated_minimum = *accelerated_minimum_it;
	const float accelerated_maximum = *accelerated_maximum_it;
	const float cpu_minimum = *cpu_minimum_it;
	const float cpu_maximum = *cpu_maximum_it;
	Normalise(&accelerated.depth);
	Normalise(&cpu.depth);
	float mean_absolute_error = 0.0f;
	float maximum_absolute_error = 0.0f;
	for (size_t index = 0; index < cpu.depth.size(); ++index) {
		const float error = std::abs(accelerated.depth[index] - cpu.depth[index]);
		mean_absolute_error += error;
		maximum_absolute_error = std::max(maximum_absolute_error, error);
	}
	mean_absolute_error /= static_cast<float>(cpu.depth.size());
	const bool core_ml = accelerated_provider == depthgen::InferenceProvider::CoreML;
	const float mean_tolerance = core_ml ? 0.02f : 0.0025f;
	const float maximum_tolerance = core_ml ? 0.11f : 0.025f;
	if (mean_absolute_error > mean_tolerance || maximum_absolute_error > maximum_tolerance) {
		std::cerr << "Provider parity exceeded tolerance at " << width << 'x' << height
			<< ": MAE=" << mean_absolute_error
			<< ", max=" << maximum_absolute_error
			<< ", accelerated_range=[" << accelerated_minimum << ", " << accelerated_maximum
			<< "], cpu_range=[" << cpu_minimum << ", " << cpu_maximum << "]\n";
		return false;
	}
	std::cout << "DepthGen provider parity passed at " << width << 'x' << height
		<< " (" << depthgen::InferenceProviderName(accelerated_provider)
		<< " vs CPU, MAE=" << mean_absolute_error << ", max="
		<< maximum_absolute_error << ").\n";
	return true;
}

} // namespace

int main(int argc, char** argv) {
	if (argc == 3) {
		try {
			const int width = std::stoi(argv[1]);
			const int height = std::stoi(argv[2]);
			if (width <= 1 || height <= 1) {
				std::cerr << "Width and height must exceed one pixel.\n";
				return 2;
			}
			return ValidateShape(width, height) ? 0 : 1;
		} catch (const std::exception&) {
			std::cerr << "Width and height must be positive integers.\n";
			return 2;
		}
	}
	if (argc != 1) {
		std::cerr << "Usage: depthgen_provider_parity [width height]\n";
		return 2;
	}
	constexpr int kShapes[][2] = {
		{928, 512},   // 1080p Fast landscape.
		{1376, 768},  // 1080p Balanced landscape.
		{1920, 1088}, // FHD High landscape, rounded to the 32-pixel patch.
		{3840, 2176}, // UHD Custom maximum, rounded to the 32-pixel patch.
	};
	for (const auto& shape : kShapes) {
		if (!ValidateShape(shape[0], shape[1])) {
			return 1;
		}
	}
	return 0;
}
