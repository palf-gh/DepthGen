#include "DepthGen_EmbeddedModel.h"

#include <cstdint>

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/getsect.h>
#include <mach-o/loader.h>
#endif

namespace depthgen {
namespace {

#if defined(_WIN32)
constexpr int kZipDepthResourceId = 256;
constexpr int kDav2ResourceId = 257;

HMODULE CurrentModule() noexcept {
	HMODULE module = nullptr;
	GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&CurrentModule),
		&module);
	return module;
}

int ResourceId(DepthModel model) noexcept {
	return model == DepthModel::DepthAnythingV2Small ? kDav2ResourceId : kZipDepthResourceId;
}
#endif

#if defined(__APPLE__)
const char* MachOSection(DepthModel model) noexcept {
	return model == DepthModel::DepthAnythingV2Small ? "__dav2" : "__zipdepth";
}
#endif

} // namespace

bool GetEmbeddedModel(DepthModel model, EmbeddedModelView* view, std::string* error) {
	if (!view) {
		if (error) *error = "DepthGen embedded-model output was not supplied.";
		return false;
	}
	*view = {};

#if defined(_WIN32)
	const HMODULE module = CurrentModule();
	const HRSRC resource = module
		? FindResourceW(module, MAKEINTRESOURCEW(ResourceId(model)), MAKEINTRESOURCEW(10))
		: nullptr;
	const HGLOBAL loaded = resource ? LoadResource(module, resource) : nullptr;
	const DWORD size = resource ? SizeofResource(module, resource) : 0;
	const void* data = loaded ? LockResource(loaded) : nullptr;
	if (!data || size == 0) {
		if (error) {
			*error = std::string("DepthGen could not load its embedded ") +
				DepthModelName(model) + " resource.";
		}
		return false;
	}
	view->data = data;
	view->size = static_cast<size_t>(size);
	return true;
#elif defined(__APPLE__)
	Dl_info image{};
	if (dladdr(reinterpret_cast<const void*>(&GetEmbeddedModel), &image) == 0 ||
		!image.dli_fbase) {
		if (error) *error = "DepthGen could not locate its loaded Mach-O image.";
		return false;
	}
	const auto* header = static_cast<const mach_header_64*>(image.dli_fbase);
	if (header->magic != MH_MAGIC_64) {
		if (error) *error = "DepthGen loaded an unsupported Mach-O image.";
		return false;
	}
	unsigned long size = 0;
	const uint8_t* data = getsectiondata(header, "__DATA_CONST", MachOSection(model), &size);
	if (!data || size == 0) {
		if (error) {
			*error = std::string("DepthGen could not load its embedded ") +
				DepthModelName(model) + " section.";
		}
		return false;
	}
	view->data = data;
	view->size = static_cast<size_t>(size);
	return true;
#else
	(void)model;
	if (error) *error = "DepthGen embedded models are supported only on Windows and macOS.";
	return false;
#endif
}

} // namespace depthgen
