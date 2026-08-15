# Deliberately explicit asset acquisition. The effect never accesses the
# network; these targets are for source builders and release assemblers only.
#
# Two-step flow:
#   depthgen_fetch_model  downloads the hash-pinned upstream FP32 export into
#                         the build tree (never the source tree).
#   depthgen_build_model  runs tools/build_accelerated_model.py to produce the
#                         shipped DirectML-ready repackage in Resources/Models.
set(DEPTHGEN_UPSTREAM_MODEL_FILENAME "depth_anything_v2_vits_dynamic.onnx")
set(DEPTHGEN_MODEL_FILENAME "depth_anything_v2_vits_dml.onnx")
set(DEPTHGEN_MODEL_URL
  "https://github.com/fabio-sim/Depth-Anything-ONNX/releases/download/v2.0.0/depth_anything_v2_vits_dynamic.onnx")
# The v2.0.0 tag resolves to this immutable Apache-2.0 exporter revision.
# Keep it beside the asset hash so a source rebuild can reproduce the model
# provenance without placing model weights or Python dependencies in this repo.
set(DEPTHGEN_ONNX_EXPORTER_REPOSITORY "https://github.com/fabio-sim/Depth-Anything-ONNX")
set(DEPTHGEN_ONNX_EXPORTER_COMMIT "40ed31643bea3f537201aeb7752d8a16b6d6d178")
# Upstream FP32 export hash, verified 2026-08-15. The shipped repackage hash
# lives in tools/build_accelerated_model.py and Source/DepthGen_ModelIntegrity.h.
# Do not change without updating THIRD_PARTY_NOTICES.md and
# docs/MODEL_PROVENANCE.md.
set(DEPTHGEN_UPSTREAM_MODEL_SHA256 "46c4e8eeda3a27f34701831b6a2ec7753d7b38779b215acb5633424703deed8f")
set(DEPTHGEN_ASSET_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(depthgen_add_model_target destination)
  # One shared upstream download serves every configured build tree.
  set(upstream_path "${CMAKE_CURRENT_SOURCE_DIR}/build/depthgen_upstream/${DEPTHGEN_UPSTREAM_MODEL_FILENAME}")
  add_custom_target(depthgen_fetch_model
    COMMAND "${CMAKE_COMMAND}"
      -DDEPTHGEN_DOWNLOAD_URL=${DEPTHGEN_MODEL_URL}
      -DDEPTHGEN_DOWNLOAD_DESTINATION=${upstream_path}
      -DDEPTHGEN_DOWNLOAD_SHA256=${DEPTHGEN_UPSTREAM_MODEL_SHA256}
      -P "${DEPTHGEN_ASSET_CMAKE_DIR}/download_verified.cmake"
    BYPRODUCTS "${upstream_path}"
    COMMENT "Downloading and SHA-256-verifying the upstream Depth Anything V2 Small ONNX model")
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_custom_target(depthgen_build_model
      COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_accelerated_model.py"
        --input "${upstream_path}"
        --output "${destination}/${DEPTHGEN_MODEL_FILENAME}"
        --expect-output-sha256 237cfaaf329bc97b9914c14e2d2497b1159cc05cca1b6d7a68aa42a262ea99bf
      COMMENT "Building the accelerated DepthGen model repackage")
    add_dependencies(depthgen_build_model depthgen_fetch_model)
  else()
    message(WARNING "Python 3 not found; depthgen_build_model is unavailable. "
      "Install Python 3 and the onnx package to regenerate the model asset.")
  endif()
endfunction()
