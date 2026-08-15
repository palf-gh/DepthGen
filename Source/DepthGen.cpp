#include "DepthGen.h"
#include "DepthGen_Image.h"
#include "DepthGen_Inference.h"
#include "DepthGen_Pipeline.h"
#include "Localise/DepthGenStrings.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

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

	if (checkout(ParamIndexFromID(DEPTHGEN_QUALITY))) {
		switch (parameter.u.pd.value) {
		case DEPTHGEN_QUALITY_FAST: settings->short_edge = DEPTHGEN_FAST_SHORT_EDGE; break;
		case DEPTHGEN_QUALITY_HIGH: settings->short_edge = DEPTHGEN_HIGH_SHORT_EDGE; break;
		case DEPTHGEN_QUALITY_CUSTOM: settings->short_edge = DEPTHGEN_BALANCED_SHORT_EDGE; break;
		default: settings->short_edge = DEPTHGEN_BALANCED_SHORT_EDGE; break;
		}
		checkin();
	}
	if (checkout(ParamIndexFromID(DEPTHGEN_CUSTOM_SHORT_EDGE))) {
		const A_long custom = static_cast<A_long>(std::lround(parameter.u.fs_d.value));
		if (settings->short_edge == DEPTHGEN_BALANCED_SHORT_EDGE) {
			// Custom can replace the default only when the popup selected it.
			PF_ParamDef quality{};
			if (PF_CHECKOUT_PARAM(in_data, ParamIndexFromID(DEPTHGEN_QUALITY), in_data->current_time,
				in_data->time_step, in_data->time_scale, &quality) == PF_Err_NONE) {
				if (quality.u.pd.value == DEPTHGEN_QUALITY_CUSTOM) {
					settings->short_edge = std::max(DEPTHGEN_CUSTOM_EDGE_MIN,
						std::min(DEPTHGEN_CUSTOM_EDGE_MAX, custom));
				}
				(void)PF_CHECKIN_PARAM(in_data, &quality);
			}
		}
		checkin();
	}
	if (checkout(ParamIndexFromID(DEPTHGEN_FAR_PERCENTILE))) { settings->far_percentile = static_cast<float>(parameter.u.fs_d.value); checkin(); }
	if (checkout(ParamIndexFromID(DEPTHGEN_NEAR_PERCENTILE))) { settings->near_percentile = static_cast<float>(parameter.u.fs_d.value); checkin(); }
	if (checkout(ParamIndexFromID(DEPTHGEN_CONTRAST))) { settings->contrast = static_cast<float>(parameter.u.fs_d.value); checkin(); }
	if (checkout(ParamIndexFromID(DEPTHGEN_INVERT))) { settings->invert = parameter.u.bd.value != 0; checkin(); }
	if (checkout(ParamIndexFromID(DEPTHGEN_INPUT_TRANSFER))) { settings->linear_to_srgb = parameter.u.pd.value == DEPTHGEN_TRANSFER_LINEAR_TO_SRGB; checkin(); }
	if (checkout(ParamIndexFromID(DEPTHGEN_USE_ALPHA_FOR_LEVELS))) { settings->use_alpha_for_levels = parameter.u.bd.value != 0; checkin(); }
	if (checkout(ParamIndexFromID(DEPTHGEN_ALPHA_THRESHOLD))) { settings->alpha_threshold = static_cast<float>(parameter.u.fs_d.value) / 100.0f; checkin(); }
	if (checkout(ParamIndexFromID(DEPTHGEN_OUTPUT_ALPHA))) { settings->preserve_alpha = parameter.u.pd.value != DEPTHGEN_ALPHA_OPAQUE; checkin(); }
	settings->far_percentile = std::max(0.0f, std::min(100.0f, settings->far_percentile));
	settings->near_percentile = std::max(0.0f, std::min(100.0f, settings->near_percentile));
	settings->contrast = std::max(0.01f, std::min(4.0f, settings->contrast));
	return PF_Err_NONE;
}

