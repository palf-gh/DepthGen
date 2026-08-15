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
	DEPTHGEN_SHOW_ADVANCED,
	DEPTHGEN_CUSTOM_SHORT_EDGE,
	DEPTHGEN_INPUT_TRANSFER,
	DEPTHGEN_USE_ALPHA_FOR_LEVELS,
	DEPTHGEN_ALPHA_THRESHOLD,
	DEPTHGEN_OUTPUT_ALPHA,
	DEPTHGEN_NUM_PARAMS
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

constexpr A_long DEPTHGEN_FAST_SHORT_EDGE = 392;
constexpr A_long DEPTHGEN_BALANCED_SHORT_EDGE = 518;
constexpr A_long DEPTHGEN_HIGH_SHORT_EDGE = 700;
constexpr A_long DEPTHGEN_CUSTOM_EDGE_MIN = 280;
constexpr A_long DEPTHGEN_CUSTOM_EDGE_MAX = 1400;

struct DepthGenRenderSettings {
	A_long short_edge = DEPTHGEN_BALANCED_SHORT_EDGE;
	float far_percentile = 2.0f;
	float near_percentile = 98.0f;
	float contrast = 1.0f;
	float alpha_threshold = 0.0f;
	bool invert = false;
	bool linear_to_srgb = false;
	bool use_alpha_for_levels = true;
	bool preserve_alpha = true;
};

struct DepthGenPreRenderData {
	DepthGenRenderSettings settings;
};

PF_Err DepthGen_RenderWorld(
	PF_InData* in_data,
	PF_OutData* out_data,
	PF_PixelFormat pixel_format,
	PF_EffectWorld* input_world,
	PF_EffectWorld* output_world,
	const DepthGenRenderSettings& settings);

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
