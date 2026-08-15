#include "DepthGen.h"

PF_Err DepthGen_UpdateParamsUI(
	PF_InData* in_data,
	PF_OutData* out_data,
	PF_ParamDef* params[],
	PF_LayerDef* output) {
	(void)out_data;
	(void)output;
	if (!in_data || !params || !in_data->pica_basicP) {
		return PF_Err_NONE;
	}
	const A_long quality_index = ParamIndexFromID(DEPTHGEN_QUALITY);
	const A_long custom_index = ParamIndexFromID(DEPTHGEN_CUSTOM_SHORT_EDGE);
	if (quality_index < 1 || custom_index < 1 || !params[quality_index] || !params[custom_index]) {
		return PF_Err_NONE;
	}
	const bool enable_custom = params[quality_index]->u.pd.value == DEPTHGEN_QUALITY_CUSTOM;
	const bool currently_disabled = (params[custom_index]->ui_flags & PF_PUI_DISABLED) != 0;
	if (currently_disabled == !enable_custom) {
		return PF_Err_NONE;
	}
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	if (!suites.ParamUtilsSuite3()) {
		return PF_Err_NONE;
	}
	PF_ParamDef updated = *params[custom_index];
	if (enable_custom) {
		updated.ui_flags &= ~static_cast<A_long>(PF_PUI_DISABLED);
	} else {
		updated.ui_flags |= PF_PUI_DISABLED;
	}
	const PF_Err err = suites.ParamUtilsSuite3()->PF_UpdateParamUI(
		in_data->effect_ref, custom_index, &updated);
	if (!err) {
		params[custom_index]->ui_flags = updated.ui_flags;
	}
	return err;
}
