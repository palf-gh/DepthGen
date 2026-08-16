#include "DepthGen.h"
#include "DepthGen_Image.h"
#include "DepthGen_Inference.h"
#include "DepthGen_Pipeline.h"
#include "DepthGen_Temporal.h"
#include "Localise/DepthGenStrings.h"
#include "AE_GeneralPlug.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <string>
#include <vector>

static_assert(DEPTHGEN_OUT_FLAGS == (PF_OutFlag_DEEP_COLOR_AWARE |
	PF_OutFlag_SEND_UPDATE_PARAMS_UI | PF_OutFlag_SEQUENCE_DATA_NEEDS_FLATTENING),
	"DEPTHGEN_OUT_FLAGS must match the GlobalSetup out_flags combination");
static_assert(DEPTHGEN_OUT_FLAGS2 == (PF_OutFlag2_SUPPORTS_SMART_RENDER |
	PF_OutFlag2_FLOAT_COLOR_AWARE | PF_OutFlag2_SUPPORTS_THREADED_RENDERING |
	PF_OutFlag2_SUPPORTS_GET_FLATTENED_SEQUENCE_DATA),
	"DEPTHGEN_OUT_FLAGS2 must keep SMART_RENDER with FLOAT_COLOR_AWARE");

namespace {

constexpr std::uint32_t kSequenceMagic = 0x44474E54u; // 'DGNT'
constexpr std::uint32_t kSequenceVersion = 2;
constexpr std::uint32_t kSequenceFlagFlat = 1u;

struct DepthGenSequenceData {
	std::uint32_t magic = kSequenceMagic;
	std::uint32_t version = kSequenceVersion;
	std::uint32_t flags = 0;
	std::uint32_t reserved = 0;
	std::uint64_t cache_id = 0;
};

bool SequenceLooksValid(const void* data) {
	const auto* header = reinterpret_cast<const DepthGenSequenceData*>(data);
	return header && header->magic == kSequenceMagic &&
		(header->version == 1 || header->version == kSequenceVersion);
}

std::uint64_t SequenceCacheId(const void* data) {
	const auto* header = reinterpret_cast<const DepthGenSequenceData*>(data);
	if (!SequenceLooksValid(header)) {
		return 0;
	}
	return header->cache_id;
}

PF_Handle NewSequenceHandle(PF_InData* in_data, A_long size) {
	if (!in_data || !in_data->pica_basicP || size <= 0) {
		return nullptr;
	}
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	if (!suites.HandleSuite1()) {
		return nullptr;
	}
	return suites.HandleSuite1()->host_new_handle(size);
}

void* LockSequenceHandle(PF_InData* in_data, PF_Handle handle) {
	if (!in_data || !in_data->pica_basicP || !handle) {
		return nullptr;
	}
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	if (!suites.HandleSuite1()) {
		return nullptr;
	}
	return suites.HandleSuite1()->host_lock_handle(handle);
}

void UnlockSequenceHandle(PF_InData* in_data, PF_Handle handle) {
	if (!in_data || !in_data->pica_basicP || !handle) {
		return;
	}
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	if (suites.HandleSuite1()) {
		suites.HandleSuite1()->host_unlock_handle(handle);
	}
}

void DisposeSequenceHandle(PF_InData* in_data, PF_Handle handle) {
	if (!in_data || !in_data->pica_basicP || !handle) {
		return;
	}
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	if (suites.HandleSuite1()) {
		suites.HandleSuite1()->host_dispose_handle(handle);
	}
}

void WriteSequence(DepthGenSequenceData* sequence, bool flat, std::uint64_t cache_id) {
	if (!sequence) {
		return;
	}
	sequence->magic = kSequenceMagic;
	sequence->version = kSequenceVersion;
	sequence->flags = flat ? kSequenceFlagFlat : 0;
	sequence->reserved = 0;
	sequence->cache_id = cache_id;
}

PF_Err AllocateSequence(PF_InData* in_data, PF_OutData* out_data) {
	PF_Handle handle = NewSequenceHandle(in_data, static_cast<A_long>(sizeof(DepthGenSequenceData)));
	if (!handle) {
		return PF_Err_OUT_OF_MEMORY;
	}
	auto* sequence = reinterpret_cast<DepthGenSequenceData*>(LockSequenceHandle(in_data, handle));
	if (!sequence) {
		DisposeSequenceHandle(in_data, handle);
		return PF_Err_OUT_OF_MEMORY;
	}
	const std::uint64_t cache_id = depthgen::TemporalCacheCreate();
	WriteSequence(sequence, false, cache_id);
	UnlockSequenceHandle(in_data, handle);
	if (out_data) {
		out_data->sequence_data = handle;
	}
	return PF_Err_NONE;
}

PF_Err SequenceSetup(PF_InData* in_data, PF_OutData* out_data) {
	if (!in_data || !out_data) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}
	return AllocateSequence(in_data, out_data);
}

