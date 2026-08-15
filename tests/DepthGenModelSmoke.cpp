#include "DepthGen_Inference.h"

#include <cmath>
#include <iostream>
#include <vector>

int main() {
	constexpr int width = 392;
	constexpr int height = 392;
	// ImageNet-normalised neutral grey. This is a deterministic synthetic input
	// that verifies dynamic graph shape, provider construction, and finite data.
	std::vector<float> input(static_cast<size_t>(width) * height * 3U, 0.0f);
	depthgen::InferenceResult result;
	depthgen::InferenceProvider provider = depthgen::InferenceProvider::Unavailable;
	std::string error;
	if (!depthgen::InferDepthAnythingSmall(input, width, height, &result, &provider, &error)) {
		std::cerr << error << '\n';
		return 1;
	}
	if (provider == depthgen::InferenceProvider::Unavailable || result.width <= 0 || result.height <= 0 ||
		result.depth.size() != static_cast<size_t>(result.width) * result.height) {
		std::cerr << "DepthGen returned an invalid inference result.\n";
		return 1;
	}
	for (float value : result.depth) {
		if (!std::isfinite(value)) {
			std::cerr << "DepthGen returned non-finite depth.\n";
			return 1;
		}
	}
	std::cout << "DepthGen model smoke test passed with "
		<< depthgen::InferenceProviderName(provider) << ".\n";
	return 0;
}
