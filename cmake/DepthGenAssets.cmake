# Deliberately explicit asset acquisition. The effect never accesses the
# network; these targets are for source builders and release assemblers only.
#
# ZipDepth:
#   depthgen_fetch_zipdepth  checks out and verifies the pinned MIT NPU
#                            checkpoint in the build tree.
#   depthgen_build_zipdepth  exports the portable dynamic ONNX.
# Depth Anything V2 Small:
#   depthgen_fetch_dav2      downloads the hash-pinned Apache-2.0 upstream
#                            FP32 export into the build tree.
#   depthgen_build_dav2      runs tools/build_accelerated_model.py.
# Umbrella targets depthgen_fetch_model / depthgen_build_model build both.
set(DEPTHGEN_ZIPDEPTH_MODEL_FILENAME "zipdepth_base_npu_dynamic.onnx")
set(DEPTHGEN_DAV2_MODEL_FILENAME "depth_anything_v2_vits_dml.onnx")
set(DEPTHGEN_MODEL_FILENAME "${DEPTHGEN_ZIPDEPTH_MODEL_FILENAME}")
set(DEPTHGEN_ZIPDEPTH_REPOSITORY "https://github.com/fabiotosi92/ZipDepth")
set(DEPTHGEN_ZIPDEPTH_COMMIT "94da7527f7030a0e79d54f33b113bdce4065d735")
set(DEPTHGEN_ZIPDEPTH_CHECKPOINT_SHA256
  "627c04fda584133ead4310074884a4a037061b4c01ba86e73e492ea30fab570d")
set(DEPTHGEN_ZIPDEPTH_OUTPUT_SHA256
  "0741a0d574609da33c5081b1054a2dd1e8845ecdbed9a5f69c48807c22400d59")
set(DEPTHGEN_DAV2_UPSTREAM_FILENAME "depth_anything_v2_vits_dynamic.onnx")
set(DEPTHGEN_DAV2_MODEL_URL
  "https://github.com/fabio-sim/Depth-Anything-ONNX/releases/download/v2.0.0/depth_anything_v2_vits_dynamic.onnx")
set(DEPTHGEN_DAV2_EXPORTER_REPOSITORY "https://github.com/fabio-sim/Depth-Anything-ONNX")
set(DEPTHGEN_DAV2_EXPORTER_COMMIT "40ed31643bea3f537201aeb7752d8a16b6d6d178")
set(DEPTHGEN_DAV2_UPSTREAM_SHA256
  "46c4e8eeda3a27f34701831b6a2ec7753d7b38779b215acb5633424703deed8f")
set(DEPTHGEN_DAV2_OUTPUT_SHA256
  "237cfaaf329bc97b9914c14e2d2497b1159cc05cca1b6d7a68aa42a262ea99bf")