PF_Err SequenceSetdown(PF_InData* in_data, PF_OutData* out_data) {
	if (!in_data || !in_data->sequence_data) {
		return PF_Err_NONE;
	}
	void* locked = LockSequenceHandle(in_data, in_data->sequence_data);
	depthgen::TemporalCacheRelease(SequenceCacheId(locked));
	UnlockSequenceHandle(in_data, in_data->sequence_data);
	DisposeSequenceHandle(in_data, in_data->sequence_data);
	if (out_data) {
		out_data->sequence_data = nullptr;
	}
	return PF_Err_NONE;
}

PF_Err WriteFlatSequence(PF_InData* in_data, PF_OutData* out_data, bool dispose_unflat) {
	if (!in_data || !out_data || !in_data->sequence_data) {
		return PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	void* locked = LockSequenceHandle(in_data, in_data->sequence_data);
	const std::uint64_t cache_id = SequenceCacheId(locked);
	UnlockSequenceHandle(in_data, in_data->sequence_data);
	PF_Handle flat_handle = NewSequenceHandle(in_data, static_cast<A_long>(sizeof(DepthGenSequenceData)));
	if (!flat_handle) {
		return PF_Err_OUT_OF_MEMORY;
	}
	auto* flat = reinterpret_cast<DepthGenSequenceData*>(LockSequenceHandle(in_data, flat_handle));
	if (!flat) {
		DisposeSequenceHandle(in_data, flat_handle);
		return PF_Err_OUT_OF_MEMORY;
	}
	WriteSequence(flat, true, cache_id);
	UnlockSequenceHandle(in_data, flat_handle);
	if (dispose_unflat) {
		DisposeSequenceHandle(in_data, in_data->sequence_data);
	}
	out_data->sequence_data = flat_handle;
	return PF_Err_NONE;
}

PF_Err SequenceFlatten(PF_InData* in_data, PF_OutData* out_data) {
	return WriteFlatSequence(in_data, out_data, true);
}

PF_Err GetFlattenedSequenceData(PF_InData* in_data, PF_OutData* out_data) {
	return WriteFlatSequence(in_data, out_data, false);
}

PF_Err SequenceResetup(PF_InData* in_data, PF_OutData* out_data) {
	if (!in_data || !out_data) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}
	if (!in_data->sequence_data) {
		return AllocateSequence(in_data, out_data);
	}
	void* locked = LockSequenceHandle(in_data, in_data->sequence_data);
	const auto* header = reinterpret_cast<const DepthGenSequenceData*>(locked);
	const bool already_unflat = SequenceLooksValid(header) && (header->flags & kSequenceFlagFlat) == 0;
	UnlockSequenceHandle(in_data, in_data->sequence_data);
	if (already_unflat) {
		out_data->sequence_data = in_data->sequence_data;
		return PF_Err_NONE;
	}
	// The flat handle's cache_id is not adopted here: After Effects persists it
	// into the project file and copies it on duplicate, so a restored id can
	// collide with a live one or be shared by two instances. Mint a fresh id
	// instead, and only free the flat handle once the new one exists.
	const PF_Err err = AllocateSequence(in_data, out_data);
	if (err == PF_Err_NONE) {
		DisposeSequenceHandle(in_data, in_data->sequence_data);
	}
	return err;
}

PF_Handle SequenceHandleFromRender(PF_InData* in_data) {
	if (!in_data) {
		return nullptr;
	}
	if (in_data->sequence_data) {
		return in_data->sequence_data;
	}
	return nullptr;
}

