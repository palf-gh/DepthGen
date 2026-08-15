# Preprocess a PiPL resource through MSVC without #line markers.
if(NOT DEFINED DEPTHGEN_PIPL_COMPILER OR NOT DEFINED DEPTHGEN_PIPL_OUTPUT OR
   NOT DEFINED DEPTHGEN_PIPL_SOURCE OR NOT DEFINED DEPTHGEN_PIPL_AESDK OR
   NOT DEFINED DEPTHGEN_PIPL_SOURCE_DIR)
  message(FATAL_ERROR "DepthGen PiPL preprocessor variables are incomplete.")
endif()

execute_process(
  COMMAND "${DEPTHGEN_PIPL_COMPILER}" /nologo /EP
    "/I${DEPTHGEN_PIPL_AESDK}/Headers"
    "/I${DEPTHGEN_PIPL_AESDK}/Resources"
    "/I${DEPTHGEN_PIPL_SOURCE_DIR}/Source"
    /DMSWindows
    "${DEPTHGEN_PIPL_SOURCE}"
  OUTPUT_FILE "${DEPTHGEN_PIPL_OUTPUT}"
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "DepthGen PiPL preprocessing failed (${result}).")
endif()
