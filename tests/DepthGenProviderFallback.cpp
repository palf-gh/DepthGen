#include "DepthGen_Inference.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
	const bool execution_failure = argc == 2 && std::string(argv[1]) == "--execution";
#if defined(_WIN32)
	_putenv_s(execution_failure ? "DEPTHGEN_TEST_FORCE_ACCELERATOR_EXECUTION_FAILURE" :
		"DEPTHGEN_TEST_FORCE_ACCELERATOR_FAILURE", "1");
#else
	setenv(execution_failure ? "DEPTHGEN_TEST_FORCE_ACCELERATOR_EXECUTION_FAILURE" :
		"DEPTHGEN_TEST_FORCE_ACCELERATOR_FAILURE", "1", 1);
#endif
	std::vector<float> input(384U * 384U * 3U, 0.5f);
	depthgen::InferenceResult result;
	depthgen::InferenceProvider provider = depthgen::InferenceProvider::Unavailable;
	std::string error;
	if (!depthgen::InferZipDepth(input, 384, 384, &result, &provider, &error,
		depthgen::InferencePreference::Accelerated)) {
		std::cerr << error << '\n';
		return 1;
	}
	if (provider != depthgen::InferenceProvider::Cpu || result.depth.empty()) {
		std::cerr << "DepthGen did not fall back to CPU after accelerated "
			<< (execution_failure ? "execution" : "session initialisation") << " failed.\n";
		return 1;
	}
	std::cout << "DepthGen accelerated-provider "
		<< (execution_failure ? "execution" : "initialisation") << " CPU fallback passed.\n";
	return 0;
}