std::uint64_t CacheIdFromRender(PF_InData* in_data) {
	if (!in_data) {
		return 0;
	}
	// PF_OutFlag2_SUPPORTS_THREADED_RENDERING is set, so the const-sequence-data
	// suite is the sanctioned way to read sequence data at render time; try it
	// first and fall back to the direct handle read below only when the suite
	// is unavailable or does not yield a usable cache id.
	if (in_data->pica_basicP) {
		PF_EffectSequenceDataSuite1* suite = nullptr;
		if (AEFX_AcquireSuite(in_data, nullptr, kPFEffectSequenceDataSuite, kPFEffectSequenceDataSuiteVersion1,
			nullptr, reinterpret_cast<void**>(&suite)) == PF_Err_NONE && suite) {
			PF_ConstHandle const_handle = nullptr;
			const PF_Err err = suite->PF_GetConstSequenceData(in_data->effect_ref, &const_handle);
			(void)AEFX_ReleaseSuite(in_data, nullptr, kPFEffectSequenceDataSuite, kPFEffectSequenceDataSuiteVersion1,
				nullptr);
			if (err == PF_Err_NONE && const_handle) {
				const std::uint64_t cache_id = SequenceCacheId(*const_handle);
				if (cache_id != 0) {
					return cache_id;
				}
			}
		}
	}
	if (in_data->sequence_data) {
		void* locked = LockSequenceHandle(in_data, in_data->sequence_data);
		const std::uint64_t cache_id = SequenceCacheId(locked);
		UnlockSequenceHandle(in_data, in_data->sequence_data);
		if (cache_id != 0) {
			return cache_id;
		}
	}
	return 0;
}

std::shared_ptr<depthgen::TemporalHistory> HistoryFromSequence(PF_InData* in_data) {
	return depthgen::TemporalCacheGet(CacheIdFromRender(in_data));
}

PF_Err ReadSettings(PF_InData* in_data, DepthGenRenderSettings* settings) {
	if (!in_data || !settings) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}
	PF_Err err = PF_Err_NONE;
	PF_ParamDef parameter{};
	auto checkout = [&](A_long index) -> bool {
		AEFX_CLR_STRUCT(parameter);
		err = PF_CHECKOUT_PARAM(in_data, index, in_data->current_time, in_data->time_step,
			in_data->time_scale, &parameter);
		return err == PF_Err_NONE;
	};
	auto checkin = [&]() { (void)PF_CHECKIN_PARAM(in_data, &parameter); };

	if (checkout(ParamIndexFromID(DEPTHGEN_MODEL))) {
		settings->model = parameter.u.pd.value == DEPTHGEN_MODEL_DAV2_SMALL
			? DEPTHGEN_MODEL_DAV2_SMALL : DEPTHGEN_MODEL_ZIPDEPTH;
		checkin();
	}
	A_long quality = DEPTHGEN_QUALITY_BALANCED;
	A_long custom_short_edge = DEPTHGEN_BALANCED_SHORT_EDGE;
	if (checkout(ParamIndexFromID(DEPTHGEN_QUALITY))) {
		quality = parameter.u.pd.value;
		checkin();
	}
	if (checkout(ParamIndexFromID(DEPTHGEN_CUSTOM_SHORT_EDGE))) {
		custom_short_edge = static_cast<A_long>(std::lround(parameter.u.fs_d.value));
		checkin();
	}
	settings->short_edge = DepthGenShortEdge(settings->model, quality, custom_short_edge);
	if (checkout(ParamIndexFromID(DEPTHGEN_FAR_PERCENTILE))) { settings->far_percentile = static_cast<float>(parameter.u.fs_d.value); checkin(); }
	if (checkout(ParamIndexFromID(DEPTHGEN_NEAR_PERCENTILE))) { settings->near_percentile = static_cast<float>(parameter.u.fs_d.value); checkin(); }
	if (checkout(ParamIndexFromID(DEPTHGEN_CONTRAST))) { settings->contrast = static_cast<float>(parameter.u.fs_d.value); checkin(); }
	if (checkout(ParamIndexFromID(DEPTHGEN_INVERT))) { settings->invert = parameter.u.bd.value != 0; checkin(); }
	if (checkout(ParamIndexFromID(DEPTHGEN_TEMPORAL_STABILITY))) {
		settings->temporal_stability = static_cast<float>(parameter.u.fs_d.value);
		checkin();
	}
	if (checkout(ParamIndexFromID(DEPTHGEN_INPUT_TRANSFER))) { settings->linear_to_srgb = parameter.u.pd.value == DEPTHGEN_TRANSFER_LINEAR_TO_SRGB; checkin(); }
	if (checkout(ParamIndexFromID(DEPTHGEN_USE_ALPHA_FOR_LEVELS))) { settings->use_alpha_for_levels = parameter.u.bd.value != 0; checkin(); }
	if (checkout(ParamIndexFromID(DEPTHGEN_ALPHA_THRESHOLD))) { settings->alpha_threshold = static_cast<float>(parameter.u.fs_d.value) / 100.0f; checkin(); }
	if (checkout(ParamIndexFromID(DEPTHGEN_OUTPUT_ALPHA))) { settings->preserve_alpha = parameter.u.pd.value != DEPTHGEN_ALPHA_OPAQUE; checkin(); }
	settings->far_percentile = std::max(0.0f, std::min(100.0f, settings->far_percentile));
	settings->near_percentile = std::max(0.0f, std::min(100.0f, settings->near_percentile));
	settings->contrast = std::max(0.01f, std::min(4.0f, settings->contrast));
	settings->temporal_stability = std::max(0.0f, std::min(100.0f, settings->temporal_stability));
	return PF_Err_NONE;
}

