#include "DepthGen_OrtHost.h"

#if defined(_WIN32) && defined(DEPTHGEN_HAS_ORT)
#include "DepthGenOrtEmbed.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#include <Windows.h>
#endif

namespace depthgen {
#if defined(_WIN32) && defined(DEPTHGEN_HAS_ORT)
namespace {

constexpr int kOrtDllResourceId = 258;

HMODULE CurrentModule() noexcept {
	HMODULE module = nullptr;
	GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&CurrentModule),
		&module);
	return module;
}

std::filesystem::path ModuleDirectory() {
	const HMODULE module = CurrentModule();
	if (!module) {
		return {};
	}
	std::array<wchar_t, 32768> path{};
	const DWORD count = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
	if (!count) {
		return {};
	}
	return std::filesystem::path(path.data(), path.data() + count).parent_path();
}

std::filesystem::path LocalAppData() {
	std::array<wchar_t, 32768> path{};
	const DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", path.data(),
		static_cast<DWORD>(path.size()));
	if (!count || count >= path.size()) {
		return {};
	}
	return std::filesystem::path(path.data());
}

bool LoadFromPath(const std::filesystem::path& dll_path, std::string* error) {
	const HMODULE loaded = LoadLibraryW(dll_path.c_str());
	if (loaded) {
		return true;
	}
	if (error) {
		*error = "DepthGen could not load ONNX Runtime from " + dll_path.u8string() + ".";
	}
	return false;
}

bool WriteFileAtomically(const std::filesystem::path& destination, const void* data, size_t size,
	std::string* error) {
	const std::filesystem::path temporary = std::filesystem::path(destination.native() + L".tmp");
	{
		std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
		if (!out) {
			if (error) {
				*error = "DepthGen could not create its ONNX Runtime cache file.";
			}
			return false;
		}
		out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
		if (!out) {
			if (error) {
				*error = "DepthGen could not write its ONNX Runtime cache file.";
			}
			return false;
		}
	}
	std::error_code status;
	std::filesystem::rename(temporary, destination, status);
	if (status) {
		std::filesystem::remove(destination, status);
		std::filesystem::rename(temporary, destination, status);
	}
	if (status) {
		if (error) {
			*error = "DepthGen could not finalise its ONNX Runtime cache file.";
		}
		return false;
	}
	return true;
}

bool ExtractEmbeddedRuntime(std::filesystem::path* dll_path, std::string* error) {
	const HMODULE module = CurrentModule();
	const HRSRC resource = module
		? FindResourceW(module, MAKEINTRESOURCEW(kOrtDllResourceId), MAKEINTRESOURCEW(10))
		: nullptr;
	const HGLOBAL loaded = resource ? LoadResource(module, resource) : nullptr;
	const DWORD size = resource ? SizeofResource(module, resource) : 0;
	const void* data = loaded ? LockResource(loaded) : nullptr;
	if (!data || size == 0) {
		if (error) {
			*error = "DepthGen could not load its embedded ONNX Runtime.";
		}
		return false;
	}

	const std::filesystem::path appdata = LocalAppData();
	if (appdata.empty()) {
		if (error) {
			*error = "DepthGen could not locate LocalAppData for ONNX Runtime.";
		}
		return false;
	}
	const std::string sha(DEPTHGEN_ORT_DLL_SHA256);
	const std::string stamp = sha.size() >= 16 ? sha.substr(0, 16) : sha;
	const std::filesystem::path directory = appdata / "PALF" / "DepthGen" / "onnxruntime" / stamp;
	std::error_code status;
	std::filesystem::create_directories(directory, status);
	if (status) {
		if (error) {
			*error = "DepthGen could not create its ONNX Runtime cache directory.";
		}
		return false;
	}
	const std::filesystem::path destination = directory / "onnxruntime.dll";
	const std::filesystem::path stamp_path = directory / "onnxruntime.dll.sha256";
	bool stamp_matches = false;
	if (std::filesystem::exists(destination) && std::filesystem::exists(stamp_path)) {
		std::ifstream in(stamp_path);
		std::string existing;
		std::getline(in, existing);
		stamp_matches = existing == sha;
	}
	if (!stamp_matches) {
		if (!WriteFileAtomically(destination, data, static_cast<size_t>(size), error)) {
			return false;
		}
		std::ofstream stamp_out(stamp_path, std::ios::trunc);
		stamp_out << sha;
	}
	*dll_path = destination;
	return true;
}

} // namespace
#endif

bool EnsureOnnxRuntimeLoaded(std::string* error) {
#if defined(_WIN32) && defined(DEPTHGEN_HAS_ORT)
	static std::once_flag once;
	static bool ok = false;
	static std::string message;
	std::call_once(once, []() {
		if (GetModuleHandleW(L"onnxruntime.dll")) {
			ok = true;
			return;
		}
		const std::filesystem::path sidecar = ModuleDirectory() / "onnxruntime.dll";
		if (std::filesystem::exists(sidecar) && LoadFromPath(sidecar, &message)) {
			ok = true;
			return;
		}
		std::filesystem::path extracted;
		if (!ExtractEmbeddedRuntime(&extracted, &message)) {
			ok = false;
			return;
		}
		ok = LoadFromPath(extracted, &message);
	});
	if (!ok && error) {
		*error = message.empty()
			? "DepthGen could not load ONNX Runtime."
			: message;
	}
	return ok;
#else
	(void)error;
	return true;
#endif
}

} // namespace depthgen
