#include "DepthGen.h"
#include "DepthGen_Image.h"
#include "DepthGen_Inference.h"
#include "Localise/DepthGenStrings.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr float kAlphaEpsilon = 1.0e-6f;

float Clamp01(float value) {
	return std::max(0.0f, std::min(1.0f, value));
}

template <typename Pixel>
Pixel* PixelAt(PF_EffectWorld* world, A_long x, A_long y) {
	return reinterpret_cast<Pixel*>(reinterpret_cast<char*>(world->data) +
		static_cast<ptrdiff_t>(y) * world->rowbytes) + x;
}

template <typename Pixel>
void ReadPixel(const Pixel& pixel, float* red, float* green, float* blue, float* alpha);

template <>
void ReadPixel(const PF_Pixel& pixel, float* red, float* green, float* blue, float* alpha) {
	*red = pixel.red / 255.0f;
	*green = pixel.green / 255.0f;
	*blue = pixel.blue / 255.0f;
	*alpha = pixel.alpha / 255.0f;
}

template <>
void ReadPixel(const PF_Pixel16& pixel, float* red, float* green, float* blue, float* alpha) {
	*red = pixel.red / 32768.0f;
	*green = pixel.green / 32768.0f;
	*blue = pixel.blue / 32768.0f;
	*alpha = pixel.alpha / 32768.0f;
}

template <>
void ReadPixel(const PF_PixelFloat& pixel, float* red, float* green, float* blue, float* alpha) {
	*red = pixel.red;
	*green = pixel.green;
	*blue = pixel.blue;
	*alpha = pixel.alpha;
}

template <typename Pixel>
void WritePixel(Pixel* pixel, float depth, float alpha);

template <>
void WritePixel(PF_Pixel* pixel, float depth, float alpha) {
	const A_u_char rgb = static_cast<A_u_char>(std::lround(Clamp01(depth) * 255.0f));
	pixel->red = rgb;
	pixel->green = rgb;
	pixel->blue = rgb;
	pixel->alpha = static_cast<A_u_char>(std::lround(Clamp01(alpha) * 255.0f));
}

template <>
void WritePixel(PF_Pixel16* pixel, float depth, float alpha) {
	const A_u_short rgb = static_cast<A_u_short>(std::lround(Clamp01(depth) * 32768.0f));
	pixel->red = rgb;
	pixel->green = rgb;
	pixel->blue = rgb;
	pixel->alpha = static_cast<A_u_short>(std::lround(Clamp01(alpha) * 32768.0f));
}

template <>
void WritePixel(PF_PixelFloat* pixel, float depth, float alpha) {
	pixel->red = Clamp01(depth);
	pixel->green = Clamp01(depth);
	pixel->blue = Clamp01(depth);
	pixel->alpha = Clamp01(alpha);
}

template <typename Pixel>
void ReadWorld(
	PF_EffectWorld* world,
	bool linear_to_srgb,
	std::vector<float>* rgb,
	std::vector<float>* alpha) {
	const size_t pixel_count = static_cast<size_t>(world->width) * static_cast<size_t>(world->height);
	rgb->resize(pixel_count * 3U);
	alpha->resize(pixel_count);
	for (A_long y = 0; y < world->height; ++y) {
		for (A_long x = 0; x < world->width; ++x) {
			float red = 0.0f;
			float green = 0.0f;
			float blue = 0.0f;
			float source_alpha = 0.0f;
			ReadPixel(*PixelAt<Pixel>(world, x, y), &red, &green, &blue, &source_alpha);
			source_alpha = Clamp01(source_alpha);
			const size_t index = static_cast<size_t>(y) * world->width + x;
			(*alpha)[index] = source_alpha;
			if (source_alpha <= kAlphaEpsilon) {
				(*rgb)[index * 3U] = 0.0f;
				(*rgb)[index * 3U + 1U] = 0.0f;
				(*rgb)[index * 3U + 2U] = 0.0f;
				continue;
			}
			red = Clamp01(red / source_alpha);
			green = Clamp01(green / source_alpha);
			blue = Clamp01(blue / source_alpha);
			if (linear_to_srgb) {
				red = depthgen::LinearToSrgb(red);
				green = depthgen::LinearToSrgb(green);
				blue = depthgen::LinearToSrgb(blue);
			}
			(*rgb)[index * 3U] = red;
			(*rgb)[index * 3U + 1U] = green;
			(*rgb)[index * 3U + 2U] = blue;
		}
	}
}

