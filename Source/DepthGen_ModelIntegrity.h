#pragma once

#include <filesystem>
#include <string>

namespace depthgen {

constexpr const char* kDepthAnythingSmallModelSha256 =
	"46c4e8eeda3a27f34701831b6a2ec7753d7b38779b215acb5633424703deed8f";

bool ComputeSha256File(const std::filesystem::path& path, std::string* digest, std::string* error);
bool VerifyDepthAnythingSmallModel(const std::filesystem::path& path, std::string* error);

} // namespace depthgen
