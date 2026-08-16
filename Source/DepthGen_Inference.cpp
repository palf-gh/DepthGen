#include "DepthGen_Inference.h"
#if defined(DEPTHGEN_EMBEDDED_MODEL)
#include "DepthGen_EmbeddedModel.h"
#endif
#include "DepthGen_ModelIntegrity.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <sstream>

#if defined(DEPTHGEN_HAS_ORT)
#include "DepthGen_OrtHost.h"
#include <onnxruntime_cxx_api.h>
#if defined(DEPTHGEN_ORT_DML)
#if __has_include(<dml_provider_factory.h>)
#include <dml_provider_factory.h>
#else
#include <onnxruntime/core/providers/dml/dml_provider_factory.h>
#endif
#endif
#if defined(DEPTHGEN_ORT_COREML)
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

std::filesystem::path ResolveModelPath(DepthModel model) {
	const char* override_name = model == DepthModel::DepthAnythingV2Small
		? "DEPTHGEN_DAV2_MODEL_PATH" : "DEPTHGEN_MODEL_PATH";
	if (const char* override_path = std::getenv(override_name); override_path && *override_path) {
		return std::filesystem::path(override_path);
	}
	const std::filesystem::path module = ModulePath();
	if (module.empty()) {
		return {};
	}
	const std::filesystem::path plugin_dir = module.parent_path();
	const char* filename = model == DepthModel::DepthAnythingV2Small
		? "depth_anything_v2_vits_dml.onnx" : "zipdepth_base_npu_dynamic.onnx";
#if defined(_WIN32)
	return plugin_dir / "Resources" / "Models" / filename;
#else
	return plugin_dir.parent_path() / "Resources" / "Models" / filename;
#endif
}

const char* ExpectedSha256(DepthModel model) noexcept {
	return model == DepthModel::DepthAnythingV2Small ? kDav2SmallModelSha256 : kZipDepthModelSha256;
}

struct ModelSource {
	DepthModel kind = DepthModel::ZipDepth;
	std::filesystem::path path;
	const void* data = nullptr;
	size_t size = 0;

	bool IsEmbedded() const noexcept {
		return data != nullptr && size > 0;
	}

	std::string Key() const {
		return std::string(IsEmbedded() ? "embedded:" : "file:") + DepthModelName(kind) +
			(IsEmbedded() ? std::string() : (":" + path.u8string()));
	}
};

bool ResolveModelSource(DepthModel kind, ModelSource* model, std::string* error) {
	if (!model) {
		if (error) *error = "DepthGen model source output was not supplied.";
		return false;
	}
	*model = {};
	model->kind = kind;
#if defined(DEPTHGEN_EMBEDDED_MODEL)
	EmbeddedModelView embedded;
	if (!GetEmbeddedModel(kind, &embedded, error)) return false;
	model->data = embedded.data;
	model->size = embedded.size;
#else
	model->path = ResolveModelPath(kind);
	if (model->path.empty() || !std::filesystem::is_regular_file(model->path)) {
		if (error) {
			*error = model->path.empty()
				? "DepthGen model is missing. Set DEPTHGEN_MODEL_PATH or DEPTHGEN_DAV2_MODEL_PATH to the verified ONNX asset."
				: ("DepthGen model is missing (" + model->path.u8string() +
					"). Set DEPTHGEN_MODEL_PATH or DEPTHGEN_DAV2_MODEL_PATH to the verified ONNX asset.");
		}
		return false;
	}
#endif
	return true;
}

