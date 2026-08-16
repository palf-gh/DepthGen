#pragma once

#include "DepthGen_Target.h"
#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectSuites.h"
#include "AE_PluginData.h"
#include "AE_Macros.h"
#include "AEGP_SuiteHandler.h"
#include "AEFX_SuiteHelper.h"
#include "Param_Utils.h"
#include "Smart_Utils.h"

#include <cstdint>

struct DepthGenGlobalData {
	AEGP_PluginID plugin_id = 0;
};

// These values are persisted in After Effects projects. Append only.
enum DepthGenParamID {
	DEPTHGEN_INPUT = 0,
	DEPTHGEN_QUALITY,
	DEPTHGEN_FAR_PERCENTILE,
	DEPTHGEN_NEAR_PERCENTILE,
	DEPTHGEN_CONTRAST,
	DEPTHGEN_INVERT,
	DEPTHGEN_SHOW_ADVANCED, // retained, permanently invisible; replaced by the Advanced topic
	DEPTHGEN_CUSTOM_SHORT_EDGE,
	DEPTHGEN_INPUT_TRANSFER,
	DEPTHGEN_USE_ALPHA_FOR_LEVELS,
	DEPTHGEN_ALPHA_THRESHOLD,
	DEPTHGEN_OUTPUT_ALPHA,
	DEPTHGEN_GRP_ADVANCED_START,
	DEPTHGEN_GRP_ADVANCED_END,
	DEPTHGEN_MODEL,
	DEPTHGEN_TEMPORAL_STABILITY,
	DEPTHGEN_ID_COUNT
};

// Registration / params[] / checkout order. IDs stay put; only this array moves.
inline constexpr A_long kDepthGenParamOrder[] = {
	DEPTHGEN_INPUT,
	DEPTHGEN_MODEL,
	DEPTHGEN_QUALITY,
	DEPTHGEN_FAR_PERCENTILE,
	DEPTHGEN_NEAR_PERCENTILE,
	DEPTHGEN_CONTRAST,
	DEPTHGEN_INVERT,
	DEPTHGEN_TEMPORAL_STABILITY,
	DEPTHGEN_SHOW_ADVANCED,
	DEPTHGEN_GRP_ADVANCED_START,
	DEPTHGEN_CUSTOM_SHORT_EDGE,
	DEPTHGEN_INPUT_TRANSFER,
	DEPTHGEN_USE_ALPHA_FOR_LEVELS,
	DEPTHGEN_ALPHA_THRESHOLD,
	DEPTHGEN_OUTPUT_ALPHA,
	DEPTHGEN_GRP_ADVANCED_END};
inline constexpr A_long kDepthGenParamOrderCount =
	static_cast<A_long>(sizeof(kDepthGenParamOrder) / sizeof(kDepthGenParamOrder[0]));

inline A_long ParamIndexFromID(A_long id) {
	for (A_long index = 0; index < kDepthGenParamOrderCount; ++index) {
		if (kDepthGenParamOrder[index] == id) {
			return index;
		}
	}
	return 0;
}

inline A_long DepthGenRenderWidth(const PF_InData* in_data) {
	if (!in_data || in_data->width <= 0) {
		return 0;
	}
	const PF_RationalScale& scale = in_data->downsample_x;
	if (scale.num <= 0 || scale.den <= 0) {
		return in_data->width;
	}
	return static_cast<A_long>(
		(static_cast<long long>(in_data->width) * scale.num) / scale.den);
}

inline A_long DepthGenRenderHeight(const PF_InData* in_data) {
	if (!in_data || in_data->height <= 0) {
		return 0;
	}
	const PF_RationalScale& scale = in_data->downsample_y;
	if (scale.num <= 0 || scale.den <= 0) {
		return in_data->height;
	}
	return static_cast<A_long>(
		(static_cast<long long>(in_data->height) * scale.num) / scale.den);
}

enum DepthGenModel {
	DEPTHGEN_MODEL_ZIPDEPTH = 1,
	DEPTHGEN_MODEL_DAV2_SMALL
};

