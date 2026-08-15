if(NOT DEFINED DEPTHGEN_BUNDLE OR NOT IS_DIRECTORY "${DEPTHGEN_BUNDLE}")
  message(FATAL_ERROR "DEPTHGEN_BUNDLE must name a staged DepthGen.plugin bundle.")
endif()

set(_depthgen_binary "${DEPTHGEN_BUNDLE}/Contents/MacOS/DepthGen")
if(NOT EXISTS "${_depthgen_binary}")
  message(FATAL_ERROR "DepthGen bundle has no Contents/MacOS/DepthGen executable.")
endif()
if(NOT IS_DIRECTORY "${DEPTHGEN_BUNDLE}/Contents/Frameworks")
  message(FATAL_ERROR "DepthGen bundle has no embedded Frameworks directory.")
endif()

find_program(DEPTHGEN_OTOOL otool REQUIRED)
execute_process(
  COMMAND "${DEPTHGEN_OTOOL}" -l "${_depthgen_binary}"
  RESULT_VARIABLE _depthgen_otool_result
  OUTPUT_VARIABLE _depthgen_otool_output
  ERROR_VARIABLE _depthgen_otool_error)
if(NOT _depthgen_otool_result EQUAL 0)
  message(FATAL_ERROR "otool failed while checking DepthGen rpath: ${_depthgen_otool_error}")
endif()
string(FIND "${_depthgen_otool_output}" "@loader_path/../Frameworks" _depthgen_rpath_offset)
if(_depthgen_rpath_offset EQUAL -1)
  message(FATAL_ERROR "DepthGen bundle does not contain the required @loader_path/../Frameworks rpath.")
endif()

find_program(DEPTHGEN_CODESIGN codesign REQUIRED)
execute_process(
  COMMAND "${DEPTHGEN_CODESIGN}" --verify --deep --strict "${DEPTHGEN_BUNDLE}"
  RESULT_VARIABLE _depthgen_codesign_result
  OUTPUT_VARIABLE _depthgen_codesign_output
  ERROR_VARIABLE _depthgen_codesign_error)
if(NOT _depthgen_codesign_result EQUAL 0)
  message(FATAL_ERROR "codesign verification failed: ${_depthgen_codesign_output}${_depthgen_codesign_error}")
endif()
message(STATUS "Verified DepthGen macOS rpath and bundle signature.")
