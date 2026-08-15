# Deliberately explicit asset acquisition. The effect never accesses the
# network; this target is for source builders and release assemblers only.
set(DEPTHGEN_MODEL_FILENAME "depth_anything_v2_vits_dynamic.onnx")
set(DEPTHGEN_MODEL_URL
  "https://github.com/fabio-sim/Depth-Anything-ONNX/releases/download/v2.0.0/depth_anything_v2_vits_dynamic.onnx")
# The v2.0.0 tag resolves to this immutable Apache-2.0 exporter revision.
# Keep it beside the asset hash so a source rebuild can reproduce the model
# provenance without placing model weights or Python dependencies in this repo.
set(DEPTHGEN_ONNX_EXPORTER_REPOSITORY "https://github.com/fabio-sim/Depth-Anything-ONNX")
set(DEPTHGEN_ONNX_EXPORTER_COMMIT "40ed31643bea3f537201aeb7752d8a16b6d6d178")
# Verified against the upstream release asset on 2026-08-15. Do not change
# without updating THIRD_PARTY_NOTICES.md and docs/MODEL_PROVENANCE.md.
set(DEPTHGEN_MODEL_SHA256 "46c4e8eeda3a27f34701831b6a2ec7753d7b38779b215acb5633424703deed8f")
set(DEPTHGEN_ASSET_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(depthgen_add_model_target destination)
  add_custom_target(depthgen_fetch_model
    COMMAND "${CMAKE_COMMAND}"
      -DDEPTHGEN_DOWNLOAD_URL=${DEPTHGEN_MODEL_URL}
      -DDEPTHGEN_DOWNLOAD_DESTINATION=${destination}/${DEPTHGEN_MODEL_FILENAME}
      -DDEPTHGEN_DOWNLOAD_SHA256=${DEPTHGEN_MODEL_SHA256}
      -P "${DEPTHGEN_ASSET_CMAKE_DIR}/download_verified.cmake"
    BYPRODUCTS "${destination}/${DEPTHGEN_MODEL_FILENAME}"
    COMMENT "Downloading and SHA-256-verifying Depth Anything V2 Small ONNX model")
endfunction()
