#pragma once

#include <string>
#include <vector>

namespace depthgen {

enum class InferenceProvider {
	Unavailable,
	Cuda,
	DirectML,
	CoreML,
	Cpu
};

enum class InferencePreference {
	Accelerated,
	Cpu
};

enum class DepthModel {
	ZipDepth,
	DepthAnythingV2Small
};

struct InferenceResult {
	int width = 0;
	int height = 0;
	std::vector<float> depth;
};

// Process-global and internally synchronised. It deliberately has no frame
// history, so its output depends only on the supplied source frame/settings.
// ZipDepth input is [0,1] NCHW RGB. Depth Anything V2 Small input is ImageNet-
// normalised planar RGB.
bool InferDepth(
	const std::vector<float>& nchw_rgb,
	int width,
	int height,
	DepthModel model,
	InferenceResult* result,
	InferenceProvider* provider,
	std::string* error,
	InferencePreference preference = InferencePreference::Accelerated);

inline bool InferZipDepth(
	const std::vector<float>& nchw_rgb,
	int width,
	int height,
	InferenceResult* result,
	InferenceProvider* provider,
	std::string* error,
	InferencePreference preference = InferencePreference::Accelerated) {
	return InferDepth(nchw_rgb, width, height, DepthModel::ZipDepth, result, provider, error,
		preference);
}

const char* InferenceProviderName(InferenceProvider provider) noexcept;
const char* DepthModelName(DepthModel model) noexcept;

} // namespace depthgen