#if defined(DEPTHGEN_HAS_ORT)
class Runtime final {
public:
	bool Infer(const std::vector<float>& rgb, int width, int height, DepthModel model,
		InferenceResult* result, InferenceProvider* provider, std::string* error,
		InferencePreference preference) {
		std::lock_guard<std::mutex> lock(mutex_);
		ModelSource source;
		if (!ResolveModelSource(model, &source, error) || !EnsureModelIntegrity(source, error)) return false;
		try {
			EnsureSession(source, width, height, provider, preference);
			CachedSession& slot = SlotFor(model);
			const std::array<int64_t, 4> shape = {1, 3, height, width};
			Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
			Ort::Value input = Ort::Value::CreateTensor<float>(memory,
				const_cast<float*>(rgb.data()), rgb.size(), shape.data(), shape.size());
			auto run_session = [&]() {
#if defined(DEPTHGEN_TESTING)
				if (slot.provider != InferenceProvider::Cpu &&
					std::getenv("DEPTHGEN_TEST_FORCE_ACCELERATOR_EXECUTION_FAILURE")) {
					throw std::runtime_error("forced accelerator execution failure");
				}
#endif
				const char* input_name = slot.input_name.c_str();
				const char* output_name = slot.output_name.c_str();
				return slot.session->Run(Ort::RunOptions{nullptr}, &input_name, &input, 1, &output_name, 1);
			};
			std::vector<Ort::Value> outputs;
			try {
				outputs = run_session();
		} catch (const std::exception& accelerated_failure) {
			if (slot.provider == InferenceProvider::Cpu) throw;
			const std::string accelerated_message = accelerated_failure.what();
			try {
				RebuildFallbackSession(source, width, height, slot.provider);
				if (provider) *provider = slot.provider;
				outputs = run_session();
			} catch (const std::exception& fallback_failure) {
				throw std::runtime_error("accelerated inference failed (" + accelerated_message +
					"); fallback inference also failed (" + fallback_failure.what() + ").");
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
	struct CachedSession {
		std::unique_ptr<Ort::Session> session;
		std::string model_key;
		int width = 0;
		int height = 0;
		InferenceProvider provider = InferenceProvider::Unavailable;
		InferencePreference preference = InferencePreference::Accelerated;
		std::string input_name;
		std::string output_name;
	};

	CachedSession& SlotFor(DepthModel model) {
		return sessions_[model == DepthModel::DepthAnythingV2Small ? 1 : 0];
	}

	static bool UsesExclusiveDevice(InferenceProvider provider) noexcept {
		return provider == InferenceProvider::Cuda ||
			provider == InferenceProvider::DirectML ||
			provider == InferenceProvider::CoreML;
	}

	static void ClearSlot(CachedSession* slot) {
		if (slot) {
			*slot = CachedSession{};
		}
	}

	void ReleaseExclusiveDeviceSessions() {
		// DirectML, CUDA, and Core ML are not reliable with two live sessions on
		// the same adapter (After Effects already holds a GPU device). Keep at
		// most one exclusive-device session; CPU sessions of the other model may
		// remain cached.
		for (CachedSession& slot : sessions_) {
			if (slot.session && UsesExclusiveDevice(slot.provider)) {
				ClearSlot(&slot);
			}
		}
	}

	static std::vector<InferenceProvider> ProviderCandidates(InferencePreference preference) {
		if (preference == InferencePreference::Cpu) {
			return {InferenceProvider::Cpu};
		}
		std::vector<InferenceProvider> candidates;
#if defined(DEPTHGEN_ORT_CUDA)
		candidates.push_back(InferenceProvider::Cuda);
#endif
#if defined(DEPTHGEN_ORT_DML)
		candidates.push_back(InferenceProvider::DirectML);
#endif
#if defined(DEPTHGEN_ORT_COREML)
		candidates.push_back(InferenceProvider::CoreML);
#endif
		candidates.push_back(InferenceProvider::Cpu);
		return candidates;
	}

	void EnsureSession(const ModelSource& model, int width, int height,
		InferenceProvider* provider, InferencePreference preference) {
		CachedSession& slot = SlotFor(model.kind);
		const std::string model_key = model.Key();
		if (slot.session && slot.model_key == model_key && slot.width == width &&
			slot.height == height && slot.preference == preference) {
			if (provider) {
				*provider = slot.provider;
			}
			return;
		}
		// Destroy GPU sessions before constructing a replacement. Creating a
		// second DirectML session while ZipDepth is still loaded is an After
		// Effects crash on model switch.
		ReleaseExclusiveDeviceSessions();
		ClearSlot(&slot);
		CreateFirstAvailableSession(model, width, height, preference, 0, &slot);
		if (provider) {
			*provider = slot.provider;
		}
	}

	void CreateFirstAvailableSession(const ModelSource& model, int width, int height,
		InferencePreference preference, size_t first_candidate, CachedSession* slot) {
		const auto candidates = ProviderCandidates(preference);
		std::string failures;
		for (size_t index = first_candidate; index < candidates.size(); ++index) {
			const InferenceProvider candidate = candidates[index];
			try {
#if defined(DEPTHGEN_TESTING)
				if (candidate != InferenceProvider::Cpu &&
					std::getenv("DEPTHGEN_TEST_FORCE_ACCELERATOR_FAILURE")) {
					throw std::runtime_error("forced accelerator initialisation failure");
				}
#endif
				SetSession(slot, model.Key(), width, height,
					CreateSession(model, width, height, candidate), candidate, preference);
				return;
			} catch (const std::exception& exception) {
				if (!failures.empty()) failures += "; ";
				failures += InferenceProviderName(candidate);
				failures += ": ";
				failures += exception.what();
			}
		}
		throw std::runtime_error("no ONNX Runtime execution provider could run DepthGen (" + failures + ")");
	}

	std::unique_ptr<Ort::Session> CreateSession(const ModelSource& model,
		int width, int height, InferenceProvider provider) {
		// Register the logger before provider factories run; ORT >= 1.22 can
		// consult DefaultLogger while validating Core ML provider options.
		Ort::Env& environment = Environment();
		Ort::SessionOptions options;
		// ORT 1.17.3's DirectML graph optimiser miscompiles this model once the
		// output height reaches 1024 pixels. The unoptimised graph remains fully
		// assigned to DirectML and matches CPU output, so constrain only that
		// affected provider/shape range.
		if (provider == InferenceProvider::DirectML && height >= 1024) {
			options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);
		} else {
			options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
		}
		options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
		// Concretising the dynamic axes before provider partitioning is required
		// for correct Core ML output and lets DirectML optimise this exact render
		// size. CUDA 1.27 can crash on a repeated run after that override, so keep
		// the graph dynamic there.
		if (provider != InferenceProvider::Cuda) {
			Ort::ThrowOnError(Ort::GetApi().AddFreeDimensionOverrideByName(options, "height", height));
			Ort::ThrowOnError(Ort::GetApi().AddFreeDimensionOverrideByName(options, "width", width));
		}
#if defined(DEPTHGEN_ORT_CUDA)
		if (provider == InferenceProvider::Cuda) {
			Ort::CUDAProviderOptions cuda_options;
			cuda_options.Update({{"device_id", "0"}});
			options.AppendExecutionProvider_CUDA_V2(*cuda_options);
		}
#endif
#if defined(DEPTHGEN_ORT_DML)
		if (provider == InferenceProvider::DirectML) {
			options.DisableMemPattern();
			Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(options, 0));
		}
#endif
#if defined(DEPTHGEN_ORT_COREML)
		if (provider == InferenceProvider::CoreML) {
			std::vector<uint32_t> flag_sets;
			uint32_t neural_network = 0;
#if defined(DEPTHGEN_COREML_CPU_AND_GPU)
			neural_network |= COREML_FLAG_USE_CPU_AND_GPU;
#endif
			flag_sets.push_back(neural_network);
#if defined(DEPTHGEN_COREML_MLPROGRAM)
			uint32_t mlprogram = COREML_FLAG_CREATE_MLPROGRAM;
#if defined(DEPTHGEN_COREML_CPU_AND_GPU)
			mlprogram |= COREML_FLAG_USE_CPU_AND_GPU;
#endif
			flag_sets.push_back(mlprogram);
#endif
			std::string coreml_failures;
			for (uint32_t flags : flag_sets) {
				try {
					Ort::SessionOptions coreml_options;
					coreml_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
					coreml_options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
					Ort::ThrowOnError(Ort::GetApi().AddFreeDimensionOverrideByName(coreml_options, "height", height));
					Ort::ThrowOnError(Ort::GetApi().AddFreeDimensionOverrideByName(coreml_options, "width", width));
					Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_CoreML(coreml_options, flags));
					if (model.IsEmbedded()) {
						return std::make_unique<Ort::Session>(environment, model.data, model.size, coreml_options);
					}
					return std::make_unique<Ort::Session>(environment, model.path.c_str(), coreml_options);
				} catch (const std::exception& exception) {
					if (!coreml_failures.empty()) coreml_failures += "; ";
#if defined(DEPTHGEN_COREML_MLPROGRAM)
					coreml_failures += (flags & COREML_FLAG_CREATE_MLPROGRAM) ? "MLProgram: " : "NeuralNetwork: ";
#else
					coreml_failures += "NeuralNetwork: ";
#endif
					coreml_failures += exception.what();
				}
			}
			throw std::runtime_error("Core ML could not create a session (" + coreml_failures + ")");
		}
#endif
		if (model.IsEmbedded()) {
			return std::make_unique<Ort::Session>(environment, model.data, model.size, options);
		}
		return std::make_unique<Ort::Session>(environment, model.path.c_str(), options);
	}

	void SetSession(CachedSession* slot, const std::string& model_key, int width, int height,
		std::unique_ptr<Ort::Session> session, InferenceProvider provider,
		InferencePreference preference) {
		slot->session = std::move(session);
		slot->provider = provider;
		Ort::AllocatorWithDefaultOptions allocator;
		auto input_name = slot->session->GetInputNameAllocated(0, allocator);
		auto output_name = slot->session->GetOutputNameAllocated(0, allocator);
		slot->input_name = input_name.get();
		slot->output_name = output_name.get();
		slot->model_key = model_key;
		slot->width = width;
		slot->height = height;
		slot->preference = preference;
	}

	void RebuildFallbackSession(const ModelSource& model, int width, int height,
		InferenceProvider failed_provider) {
		CachedSession& slot = SlotFor(model.kind);
		const InferencePreference preference = slot.preference;
		const auto candidates = ProviderCandidates(preference);
		const auto failed = std::find(candidates.begin(), candidates.end(), failed_provider);
		const size_t next = failed == candidates.end() ? 0 : (failed - candidates.begin()) + 1;
		if (next >= candidates.size()) {
			throw std::runtime_error("no lower-priority execution provider remains");
		}
		ReleaseExclusiveDeviceSessions();
		ClearSlot(&slot);
		CreateFirstAvailableSession(model, width, height, preference, next, &slot);
	}

	bool EnsureModelIntegrity(const ModelSource& model, std::string* error) {
		const char* expected = ExpectedSha256(model.kind);
		if (model.IsEmbedded()) {
			bool& verified = model.kind == DepthModel::DepthAnythingV2Small
				? embedded_dav2_verified_ : embedded_zipdepth_verified_;
			if (verified) return true;
			if (!VerifyModelSha256(model.data, model.size, expected, error)) return false;
			verified = true;
			return true;
		}
		std::error_code status_error;
		const uintmax_t size = std::filesystem::file_size(model.path, status_error);
		if (status_error) {
			if (error) *error = "DepthGen could not inspect the model before SHA-256 verification.";
			return false;
		}
		const auto modified = std::filesystem::last_write_time(model.path, status_error);
		if (status_error) {
			if (error) *error = "DepthGen could not inspect the model timestamp before SHA-256 verification.";
			return false;
		}
		if (verified_model_path_ == model.path && verified_model_size_ == size &&
			verified_model_modified_ == modified) return true;
		if (!VerifyModelSha256(model.path, expected, error)) return false;
		verified_model_path_ = model.path;
		verified_model_size_ = size;
		verified_model_modified_ = modified;
		return true;
	}

	static Ort::Env& Environment() {
#if defined(DEPTHGEN_ORT_CUDA)
		// CUDA EP teardown in current ORT packages can dereference a dead logger
		// during process/module unload. Process-lifetime ownership avoids that
		// shutdown-order defect; the OS reclaims the singleton on process exit.
		static Ort::Env* environment = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "DepthGen");
		return *environment;
#else
		static Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "DepthGen");
		return environment;
#endif
	}

	std::mutex mutex_;
	CachedSession sessions_[2];
	bool embedded_zipdepth_verified_ = false;
	bool embedded_dav2_verified_ = false;
	std::filesystem::path verified_model_path_;
	uintmax_t verified_model_size_ = 0;
	std::filesystem::file_time_type verified_model_modified_{};
};
#endif

} // namespace

