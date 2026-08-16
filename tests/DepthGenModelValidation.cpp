#include "DepthGen_Inference.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void SetModelPath(const std::string& path) {
#if defined(_WIN32)
	_putenv_s("DEPTHGEN_MODEL_PATH", path.c_str());
#else
	setenv("DEPTHGEN_MODEL_PATH", path.c_str(), 1);
#endif
}

bool ExpectFailure(const char* expected) {
	std::vector<float> input(32U * 32U * 3U, 0.5f);
	depthgen::InferenceResult result;
	depthgen::InferenceProvider provider = depthgen::InferenceProvider::Unavailable;
	std::string error;
	if (depthgen::InferZipDepth(input, 32, 32, &result, &provider, &error)) {
		std::cerr << "Expected model validation failure.\n";
		return false;
	}
	if (provider != depthgen::InferenceProvider::Unavailable || error.find(expected) == std::string::npos) {
		std::cerr << "Unexpected validation error: " << error << '\n';
		return false;
	}
	return true;
}

} // namespace

int main() {
	const std::filesystem::path invalid = std::filesystem::temp_directory_path() /
		"DepthGenModelValidation.invalid.onnx";
	std::error_code ignored;
	std::filesystem::remove(invalid, ignored);
	SetModelPath(invalid.string());
	if (!ExpectFailure("missing")) return 1;
	{
		std::ofstream stream(invalid, std::ios::binary);
		stream << "not a ZipDepth model";
	}
	if (!ExpectFailure("SHA-256 mismatch")) {
		std::filesystem::remove(invalid, ignored);
		return 1;
	}
	std::filesystem::remove(invalid, ignored);
	std::cout << "DepthGen missing and mismatched model validation passed.\n";
	return 0;
}
