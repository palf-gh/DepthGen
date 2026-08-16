if(NOT DEFINED DEPTHGEN_BUNDLE)
  message(FATAL_ERROR "sign_macos_runtime requires DEPTHGEN_BUNDLE.")
endif()

file(GLOB runtime_libraries "${DEPTHGEN_BUNDLE}/Contents/Frameworks/*.dylib")
foreach(runtime_library IN LISTS runtime_libraries)
  execute_process(
    COMMAND codesign --force --sign - "${runtime_library}"
    RESULT_VARIABLE sign_result)
  if(NOT sign_result EQUAL 0)
    message(FATAL_ERROR "Failed to ad-hoc sign ${runtime_library}")
  endif()
endforeach()