void DisposePreRenderData(void* data) {
	delete reinterpret_cast<DepthGenPreRenderData*>(data);
}

void WriteReturnMessage(PF_OutData* out_data, const char* format, ...) {
	if (!out_data || !format) {
		return;
	}
	va_list args;
	va_start(args, format);
	const int written = std::vsnprintf(out_data->return_msg, sizeof(out_data->return_msg), format, args);
	va_end(args);
	if (written < 0) {
		out_data->return_msg[0] = '\0';
	}
}

PF_Err GlobalSetup(PF_InData* in_data, PF_OutData* out_data) {
	if (!in_data || !out_data) return PF_Err_BAD_CALLBACK_PARAM;
	out_data->my_version = DEPTHGEN_VERSION_PACKED;
	out_data->out_flags = DEPTHGEN_OUT_FLAGS;
	out_data->out_flags2 = DEPTHGEN_OUT_FLAGS2;
	if (!in_data->pica_basicP) return PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	if (!suites.HandleSuite1() || !suites.UtilitySuite5()) return PF_Err_NONE;
	out_data->global_data = suites.HandleSuite1()->host_new_handle(sizeof(DepthGenGlobalData));
	if (!out_data->global_data) return PF_Err_OUT_OF_MEMORY;
	DepthGenGlobalData* data = reinterpret_cast<DepthGenGlobalData*>(
		suites.HandleSuite1()->host_lock_handle(out_data->global_data));
	if (!data) {
		(void)suites.HandleSuite1()->host_dispose_handle(out_data->global_data);
		out_data->global_data = nullptr;
		return PF_Err_OUT_OF_MEMORY;
	}
	data->plugin_id = 0;
	const PF_Err err = suites.UtilitySuite5()->AEGP_RegisterWithAEGP(nullptr, DEPTHGEN_NAME, &data->plugin_id);
	suites.HandleSuite1()->host_unlock_handle(out_data->global_data);
	if (err) {
		(void)suites.HandleSuite1()->host_dispose_handle(out_data->global_data);
		out_data->global_data = nullptr;
	}
	return err;
}

PF_Err GlobalSetdown(PF_InData* in_data) {
	if (in_data && in_data->global_data && in_data->pica_basicP) {
		AEGP_SuiteHandler suites(in_data->pica_basicP);
		if (suites.HandleSuite1()) {
		(void)suites.HandleSuite1()->host_dispose_handle(in_data->global_data);
		}
	}
	return PF_Err_NONE;
}

