#include "DepthGen_OrtHost.h"

#if defined(_WIN32) && defined(DEPTHGEN_HAS_ORT)
#include "DepthGenOrtEmbed.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#include <Windows.h>

#if defined(ORT_API_MANUAL_INIT)
// See the rationale comment on TryLoadAndInit below before touching anything
// in this block: the plug-in build must never link an ONNX Runtime import
// library, or the Windows loader could bind it to a same-named copy of
// onnxruntime.dll that After Effects has already loaded.
#include <stdexcept>

#include <onnxruntime_cxx_api.h>
#if defined(DEPTHGEN_ORT_DML)
#if __has_include(<dml_provider_factory.h>)
#include <dml_provider_factory.h>
#else
#include <onnxruntime/core/providers/dml/dml_provider_factory.h>
#endif
#endif
#endif
#endif

namespace depthgen {
#if defined(_WIN32) && defined(DEPTHGEN_HAS_ORT)

// Exposed via DepthGen_OrtHost.h so DepthGenOrtHostTests.cpp can exercise
// these directly; every caller in this file still goes through them exactly
// as before.
namespace orthost_detail {

std::filesystem::path ScratchPathFor(const std::filesystem::path& destination) {
	// GetCurrentProcessId() separates the concurrent extractors this guards
	// against (After Effects, Media Encoder, and aerendercore can all load
	// this plug-in at once); the tick count and the sequence number
	// additionally separate repeat calls within one process, since
	// GetTickCount64()'s resolution is coarser than back-to-back calls can
	// rely on.
	static std::atomic<unsigned long> sequence{0};
	const unsigned long ordinal = sequence.fetch_add(1, std::memory_order_relaxed);
	return std::filesystem::path(destination.native() + L"." +
		std::to_wstring(GetCurrentProcessId()) + L"." +
		std::to_wstring(GetTickCount64()) + L"." +
		std::to_wstring(ordinal) + L".tmp");
}

bool WriteFileAtomically(const std::filesystem::path& destination, const void* data, size_t size,
	std::string* error) {
	const std::filesystem::path temporary = ScratchPathFor(destination);
	bool write_failed = false;
	{
		std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
		if (!out) {
			if (error) {
				*error = "DepthGen could not create its ONNX Runtime cache file.";
			}
			write_failed = true;
		} else {
			out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
			if (!out) {
				if (error) {
					*error = "DepthGen could not write its ONNX Runtime cache file.";
				}
				write_failed = true;
			}
		}
	} // The scratch file closes here, whether or not the write succeeded.
	if (write_failed) {
		std::error_code remove_status;
		std::filesystem::remove(temporary, remove_status);
		return false;
	}

	std::error_code status;
	std::filesystem::rename(temporary, destination, status);
	if (status) {
		std::error_code remove_existing_status;
		std::filesystem::remove(destination, remove_existing_status);
		std::filesystem::rename(temporary, destination, status);
	}
	if (status) {
		// A concurrent extractor may have already recreated the destination
		// (or still hold it open, e.g. mapped by another process's already
		// running session) between our remove and rename above, so both fail
		// again. The cache directory is keyed by the payload's own SHA-256,
		// so anything genuinely at this path is the same payload; accept it
		// rather than failing the launch when the size matches.
		std::error_code size_status;
		const uintmax_t existing_size = std::filesystem::file_size(destination, size_status);
		if (!size_status && existing_size == static_cast<uintmax_t>(size)) {
			std::error_code remove_status;
			std::filesystem::remove(temporary, remove_status);
			return true;
		}
		if (error) {
			*error = "DepthGen could not finalise its ONNX Runtime cache file.";
		}
		std::error_code remove_status;
		std::filesystem::remove(temporary, remove_status);
		return false;
	}
	return true;
}

} // namespace orthost_detail

namespace {

constexpr int kOrtDllResourceId = 258;
constexpr wchar_t kPrivateRuntimeFileName[] = L"DepthGen-onnxruntime.dll";

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

#if !defined(ORT_API_MANUAL_INIT)
// Only the non-manual-init build (test executables, which link ONNX Runtime
// normally) ever loads a module without also binding the API through it;
// the plug-in build binds and loads together in TryLoadAndInit below.
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
#endif

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

	// Best-effort cleanup of the plain "onnxruntime.dll" this cache directory
	// held before 1.0.1 gave the extracted runtime a private name. Ignore
	// failure: another still-running process may still have the old file
	// mapped.
	std::error_code legacy_status;
	std::filesystem::remove(directory / L"onnxruntime.dll", legacy_status);
	std::filesystem::remove(directory / L"onnxruntime.dll.sha256", legacy_status);

