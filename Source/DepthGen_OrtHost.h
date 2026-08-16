#pragma once

#include <string>

namespace depthgen {

// Windows: delay-loads onnxruntime.dll from a LocalAppData cache extracted from
// the plug-in resource, or from a sidecar next to the current module (tests).
// Other platforms: no-op.
bool EnsureOnnxRuntimeLoaded(std::string* error);

} // namespace depthgen
