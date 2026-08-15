#pragma once

#include <cstring>

enum class DepthGenString {
	QualityName,
	QualityItems,
	FarPercentile,
	NearPercentile,
	Contrast,
	Invert,
	ShowAdvanced,
	AdvancedGroup,
	CustomShortEdge,
	InputTransfer,
	InputTransferItems,
	UseAlpha,
	AlphaThreshold,
	OutputAlpha,
	OutputAlphaItems
};

namespace depthgen_localise {

inline const char* English(DepthGenString key) {
	switch (key) {
	case DepthGenString::QualityName: return "Quality";
	case DepthGenString::QualityItems: return "Fast (392 px)|Balanced (518 px)|High (700 px)|Custom";
	case DepthGenString::FarPercentile: return "Far Clip";
	case DepthGenString::NearPercentile: return "Near Clip";
	case DepthGenString::Contrast: return "Contrast";
	case DepthGenString::Invert: return "Invert Depth";
	case DepthGenString::ShowAdvanced: return "Show Advanced Controls";
	case DepthGenString::AdvancedGroup: return "Advanced";
	case DepthGenString::CustomShortEdge: return "Custom Short Edge";
	case DepthGenString::InputTransfer: return "Input Transfer";
	case DepthGenString::InputTransferItems: return "Assume sRGB|Linear to sRGB";
	case DepthGenString::UseAlpha: return "Use Alpha for Levels";
	case DepthGenString::AlphaThreshold: return "Alpha Threshold";
	case DepthGenString::OutputAlpha: return "Output Alpha";
	case DepthGenString::OutputAlphaItems: return "Preserve Source Alpha|Opaque";
	}
	return "";
}

inline const char* Japanese(DepthGenString key) {
	switch (key) {
	case DepthGenString::QualityName: return u8"品質";
	case DepthGenString::QualityItems: return u8"高速 (392 px)|標準 (518 px)|高品質 (700 px)|カスタム";
	case DepthGenString::FarPercentile: return u8"遠景クリップ";
	case DepthGenString::NearPercentile: return u8"近景クリップ";
	case DepthGenString::Contrast: return u8"コントラスト";
	case DepthGenString::Invert: return u8"深度を反転";
	case DepthGenString::ShowAdvanced: return u8"詳細設定を表示";
	case DepthGenString::AdvancedGroup: return u8"詳細設定";
	case DepthGenString::CustomShortEdge: return u8"カスタム短辺";
	case DepthGenString::InputTransfer: return u8"入力トランスファー";
	case DepthGenString::InputTransferItems: return u8"sRGB として扱う|リニアから sRGB";
	case DepthGenString::UseAlpha: return u8"レベルにアルファを使用";
	case DepthGenString::AlphaThreshold: return u8"アルファしきい値";
	case DepthGenString::OutputAlpha: return u8"出力アルファ";
	case DepthGenString::OutputAlphaItems: return u8"ソースアルファを保持|不透明";
	}
	return English(key);
}

inline const char* Chinese(DepthGenString key) {
	switch (key) {
	case DepthGenString::QualityName: return u8"质量";
	case DepthGenString::QualityItems: return u8"快速 (392 px)|均衡 (518 px)|高质量 (700 px)|自定义";
	case DepthGenString::FarPercentile: return u8"远景裁剪";
	case DepthGenString::NearPercentile: return u8"近景裁剪";
	case DepthGenString::Contrast: return u8"对比度";
	case DepthGenString::Invert: return u8"反转深度";
	case DepthGenString::ShowAdvanced: return u8"显示高级控件";
	case DepthGenString::AdvancedGroup: return u8"高级";
	case DepthGenString::CustomShortEdge: return u8"自定义短边";
	case DepthGenString::InputTransfer: return u8"输入传递函数";
	case DepthGenString::InputTransferItems: return u8"假定 sRGB|线性转 sRGB";
	case DepthGenString::UseAlpha: return u8"使用 Alpha 计算层级";
	case DepthGenString::AlphaThreshold: return u8"Alpha 阈值";
	case DepthGenString::OutputAlpha: return u8"输出 Alpha";
	case DepthGenString::OutputAlphaItems: return u8"保留源 Alpha|不透明";
	}
	return English(key);
}

inline const char* Korean(DepthGenString key) {
	switch (key) {
	case DepthGenString::QualityName: return u8"품질";
	case DepthGenString::QualityItems: return u8"빠름 (392 px)|균형 (518 px)|고품질 (700 px)|사용자 지정";
	case DepthGenString::FarPercentile: return u8"원거리 클립";
	case DepthGenString::NearPercentile: return u8"근거리 클립";
	case DepthGenString::Contrast: return u8"대비";
	case DepthGenString::Invert: return u8"깊이 반전";
	case DepthGenString::ShowAdvanced: return u8"고급 컨트롤 표시";
	case DepthGenString::AdvancedGroup: return u8"고급";
	case DepthGenString::CustomShortEdge: return u8"사용자 지정 짧은 변";
	case DepthGenString::InputTransfer: return u8"입력 전달 함수";
	case DepthGenString::InputTransferItems: return u8"sRGB로 간주|선형에서 sRGB";
	case DepthGenString::UseAlpha: return u8"레벨에 알파 사용";
	case DepthGenString::AlphaThreshold: return u8"알파 임계값";
	case DepthGenString::OutputAlpha: return u8"출력 알파";
	case DepthGenString::OutputAlphaItems: return u8"소스 알파 유지|불투명";
	}
	return English(key);
}

inline const char* GetString(DepthGenString key, PF_InData* in_data) {
	if (!in_data || !in_data->pica_basicP) {
		return English(key);
	}
	PFAppSuite6* app_suite = nullptr;
	if (AEFX_AcquireSuite(in_data, nullptr, kPFAppSuite, kPFAppSuiteVersion6, nullptr,
	reinterpret_cast<void**>(&app_suite)) != PF_Err_NONE || !app_suite) {
		return English(key);
	}
	A_char language[PF_APP_LANG_TAG_SIZE]{};
	const PF_Err err = app_suite->PF_AppGetLanguage(language);
	(void)AEFX_ReleaseSuite(in_data, nullptr, kPFAppSuite, kPFAppSuiteVersion6, nullptr);
	if (err != PF_Err_NONE) {
		return English(key);
	}
	if (std::strncmp(language, "ja", 2) == 0) return Japanese(key);
	if (std::strncmp(language, "zh", 2) == 0) return Chinese(key);
	if (std::strncmp(language, "ko", 2) == 0) return Korean(key);
	return English(key);
}

} // namespace depthgen_localise
