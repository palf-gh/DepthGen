#include "DepthGen.h"
#include "Localise/DepthGenStrings.h"

#include <cstring>

PF_Err DepthGen_UpdateParamsUI(
	PF_InData* in_data,
	PF_OutData* out_data,
	PF_ParamDef* params[],
	PF_LayerDef* output) {
	(void)output;
	if (!in_data || !params || !in_data->pica_basicP) {
		return PF_Err_NONE;
	}
	const A_long model_index = ParamIndexFromID(DEPTHGEN_MODEL);
	const A_long quality_index = ParamIndexFromID(DEPTHGEN_QUALITY);
	const A_long custom_index = ParamIndexFromID(DEPTHGEN_CUSTOM_SHORT_EDGE);
	if (model_index < 1 || quality_index < 1 || custom_index < 1 ||
		!params[model_index] || !params[quality_index] || !params[custom_index]) {
		return PF_Err_NONE;
	}
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	if (!suites.ParamUtilsSuite3()) {
		return PF_Err_NONE;
	}
	using depthgen_localise::GetString;
	const bool dav2 = params[model_index]->u.pd.value == DEPTHGEN_MODEL_DAV2_SMALL;
	const char* quality_items = GetString(
		dav2 ? DepthGenString::Dav2QualityItems : DepthGenString::QualityItems, in_data);
	PF_Err err = PF_Err_NONE;
	if (!params[quality_index]->u.pd.u.namesptr ||
		std::strcmp(params[quality_index]->u.pd.u.namesptr, quality_items) != 0) {
		PF_ParamDef updated = *params[quality_index];
		updated.u.pd.u.namesptr = quality_items;
		err = suites.ParamUtilsSuite3()->PF_UpdateParamUI(
			in_data->effect_ref, quality_index, &updated);
	}
	const bool enable_custom = params[quality_index]->u.pd.value == DEPTHGEN_QUALITY_CUSTOM;
	const bool currently_disabled = (params[custom_index]->ui_flags & PF_PUI_DISABLED) != 0;
	if (!err && currently_disabled != !enable_custom) {
		PF_ParamDef updated = *params[custom_index];
		if (enable_custom) {
			updated.ui_flags &= ~static_cast<A_long>(PF_PUI_DISABLED);
		} else {
			updated.ui_flags |= PF_PUI_DISABLED;
		}
		err = suites.ParamUtilsSuite3()->PF_UpdateParamUI(
			in_data->effect_ref, custom_index, &updated);
	}
	(void)out_data;
	return err;
}