PF_Err ParamsSetup(PF_InData* in_data, PF_OutData* out_data) {
	if (!in_data || !out_data) return PF_Err_BAD_CALLBACK_PARAM;
	PF_ParamDef def{};
	using depthgen_localise::GetString;
	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_SUPERVISE | PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS;
	PF_ADD_POPUP(GetString(DepthGenString::ModelName, in_data), 2, DEPTHGEN_MODEL_ZIPDEPTH,
		GetString(DepthGenString::ModelItems, in_data), DEPTHGEN_MODEL);
	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_SUPERVISE | PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS;
	PF_ADD_POPUP(GetString(DepthGenString::QualityName, in_data), 4, DEPTHGEN_QUALITY_BALANCED,
		GetString(DepthGenString::QualityItems, in_data), DEPTHGEN_QUALITY);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(GetString(DepthGenString::FarPercentile, in_data), 0, 25, 0, 25, 2, 1,
		PF_ValueDisplayFlag_PERCENT, 0, DEPTHGEN_FAR_PERCENTILE);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(GetString(DepthGenString::NearPercentile, in_data), 75, 100, 75, 100, 98, 1,
		PF_ValueDisplayFlag_PERCENT, 0, DEPTHGEN_NEAR_PERCENTILE);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(GetString(DepthGenString::Contrast, in_data), 0.1, 4.0, 0.1, 4.0, 1.0, 2,
		PF_ValueDisplayFlag_NONE, 0, DEPTHGEN_CONTRAST);
	AEFX_CLR_STRUCT(def);
	PF_ADD_CHECKBOX(GetString(DepthGenString::Invert, in_data), "On", FALSE, 0, DEPTHGEN_INVERT);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(GetString(DepthGenString::TemporalStability, in_data), 0, 100, 0, 100, 0, 1,
		PF_ValueDisplayFlag_PERCENT, PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, DEPTHGEN_TEMPORAL_STABILITY);
	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	def.flags = PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS;
	PF_ADD_CHECKBOX(GetString(DepthGenString::ShowAdvanced, in_data), "On", FALSE, 0, DEPTHGEN_SHOW_ADVANCED);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPICX(GetString(DepthGenString::AdvancedGroup, in_data),
		PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_COLLAPSE_TWIRLY,
		DEPTHGEN_GRP_ADVANCED_START);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX_DISABLED(GetString(DepthGenString::CustomShortEdge, in_data),
		DEPTHGEN_CUSTOM_EDGE_MIN, DEPTHGEN_CUSTOM_EDGE_MAX, DEPTHGEN_CUSTOM_EDGE_MIN,
		DEPTHGEN_CUSTOM_EDGE_MAX, DEPTHGEN_BALANCED_SHORT_EDGE, 0, PF_ValueDisplayFlag_NONE, 0,
		DEPTHGEN_CUSTOM_SHORT_EDGE);
	AEFX_CLR_STRUCT(def);
	PF_ADD_POPUP(GetString(DepthGenString::InputTransfer, in_data), 2, DEPTHGEN_TRANSFER_SRGB,
		GetString(DepthGenString::InputTransferItems, in_data), DEPTHGEN_INPUT_TRANSFER);
	AEFX_CLR_STRUCT(def);
	PF_ADD_CHECKBOX(GetString(DepthGenString::UseAlpha, in_data), "On", TRUE, 0, DEPTHGEN_USE_ALPHA_FOR_LEVELS);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(GetString(DepthGenString::AlphaThreshold, in_data), 0, 100, 0, 100, 0, 1,
		PF_ValueDisplayFlag_PERCENT, 0, DEPTHGEN_ALPHA_THRESHOLD);
	AEFX_CLR_STRUCT(def);
	PF_ADD_POPUP(GetString(DepthGenString::OutputAlpha, in_data), 2, DEPTHGEN_ALPHA_PRESERVE,
		GetString(DepthGenString::OutputAlphaItems, in_data), DEPTHGEN_OUTPUT_ALPHA);
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(DEPTHGEN_GRP_ADVANCED_END);
	out_data->num_params = kDepthGenParamOrderCount;
	return PF_Err_NONE;
}

PF_Err PreRender(PF_InData* in_data, PF_PreRenderExtra* extra) {
	if (!in_data || !extra) return PF_Err_BAD_CALLBACK_PARAM;
	auto data = std::make_unique<DepthGenPreRenderData>();
	PF_Err err = ReadSettings(in_data, &data->settings);
	if (err) return err;
	const A_long render_width = DepthGenRenderWidth(in_data);
	const A_long render_height = DepthGenRenderHeight(in_data);
	if (render_width <= 0 || render_height <= 0) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}
	PF_RenderRequest request = extra->input->output_request;
	const PF_LRect requested = request.rect;
	request.rect.left = 0;
	request.rect.top = 0;
	request.rect.right = render_width;
	request.rect.bottom = render_height;
	request.field = PF_Field_FRAME;
	request.preserve_rgb_of_zero_alpha = TRUE;
	PF_CheckoutResult checkout{};
	err = extra->cb->checkout_layer(in_data->effect_ref, ParamIndexFromID(DEPTHGEN_INPUT),
		DEPTHGEN_INPUT, &request, in_data->current_time, in_data->time_step, in_data->time_scale,
		&checkout);
	if (err) return err;
	(void)checkout;
	extra->output->result_rect = request.rect;
	extra->output->max_result_rect = request.rect;
	if (requested.left != request.rect.left || requested.top != request.rect.top ||
		requested.right != request.rect.right || requested.bottom != request.rect.bottom) {
		extra->output->flags |= PF_RenderOutputFlag_RETURNS_EXTRA_PIXELS;
	}
	extra->output->pre_render_data = data.release();
	extra->output->delete_pre_render_data_func = DisposePreRenderData;
	return PF_Err_NONE;
}

