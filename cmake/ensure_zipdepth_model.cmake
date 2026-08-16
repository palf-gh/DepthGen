if(NOT DEFINED DEPTHGEN_MODEL_OUTPUT OR
   NOT DEFINED DEPTHGEN_EXPECTED_SHA256 OR
   NOT DEFINED DEPTHGEN_PYTHON_EXECUTABLE OR
   NOT DEFINED DEPTHGEN_EXPORT_SCRIPT OR
   NOT DEFINED DEPTHGEN_SOURCE_ROOT OR
   NOT DEFINED DEPTHGEN_CHECKPOINT)
  message(FATAL_ERROR "ZipDepth model export requires output, hash, Python, script, source, and checkpoint.")
endif()

if(EXISTS "${DEPTHGEN_MODEL_OUTPUT}")
  file(SHA256 "${DEPTHGEN_MODEL_OUTPUT}" existing_sha256)
  if(existing_sha256 STREQUAL DEPTHGEN_EXPECTED_SHA256)
    # Keep the custom-command output newer than its verified acquisition stamp
    # without requiring PyTorch again on machines that only build the plug-in.
    file(TOUCH "${DEPTHGEN_MODEL_OUTPUT}")
    message(STATUS "Existing ZipDepth ONNX is SHA-256 verified.")
    return()
  endif()
endif()

execute_process(
  COMMAND "${DEPTHGEN_PYTHON_EXECUTABLE}" "${DEPTHGEN_EXPORT_SCRIPT}"
    --source-root "${DEPTHGEN_SOURCE_ROOT}"
    --checkpoint "${DEPTHGEN_CHECKPOINT}"
    --output "${DEPTHGEN_MODEL_OUTPUT}"
    --expect-output-sha256 "${DEPTHGEN_EXPECTED_SHA256}"
  RESULT_VARIABLE export_result)
if(NOT export_result EQUAL 0)
  message(FATAL_ERROR "Failed to export the verified ZipDepth ONNX model.")
endif()