const char* InferenceProviderName(InferenceProvider provider) noexcept {
	switch (provider) {
	case InferenceProvider::Cuda: return "CUDA";
	case InferenceProvider::DirectML: return "DirectML";
	case InferenceProvider::CoreML: return "Core ML";
	case InferenceProvider::Cpu: return "CPU";
	default: return "Unavailable";
	}
}

const char* DepthModelName(DepthModel model) noexcept {
	switch (model) {
	case DepthModel::DepthAnythingV2Small: return "Depth Anything V2 Small";
	default: return "ZipDepth";
	}
}

bool InferDepth(
	const std::vector<float>& nchw_rgb,
	int width,
	int height,
	DepthModel model,
	InferenceResult* result,
	InferenceProvider* provider,
	std::string* error,
	InferencePreference preference) {
	if (!result || width <= 0 || height <= 0 ||
		nchw_rgb.size() != static_cast<size_t>(width) * static_cast<size_t>(height) * 3U) {
		if (error) {
			*error = "DepthGen received an invalid inference image.";
		}
		return false;
	}
#if defined(DEPTHGEN_HAS_ORT)
	if (!EnsureOnnxRuntimeLoaded(error)) {
		return false;
	}
#if defined(DEPTHGEN_ORT_CUDA)
	static Runtime* runtime = new Runtime();
	return runtime->Infer(nchw_rgb, width, height, model, result, provider, error, preference);
#else
	static Runtime runtime;
	return runtime.Infer(nchw_rgb, width, height, model, result, provider, error, preference);
#endif
#else
	(void)nchw_rgb;
	(void)model;
	(void)preference;
	if (provider) {
		*provider = InferenceProvider::Unavailable;
	}
	if (error) {
		*error = "This DepthGen build has no ONNX Runtime. Configure CMake with DEPTHGEN_ORT_ROOT to enable CPU, CUDA, DirectML, or Core ML inference.";
	}
	return false;
#endif
}

} // namespace depthgen
