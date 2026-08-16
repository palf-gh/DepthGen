if(NOT DEFINED DEPTHGEN_RUNTIME_DESTINATION)
  message(FATAL_ERROR "clean_onnxruntime requires DEPTHGEN_RUNTIME_DESTINATION.")
endif()

file(GLOB stale_runtime_files
  "${DEPTHGEN_RUNTIME_DESTINATION}/onnxruntime*.dll"
  "${DEPTHGEN_RUNTIME_DESTINATION}/libonnxruntime*.dylib")
foreach(stale_runtime_file IN LISTS stale_runtime_files)
  file(REMOVE "${stale_runtime_file}")
endforeach()
