// Shared PiPL and C++ identity constants. Keep this preprocessor-only.
#pragma once

#if defined(_DEBUG) || defined(DEBUG) || defined(DEPTHGEN_DEBUG_BUILD)
#define DEPTHGEN_NAME "DepthGen debug"
#define DEPTHGEN_MATCH_NAME "PALF DepthGen debug"
#else
#define DEPTHGEN_NAME "DepthGen"
#define DEPTHGEN_MATCH_NAME "PALF DepthGen"
#endif
#define DEPTHGEN_CATEGORY "3D Channel"
#define DEPTHGEN_DESCRIPTION "\nRelative depth-map generation with ZipDepth and Depth Anything V2 Small."
#define DEPTHGEN_SUPPORT_URL "https://github.com/palf-gh/DepthGen"

// The PiPL floor intentionally matches AE 2022-era SmartFX/MFR support.
#define DEPTHGEN_PIPL_SPEC_VERSION 13
#define DEPTHGEN_PIPL_SPEC_SUBVERS 27
// AE_Effect_Support_URL arrived with API 13.28. Omit it from the PiPL so AE
// 2023.0-23.3 still recognise the plug-in; PluginDataEntryFunction2 still
// supplies the URL on newer hosts.
#define DEPTHGEN_PIPL_HAS_SUPPORT_URL 0
#define DEPTHGEN_VERSION_MAJOR 1
#define DEPTHGEN_VERSION_MINOR 0
#define DEPTHGEN_VERSION_BUG 1
#define DEPTHGEN_VERSION_STAGE 3
#define DEPTHGEN_VERSION_BUILD 1
// PF_VERSION(1, 0, 1, PF_Stage_RELEASE, 1), written literally for PiPLtool.
// vers<<19 | subvers<<15 | bugvers<<11 | stage<<9 | build
// = (1<<19)|(1<<11)|(3<<9)|1 = 527873.
#define DEPTHGEN_VERSION_PACKED 527873

// PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_SEND_UPDATE_PARAMS_UI |
// PF_OutFlag_SEQUENCE_DATA_NEEDS_FLATTENING. PiPLtool needs a literal.
#define DEPTHGEN_OUT_FLAGS 100663312
// PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_FLOAT_COLOR_AWARE |
// PF_OutFlag2_SUPPORTS_THREADED_RENDERING |
// PF_OutFlag2_SUPPORTS_GET_FLATTENED_SEQUENCE_DATA.
// 1024 + 4096 + 134217728 + 8388608
#define DEPTHGEN_OUT_FLAGS2 142611456
