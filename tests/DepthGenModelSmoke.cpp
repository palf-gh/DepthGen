#include "DepthGen_Inference.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace {

bool Smoke(depthgen::DepthModel model, int width, int height) {
	std::vector<float> input(static_cast<size_t>(width) * height * 3U, 0.5f);
	depthgen::InferenceResult result;
	depthgen::InferenceProvider provider = depthgen::InferenceProvider::Unavailable;
	std::string error;
	if (!depthgen::InferDepth(input, width, height, model, &result, &provider, &error)) {
		std::cerr << depthgen::DepthModelName(model) << ": " << error << '\n';
		return false;
	}
	if (provider == depthgen::InferenceProvider::Unavailable || result.width <= 0 || result.height <= 0 ||
		result.depth.size() != static_cast<size_t>(result.width) * result.height) {
		std::cerr << depthgen::DepthModelName(model) << " returned an invalid inference result.\n";
		return false;
	}
	for (float value : result.depth) {
		if (!std::isfinite(value)) {
			std::cerr << depthgen::DepthModelName(model) << " returned non-finite depth.\n";
			return false;
		}
	}
	std::cout << "DepthGen " << depthgen::DepthModelName(model) << " smoke passed with "
		<< depthgen::InferenceProviderName(provider) << ".\n";
	return true;
}

} // namespace

int main() {
#if defined(DEPTHGEN_EMBEDDED_MODEL)
	return Smoke(depthgen::DepthModel::ZipDepth, 384, 384) &&
		Smoke(depthgen::DepthModel::DepthAnythingV2Small, 392, 392) ? 0 : 1;
#else
	return Smoke(depthgen::DepthModel::ZipDepth, 384, 384) ? 0 : 1;
#endif
}