std::vector<float> ResizeRgbForInference(
	const std::vector<float>& source_rgb,
	int source_width,
	int source_height,
	int inference_width,
	int inference_height) {
	std::vector<float> output(static_cast<size_t>(inference_width) * inference_height * 3U);
	for (int channel = 0; channel < 3; ++channel) {
		depthgen::FloatImage plane;
		plane.width = source_width;
		plane.height = source_height;
		plane.values.resize(static_cast<size_t>(source_width) * source_height);
		for (size_t pixel = 0; pixel < plane.values.size(); ++pixel) {
			plane.values[pixel] = source_rgb[pixel * 3U + static_cast<size_t>(channel)];
		}
		const depthgen::FloatImage resized = depthgen::ResizeCubic(plane, inference_width, inference_height);
		for (size_t pixel = 0; pixel < resized.values.size(); ++pixel) {
			output[pixel * 3U + static_cast<size_t>(channel)] = resized.values[pixel];
		}
	}
	depthgen::ImageNetNormaliseInterleavedRgb(&output);
	return output;
}

template <typename Pixel>
void WriteDepthWorld(
	PF_EffectWorld* output_world,
	const std::vector<float>& depth,
	const std::vector<float>& source_alpha,
	bool preserve_alpha) {
	for (A_long y = 0; y < output_world->height; ++y) {
		for (A_long x = 0; x < output_world->width; ++x) {
			const size_t index = static_cast<size_t>(y) * output_world->width + x;
			const float alpha = preserve_alpha ? source_alpha[index] : 1.0f;
			const float value = source_alpha[index] <= kAlphaEpsilon ? 0.0f : depth[index];
			WritePixel(PixelAt<Pixel>(output_world, x, y), value, alpha);
		}
	}
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

	if (checkout(DEPTHGEN_QUALITY)) {
		switch (parameter.u.pd.value) {
		case DEPTHGEN_QUALITY_FAST: settings->short_edge = DEPTHGEN_FAST_SHORT_EDGE; break;
		case DEPTHGEN_QUALITY_HIGH: settings->short_edge = DEPTHGEN_HIGH_SHORT_EDGE; break;
		case DEPTHGEN_QUALITY_CUSTOM: settings->short_edge = DEPTHGEN_BALANCED_SHORT_EDGE; break;
		default: settings->short_edge = DEPTHGEN_BALANCED_SHORT_EDGE; break;
		}
		checkin();
	}
	if (checkout(DEPTHGEN_CUSTOM_SHORT_EDGE)) {
		const A_long custom = static_cast<A_long>(std::lround(parameter.u.fs_d.value));
		if (settings->short_edge == DEPTHGEN_BALANCED_SHORT_EDGE) {
			// Custom can replace the default only when the popup selected it.
			PF_ParamDef quality{};
			if (PF_CHECKOUT_PARAM(in_data, DEPTHGEN_QUALITY, in_data->current_time, in_data->time_step,
				in_data->time_scale, &quality) == PF_Err_NONE) {
				if (quality.u.pd.value == DEPTHGEN_QUALITY_CUSTOM) {
					settings->short_edge = std::max(DEPTHGEN_CUSTOM_EDGE_MIN,
						std::min(DEPTHGEN_CUSTOM_EDGE_MAX, custom));
				}
				(void)PF_CHECKIN_PARAM(in_data, &quality);
			}
		}
		checkin();
	}
	if (checkout(DEPTHGEN_FAR_PERCENTILE)) { settings->far_percentile = static_cast<float>(parameter.u.fs_d.value); checkin(); }
	if (checkout(DEPTHGEN_NEAR_PERCENTILE)) { settings->near_percentile = static_cast<float>(parameter.u.fs_d.value); checkin(); }
	if (checkout(DEPTHGEN_CONTRAST)) { settings->contrast = static_cast<float>(parameter.u.fs_d.value); checkin(); }
	if (checkout(DEPTHGEN_INVERT)) { settings->invert = parameter.u.bd.value != 0; checkin(); }
	if (checkout(DEPTHGEN_INPUT_TRANSFER)) { settings->linear_to_srgb = parameter.u.pd.value == DEPTHGEN_TRANSFER_LINEAR_TO_SRGB; checkin(); }
	if (checkout(DEPTHGEN_USE_ALPHA_FOR_LEVELS)) { settings->use_alpha_for_levels = parameter.u.bd.value != 0; checkin(); }
	if (checkout(DEPTHGEN_ALPHA_THRESHOLD)) { settings->alpha_threshold = static_cast<float>(parameter.u.fs_d.value) / 100.0f; checkin(); }
	if (checkout(DEPTHGEN_OUTPUT_ALPHA)) { settings->preserve_alpha = parameter.u.pd.value != DEPTHGEN_ALPHA_OPAQUE; checkin(); }
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
	def.flags = PF_ParamFlag_SUPERVISE | PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS;
	PF_ADD_CHECKBOX(GetString(DepthGenString::ShowAdvanced, in_data), "On", FALSE, 0, DEPTHGEN_SHOW_ADVANCED);

	// Advanced controls begin invisible. Runtime uses AEGP dynamic stream flags.
	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_FLOAT_SLIDERX(GetString(DepthGenString::CustomShortEdge, in_data), DEPTHGEN_CUSTOM_EDGE_MIN,
		DEPTHGEN_CUSTOM_EDGE_MAX, DEPTHGEN_CUSTOM_EDGE_MIN, DEPTHGEN_CUSTOM_EDGE_MAX,
		DEPTHGEN_BALANCED_SHORT_EDGE, 0, PF_ValueDisplayFlag_NONE, 0, DEPTHGEN_CUSTOM_SHORT_EDGE);
	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_POPUP(GetString(DepthGenString::InputTransfer, in_data), 2, DEPTHGEN_TRANSFER_SRGB,
		GetString(DepthGenString::InputTransferItems, in_data), DEPTHGEN_INPUT_TRANSFER);
	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_CHECKBOX(GetString(DepthGenString::UseAlpha, in_data), "On", TRUE, 0, DEPTHGEN_USE_ALPHA_FOR_LEVELS);
	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_FLOAT_SLIDERX(GetString(DepthGenString::AlphaThreshold, in_data), 0, 100, 0, 100, 0, 1,
		PF_ValueDisplayFlag_PERCENT, 0, DEPTHGEN_ALPHA_THRESHOLD);
	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_POPUP(GetString(DepthGenString::OutputAlpha, in_data), 2, DEPTHGEN_ALPHA_PRESERVE,
		GetString(DepthGenString::OutputAlphaItems, in_data), DEPTHGEN_OUTPUT_ALPHA);
	out_data->num_params = DEPTHGEN_NUM_PARAMS;
	return PF_Err_NONE;
}

