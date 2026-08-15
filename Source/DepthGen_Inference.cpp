#include "DepthGen_Inference.h"
#include "DepthGen_ModelIntegrity.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <sstream>

#if defined(DEPTHGEN_HAS_ORT)
#include <onnxruntime_cxx_api.h>
#if defined(DEPTHGEN_ORT_DML)
#if __has_include(<dml_provider_factory.h>)
#include <dml_provider_factory.h>
#else
#include <onnxruntime/core/providers/dml/dml_provider_factory.h>
#endif
#elif defined(DEPTHGEN_ORT_COREML)
#if __has_include(<coreml_provider_factory.h>)
#include <coreml_provider_factory.h>
#else
#include <onnxruntime/core/providers/coreml/coreml_provider_factory.h>
#endif
#endif
#endif

#if defined(_WIN32)
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace depthgen {
namespace {

std::filesystem::path ModulePath() {
#if defined(_WIN32)
	HMODULE module = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&ModulePath), &module)) {
		return {};
	}
	std::array<wchar_t, 32768> path{};
	const DWORD count = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
	return count ? std::filesystem::path(path.data(), path.data() + count) : std::filesystem::path{};
#else
	Dl_info info{};
	if (dladdr(reinterpret_cast<const void*>(&ModulePath), &info) == 0 || !info.dli_fname) {
		return {};
	}
	return std::filesystem::path(info.dli_fname);
#endif
}

std::filesystem::path ResolveModelPath() {
	if (const char* override_path = std::getenv("DEPTHGEN_MODEL_PATH"); override_path && *override_path) {
		return std::filesystem::path(override_path);
	}
	const std::filesystem::path module = ModulePath();
	if (module.empty()) {
		return {};
	}
	const std::filesystem::path plugin_dir = module.parent_path();
#if defined(_WIN32)
	// Windows layout: DepthGen.aex beside Resources/Models/.
	return plugin_dir / "Resources" / "Models" / "depth_anything_v2_vits_dynamic.onnx";
#else
	// macOS layout: Contents/MacOS/DepthGen -> Contents/Resources/Models/.
	return plugin_dir.parent_path() / "Resources" / "Models" /
		"depth_anything_v2_vits_dynamic.onnx";
#endif
}

#if defined(DEPTHGEN_HAS_ORT)
class Runtime final {
public:
	bool Infer(const std::vector<float>& rgb, int width, int height, InferenceResult* result,
		InferenceProvider* provider, std::string* error, InferencePreference preference) {
		std::lock_guard<std::mutex> lock(mutex_);
		const std::filesystem::path model_path = ResolveModelPath();
		if (model_path.empty() || !std::filesystem::is_regular_file(model_path)) {
			if (error) {
				*error = model_path.empty()
					? "DepthGen model is missing. Install the verified Resources/Models asset or set DEPTHGEN_MODEL_PATH."
					: ("DepthGen model is missing (" + model_path.u8string() +
						"). Install the verified Resources/Models asset or set DEPTHGEN_MODEL_PATH.");
			}
			return false;
		}
		if (!EnsureModelIntegrity(model_path, error)) return false;
		try {
			EnsureSession(model_path, provider, preference);
			const std::array<int64_t, 4> shape = {1, 3, height, width};
			Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
			Ort::Value input = Ort::Value::CreateTensor<float>(memory,
				const_cast<float*>(rgb.data()), rgb.size(), shape.data(), shape.size());
			auto run_session = [&]() {
#if defined(DEPTHGEN_TESTING)
				if (provider_ != InferenceProvider::Cpu &&
					std::getenv("DEPTHGEN_TEST_FORCE_ACCELERATOR_EXECUTION_FAILURE")) {
					throw std::runtime_error("forced accelerator execution failure");
				}
#endif
				const char* input_name = input_name_.c_str();
				const char* output_name = output_name_.c_str();
				return session_->Run(Ort::RunOptions{nullptr}, &input_name, &input, 1, &output_name, 1);
			};
			std::vector<Ort::Value> outputs;
			try {
				outputs = run_session();
			} catch (const std::exception& accelerated_failure) {
				if (provider_ == InferenceProvider::Cpu) throw;
				const std::string accelerated_message = accelerated_failure.what();
				try {
					RebuildCpuSession(model_path);
					if (provider) *provider = provider_;
					outputs = run_session();
				} catch (const std::exception& cpu_failure) {
					throw std::runtime_error("accelerated inference failed (" + accelerated_message +
						"); CPU fallback also failed (" + cpu_failure.what() + ").");
				}
			}
			if (outputs.size() != 1 || !outputs[0].IsTensor()) {
				throw std::runtime_error("the model returned no depth tensor");
			}
			const auto info = outputs[0].GetTensorTypeAndShapeInfo();
			const auto output_shape = info.GetShape();
			if (output_shape.size() < 2) {
				throw std::runtime_error("the model returned an invalid depth shape");
			}
			const int output_height = static_cast<int>(output_shape[output_shape.size() - 2]);
			const int output_width = static_cast<int>(output_shape[output_shape.size() - 1]);
			if (output_width <= 0 || output_height <= 0) {
				throw std::runtime_error("the model returned a dynamic depth shape");
			}
			const size_t count = static_cast<size_t>(output_width) * static_cast<size_t>(output_height);
			const float* values = outputs[0].GetTensorData<float>();
			result->width = output_width;
			result->height = output_height;
			result->depth.assign(values, values + count);
			return true;
		} catch (const Ort::Exception& exception) {
			if (error) {
				*error = std::string("ONNX Runtime inference failed: ") + exception.what();
			}
		} catch (const std::exception& exception) {
			if (error) {
				*error = std::string("DepthGen inference failed: ") + exception.what();
			}
		}
		return false;
	}

private:
	void EnsureSession(const std::filesystem::path& model_path, InferenceProvider* provider,
		InferencePreference preference) {
		InferenceProvider requested = InferenceProvider::Cpu;
#if defined(DEPTHGEN_ORT_DML)
		requested = InferenceProvider::DirectML;
#elif defined(DEPTHGEN_ORT_COREML)
		requested = InferenceProvider::CoreML;
#endif
		if (preference == InferencePreference::Cpu) {
			requested = InferenceProvider::Cpu;
		}
		if (session_ && model_path_ == model_path && requested_provider_ == requested) {
			if (provider) {
				*provider = provider_;
			}
			return;
		}
 		// Provider factories are intentionally compile-time opt-in. Distribution
		// builds link a pinned ORT build with the appropriate provider enabled;
		// development builds remain a correct CPU implementation. If a provider
		// rejects the model or device, rebuild the session on CPU rather than
		// making a frame-dependent acceleration decision.
		try {
#if defined(DEPTHGEN_TESTING)
			if (requested != InferenceProvider::Cpu && std::getenv("DEPTHGEN_TEST_FORCE_ACCELERATOR_FAILURE")) {
				throw std::runtime_error("forced accelerator initialisation failure");
			}
#endif
			SetSession(model_path, CreateSession(model_path, requested), requested);
		} catch (const std::exception&) {
			if (requested == InferenceProvider::Cpu) {
				throw;
			}
			RebuildCpuSession(model_path);
		}
		requested_provider_ = requested;
		if (provider) {
			*provider = provider_;
		}
	}