PF_Err SmartRender(PF_InData* in_data, PF_OutData* out_data, PF_SmartRenderExtra* extra) {
	if (!in_data || !out_data || !extra || !extra->input->pre_render_data) return PF_Err_BAD_CALLBACK_PARAM;
	PF_Err err = PF_Err_NONE;
	PF_EffectWorld* input = nullptr;
	PF_EffectWorld* output = nullptr;
	err = extra->cb->checkout_layer_pixels(in_data->effect_ref, DEPTHGEN_INPUT, &input);
	if (!err) err = extra->cb->checkout_output(in_data->effect_ref, &output);
	if (!err && input && output) {
		try {
			AEFX_SuiteScoper<PF_WorldSuite2> world_suite(in_data, kPFWorldSuite, kPFWorldSuiteVersion2, out_data);
			PF_PixelFormat format = PF_PixelFormat_INVALID;
			err = world_suite->PF_GetPixelFormat(input, &format);
			if (!err) {
				const auto history = HistoryFromSequence(in_data);
				err = DepthGen_RenderWorld(in_data, out_data, format, input, output,
					reinterpret_cast<DepthGenPreRenderData*>(extra->input->pre_render_data)->settings,
					history.get(), in_data->current_time, in_data->time_step);
			}
		} catch (const A_long thrown) {
			err = thrown;
		} catch (const std::bad_alloc&) {
			err = PF_Err_OUT_OF_MEMORY;
		} catch (const std::exception&) {
			err = PF_Err_INTERNAL_STRUCT_DAMAGED;
		} catch (...) {
			err = PF_Err_INTERNAL_STRUCT_DAMAGED;
		}
	}
	if (input) (void)extra->cb->checkin_layer_pixels(in_data->effect_ref, DEPTHGEN_INPUT);
	return err;
}

// Polls the host for a user-requested cancellation. DepthGen_RenderWorld
// tolerates a null in_data at other call sites, so this does too: no in_data
// means no host to poll, and PF_Err_NONE lets the caller carry on. Otherwise
// forwards PF_ABORT's result verbatim (non-zero, typically
// PF_Interrupt_CANCEL, when the user has cancelled).
PF_Err CheckAbort(PF_InData* in_data) {
	if (!in_data) {
		return PF_Err_NONE;
	}
	return PF_ABORT(in_data);
}

} // namespace