set(DEPTHGEN_ASSET_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(depthgen_add_model_target destination)
  find_package(Git REQUIRED)
  set(zipdepth_source_root "${CMAKE_CURRENT_SOURCE_DIR}/build/depthgen_upstream/ZipDepth")
  set(zipdepth_checkpoint "${zipdepth_source_root}/checkpoints/zipdepth_base_npu.pth")
  set(zipdepth_fetch_stamp "${CMAKE_CURRENT_SOURCE_DIR}/build/depthgen_upstream/zipdepth.fetch.stamp")
  set(zipdepth_model_path "${destination}/${DEPTHGEN_ZIPDEPTH_MODEL_FILENAME}")
  set(dav2_upstream_path
    "${CMAKE_CURRENT_SOURCE_DIR}/build/depthgen_upstream/${DEPTHGEN_DAV2_UPSTREAM_FILENAME}")
  set(dav2_model_path "${destination}/${DEPTHGEN_DAV2_MODEL_FILENAME}")

  add_custom_command(OUTPUT "${zipdepth_fetch_stamp}"
    COMMAND "${CMAKE_COMMAND}"
      -DDEPTHGEN_GIT_EXECUTABLE=${GIT_EXECUTABLE}
      -DDEPTHGEN_ZIPDEPTH_REPOSITORY=${DEPTHGEN_ZIPDEPTH_REPOSITORY}
      -DDEPTHGEN_ZIPDEPTH_COMMIT=${DEPTHGEN_ZIPDEPTH_COMMIT}
      -DDEPTHGEN_ZIPDEPTH_SOURCE_ROOT=${zipdepth_source_root}
      -DDEPTHGEN_ZIPDEPTH_CHECKPOINT=${zipdepth_checkpoint}
      -DDEPTHGEN_ZIPDEPTH_CHECKPOINT_SHA256=${DEPTHGEN_ZIPDEPTH_CHECKPOINT_SHA256}
      -P "${DEPTHGEN_ASSET_CMAKE_DIR}/fetch_zipdepth.cmake"
    COMMAND "${CMAKE_COMMAND}" -E touch "${zipdepth_fetch_stamp}"
    DEPENDS "${DEPTHGEN_ASSET_CMAKE_DIR}/fetch_zipdepth.cmake"
    BYPRODUCTS "${zipdepth_checkpoint}"
    COMMENT "Checking out and SHA-256-verifying ZipDepth NPU")
  add_custom_target(depthgen_fetch_zipdepth DEPENDS "${zipdepth_fetch_stamp}")

  add_custom_target(depthgen_fetch_dav2
    COMMAND "${CMAKE_COMMAND}"
      -DDEPTHGEN_DOWNLOAD_URL=${DEPTHGEN_DAV2_MODEL_URL}
      -DDEPTHGEN_DOWNLOAD_DESTINATION=${dav2_upstream_path}
      -DDEPTHGEN_DOWNLOAD_SHA256=${DEPTHGEN_DAV2_UPSTREAM_SHA256}
      -P "${DEPTHGEN_ASSET_CMAKE_DIR}/download_verified.cmake"
    BYPRODUCTS "${dav2_upstream_path}"
    COMMENT "Downloading and SHA-256-verifying Depth Anything V2 Small")
  add_custom_target(depthgen_fetch_model)
  add_dependencies(depthgen_fetch_model depthgen_fetch_zipdepth depthgen_fetch_dav2)

  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_custom_command(OUTPUT "${zipdepth_model_path}"
      COMMAND "${CMAKE_COMMAND}"
        -DDEPTHGEN_MODEL_OUTPUT=${zipdepth_model_path}
        -DDEPTHGEN_EXPECTED_SHA256=${DEPTHGEN_ZIPDEPTH_OUTPUT_SHA256}
        -DDEPTHGEN_PYTHON_EXECUTABLE=${Python3_EXECUTABLE}
        -DDEPTHGEN_EXPORT_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/tools/build_zipdepth_model.py
        -DDEPTHGEN_SOURCE_ROOT=${zipdepth_source_root}
        -DDEPTHGEN_CHECKPOINT=${zipdepth_checkpoint}
        -P "${DEPTHGEN_ASSET_CMAKE_DIR}/ensure_zipdepth_model.cmake"
      DEPENDS "${zipdepth_fetch_stamp}"
        "${DEPTHGEN_ASSET_CMAKE_DIR}/ensure_zipdepth_model.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_zipdepth_model.py"
      COMMENT "Exporting the portable ZipDepth ONNX model")
    add_custom_target(depthgen_build_zipdepth DEPENDS "${zipdepth_model_path}")
    add_custom_command(OUTPUT "${dav2_model_path}"
      COMMAND "${CMAKE_COMMAND}"
        -DDEPTHGEN_MODEL_OUTPUT=${dav2_model_path}
        -DDEPTHGEN_EXPECTED_SHA256=${DEPTHGEN_DAV2_OUTPUT_SHA256}
        -DDEPTHGEN_PYTHON_EXECUTABLE=${Python3_EXECUTABLE}
        -DDEPTHGEN_EXPORT_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/tools/build_accelerated_model.py
        -DDEPTHGEN_INPUT_MODEL=${dav2_upstream_path}
        -P "${DEPTHGEN_ASSET_CMAKE_DIR}/ensure_dav2_model.cmake"
      DEPENDS depthgen_fetch_dav2
        "${DEPTHGEN_ASSET_CMAKE_DIR}/ensure_dav2_model.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_accelerated_model.py"
      COMMENT "Building the accelerated Depth Anything V2 Small ONNX")
    add_custom_target(depthgen_build_dav2 DEPENDS "${dav2_model_path}")
    add_custom_target(depthgen_build_model)
    add_dependencies(depthgen_build_model depthgen_build_zipdepth depthgen_build_dav2)
  else()
    message(WARNING "Python 3 not found; depthgen_build_model is unavailable. "
      "Install Python 3, onnx, and PyTorch to regenerate the model assets.")
  endif()
endfunction()