	std::unique_ptr<Ort::Session> CreateSession(const std::filesystem::path& model_path,
		InferenceProvider provider) {
		Ort::SessionOptions options;
		options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
		options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
#if defined(DEPTHGEN_ORT_DML)
		if (provider == InferenceProvider::DirectML) {
			Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(options, 0));
		}
#elif defined(DEPTHGEN_ORT_COREML)
		if (provider == InferenceProvider::CoreML) {
			Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_CoreML(options, 0));
		}
#endif
		return std::make_unique<Ort::Session>(Environment(), model_path.c_str(), options);
	}

	void SetSession(const std::filesystem::path& model_path, std::unique_ptr<Ort::Session> session,
		InferenceProvider provider) {
		session_ = std::move(session);
		provider_ = provider;
		Ort::AllocatorWithDefaultOptions allocator;
		auto input_name = session_->GetInputNameAllocated(0, allocator);
		auto output_name = session_->GetOutputNameAllocated(0, allocator);
		input_name_ = input_name.get();
		output_name_ = output_name.get();
		model_path_ = model_path;
	}

	void RebuildCpuSession(const std::filesystem::path& model_path) {
		SetSession(model_path, CreateSession(model_path, InferenceProvider::Cpu), InferenceProvider::Cpu);
	}

	bool EnsureModelIntegrity(const std::filesystem::path& model_path, std::string* error) {
		std::error_code status_error;
		const uintmax_t size = std::filesystem::file_size(model_path, status_error);
		if (status_error) {
			if (error) *error = "DepthGen could not inspect the model before SHA-256 verification.";
			return false;
		}
		const auto modified = std::filesystem::last_write_time(model_path, status_error);
		if (status_error) {
			if (error) *error = "DepthGen could not inspect the model timestamp before SHA-256 verification.";
			return false;
		}
		if (verified_model_path_ == model_path && verified_model_size_ == size &&
			verified_model_modified_ == modified) return true;
		if (!VerifyDepthAnythingSmallModel(model_path, error)) return false;
		verified_model_path_ = model_path;
		verified_model_size_ = size;
		verified_model_modified_ = modified;
		return true;
	}

	static Ort::Env& Environment() {
		static Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "DepthGen");
		return environment;
	}

	std::mutex mutex_;
	std::filesystem::path model_path_;
	std::filesystem::path verified_model_path_;
	uintmax_t verified_model_size_ = 0;
	std::filesystem::file_time_type verified_model_modified_{};
	InferenceProvider provider_ = InferenceProvider::Unavailable;
	InferenceProvider requested_provider_ = InferenceProvider::Unavailable;
	std::unique_ptr<Ort::Session> session_;
	std::string input_name_;
	std::string output_name_;
};
#endif

} // namespace

const char* InferenceProviderName(InferenceProvider provider) noexcept {
	switch (provider) {
	case InferenceProvider::DirectML: return "DirectML";
	case InferenceProvider::CoreML: return "Core ML";
	case InferenceProvider::Cpu: return "CPU";
	default: return "Unavailable";
	}
}

bool InferDepthAnythingSmall(
	const std::vector<float>& normalised_nchw_rgb,
	int width,
	int height,
	InferenceResult* result,
	InferenceProvider* provider,
	std::string* error,
	InferencePreference preference) {
	if (!result || width <= 0 || height <= 0 ||
		normalised_nchw_rgb.size() != static_cast<size_t>(width) * static_cast<size_t>(height) * 3U) {
		if (error) {
			*error = "DepthGen received an invalid inference image.";
		}
		return false;
	}
#if defined(DEPTHGEN_HAS_ORT)
	static Runtime runtime;
	return runtime.Infer(normalised_nchw_rgb, width, height, result, provider, error, preference);
#else
	(void)normalised_nchw_rgb;
	(void)preference;
	if (provider) {
		*provider = InferenceProvider::Unavailable;
	}
	if (error) {
		*error = "This DepthGen build has no ONNX Runtime. Configure CMake with DEPTHGEN_ORT_ROOT to enable CPU, DirectML, or Core ML inference.";
	}
	return false;
#endif
}

} // namespace depthgen
