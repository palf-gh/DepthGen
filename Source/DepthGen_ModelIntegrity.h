#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace depthgen {

// SHA-256 of the shipped dynamic IR-v8 / opset-17 ZipDepth export built by
// tools/build_zipdepth_model.py from the hash-verified NPU checkpoint.
constexpr const char* kZipDepthModelSha256 =
	"0741a0d574609da33c5081b1054a2dd1e8845ecdbed9a5f69c48807c22400d59";
constexpr const char* kDav2SmallModelSha256 =
	"237cfaaf329bc97b9914c14e2d2497b1159cc05cca1b6d7a68aa42a262ea99bf";

bool ComputeSha256File(const std::filesystem::path& path, std::string* digest, std::string* error);
bool ComputeSha256Bytes(const void* data, size_t size, std::string* digest, std::string* error);
bool VerifyModelSha256(const std::filesystem::path& path, const char* expected, std::string* error);
bool VerifyModelSha256(const void* data, size_t size, const char* expected, std::string* error);

inline bool VerifyZipDepthModel(const std::filesystem::path& path, std::string* error) {
	return VerifyModelSha256(path, kZipDepthModelSha256, error);
}
inline bool VerifyZipDepthModel(const void* data, size_t size, std::string* error) {
	return VerifyModelSha256(data, size, kZipDepthModelSha256, error);
}

} // namespace depthgen