PF_Err PreRender(PF_InData* in_data, PF_PreRenderExtra* extra) {
	if (!in_data || !extra) return PF_Err_BAD_CALLBACK_PARAM;
	auto data = std::make_unique<DepthGenPreRenderData>();
	PF_Err err = ReadSettings(in_data, &data->settings);
	if (err) return err;
	PF_RenderRequest request = extra->input->output_request;
	request.rect.left = 0;
	request.rect.top = 0;
	request.rect.right = in_data->width;
	request.rect.bottom = in_data->height;
	request.field = PF_Field_FRAME;
	PF_CheckoutResult checkout{};
	err = extra->cb->checkout_layer(in_data->effect_ref, DEPTHGEN_INPUT, DEPTHGEN_INPUT, &request,
		in_data->current_time, in_data->time_step, in_data->time_scale, &checkout);
	if (err) return err;
	extra->output->result_rect = request.rect;
	extra->output->max_result_rect = request.rect;
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
	(void)in_data;
	(void)out_data;
	if (!input_world || !output_world || input_world->width != output_world->width ||
		input_world->height != output_world->height) return PF_Err_BAD_CALLBACK_PARAM;
	std::vector<float> rgb;
	std::vector<float> alpha;
	switch (pixel_format) {
	case PF_PixelFormat_ARGB32: ReadWorld<PF_Pixel>(input_world, settings.linear_to_srgb, &rgb, &alpha); break;
	case PF_PixelFormat_ARGB64: ReadWorld<PF_Pixel16>(input_world, settings.linear_to_srgb, &rgb, &alpha); break;
	case PF_PixelFormat_ARGB128: ReadWorld<PF_PixelFloat>(input_world, settings.linear_to_srgb, &rgb, &alpha); break;
	default: return PF_Err_UNRECOGNIZED_PARAM_TYPE;
	}
	int inference_width = 0;
	int inference_height = 0;
	depthgen::ComputeInferenceSize(input_world->width, input_world->height, settings.short_edge,
		&inference_width, &inference_height);
	std::vector<float> inference_rgb = ResizeRgbForInference(rgb, input_world->width, input_world->height,
		inference_width, inference_height);
	depthgen::InferenceResult result;
	depthgen::InferenceProvider provider = depthgen::InferenceProvider::Unavailable;
	std::string inference_error;
	if (!depthgen::InferDepthAnythingSmall(inference_rgb, inference_width, inference_height,
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
	case PF_PixelFormat_ARGB32: WriteDepthWorld<PF_Pixel>(output_world, full_depth.values, alpha, settings.preserve_alpha); break;
	case PF_PixelFormat_ARGB64: WriteDepthWorld<PF_Pixel16>(output_world, full_depth.values, alpha, settings.preserve_alpha); break;
	case PF_PixelFormat_ARGB128: WriteDepthWorld<PF_PixelFloat>(output_world, full_depth.values, alpha, settings.preserve_alpha); break;
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
		if (extra && reinterpret_cast<PF_UserChangedParamExtra*>(extra)->param_index == DEPTHGEN_SHOW_ADVANCED) {
			return DepthGen_UpdateParamsUI(in_data, out_data, params, output);
		}
		return PF_Err_NONE;
	case PF_Cmd_UPDATE_PARAMS_UI:
	case PF_Cmd_SEQUENCE_SETUP:
	case PF_Cmd_SEQUENCE_RESETUP:
		return DepthGen_UpdateParamsUI(in_data, out_data, params, output);
	default: return PF_Err_NONE;
	}
}