	const std::filesystem::path destination = directory / kPrivateRuntimeFileName;
	const std::filesystem::path stamp_path =
		directory / (std::wstring(kPrivateRuntimeFileName) + L".sha256");
	std::error_code exists_status;
	bool stamp_matches = false;
	if (std::filesystem::exists(destination, exists_status) &&
		std::filesystem::exists(stamp_path, exists_status)) {
		std::ifstream in(stamp_path);
		std::string existing;
		std::getline(in, existing);
		stamp_matches = existing == sha;
	}
	if (!stamp_matches) {
		if (!orthost_detail::WriteFileAtomically(destination, data, static_cast<size_t>(size), error)) {
			return false;
		}
		// A torn or stale stamp only costs one redundant re-extraction on the
		// next launch (all SHA-256 stamps are the same length, so a blocked
		// write's size-match acceptance in WriteFileAtomically cannot
		// mistake a stale stamp for this one; it can only delay noticing).
		// The DLL itself is already in place and about to be loaded by path,
		// so a stamp write failure must not fail this launch.
		std::string stamp_error;
		orthost_detail::WriteFileAtomically(stamp_path, sha.data(), sha.size(), &stamp_error);
	}
	*dll_path = destination;
	return true;
}

#if defined(ORT_API_MANUAL_INIT)
HMODULE g_ort_module = nullptr;

// Resolves and binds the ONNX Runtime API from a specific, already-loaded
// module by name, rather than calling OrtGetApiBase() directly. A direct
// call needs an import-library entry for "onnxruntime.dll", and the Windows
// loader resolves that entry by base name alone: whenever After Effects (or
// Premiere Pro, or Media Encoder) has already loaded its own onnxruntime.dll
// -- which any ML-based host feature does -- a direct call would silently
// bind to that copy instead of DepthGen's pinned one. On AE 2023 that host
// copy is API 13 against this header's API 17, GetApi() returns nullptr, and
// the first Ort:: call null-derefs; on newer hosts it would instead
// "succeed" against an untested, unpinned runtime build. DepthGen's own copy
// is extracted under a private file name specifically so the loader can
// never make this substitution; resolving OrtGetApiBase via GetProcAddress
// on that specific module is what makes the substitution impossible rather
// than merely unlikely. Do not simplify this back into a direct call.
bool TryLoadAndInit(const std::filesystem::path& dll_path, std::string* error) {
	const HMODULE module = LoadLibraryW(dll_path.c_str());
	if (!module) {
		if (error) {
			*error = "DepthGen could not load its private ONNX Runtime from " + dll_path.u8string() + ".";
		}
		return false;
	}
	using GetApiBaseFn = decltype(&OrtGetApiBase);
	const auto get_api_base = reinterpret_cast<GetApiBaseFn>(GetProcAddress(module, "OrtGetApiBase"));
	if (!get_api_base) {
		if (error) {
			*error = "DepthGen's private ONNX Runtime at " + dll_path.u8string() +
				" does not export OrtGetApiBase.";
		}
		FreeLibrary(module);
		return false;
	}
	const OrtApiBase* api_base = get_api_base();
	const OrtApi* api = api_base ? api_base->GetApi(ORT_API_VERSION) : nullptr;
	if (!api) {
		// A resident host onnxruntime.dll can never reach this branch --
		// DepthGen's private module name keeps the two apart -- so this
		// guards a mismatched *private* runtime instead, e.g. a developer
		// sidecar built against a different ORT_API_VERSION than this header.
		if (error) {
			*error = "DepthGen's private ONNX Runtime at " + dll_path.u8string() +
				" does not provide ONNX Runtime API version " + std::to_string(ORT_API_VERSION) + ".";
		}
		FreeLibrary(module);
		return false;
	}
	Ort::InitApi(api);
	g_ort_module = module;
	return true;
}
#endif

} // namespace

#if defined(ORT_API_MANUAL_INIT)
// Resolves OrtSessionOptionsAppendExecutionProvider_DML the same way
// TryLoadAndInit resolves OrtGetApiBase, and for the same reason: see the
// rationale comment there.
OrtStatus* AppendDmlExecutionProviderFromHost(OrtSessionOptions* options, int device_id) {
#if defined(DEPTHGEN_ORT_DML)
	using AppendDmlFn = decltype(&OrtSessionOptionsAppendExecutionProvider_DML);
	static const AppendDmlFn append_dml = reinterpret_cast<AppendDmlFn>(
		g_ort_module ? GetProcAddress(g_ort_module, "OrtSessionOptionsAppendExecutionProvider_DML") : nullptr);
	if (!append_dml) {
		throw std::runtime_error(
			"DepthGen's private ONNX Runtime does not export OrtSessionOptionsAppendExecutionProvider_DML.");
	}
	return append_dml(options, device_id);
#else
	(void)options;
	(void)device_id;
	throw std::runtime_error("DepthGen was built without DirectML support.");
#endif
}
#endif

#endif // defined(_WIN32) && defined(DEPTHGEN_HAS_ORT)

bool EnsureOnnxRuntimeLoaded(std::string* error) {
#if defined(_WIN32) && defined(DEPTHGEN_HAS_ORT)
	static std::once_flag once;
	static bool ok = false;
	static std::string message;
	std::call_once(once, []() {
#if defined(ORT_API_MANUAL_INIT)
		// No GetModuleHandleW(L"onnxruntime.dll") short-circuit here: that
		// resident-module check is exactly the defect this file fixes. A
		// developer sidecar next to the plug-in is still supported.
		const std::filesystem::path sidecar = ModuleDirectory() / kPrivateRuntimeFileName;
		std::error_code sidecar_exists_status;
		if (std::filesystem::exists(sidecar, sidecar_exists_status) && TryLoadAndInit(sidecar, &message)) {
			ok = true;
			return;
		}
		std::filesystem::path extracted;
		if (!ExtractEmbeddedRuntime(&extracted, &message)) {
			ok = false;
			return;
		}
		ok = TryLoadAndInit(extracted, &message);
#else
		if (GetModuleHandleW(L"onnxruntime.dll")) {
			ok = true;
			return;
		}
		const std::filesystem::path sidecar = ModuleDirectory() / kPrivateRuntimeFileName;
		std::error_code sidecar_exists_status;
		if (std::filesystem::exists(sidecar, sidecar_exists_status) && LoadFromPath(sidecar, &message)) {
			ok = true;
			return;
		}
		std::filesystem::path extracted;
		if (!ExtractEmbeddedRuntime(&extracted, &message)) {
			ok = false;
			return;
		}
		ok = LoadFromPath(extracted, &message);
#endif
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