PF_Err DepthGen_RenderWorld(
	PF_InData* in_data,
	PF_OutData* out_data,
	PF_PixelFormat pixel_format,
	PF_EffectWorld* input_world,
	PF_EffectWorld* output_world,
	const DepthGenRenderSettings& settings,
	depthgen::TemporalHistory* history,
	A_long time,
	A_long time_step) {
	if (!input_world || !output_world || !input_world->data || !output_world->data ||
		input_world->width <= 0 || input_world->height <= 0 ||
		output_world->width <= 0 || output_world->height <= 0) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}
	int inference_width = 0;
	int inference_height = 0;
	const A_long full_width = (in_data && in_data->width > 0) ? in_data->width : input_world->width;
	const A_long full_height = (in_data && in_data->height > 0) ? in_data->height : input_world->height;
	A_long render_width = DepthGenRenderWidth(in_data);
	A_long render_height = DepthGenRenderHeight(in_data);
	if (render_width <= 0) {
		render_width = input_world->width;
	}
	if (render_height <= 0) {
		render_height = input_world->height;
	}
	const int patch = settings.model == DEPTHGEN_MODEL_DAV2_SMALL
		? depthgen::kDav2Patch : depthgen::kZipDepthPatch;
	const int short_edge = depthgen::ScaleShortEdgeToRender(
		full_width, full_height, render_width, render_height, settings.short_edge, patch);
	depthgen::ComputeInferenceSize(input_world->width, input_world->height, short_edge,
		&inference_width, &inference_height, patch);
	if (inference_width <= 0 || inference_height <= 0) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}
	if (const PF_Err abort_err = CheckAbort(in_data)) {
		return abort_err;
	}
	std::vector<float> alpha;
	std::vector<float> tensor;
	switch (pixel_format) {
	case PF_PixelFormat_ARGB32:
		alpha = depthgen::ReadWorldAlpha<PF_Pixel>(input_world);
		tensor = depthgen::ReadWorldToInferenceTensor<PF_Pixel>(input_world, inference_width,
			inference_height, settings.linear_to_srgb);
		break;
	case PF_PixelFormat_ARGB64:
		alpha = depthgen::ReadWorldAlpha<PF_Pixel16>(input_world);
		tensor = depthgen::ReadWorldToInferenceTensor<PF_Pixel16>(input_world, inference_width,
			inference_height, settings.linear_to_srgb);
		break;
	case PF_PixelFormat_ARGB128:
		alpha = depthgen::ReadWorldAlpha<PF_PixelFloat>(input_world);
		tensor = depthgen::ReadWorldToInferenceTensor<PF_PixelFloat>(input_world, inference_width,
			inference_height, settings.linear_to_srgb);
		break;
	default: return PF_Err_UNRECOGNIZED_PARAM_TYPE;
	}
	if (tensor.empty() || alpha.empty()) {
		return PF_Err_OUT_OF_MEMORY;
	}
	if (settings.model == DEPTHGEN_MODEL_DAV2_SMALL) {
		depthgen::ApplyImageNetToPlanarRgb(&tensor, inference_width, inference_height);
	}
	depthgen::InferenceResult result;
	depthgen::InferenceProvider provider = depthgen::InferenceProvider::Unavailable;
	std::string inference_error;
	const depthgen::DepthModel inference_model = settings.model == DEPTHGEN_MODEL_DAV2_SMALL
		? depthgen::DepthModel::DepthAnythingV2Small : depthgen::DepthModel::ZipDepth;
	if (const PF_Err abort_err = CheckAbort(in_data)) {
		return abort_err;
	}
	if (!depthgen::InferDepth(tensor, inference_width, inference_height, inference_model,
		&result, &provider, &inference_error)) {
		if (out_data && !inference_error.empty()) {
			WriteReturnMessage(out_data, "DepthGen: %s", inference_error.c_str());
		}
		return PF_Err_BAD_CALLBACK_PARAM;
	}
	depthgen::FloatImage raw_depth;
	raw_depth.width = result.width;
	raw_depth.height = result.height;
	raw_depth.values = std::move(result.depth);
	depthgen::FloatImage full_depth = depthgen::ResizeBilinearAligned(raw_depth,
		input_world->width, input_world->height);
	depthgen::DepthLevels levels = depthgen::ComputeDepthLevels(full_depth.values, alpha,
		settings.alpha_threshold, settings.use_alpha_for_levels, settings.far_percentile,
		settings.near_percentile);
	depthgen::TemporalRange previous;
	const depthgen::TemporalLayout layout{
		input_world->width,
		input_world->height,
		static_cast<int>(settings.model),
		static_cast<int>(settings.short_edge)};
	const bool have_previous = history && settings.temporal_stability > 0.0f &&
		history->CopyPrevious(static_cast<std::int32_t>(time), static_cast<std::int32_t>(time_step),
			layout, &previous);
	if (have_previous) {
		levels = depthgen::SmoothMappingRange(levels, previous.mapping,
			settings.temporal_stability / 100.0f);
	}
	depthgen::ApplyDepthLevels(&full_depth.values, levels, settings.contrast, settings.invert);
	if (have_previous) {
		depthgen::AlignUnitQuantiles(&full_depth.values, alpha, settings.alpha_threshold,
			previous.quantiles, settings.temporal_stability / 100.0f);
	}
	if (history) {
		history->Store(static_cast<std::int32_t>(time), layout,
			depthgen::MeasureUnitRange(full_depth.values, alpha, settings.alpha_threshold, levels));
	}
	if (const PF_Err abort_err = CheckAbort(in_data)) {
		return abort_err;
	}
	switch (pixel_format) {
	case PF_PixelFormat_ARGB32:
		depthgen::WriteDepthWorld<PF_Pixel>(output_world, input_world, full_depth.values, alpha,
			settings.preserve_alpha);
		break;
	case PF_PixelFormat_ARGB64:
		depthgen::WriteDepthWorld<PF_Pixel16>(output_world, input_world, full_depth.values, alpha,
			settings.preserve_alpha);
		break;
	case PF_PixelFormat_ARGB128:
		depthgen::WriteDepthWorld<PF_PixelFloat>(output_world, input_world, full_depth.values, alpha,
			settings.preserve_alpha);
		break;
	default: break;
	}
	return PF_Err_NONE;
}

