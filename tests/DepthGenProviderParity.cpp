#include "DepthGen_Inference.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

bool Run(depthgen::InferencePreference preference, depthgen::InferenceResult* result,
	depthgen::InferenceProvider* provider) {
	constexpr int width = 392;
	constexpr int height = 392;
	std::vector<float> input(static_cast<size_t>(width) * height * 3U, 0.0f);
	std::string error;
	if (!depthgen::InferDepthAnythingSmall(input, width, height, result, provider, &error, preference)) {
		std::cerr << error << '\n';
		return false;
	}
	return true;
}

void Normalise(std::vector<float>* values) {
	const auto [minimum, maximum] = std::minmax_element(values->begin(), values->end());
	const float span = std::max(*maximum - *minimum, 1.0e-6f);
	for (float& value : *values) {
		value = (value - *minimum) / span;
	}
}

} // namespace

int main() {
	depthgen::InferenceResult accelerated;
	depthgen::InferenceResult cpu;
	depthgen::InferenceProvider accelerated_provider = depthgen::InferenceProvider::Unavailable;
	depthgen::InferenceProvider cpu_provider = depthgen::InferenceProvider::Unavailable;
	if (!Run(depthgen::InferencePreference::Accelerated, &accelerated, &accelerated_provider) ||
		!Run(depthgen::InferencePreference::Cpu, &cpu, &cpu_provider)) {
		return 1;
	}
	if (accelerated_provider == depthgen::InferenceProvider::Cpu || cpu_provider != depthgen::InferenceProvider::Cpu ||
		accelerated.width != cpu.width || accelerated.height != cpu.height ||
		accelerated.depth.size() != cpu.depth.size()) {
		std::cerr << "Expected an accelerator result and a matching CPU result.\n";
		return 1;
	}
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
	if (mean_absolute_error > 0.0025f || maximum_absolute_error > 0.025f) {
		std::cerr << "Provider parity exceeded tolerance: MAE=" << mean_absolute_error
			<< ", max=" << maximum_absolute_error << '\n';
		return 1;
	}
	std::cout << "DepthGen provider parity passed (" << depthgen::InferenceProviderName(accelerated_provider)
		<< " vs CPU, MAE=" << mean_absolute_error << ", max=" << maximum_absolute_error << ").\n";
	return 0;
}