void DisposePreRenderData(void* data) {
	delete reinterpret_cast<DepthGenPreRenderData*>(data);
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
	if (!data) return PF_Err_OUT_OF_MEMORY;
	data->plugin_id = 0;
	const PF_Err err = suites.UtilitySuite5()->AEGP_RegisterWithAEGP(nullptr, DEPTHGEN_NAME, &data->plugin_id);
	suites.HandleSuite1()->host_unlock_handle(out_data->global_data);
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
		AEFX_SuiteScoper<PF_WorldSuite2> world_suite(in_data, kPFWorldSuite, kPFWorldSuiteVersion2, out_data);
		PF_PixelFormat format = PF_PixelFormat_INVALID;
		err = world_suite->PF_GetPixelFormat(input, &format);
		if (!err) {
			err = DepthGen_RenderWorld(in_data, out_data, format, input, output,
				reinterpret_cast<DepthGenPreRenderData*>(extra->input->pre_render_data)->settings);
		}
	}
	if (input) (void)extra->cb->checkin_layer_pixels(in_data->effect_ref, DEPTHGEN_INPUT);
	return err;
}

} // namespace

PF_Err DepthGen_RenderWorld(
	PF_InData* in_data,
	PF_OutData* out_data,
	PF_PixelFormat pixel_format,
	PF_EffectWorld* input_world,
	PF_EffectWorld* output_world,
	const DepthGenRenderSettings& settings) {
	if (!input_world || !output_world || input_world->width <= 0 || input_world->height <= 0 ||
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
	const int short_edge = depthgen::ScaleShortEdgeToRender(
		full_width, full_height, render_width, render_height, settings.short_edge);
	depthgen::ComputeInferenceSize(input_world->width, input_world->height, short_edge,
		&inference_width, &inference_height);
	if (inference_width <= 0 || inference_height <= 0) {
		return PF_Err_BAD_CALLBACK_PARAM;
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
	depthgen::InferenceResult result;
	depthgen::InferenceProvider provider = depthgen::InferenceProvider::Unavailable;
	std::string inference_error;
	if (!depthgen::InferDepthAnythingSmall(tensor, inference_width, inference_height,
		&result, &provider, &inference_error)) {
		if (out_data && !inference_error.empty()) {
			PF_SPRINTF(out_data->return_msg, "DepthGen: %s", inference_error.c_str());
		}
		return PF_Err_BAD_CALLBACK_PARAM;
	}
	depthgen::FloatImage raw_depth;
	raw_depth.width = result.width;
	raw_depth.height = result.height;
	raw_depth.values = std::move(result.depth);
	depthgen::FloatImage full_depth = depthgen::ResizeBilinearAligned(raw_depth,
		input_world->width, input_world->height);
	depthgen::MapRelativeDepthToUnit(&full_depth.values, alpha, settings.alpha_threshold,
		settings.use_alpha_for_levels, settings.far_percentile, settings.near_percentile,
		settings.contrast, settings.invert);
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
	switch (cmd) {
	case PF_Cmd_ABOUT:
		PF_SPRINTF(out_data->return_msg, "%s v%d.%d\\r%s", DEPTHGEN_NAME,
			DEPTHGEN_VERSION_MAJOR, DEPTHGEN_VERSION_MINOR, DEPTHGEN_DESCRIPTION);
		return PF_Err_NONE;
	case PF_Cmd_GLOBAL_SETUP: return GlobalSetup(in_data, out_data);
	case PF_Cmd_GLOBAL_SETDOWN: return GlobalSetdown(in_data);
	case PF_Cmd_PARAMS_SETUP: return ParamsSetup(in_data, out_data);
	case PF_Cmd_SMART_PRE_RENDER: return PreRender(in_data, reinterpret_cast<PF_PreRenderExtra*>(extra));
	case PF_Cmd_SMART_RENDER: return SmartRender(in_data, out_data, reinterpret_cast<PF_SmartRenderExtra*>(extra));
	case PF_Cmd_USER_CHANGED_PARAM:
		if (extra && reinterpret_cast<PF_UserChangedParamExtra*>(extra)->param_index ==
			ParamIndexFromID(DEPTHGEN_QUALITY)) {
			return DepthGen_UpdateParamsUI(in_data, out_data, params, output);
		}
		return PF_Err_NONE;
	case PF_Cmd_UPDATE_PARAMS_UI:
		return DepthGen_UpdateParamsUI(in_data, out_data, params, output);
	default: return PF_Err_NONE;
	}
}