extern "C" DllExport
PF_Err PluginDataEntryFunction2(
	PF_PluginDataPtr in_ptr, PF_PluginDataCB2 callback, SPBasicSuite* basic,
	const char* host_name, const char* host_version) {
	(void)basic;
	(void)host_name;
	(void)host_version;
	const A_Err result = callback(in_ptr,
		reinterpret_cast<const A_u_char*>(DEPTHGEN_NAME),
		reinterpret_cast<const A_u_char*>(DEPTHGEN_MATCH_NAME),
		reinterpret_cast<const A_u_char*>(DEPTHGEN_CATEGORY),
		reinterpret_cast<const A_u_char*>(AE_ENTRY_POINT), 'eFKT',
		PF_AE_PLUG_IN_VERSION, PF_AE_PLUG_IN_SUBVERS, 0,
		reinterpret_cast<const A_u_char*>(DEPTHGEN_SUPPORT_URL));
	return result == A_Err_NONE ? PF_Err_NONE : result;
}

extern "C" DllExport
PF_Err PluginDataEntryFunction(
	PF_PluginDataPtr in_ptr, PF_PluginDataCB callback, SPBasicSuite* basic,
	const char* host_name, const char* host_version) {
	(void)basic;
	(void)host_name;
	(void)host_version;
	const A_Err result = callback(in_ptr,
		reinterpret_cast<const A_u_char*>(DEPTHGEN_NAME),
		reinterpret_cast<const A_u_char*>(DEPTHGEN_MATCH_NAME),
		reinterpret_cast<const A_u_char*>(DEPTHGEN_CATEGORY),
		reinterpret_cast<const A_u_char*>(AE_ENTRY_POINT), 'eFKT',
		PF_AE_PLUG_IN_VERSION, PF_AE_PLUG_IN_SUBVERS, 0);
	return result == A_Err_NONE ? PF_Err_NONE : result;
}

extern "C" DllExport
PF_Err EffectMain(
	PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[],
	PF_LayerDef* output, void* extra) {
	PF_Err err = PF_Err_NONE;
	try {
		switch (cmd) {
		case PF_Cmd_ABOUT:
			WriteReturnMessage(out_data, "%s v%d.%d.%d\r%s", DEPTHGEN_NAME,
				DEPTHGEN_VERSION_MAJOR, DEPTHGEN_VERSION_MINOR, DEPTHGEN_VERSION_BUG,
				DEPTHGEN_DESCRIPTION);
			break;
		case PF_Cmd_GLOBAL_SETUP: err = GlobalSetup(in_data, out_data); break;
		case PF_Cmd_GLOBAL_SETDOWN: err = GlobalSetdown(in_data); break;
		case PF_Cmd_PARAMS_SETUP: err = ParamsSetup(in_data, out_data); break;
		case PF_Cmd_SEQUENCE_SETUP: err = SequenceSetup(in_data, out_data); break;
		case PF_Cmd_SEQUENCE_SETDOWN: err = SequenceSetdown(in_data, out_data); break;
		case PF_Cmd_SEQUENCE_FLATTEN: err = SequenceFlatten(in_data, out_data); break;
		case PF_Cmd_SEQUENCE_RESETUP: err = SequenceResetup(in_data, out_data); break;
		case PF_Cmd_GET_FLATTENED_SEQUENCE_DATA: err = GetFlattenedSequenceData(in_data, out_data); break;
		case PF_Cmd_SMART_PRE_RENDER: err = PreRender(in_data, reinterpret_cast<PF_PreRenderExtra*>(extra)); break;
		case PF_Cmd_SMART_RENDER: err = SmartRender(in_data, out_data, reinterpret_cast<PF_SmartRenderExtra*>(extra)); break;
		case PF_Cmd_USER_CHANGED_PARAM:
			if (extra) {
				const A_long index = reinterpret_cast<PF_UserChangedParamExtra*>(extra)->param_index;
				if (index == ParamIndexFromID(DEPTHGEN_QUALITY) || index == ParamIndexFromID(DEPTHGEN_MODEL)) {
					err = DepthGen_UpdateParamsUI(in_data, out_data, params, output);
					break;
				}
			}
			break;
		case PF_Cmd_UPDATE_PARAMS_UI:
			err = DepthGen_UpdateParamsUI(in_data, out_data, params, output);
			break;
		default: break;
		}
	} catch (const A_long thrown) {
		// PF_Err is A_long: MissingSuiteError and A_THROW both arrive here.
		err = thrown;
	} catch (const std::bad_alloc&) {
		err = PF_Err_OUT_OF_MEMORY;
	} catch (const std::exception&) {
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	} catch (...) {
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	return err;
}
