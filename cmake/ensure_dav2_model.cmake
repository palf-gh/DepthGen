if(NOT DEFINED DEPTHGEN_MODEL_OUTPUT OR
   NOT DEFINED DEPTHGEN_EXPECTED_SHA256 OR
   NOT DEFINED DEPTHGEN_PYTHON_EXECUTABLE OR
   NOT DEFINED DEPTHGEN_EXPORT_SCRIPT OR
   NOT DEFINED DEPTHGEN_INPUT_MODEL)
  message(FATAL_ERROR "DAV2 model export requires output, hash, Python, script, and input.")
endif()

if(EXISTS "${DEPTHGEN_MODEL_OUTPUT}")
  file(SHA256 "${DEPTHGEN_MODEL_OUTPUT}" existing_sha256)
  if(existing_sha256 STREQUAL DEPTHGEN_EXPECTED_SHA256)
    file(TOUCH "${DEPTHGEN_MODEL_OUTPUT}")
    message(STATUS "Existing Depth Anything V2 Small ONNX is SHA-256 verified.")
    return()
  endif()
endif()

execute_process(
  COMMAND "${DEPTHGEN_PYTHON_EXECUTABLE}" "${DEPTHGEN_EXPORT_SCRIPT}"
    --input "${DEPTHGEN_INPUT_MODEL}"
    --output "${DEPTHGEN_MODEL_OUTPUT}"
    --expect-output-sha256 "${DEPTHGEN_EXPECTED_SHA256}"
  RESULT_VARIABLE export_result)
if(NOT export_result EQUAL 0)
  message(FATAL_ERROR "Failed to export the verified Depth Anything V2 Small ONNX model.")
endif()
