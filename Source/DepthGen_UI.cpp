#include "DepthGen.h"

namespace {

bool ReadCheckbox(PF_InData* in_data, A_long parameter_id, bool fallback) {
	PF_ParamDef parameter{};
	if (PF_CHECKOUT_PARAM(in_data, parameter_id, in_data->current_time, in_data->time_step,
		in_data->time_scale, &parameter) != PF_Err_NONE) {
		return fallback;
	}
	const bool value = parameter.u.bd.value != 0;
	(void)PF_CHECKIN_PARAM(in_data, &parameter);
	return value;
}

PF_Err SetVisible(
	PF_InData* in_data,
	AEGP_SuiteHandler& suites,
	AEGP_PluginID plugin_id,
	AEGP_EffectRefH effect,
	A_long parameter_index,
	bool visible) {
	(void)in_data;
	if (!plugin_id || !effect || !suites.StreamSuite2() || !suites.DynamicStreamSuite2()) {
		return PF_Err_NONE;
	}
	AEGP_StreamRefH stream = nullptr;
	PF_Err err = suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(
		plugin_id, effect, parameter_index, &stream);
	if (!err && stream) {
		err = suites.DynamicStreamSuite2()->AEGP_SetDynamicStreamFlag(
			stream, AEGP_DynStreamFlag_HIDDEN, FALSE, !visible);
		(void)suites.StreamSuite2()->AEGP_DisposeStream(stream);
	}
	return err;
}

} // namespace

PF_Err DepthGen_UpdateParamsUI(
	PF_InData* in_data,
	PF_OutData* out_data,
	PF_ParamDef* params[],
	PF_LayerDef* output) {
	(void)out_data;
	(void)params;
	(void)output;
	if (!in_data || !in_data->global_data || !in_data->pica_basicP || !params) {
		return PF_Err_NONE;
	}
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	if (!suites.HandleSuite1() || !suites.PFInterfaceSuite1() || !suites.EffectSuite2()) {
		return PF_Err_NONE;
	}
	DepthGenGlobalData* global = reinterpret_cast<DepthGenGlobalData*>(
		suites.HandleSuite1()->host_lock_handle(in_data->global_data));
	const AEGP_PluginID plugin_id = global ? global->plugin_id : 0;
	if (global) {
		suites.HandleSuite1()->host_unlock_handle(in_data->global_data);
	}
	AEGP_EffectRefH effect = nullptr;
	PF_Err err = suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(plugin_id, in_data->effect_ref, &effect);
	if (err || !effect) {
		return err;
	}
	const bool show_advanced = ReadCheckbox(in_data, DEPTHGEN_SHOW_ADVANCED, false);
	const A_long advanced[] = {
		DEPTHGEN_CUSTOM_SHORT_EDGE,
		DEPTHGEN_INPUT_TRANSFER,
		DEPTHGEN_USE_ALPHA_FOR_LEVELS,
		DEPTHGEN_ALPHA_THRESHOLD,
		DEPTHGEN_OUTPUT_ALPHA};
	for (A_long index : advanced) {
		const PF_Err visibility_error = SetVisible(in_data, suites, plugin_id, effect, index, show_advanced);
		if (!err && visibility_error) {
			err = visibility_error;
		}
	}
	(void)suites.EffectSuite2()->AEGP_DisposeEffect(effect);
	return err;
}
