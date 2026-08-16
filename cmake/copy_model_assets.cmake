# Copies the tracked model manifest into a plug-in or package layout. Verified
# ONNX files are linked into the executable and must not remain as sidecars.
if(NOT DEFINED DEPTHGEN_MODELS_SOURCE OR NOT DEFINED DEPTHGEN_MODELS_DESTINATION)
  message(FATAL_ERROR "copy_model_assets requires source and destination.")
endif()

file(MAKE_DIRECTORY "${DEPTHGEN_MODELS_DESTINATION}")
file(REMOVE
  "${DEPTHGEN_MODELS_DESTINATION}/depth_anything_v2_vits_dml.onnx"
  "${DEPTHGEN_MODELS_DESTINATION}/depth_anything_v2_vits_dynamic.onnx"
  "${DEPTHGEN_MODELS_DESTINATION}/zipdepth_base_npu_dynamic.onnx")
if(NOT EXISTS "${DEPTHGEN_MODELS_SOURCE}/model-manifest.json")
  message(FATAL_ERROR "model-manifest.json is missing from ${DEPTHGEN_MODELS_SOURCE}")
endif()
file(COPY "${DEPTHGEN_MODELS_SOURCE}/model-manifest.json"
  DESTINATION "${DEPTHGEN_MODELS_DESTINATION}")
