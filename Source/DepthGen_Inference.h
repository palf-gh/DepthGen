#pragma once

#include <string>
#include <vector>

namespace depthgen {

enum class InferenceProvider {
	Unavailable,
	DirectML,
	CoreML,
	Cpu
};

enum class InferencePreference {
	Accelerated,
	Cpu
};

struct InferenceResult {
	int width = 0;
	int height = 0;
	std::vector<float> depth;
};

// Process-global and internally synchronised. It deliberately has no frame
// history, so its output depends only on the supplied source frame/settings.
bool InferDepthAnythingSmall(
	const std::vector<float>& normalised_interleaved_rgb,
	int width,
	int height,
	InferenceResult* result,
	InferenceProvider* provider,
	std::string* error,
	InferencePreference preference = InferencePreference::Accelerated);

const char* InferenceProviderName(InferenceProvider provider) noexcept;

} // namespace depthgen
