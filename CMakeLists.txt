cmake_minimum_required(VERSION 3.18)
project(DepthGen LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

option(DEPTHGEN_ENABLE_ONNX_RUNTIME "Compile inference support when DEPTHGEN_ORT_ROOT is supplied" ON)
option(DEPTHGEN_ENABLE_CUDA "Use CUDA when the supplied Windows ORT build exports it" ON)
option(DEPTHGEN_ENABLE_DIRECTML "Use DirectML when the supplied Windows ORT build exports it" ON)
option(DEPTHGEN_ENABLE_COREML "Use Core ML when the supplied macOS ORT build exports it" ON)
option(DEPTHGEN_COREML_MLPROGRAM "Compile Core ML MLProgram support and fall back at runtime when unavailable" ON)
option(DEPTHGEN_COREML_CPU_AND_GPU "Restrict Core ML MLProgram to CPU+GPU units instead of allowing ANE" OFF)
option(DEPTHGEN_BUILD_TESTS "Build deterministic image-pipeline tests" ON)
option(DEPTHGEN_PACKAGE_RELEASE "Assemble a distribution-ready package; requires verified model and ORT" OFF)
set(DEPTHGEN_ORT_ROOT "" CACHE PATH "Pinned ONNX Runtime SDK root")
set(DEPTHGEN_AE_PLUGIN_DIR "" CACHE PATH "Optional After Effects plug-in installation directory")

# Locate the SDK Examples root from this standalone repository.
if(NOT DEFINED AESDK_ROOT OR NOT EXISTS "${AESDK_ROOT}/Headers")
  get_filename_component(_examples_root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
  if(EXISTS "${_examples_root}/Headers")
    set(AESDK_ROOT "${_examples_root}" CACHE PATH "After Effects SDK Examples root" FORCE)
  endif()
endif()
if(NOT EXISTS "${AESDK_ROOT}/Headers")
  message(FATAL_ERROR "AESDK_ROOT must name an After Effects SDK Examples directory with Headers/.")
endif()

include(cmake/DepthGenAssets.cmake)
depthgen_add_model_target("${CMAKE_CURRENT_SOURCE_DIR}/Resources/Models")
set(DEPTHGEN_ZIPDEPTH_MODEL_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Models/${DEPTHGEN_ZIPDEPTH_MODEL_FILENAME}")
set(DEPTHGEN_DAV2_MODEL_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Models/${DEPTHGEN_DAV2_MODEL_FILENAME}")
set(DEPTHGEN_MODEL_PATH "${DEPTHGEN_ZIPDEPTH_MODEL_PATH}")

set(DEPTHGEN_SOURCES
  Source/DepthGen.h
  Source/DepthGen_Target.h
  Source/DepthGen.cpp
  Source/DepthGen_UI.cpp
  Source/DepthGen_Image.h
  Source/DepthGen_Image.cpp
  Source/DepthGen_Pipeline.h
  Source/DepthGen_Temporal.h
  Source/DepthGen_Temporal.cpp
  Source/DepthGen_EmbeddedModel.h
  Source/DepthGen_EmbeddedModel.cpp
  Source/DepthGen_ModelIntegrity.h
  Source/DepthGen_ModelIntegrity.cpp
  Source/DepthGen_Inference.h
  Source/DepthGen_Inference.cpp
  Localise/DepthGenStrings.h
  "${AESDK_ROOT}/Util/AEGP_SuiteHandler.cpp"
  "${AESDK_ROOT}/Util/MissingSuiteError.cpp"
  "${AESDK_ROOT}/Util/AEFX_SuiteHelper.c"
  "${AESDK_ROOT}/Util/Smart_Utils.cpp")

add_library(DepthGen MODULE ${DEPTHGEN_SOURCES})
target_include_directories(DepthGen PRIVATE
  "${AESDK_ROOT}/Headers"
  "${AESDK_ROOT}/Headers/SP"
  "${AESDK_ROOT}/Util"
  "${AESDK_ROOT}/Resources"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_definitions(DepthGen PRIVATE
  $<$<CONFIG:Debug>:DEPTHGEN_DEBUG_BUILD=1>
  _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
  $<$<BOOL:${WIN32}>:_CRT_SECURE_NO_WARNINGS>)

if(MSVC)
  target_compile_options(DepthGen PRIVATE /utf-8 /W4 /permissive-)
endif()

set(DEPTHGEN_HAS_ORT FALSE)
set(DEPTHGEN_ORT_PROVIDER_DEFINITIONS)
if(DEPTHGEN_ENABLE_ONNX_RUNTIME AND DEPTHGEN_ORT_ROOT)
  find_path(DEPTHGEN_ORT_INCLUDE_DIR onnxruntime_cxx_api.h
    PATHS "${DEPTHGEN_ORT_ROOT}/include" "${DEPTHGEN_ORT_ROOT}/build/native/include" NO_DEFAULT_PATH)
  find_library(DEPTHGEN_ORT_LIBRARY NAMES onnxruntime
    PATHS "${DEPTHGEN_ORT_ROOT}/lib" "${DEPTHGEN_ORT_ROOT}/lib64"
      "${DEPTHGEN_ORT_ROOT}/runtimes/win-x64/native" NO_DEFAULT_PATH)
  if(DEPTHGEN_ORT_INCLUDE_DIR AND DEPTHGEN_ORT_LIBRARY)
    set(DEPTHGEN_HAS_ORT TRUE)
    target_include_directories(DepthGen PRIVATE "${DEPTHGEN_ORT_INCLUDE_DIR}")
    target_link_libraries(DepthGen PRIVATE "${DEPTHGEN_ORT_LIBRARY}")
    target_compile_definitions(DepthGen PRIVATE DEPTHGEN_HAS_ORT=1 DEPTHGEN_EMBEDDED_MODEL=1)
    if(TARGET depthgen_build_model)
      add_dependencies(DepthGen depthgen_build_model)
    else()
      if(NOT EXISTS "${DEPTHGEN_ZIPDEPTH_MODEL_PATH}" OR NOT EXISTS "${DEPTHGEN_DAV2_MODEL_PATH}")
        message(FATAL_ERROR "DepthGen requires ${DEPTHGEN_ZIPDEPTH_MODEL_PATH} and "
          "${DEPTHGEN_DAV2_MODEL_PATH}; install Python 3 and build depthgen_build_model.")
      endif()
      file(SHA256 "${DEPTHGEN_ZIPDEPTH_MODEL_PATH}" DEPTHGEN_EXISTING_ZIPDEPTH_SHA256)
      file(SHA256 "${DEPTHGEN_DAV2_MODEL_PATH}" DEPTHGEN_EXISTING_DAV2_SHA256)
      if(NOT DEPTHGEN_EXISTING_ZIPDEPTH_SHA256 STREQUAL DEPTHGEN_ZIPDEPTH_OUTPUT_SHA256)
        message(FATAL_ERROR "The existing ZipDepth ONNX does not match the pinned SHA-256.")
      endif()
      if(NOT DEPTHGEN_EXISTING_DAV2_SHA256 STREQUAL DEPTHGEN_DAV2_OUTPUT_SHA256)
        message(FATAL_ERROR "The existing Depth Anything V2 Small ONNX does not match the pinned SHA-256.")
      endif()
    endif()
    if(WIN32)
      if(DEPTHGEN_ENABLE_CUDA AND
         (EXISTS "${DEPTHGEN_ORT_ROOT}/lib/onnxruntime_providers_cuda.dll" OR
          EXISTS "${DEPTHGEN_ORT_ROOT}/runtimes/win-x64/native/onnxruntime_providers_cuda.dll"))
        list(APPEND DEPTHGEN_ORT_PROVIDER_DEFINITIONS DEPTHGEN_ORT_CUDA=1)
      endif()
      if(DEPTHGEN_ENABLE_DIRECTML AND
         (EXISTS "${DEPTHGEN_ORT_INCLUDE_DIR}/dml_provider_factory.h" OR
          EXISTS "${DEPTHGEN_ORT_INCLUDE_DIR}/onnxruntime/core/providers/dml/dml_provider_factory.h" OR
          EXISTS "${DEPTHGEN_ORT_INCLUDE_DIR}/core/providers/dml/dml_provider_factory.h"))
        list(APPEND DEPTHGEN_ORT_PROVIDER_DEFINITIONS DEPTHGEN_ORT_DML=1)
      endif()
    elseif(APPLE AND DEPTHGEN_ENABLE_COREML)
      set(DEPTHGEN_COREML_PROVIDER_HEADER "")
      if(EXISTS "${DEPTHGEN_ORT_INCLUDE_DIR}/coreml_provider_factory.h")
        set(DEPTHGEN_COREML_PROVIDER_HEADER "${DEPTHGEN_ORT_INCLUDE_DIR}/coreml_provider_factory.h")
      elseif(EXISTS "${DEPTHGEN_ORT_INCLUDE_DIR}/onnxruntime/core/providers/coreml/coreml_provider_factory.h")
        set(DEPTHGEN_COREML_PROVIDER_HEADER
          "${DEPTHGEN_ORT_INCLUDE_DIR}/onnxruntime/core/providers/coreml/coreml_provider_factory.h")
      endif()
      if(DEPTHGEN_COREML_PROVIDER_HEADER)
        list(APPEND DEPTHGEN_ORT_PROVIDER_DEFINITIONS DEPTHGEN_ORT_COREML=1)
        file(READ "${DEPTHGEN_COREML_PROVIDER_HEADER}" DEPTHGEN_COREML_PROVIDER_HEADER_TEXT)
        if(DEPTHGEN_COREML_MLPROGRAM AND
           DEPTHGEN_COREML_PROVIDER_HEADER_TEXT MATCHES "COREML_FLAG_CREATE_MLPROGRAM")
          list(APPEND DEPTHGEN_ORT_PROVIDER_DEFINITIONS DEPTHGEN_COREML_MLPROGRAM=1)
        elseif(DEPTHGEN_COREML_MLPROGRAM)
          message(STATUS "DepthGen: this ONNX Runtime has no Core ML MLProgram flag; "
            "NeuralNetwork will be used with CPU fallback.")
        endif()
        if(DEPTHGEN_COREML_CPU_AND_GPU AND
           DEPTHGEN_COREML_PROVIDER_HEADER_TEXT MATCHES "COREML_FLAG_USE_CPU_AND_GPU")
          list(APPEND DEPTHGEN_ORT_PROVIDER_DEFINITIONS DEPTHGEN_COREML_CPU_AND_GPU=1)
        endif()
      endif()
    endif()
    if(DEPTHGEN_ORT_PROVIDER_DEFINITIONS)
      target_compile_definitions(DepthGen PRIVATE ${DEPTHGEN_ORT_PROVIDER_DEFINITIONS})
    endif()
  else()
    message(WARNING "DEPTHGEN_ORT_ROOT does not contain an ONNX Runtime SDK; DepthGen will report an actionable runtime error.")
  endif()
elseif(DEPTHGEN_ENABLE_ONNX_RUNTIME)
  message(STATUS "DepthGen: CPU-only source validation build; set DEPTHGEN_ORT_ROOT for inference.")
endif()

if(DEPTHGEN_HAS_ORT)
  file(GLOB DEPTHGEN_ORT_RUNTIME_FILES
    "${DEPTHGEN_ORT_ROOT}/lib/*.dll"
    "${DEPTHGEN_ORT_ROOT}/lib/*.dylib"
    "${DEPTHGEN_ORT_ROOT}/runtimes/win-x64/native/*.dll")
  list(FILTER DEPTHGEN_ORT_RUNTIME_FILES EXCLUDE REGEX "onnxruntime_providers_tensorrt.*\\.dll$")
  if(WIN32)
    add_custom_command(TARGET DepthGen POST_BUILD
      COMMAND "${CMAKE_COMMAND}"
        -DDEPTHGEN_RUNTIME_DESTINATION=$<TARGET_FILE_DIR:DepthGen>
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/clean_onnxruntime.cmake"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPTHGEN_ORT_RUNTIME_FILES}
        "$<TARGET_FILE_DIR:DepthGen>"
      COMMENT "Copying ONNX Runtime beside DepthGen.aex")
  elseif(APPLE)
    add_custom_command(TARGET DepthGen POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_DIR:DepthGen>/Contents/Frameworks"
      COMMAND "${CMAKE_COMMAND}"
        -DDEPTHGEN_RUNTIME_DESTINATION=$<TARGET_BUNDLE_DIR:DepthGen>/Contents/Frameworks
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/clean_onnxruntime.cmake"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPTHGEN_ORT_RUNTIME_FILES}
        "$<TARGET_BUNDLE_DIR:DepthGen>/Contents/Frameworks"
      COMMENT "Embedding ONNX Runtime in DepthGen.plugin")
  endif()
endif()

if(WIN32)
  if(DEPTHGEN_HAS_ORT)
    set(DEPTHGEN_ZIPDEPTH_MODEL_PATH_NATIVE "${DEPTHGEN_ZIPDEPTH_MODEL_PATH}")
    set(DEPTHGEN_DAV2_MODEL_PATH_NATIVE "${DEPTHGEN_DAV2_MODEL_PATH}")
    set(DEPTHGEN_MODEL_RC "${CMAKE_CURRENT_BINARY_DIR}/DepthGenModel.rc")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DepthGenModel.rc.in"
      "${DEPTHGEN_MODEL_RC}" @ONLY)
    set_source_files_properties("${DEPTHGEN_MODEL_RC}" PROPERTIES
      OBJECT_DEPENDS "${DEPTHGEN_ZIPDEPTH_MODEL_PATH};${DEPTHGEN_DAV2_MODEL_PATH}")
    target_sources(DepthGen PRIVATE "${DEPTHGEN_MODEL_RC}")
  endif()
  add_custom_command(TARGET DepthGen POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      -DDEPTHGEN_MODELS_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/Resources/Models
      -DDEPTHGEN_MODELS_DESTINATION=$<TARGET_FILE_DIR:DepthGen>/Resources/Models
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_model_assets.cmake"
    COMMENT "Copying the DepthGen model manifest beside DepthGen.aex")
elseif(APPLE)
  add_custom_command(TARGET DepthGen POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      -DDEPTHGEN_MODELS_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/Resources/Models
      -DDEPTHGEN_MODELS_DESTINATION=$<TARGET_BUNDLE_DIR:DepthGen>/Contents/Resources/Models
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_model_assets.cmake"
    COMMENT "Copying the DepthGen model manifest into DepthGen.plugin")
endif()

# Debug builds use a distinct AE identity and filename so they can sit beside Release.
set(DEPTHGEN_OUTPUT_NAME "$<IF:$<CONFIG:Debug>,DepthGen_debug,DepthGen>")

set(PIPL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/PiPL/DepthGenPiPL.r")
set(TARGET_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/Source/DepthGen_Target.h")
set(PIPL_I_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/DepthGenPiPL_Debug.i")
set(PIPL_I_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/DepthGenPiPL_Release.i")
if(WIN32)
  set(PIPL_RC_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/DepthGenPiPL_Debug.rc")
  set(PIPL_RC_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/DepthGenPiPL_Release.rc")
  add_custom_command(OUTPUT "${PIPL_I_DEBUG}"
    COMMAND "${CMAKE_COMMAND}"
      -DDEPTHGEN_PIPL_COMPILER=${CMAKE_CXX_COMPILER}
      -DDEPTHGEN_PIPL_OUTPUT=${PIPL_I_DEBUG}
      -DDEPTHGEN_PIPL_SOURCE=${PIPL_SOURCE}
      -DDEPTHGEN_PIPL_AESDK=${AESDK_ROOT}
      -DDEPTHGEN_PIPL_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DDEPTHGEN_PIPL_DEBUG=1
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/preprocess_pipl_win.cmake"
    DEPENDS "${PIPL_SOURCE}" "${TARGET_HEADER}"
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/preprocess_pipl_win.cmake"
    COMMENT "Preprocessing DepthGen PiPL (Debug)")
  add_custom_command(OUTPUT "${PIPL_I_RELEASE}"
    COMMAND "${CMAKE_COMMAND}"
      -DDEPTHGEN_PIPL_COMPILER=${CMAKE_CXX_COMPILER}
      -DDEPTHGEN_PIPL_OUTPUT=${PIPL_I_RELEASE}
      -DDEPTHGEN_PIPL_SOURCE=${PIPL_SOURCE}
      -DDEPTHGEN_PIPL_AESDK=${AESDK_ROOT}
      -DDEPTHGEN_PIPL_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/preprocess_pipl_win.cmake"
    DEPENDS "${PIPL_SOURCE}" "${TARGET_HEADER}"
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/preprocess_pipl_win.cmake"
    COMMENT "Preprocessing DepthGen PiPL (Release)")
  add_custom_command(OUTPUT "${PIPL_RC_DEBUG}"
    COMMAND "${AESDK_ROOT}/Resources/PiPLtool.exe" "${PIPL_I_DEBUG}" "${PIPL_RC_DEBUG}"
    DEPENDS "${PIPL_I_DEBUG}"
    COMMENT "Compiling DepthGen PiPL (Debug)")
  add_custom_command(OUTPUT "${PIPL_RC_RELEASE}"
    COMMAND "${AESDK_ROOT}/Resources/PiPLtool.exe" "${PIPL_I_RELEASE}" "${PIPL_RC_RELEASE}"
    DEPENDS "${PIPL_I_RELEASE}"
    COMMENT "Compiling DepthGen PiPL (Release)")
  add_custom_target(DepthGen_pipl DEPENDS "${PIPL_RC_DEBUG}" "${PIPL_RC_RELEASE}")
  add_dependencies(DepthGen DepthGen_pipl)
  set_target_properties(DepthGen PROPERTIES PREFIX "" SUFFIX ".aex" OUTPUT_NAME "${DEPTHGEN_OUTPUT_NAME}"
    LINK_FLAGS "/EXPORT:EffectMain")
  target_compile_definitions(DepthGen PRIVATE MSWindows _WINDOWS)
  target_sources(DepthGen PRIVATE "$<IF:$<CONFIG:Debug>,${PIPL_RC_DEBUG},${PIPL_RC_RELEASE}>")
  set(DEPTHGEN_DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dist/Win/$<CONFIG>")
elseif(APPLE)
  find_program(REZ_TOOL Rez REQUIRED)
  set(PIPL_RSRC_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/DepthGenPiPL_Debug.rsrc")
  set(PIPL_RSRC_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/DepthGenPiPL_Release.rsrc")
  add_custom_command(OUTPUT "${PIPL_I_DEBUG}"
    COMMAND "${CMAKE_CXX_COMPILER}" -E -P -x c
      -I "${AESDK_ROOT}/Headers" -I "${AESDK_ROOT}/Resources" -I "${CMAKE_CURRENT_SOURCE_DIR}/Source"
      -D__MACH__ -DAE_OS_MAC -DDEBUG=1 -DDEPTHGEN_DEBUG_BUILD=1
      -o "${PIPL_I_DEBUG}" "${PIPL_SOURCE}"
    DEPENDS "${PIPL_SOURCE}" "${TARGET_HEADER}")
  add_custom_command(OUTPUT "${PIPL_I_RELEASE}"
    COMMAND "${CMAKE_CXX_COMPILER}" -E -P -x c
      -I "${AESDK_ROOT}/Headers" -I "${AESDK_ROOT}/Resources" -I "${CMAKE_CURRENT_SOURCE_DIR}/Source"
      -D__MACH__ -DAE_OS_MAC
      -o "${PIPL_I_RELEASE}" "${PIPL_SOURCE}"
    DEPENDS "${PIPL_SOURCE}" "${TARGET_HEADER}")
  add_custom_command(OUTPUT "${PIPL_RSRC_DEBUG}"
    COMMAND "${REZ_TOOL}" -o "${PIPL_RSRC_DEBUG}" -useDF
      -i "${AESDK_ROOT}/Headers" -i "${AESDK_ROOT}/Resources" -i "${CMAKE_CURRENT_SOURCE_DIR}/Source" "${PIPL_I_DEBUG}"
    DEPENDS "${PIPL_I_DEBUG}")
  add_custom_command(OUTPUT "${PIPL_RSRC_RELEASE}"
    COMMAND "${REZ_TOOL}" -o "${PIPL_RSRC_RELEASE}" -useDF
      -i "${AESDK_ROOT}/Headers" -i "${AESDK_ROOT}/Resources" -i "${CMAKE_CURRENT_SOURCE_DIR}/Source" "${PIPL_I_RELEASE}"
    DEPENDS "${PIPL_I_RELEASE}")
  add_custom_target(DepthGen_pipl DEPENDS "${PIPL_RSRC_DEBUG}" "${PIPL_RSRC_RELEASE}")
  add_dependencies(DepthGen DepthGen_pipl)
  set(BUNDLE_EXECUTABLE "DepthGen")
  set(BUNDLE_NAME "DepthGen")
  set(BUNDLE_IDENTIFIER "com.palf.depthgen")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Mac/Info.plist.in" "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)
  set(BUNDLE_EXECUTABLE "DepthGen_debug")
  set(BUNDLE_NAME "DepthGen debug")
  set(BUNDLE_IDENTIFIER "com.palf.depthgen.debug")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Mac/Info.plist.in" "${CMAKE_CURRENT_BINARY_DIR}/Info_Debug.plist" @ONLY)
  set_target_properties(DepthGen PROPERTIES BUNDLE TRUE BUNDLE_EXTENSION "plugin" OUTPUT_NAME "${DEPTHGEN_OUTPUT_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.palf.depthgen" OSX_ARCHITECTURES "x86_64;arm64"
    # The executable is Contents/MacOS/DepthGen and the embedded runtime is
    # Contents/Frameworks. Keep this in the final binary as well as in the
    # build tree: release staging copies an already-linked bundle.
    BUILD_RPATH "@loader_path/../Frameworks"
    INSTALL_RPATH "@loader_path/../Frameworks")
  target_link_options(DepthGen PRIVATE
    "-Wl,-sectcreate,__TEXT,__plgin,$<IF:$<CONFIG:Debug>,${PIPL_RSRC_DEBUG},${PIPL_RSRC_RELEASE}>"
    "-Wl,-rpath,@loader_path/../Frameworks")
  if(DEPTHGEN_HAS_ORT)
    target_link_options(DepthGen PRIVATE
      "-Wl,-sectcreate,__DATA_CONST,__zipdepth,${DEPTHGEN_ZIPDEPTH_MODEL_PATH}"
      "-Wl,-sectcreate,__DATA_CONST,__dav2,${DEPTHGEN_DAV2_MODEL_PATH}")
    set_property(TARGET DepthGen APPEND PROPERTY LINK_DEPENDS
      "${DEPTHGEN_ZIPDEPTH_MODEL_PATH};${DEPTHGEN_DAV2_MODEL_PATH}")
  endif()
  find_library(COCOA_LIBRARY Cocoa REQUIRED)
  target_link_libraries(DepthGen PRIVATE "${COCOA_LIBRARY}")
  add_custom_command(TARGET DepthGen POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_DIR:DepthGen>/Contents/Resources/Models"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "$<IF:$<CONFIG:Debug>,${CMAKE_CURRENT_BINARY_DIR}/Info_Debug.plist,${CMAKE_CURRENT_BINARY_DIR}/Info.plist>"
      "$<TARGET_BUNDLE_DIR:DepthGen>/Contents/Info.plist"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "$<IF:$<CONFIG:Debug>,${PIPL_RSRC_DEBUG},${PIPL_RSRC_RELEASE}>"
      "$<TARGET_BUNDLE_DIR:DepthGen>/Contents/Resources/DepthGenPiPL.rsrc"
    COMMAND "${CMAKE_COMMAND}"
      -DDEPTHGEN_BUNDLE=$<TARGET_BUNDLE_DIR:DepthGen>
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sign_macos_runtime.cmake"
    COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:DepthGen>")
  set(DEPTHGEN_DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dist/Mac/$<CONFIG>")
else()
  message(FATAL_ERROR "DepthGen supports Windows and macOS only.")
endif()

set_target_properties(DepthGen PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${DEPTHGEN_DIST_DIR}"
  LIBRARY_OUTPUT_DIRECTORY "${DEPTHGEN_DIST_DIR}")

if(DEPTHGEN_AE_PLUGIN_DIR)
  add_custom_command(TARGET DepthGen POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:DepthGen>" "${DEPTHGEN_AE_PLUGIN_DIR}/"
    COMMENT "Installing DepthGen into the configured After Effects plug-in directory")
endif()

if(DEPTHGEN_PACKAGE_RELEASE)
  if(NOT DEPTHGEN_HAS_ORT)
    message(FATAL_ERROR "DEPTHGEN_PACKAGE_RELEASE requires a pinned DEPTHGEN_ORT_ROOT.")
  endif()
  if(NOT TARGET depthgen_build_model AND
     (NOT EXISTS "${DEPTHGEN_ZIPDEPTH_MODEL_PATH}" OR NOT EXISTS "${DEPTHGEN_DAV2_MODEL_PATH}"))
    message(FATAL_ERROR "DEPTHGEN_PACKAGE_RELEASE requires verified ZipDepth and "
      "Depth Anything V2 Small ONNX files, or Python 3 so depthgen_build_model can regenerate them.")
  endif()
  set(DEPTHGEN_PACKAGE_MODEL_DEPENDENCY)
  if(TARGET depthgen_build_model)
    list(APPEND DEPTHGEN_PACKAGE_MODEL_DEPENDENCY depthgen_build_model)
  endif()
  set(DEPTHGEN_PACKAGE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/package/$<CONFIG>")
	set(DEPTHGEN_ORT_NOTICE_FILES)
	if(EXISTS "${DEPTHGEN_ORT_ROOT}/ThirdPartyNotices.txt")
	  list(APPEND DEPTHGEN_ORT_NOTICE_FILES "${DEPTHGEN_ORT_ROOT}/ThirdPartyNotices.txt")
	endif()
  if(WIN32)
    set(DEPTHGEN_PACKAGE_PLATFORM_DIR "${DEPTHGEN_PACKAGE_ROOT}/Win")
    set(DEPTHGEN_PACKAGE_MODEL_DIR "${DEPTHGEN_PACKAGE_PLATFORM_DIR}/Resources/Models")
  else()
    set(DEPTHGEN_PACKAGE_PLATFORM_DIR "${DEPTHGEN_PACKAGE_ROOT}/Mac")
    set(DEPTHGEN_PACKAGE_MODEL_DIR
      "${DEPTHGEN_PACKAGE_PLATFORM_DIR}/DepthGen.plugin/Contents/Resources/Models")
  endif()
  if(WIN32)
    add_custom_target(depthgen_package
      DEPENDS DepthGen ${DEPTHGEN_PACKAGE_MODEL_DEPENDENCY}
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEPTHGEN_PACKAGE_PLATFORM_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Models/model-manifest.json"
		${DEPTHGEN_ORT_NOTICE_FILES}
        "${DEPTHGEN_PACKAGE_ROOT}/"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/licenses"
        "${DEPTHGEN_PACKAGE_ROOT}/third_party/licenses"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/docs"
        "${DEPTHGEN_PACKAGE_ROOT}/docs"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:DepthGen>"
        "${DEPTHGEN_PACKAGE_PLATFORM_DIR}/DepthGen.aex"
      COMMAND "${CMAKE_COMMAND}"
        -DDEPTHGEN_RUNTIME_DESTINATION=${DEPTHGEN_PACKAGE_PLATFORM_DIR}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/clean_onnxruntime.cmake"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPTHGEN_ORT_RUNTIME_FILES}
        "${DEPTHGEN_PACKAGE_PLATFORM_DIR}/"
      COMMAND "${CMAKE_COMMAND}"
        -DDEPTHGEN_MODELS_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/Resources/Models
        -DDEPTHGEN_MODELS_DESTINATION=${DEPTHGEN_PACKAGE_MODEL_DIR}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_model_assets.cmake"
      COMMAND "${CMAKE_COMMAND}"
        -DDEPTHGEN_PACKAGE_ROOT=${DEPTHGEN_PACKAGE_ROOT}
        -DDEPTHGEN_PACKAGE_NAME=${CMAKE_CURRENT_BINARY_DIR}/package/DepthGen-$<CONFIG>-Windows.zip
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/create_zip.cmake"
      COMMENT "Staging and archiving DepthGen Windows release package")
  else()
    add_custom_target(depthgen_package
      DEPENDS DepthGen ${DEPTHGEN_PACKAGE_MODEL_DEPENDENCY}
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEPTHGEN_PACKAGE_PLATFORM_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Models/model-manifest.json"
		${DEPTHGEN_ORT_NOTICE_FILES}
        "${DEPTHGEN_PACKAGE_ROOT}/"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/licenses"
        "${DEPTHGEN_PACKAGE_ROOT}/third_party/licenses"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/docs"
        "${DEPTHGEN_PACKAGE_ROOT}/docs"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "$<TARGET_BUNDLE_DIR:DepthGen>"
        "${DEPTHGEN_PACKAGE_PLATFORM_DIR}/DepthGen.plugin"
      COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${DEPTHGEN_PACKAGE_PLATFORM_DIR}/DepthGen.plugin/Contents/Frameworks"
      COMMAND "${CMAKE_COMMAND}"
        -DDEPTHGEN_RUNTIME_DESTINATION=${DEPTHGEN_PACKAGE_PLATFORM_DIR}/DepthGen.plugin/Contents/Frameworks
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/clean_onnxruntime.cmake"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPTHGEN_ORT_RUNTIME_FILES}
        "${DEPTHGEN_PACKAGE_PLATFORM_DIR}/DepthGen.plugin/Contents/Frameworks"
      COMMAND "${CMAKE_COMMAND}"
        -DDEPTHGEN_MODELS_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/Resources/Models
        -DDEPTHGEN_MODELS_DESTINATION=${DEPTHGEN_PACKAGE_MODEL_DIR}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_model_assets.cmake"
      COMMAND "${CMAKE_COMMAND}"
        -DDEPTHGEN_BUNDLE=${DEPTHGEN_PACKAGE_PLATFORM_DIR}/DepthGen.plugin
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sign_macos_runtime.cmake"
      COMMAND codesign --force --sign - "${DEPTHGEN_PACKAGE_PLATFORM_DIR}/DepthGen.plugin"
      COMMAND "${CMAKE_COMMAND}"
        -DDEPTHGEN_BUNDLE=${DEPTHGEN_PACKAGE_PLATFORM_DIR}/DepthGen.plugin
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_macos_bundle.cmake"
      COMMAND "${CMAKE_COMMAND}"
        -DDEPTHGEN_PACKAGE_ROOT=${DEPTHGEN_PACKAGE_ROOT}
        -DDEPTHGEN_PACKAGE_NAME=${CMAKE_CURRENT_BINARY_DIR}/package/DepthGen-$<CONFIG>-macOS.zip
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/create_zip.cmake"
      COMMENT "Staging, signing, and archiving DepthGen macOS release package")
  endif()
endif()

if(DEPTHGEN_BUILD_TESTS)
  enable_testing()
  add_executable(depthgen_image_tests
    tests/DepthGenImageTests.cpp
    Source/DepthGen_Image.cpp
    Source/DepthGen_Temporal.cpp)
  target_include_directories(depthgen_image_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${AESDK_ROOT}/Headers"
    "${AESDK_ROOT}/Headers/SP"
    "${AESDK_ROOT}/Util")
  target_compile_definitions(depthgen_image_tests PRIVATE
    _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
    $<$<BOOL:${WIN32}>:MSWindows;_WINDOWS>)
  if(MSVC)
    target_compile_options(depthgen_image_tests PRIVATE /utf-8 /W4 /permissive-)
  endif()
  add_test(NAME depthgen_image_tests COMMAND depthgen_image_tests)
  add_executable(depthgen_pipeline_bench
    tests/DepthGenPipelineBench.cpp
    Source/DepthGen_Image.cpp)
  target_include_directories(depthgen_pipeline_bench PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${AESDK_ROOT}/Headers"
    "${AESDK_ROOT}/Headers/SP"
    "${AESDK_ROOT}/Util")
  target_compile_definitions(depthgen_pipeline_bench PRIVATE
    $<$<BOOL:${WIN32}>:MSWindows;_WINDOWS>
    _CRT_SECURE_NO_WARNINGS)
  if(MSVC)
    target_compile_options(depthgen_pipeline_bench PRIVATE /utf-8 /W4 /permissive-)
  endif()
  set(DEPTHGEN_TEST_MODEL_PATH "" CACHE FILEPATH "Optional verified ONNX model for smoke testing")
  if(DEPTHGEN_HAS_ORT AND DEPTHGEN_TEST_MODEL_PATH)
    add_executable(depthgen_model_smoke
      tests/DepthGenModelSmoke.cpp
      Source/DepthGen_Inference.cpp
      Source/DepthGen_ModelIntegrity.cpp)
    target_include_directories(depthgen_model_smoke PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/Source" "${DEPTHGEN_ORT_INCLUDE_DIR}")
    target_link_libraries(depthgen_model_smoke PRIVATE "${DEPTHGEN_ORT_LIBRARY}")
    target_compile_definitions(depthgen_model_smoke PRIVATE DEPTHGEN_HAS_ORT=1 _CRT_SECURE_NO_WARNINGS
      ${DEPTHGEN_ORT_PROVIDER_DEFINITIONS})
    if(MSVC)
      target_compile_options(depthgen_model_smoke PRIVATE /utf-8 /W4 /permissive-)
    endif()
    if(WIN32)
      add_custom_command(TARGET depthgen_model_smoke POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPTHGEN_ORT_RUNTIME_FILES}
          "$<TARGET_FILE_DIR:depthgen_model_smoke>")
      add_test(NAME depthgen_model_smoke
        COMMAND "$<TARGET_FILE:depthgen_model_smoke>")
    else()
      add_test(NAME depthgen_model_smoke
        COMMAND "${CMAKE_COMMAND}" -E env
          "DEPTHGEN_MODEL_PATH=${DEPTHGEN_TEST_MODEL_PATH}"
          "$<TARGET_FILE:depthgen_model_smoke>")
    endif()
    set_tests_properties(depthgen_model_smoke PROPERTIES
      ENVIRONMENT "DEPTHGEN_MODEL_PATH=${DEPTHGEN_TEST_MODEL_PATH}")
    add_executable(depthgen_embedded_model_smoke
      tests/DepthGenModelSmoke.cpp
      Source/DepthGen_EmbeddedModel.cpp
      Source/DepthGen_Inference.cpp
      Source/DepthGen_ModelIntegrity.cpp)
    target_include_directories(depthgen_embedded_model_smoke PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/Source" "${DEPTHGEN_ORT_INCLUDE_DIR}")
    target_link_libraries(depthgen_embedded_model_smoke PRIVATE "${DEPTHGEN_ORT_LIBRARY}")
    target_compile_definitions(depthgen_embedded_model_smoke PRIVATE
      DEPTHGEN_HAS_ORT=1 DEPTHGEN_EMBEDDED_MODEL=1 _CRT_SECURE_NO_WARNINGS
      ${DEPTHGEN_ORT_PROVIDER_DEFINITIONS})
    if(TARGET depthgen_build_model)
      add_dependencies(depthgen_embedded_model_smoke depthgen_build_model)
    endif()
    if(MSVC)
      target_compile_options(depthgen_embedded_model_smoke PRIVATE /utf-8 /W4 /permissive-)
    endif()
    if(WIN32)
      target_sources(depthgen_embedded_model_smoke PRIVATE "${DEPTHGEN_MODEL_RC}")
      add_custom_command(TARGET depthgen_embedded_model_smoke POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPTHGEN_ORT_RUNTIME_FILES}
          "$<TARGET_FILE_DIR:depthgen_embedded_model_smoke>")
    elseif(APPLE)
      target_link_options(depthgen_embedded_model_smoke PRIVATE
        "-Wl,-sectcreate,__DATA_CONST,__zipdepth,${DEPTHGEN_ZIPDEPTH_MODEL_PATH}"
        "-Wl,-sectcreate,__DATA_CONST,__dav2,${DEPTHGEN_DAV2_MODEL_PATH}")
      set_property(TARGET depthgen_embedded_model_smoke APPEND PROPERTY
        LINK_DEPENDS "${DEPTHGEN_ZIPDEPTH_MODEL_PATH};${DEPTHGEN_DAV2_MODEL_PATH}")
    endif()
    add_test(NAME depthgen_embedded_model_smoke
      COMMAND "$<TARGET_FILE:depthgen_embedded_model_smoke>")
    if(DEPTHGEN_ORT_PROVIDER_DEFINITIONS)
      add_executable(depthgen_provider_parity
        tests/DepthGenProviderParity.cpp
        Source/DepthGen_Inference.cpp
        Source/DepthGen_ModelIntegrity.cpp)
      target_include_directories(depthgen_provider_parity PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source" "${DEPTHGEN_ORT_INCLUDE_DIR}")
      target_link_libraries(depthgen_provider_parity PRIVATE "${DEPTHGEN_ORT_LIBRARY}")
      target_compile_definitions(depthgen_provider_parity PRIVATE DEPTHGEN_HAS_ORT=1 DEPTHGEN_TESTING=1 _CRT_SECURE_NO_WARNINGS
        ${DEPTHGEN_ORT_PROVIDER_DEFINITIONS})
      if(MSVC)
        target_compile_options(depthgen_provider_parity PRIVATE /utf-8 /W4 /permissive-)
      endif()
      if(WIN32)
        add_custom_command(TARGET depthgen_provider_parity POST_BUILD
          COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPTHGEN_ORT_RUNTIME_FILES}
            "$<TARGET_FILE_DIR:depthgen_provider_parity>")
      endif()
      add_test(NAME depthgen_provider_parity COMMAND "$<TARGET_FILE:depthgen_provider_parity>")
      set_tests_properties(depthgen_provider_parity PROPERTIES
        ENVIRONMENT "DEPTHGEN_MODEL_PATH=${DEPTHGEN_TEST_MODEL_PATH}")
      add_executable(depthgen_provider_fallback
        tests/DepthGenProviderFallback.cpp
        Source/DepthGen_Inference.cpp
        Source/DepthGen_ModelIntegrity.cpp)
      target_include_directories(depthgen_provider_fallback PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source" "${DEPTHGEN_ORT_INCLUDE_DIR}")
      target_link_libraries(depthgen_provider_fallback PRIVATE "${DEPTHGEN_ORT_LIBRARY}")
      target_compile_definitions(depthgen_provider_fallback PRIVATE DEPTHGEN_HAS_ORT=1 DEPTHGEN_TESTING=1 _CRT_SECURE_NO_WARNINGS
        ${DEPTHGEN_ORT_PROVIDER_DEFINITIONS})
      if(MSVC)
        target_compile_options(depthgen_provider_fallback PRIVATE /utf-8 /W4 /permissive-)
      endif()
      if(WIN32)
        add_custom_command(TARGET depthgen_provider_fallback POST_BUILD
          COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPTHGEN_ORT_RUNTIME_FILES}
            "$<TARGET_FILE_DIR:depthgen_provider_fallback>")
      endif()
      add_test(NAME depthgen_provider_initialisation_fallback COMMAND "$<TARGET_FILE:depthgen_provider_fallback>")
      add_test(NAME depthgen_provider_execution_fallback COMMAND "$<TARGET_FILE:depthgen_provider_fallback>" "--execution")
      set_tests_properties(depthgen_provider_initialisation_fallback depthgen_provider_execution_fallback PROPERTIES
        ENVIRONMENT "DEPTHGEN_MODEL_PATH=${DEPTHGEN_TEST_MODEL_PATH}")
    endif()
    add_executable(depthgen_benchmark
      tests/DepthGenBenchmark.cpp
      Source/DepthGen_Image.cpp
      Source/DepthGen_Inference.cpp
      Source/DepthGen_ModelIntegrity.cpp)
    target_include_directories(depthgen_benchmark PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/Source" "${DEPTHGEN_ORT_INCLUDE_DIR}")
    target_link_libraries(depthgen_benchmark PRIVATE "${DEPTHGEN_ORT_LIBRARY}")
    target_compile_definitions(depthgen_benchmark PRIVATE DEPTHGEN_HAS_ORT=1 _CRT_SECURE_NO_WARNINGS
      ${DEPTHGEN_ORT_PROVIDER_DEFINITIONS})
    if(MSVC)
      target_compile_options(depthgen_benchmark PRIVATE /utf-8 /W4 /permissive-)
    endif()
    if(WIN32)
      add_custom_command(TARGET depthgen_benchmark POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPTHGEN_ORT_RUNTIME_FILES}
          "$<TARGET_FILE_DIR:depthgen_benchmark>")
    endif()
  endif()
  if(DEPTHGEN_HAS_ORT)
    add_executable(depthgen_model_validation
      tests/DepthGenModelValidation.cpp
      Source/DepthGen_Inference.cpp
      Source/DepthGen_ModelIntegrity.cpp)
    target_include_directories(depthgen_model_validation PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/Source" "${DEPTHGEN_ORT_INCLUDE_DIR}")
    target_link_libraries(depthgen_model_validation PRIVATE "${DEPTHGEN_ORT_LIBRARY}")
    target_compile_definitions(depthgen_model_validation PRIVATE DEPTHGEN_HAS_ORT=1 _CRT_SECURE_NO_WARNINGS
      ${DEPTHGEN_ORT_PROVIDER_DEFINITIONS})
    if(MSVC)
      target_compile_options(depthgen_model_validation PRIVATE /utf-8 /W4 /permissive-)
    endif()
    if(WIN32)
      add_custom_command(TARGET depthgen_model_validation POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPTHGEN_ORT_RUNTIME_FILES}
          "$<TARGET_FILE_DIR:depthgen_model_validation>")
    endif()
    add_test(NAME depthgen_model_validation COMMAND "$<TARGET_FILE:depthgen_model_validation>")
  endif()
endif()