enum DepthGenQuality {
	DEPTHGEN_QUALITY_FAST = 1,
	DEPTHGEN_QUALITY_BALANCED,
	DEPTHGEN_QUALITY_HIGH,
	DEPTHGEN_QUALITY_CUSTOM
};

enum DepthGenInputTransfer {
	DEPTHGEN_TRANSFER_SRGB = 1,
	DEPTHGEN_TRANSFER_LINEAR_TO_SRGB
};

enum DepthGenOutputAlpha {
	DEPTHGEN_ALPHA_PRESERVE = 1,
	DEPTHGEN_ALPHA_OPAQUE
};

constexpr A_long DEPTHGEN_FAST_SHORT_EDGE = 512;
constexpr A_long DEPTHGEN_BALANCED_SHORT_EDGE = 768;
constexpr A_long DEPTHGEN_HIGH_SHORT_EDGE = 1080;
constexpr A_long DEPTHGEN_DAV2_FAST_SHORT_EDGE = 384;
constexpr A_long DEPTHGEN_DAV2_BALANCED_SHORT_EDGE = 518;
constexpr A_long DEPTHGEN_DAV2_HIGH_SHORT_EDGE = 736;
constexpr A_long DEPTHGEN_CUSTOM_EDGE_MIN = 256;
constexpr A_long DEPTHGEN_CUSTOM_EDGE_MAX = 2160;

inline A_long DepthGenShortEdge(DepthGenModel model, A_long quality, A_long custom) {
	if (quality == DEPTHGEN_QUALITY_CUSTOM) {
		if (custom < DEPTHGEN_CUSTOM_EDGE_MIN) {
			return DEPTHGEN_CUSTOM_EDGE_MIN;
		}
		if (custom > DEPTHGEN_CUSTOM_EDGE_MAX) {
			return DEPTHGEN_CUSTOM_EDGE_MAX;
		}
		return custom;
	}
	if (model == DEPTHGEN_MODEL_DAV2_SMALL) {
		if (quality == DEPTHGEN_QUALITY_FAST) {
			return DEPTHGEN_DAV2_FAST_SHORT_EDGE;
		}
		if (quality == DEPTHGEN_QUALITY_HIGH) {
			return DEPTHGEN_DAV2_HIGH_SHORT_EDGE;
		}
		return DEPTHGEN_DAV2_BALANCED_SHORT_EDGE;
	}
	if (quality == DEPTHGEN_QUALITY_FAST) {
		return DEPTHGEN_FAST_SHORT_EDGE;
	}
	if (quality == DEPTHGEN_QUALITY_HIGH) {
		return DEPTHGEN_HIGH_SHORT_EDGE;
	}
	return DEPTHGEN_BALANCED_SHORT_EDGE;
}

struct DepthGenRenderSettings {
	DepthGenModel model = DEPTHGEN_MODEL_ZIPDEPTH;
	A_long short_edge = DEPTHGEN_BALANCED_SHORT_EDGE;
	float far_percentile = 2.0f;
	float near_percentile = 98.0f;
	float contrast = 1.0f;
	float alpha_threshold = 0.0f;
	float temporal_stability = 0.0f;
	bool invert = false;
	bool linear_to_srgb = false;
	bool use_alpha_for_levels = true;
	bool preserve_alpha = true;
};

struct DepthGenPreRenderData {
	DepthGenRenderSettings settings;
};

namespace depthgen {
class TemporalHistory;
}

PF_Err DepthGen_RenderWorld(
	PF_InData* in_data,
	PF_OutData* out_data,
	PF_PixelFormat pixel_format,
	PF_EffectWorld* input_world,
	PF_EffectWorld* output_world,
	const DepthGenRenderSettings& settings,
	depthgen::TemporalHistory* history,
	A_long time,
	A_long time_step);

PF_Err DepthGen_UpdateParamsUI(
	PF_InData* in_data,
	PF_OutData* out_data,
	PF_ParamDef* params[],
	PF_LayerDef* output);

extern "C" {
DllExport PF_Err EffectMain(
	PF_Cmd cmd,
	PF_InData* in_data,
	PF_OutData* out_data,
	PF_ParamDef* params[],
	PF_LayerDef* output,
	void* extra);
}
