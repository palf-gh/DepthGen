#pragma once

#include <filesystem>
#include <string>

namespace depthgen {

// SHA-256 of the shipped accelerated repackage built by
// tools/build_accelerated_model.py from the hash-verified upstream export.
constexpr const char* kDepthAnythingSmallModelSha256 =
	"237cfaaf329bc97b9914c14e2d2497b1159cc05cca1b6d7a68aa42a262ea99bf";

bool ComputeSha256File(const std::filesystem::path& path, std::string* digest, std::string* error);
bool VerifyDepthAnythingSmallModel(const std::filesystem::path& path, std::string* error);

} // namespace depthgen
