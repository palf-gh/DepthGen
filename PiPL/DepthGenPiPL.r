#include "AEConfig.h"
#include "AE_EffectVers.h"
#include "DepthGen_Target.h"

#ifndef AE_OS_WIN
#include "AE_General.r"
#endif

resource 'PiPL' (16000) {
    {
        Kind { AEEffect },
        Name { DEPTHGEN_NAME },
        Category { DEPTHGEN_CATEGORY },
#ifdef AE_OS_WIN
#ifdef AE_PROC_INTELx64
        CodeWin64X86 { "EffectMain" },
#endif
#else
#ifdef AE_OS_MAC
        CodeMacIntel64 { "EffectMain" },
        CodeMacARM64 { "EffectMain" },
#endif
#endif
        AE_PiPL_Version { 2, 0 },
        AE_Effect_Spec_Version { DEPTHGEN_PIPL_SPEC_VERSION, DEPTHGEN_PIPL_SPEC_SUBVERS },
        AE_Effect_Version { DEPTHGEN_VERSION_PACKED },
        AE_Effect_Info_Flags { 0 },
        AE_Effect_Global_OutFlags { DEPTHGEN_OUT_FLAGS },
        AE_Effect_Global_OutFlags_2 { DEPTHGEN_OUT_FLAGS2 },
        AE_Effect_Match_Name { DEPTHGEN_MATCH_NAME },
#if DEPTHGEN_PIPL_HAS_SUPPORT_URL
        AE_Effect_Support_URL { DEPTHGEN_SUPPORT_URL },
#endif
        AE_Reserved_Info { 0 }
    }
};
