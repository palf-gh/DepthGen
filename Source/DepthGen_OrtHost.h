#pragma once

#include <string>

#if defined(_WIN32) && defined(DEPTHGEN_HAS_ORT)
#include <cstddef>
#include <filesystem>
#endif

#if defined(_WIN32) && defined(DEPTHGEN_HAS_ORT) && defined(ORT_API_MANUAL_INIT)
// Opaque ONNX Runtime C API handles, forward-declared exactly as
// onnxruntime_c_api.h declares them (ORT_RUNTIME_CLASS never gives them a
// public body, so this is not a guess at their layout). Forward-declaring
// them here lets DepthGen_Inference.cpp see AppendDmlExecutionProviderFromHost's
// signature whether it includes this header before or after the ONNX Runtime
// SDK headers.
struct OrtStatus;
struct OrtSessionOptions;
#endif

namespace depthgen {

// Windows: binds DepthGen's own privately named ONNX Runtime, extracted from
// the plug-in resource into a LocalAppData cache, or loaded from a developer
// sidecar next to the current module. The plug-in build (ORT_API_MANUAL_INIT)
// never consults a same-named module the host process may already have
// loaded; see the rationale comment on TryLoadAndInit in DepthGen_OrtHost.cpp.
// Other platforms, and builds with no configured ONNX Runtime SDK, are a
// no-op that always succeeds.
bool EnsureOnnxRuntimeLoaded(std::string* error);

#if defined(_WIN32) && defined(DEPTHGEN_HAS_ORT)
// Exposed only so DepthGenOrtHostTests.cpp can exercise the extraction
// primitives directly; behaviour for the plug-in and the other test
// executables is unchanged by this exposure.
namespace orthost_detail {

// A scratch path beside `destination` that is unique per process and per
// call, so concurrent extractors (After Effects, Media Encoder, and
// aerendercore can all load this plug-in at once) never collide.
std::filesystem::path ScratchPathFor(const std::filesystem::path& destination);

// Writes `size` bytes from `data` to `destination` through a uniquely named
// scratch file and an atomic rename. Tolerates a concurrent writer reaching
// `destination` first, provided it left behind a file of the same size.
bool WriteFileAtomically(const std::filesystem::path& destination, const void* data, size_t size,
	std::string* error);

} // namespace orthost_detail
#endif

#if defined(_WIN32) && defined(DEPTHGEN_HAS_ORT) && defined(ORT_API_MANUAL_INIT)
// Appends the DirectML execution provider by resolving
// OrtSessionOptionsAppendExecutionProvider_DML from DepthGen's own privately
// loaded ONNX Runtime module (see DepthGen_OrtHost.cpp), instead of linking
// the export directly: the plug-in build has no ONNX Runtime import library
// to link against. Throws std::runtime_error if the export cannot be
// resolved; otherwise returns whatever the export itself returns, so
// Ort::ThrowOnError at the call site behaves exactly as it would for a
// direct call.
OrtStatus* AppendDmlExecutionProviderFromHost(OrtSessionOptions* options, int device_id);
#endif

} // namespace depthgen
