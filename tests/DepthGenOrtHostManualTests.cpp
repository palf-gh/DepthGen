// Exercises the manual-init loader that ships only inside DepthGen.aex.
// Every other test target compiles DepthGen_OrtHost.cpp without
// ORT_API_MANUAL_INIT and so takes the linked-import path instead, leaving
// LoadLibraryW -> GetProcAddress -> GetApi -> InitApi -> DirectML resolution
// untested. A failure here is otherwise silent in the host: a DirectML
// candidate that throws is caught per provider and degrades to a CPU session,
// which still renders correct frames, only far slower and with no message.
#include "DepthGen_OrtHost.h"

#include <onnxruntime_cxx_api.h>

#include <exception>
#include <iostream>
#include <string>

int main() {
	// The sidecar copied beside this executable by the build is the only route
	// into the load path here: a test executable carries no embedded runtime
	// resource for ExtractEmbeddedRuntime to unpack.
	std::string error;
	if (!depthgen::EnsureOnnxRuntimeLoaded(&error)) {
		std::cerr << "FAIL: EnsureOnnxRuntimeLoaded reported: " << error << '\n';
		return 1;
	}

	// Proves Ort::InitApi actually bound the API table. Without it the C++
	// wrapper dereferences a null OrtApi on the first call.
	try {
		Ort::SessionOptions probe;
		probe.SetIntraOpNumThreads(1);
	} catch (const std::exception& exception) {
		std::cerr << "FAIL: the ONNX Runtime API was not bound: " << exception.what() << '\n';
		return 1;
	}

	// The assertion that catches a silently lost DirectML provider.
	try {
		Ort::SessionOptions options;
		options.DisableMemPattern();
		Ort::ThrowOnError(depthgen::AppendDmlExecutionProviderFromHost(options, 0));
	} catch (const Ort::Exception& exception) {
		// A resolved export that declines this adapter is a machine result,
		// not the regression this test guards against; the two messages differ
		// so they stay distinguishable in CTest output.
		std::cerr << "FAIL: DirectML refused the session options: " << exception.what() << '\n';
		return 1;
	} catch (const std::exception& exception) {
		std::cerr << "FAIL: the DirectML provider entry point could not be resolved: "
			<< exception.what() << '\n';
		return 1;
	}

	std::cout << "DepthGen ONNX Runtime manual-init tests passed\n";
	return 0;
}
