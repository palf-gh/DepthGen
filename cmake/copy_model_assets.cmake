# Copies the tracked model manifest and the built accelerated model into a
# plug-in or package layout. Stray files in Resources/Models (for example a
# manually downloaded upstream export) are deliberately not copied.
if(NOT DEFINED DEPTHGEN_MODELS_SOURCE OR NOT DEFINED DEPTHGEN_MODELS_DESTINATION OR
   NOT DEFINED DEPTHGEN_MODEL_FILENAME)
  message(FATAL_ERROR "copy_model_assets requires source, destination, and filename.")
endif()

file(MAKE_DIRECTORY "${DEPTHGEN_MODELS_DESTINATION}")
if(NOT EXISTS "${DEPTHGEN_MODELS_SOURCE}/model-manifest.json")
  message(FATAL_ERROR "model-manifest.json is missing from ${DEPTHGEN_MODELS_SOURCE}")
endif()
file(COPY "${DEPTHGEN_MODELS_SOURCE}/model-manifest.json"
  DESTINATION "${DEPTHGEN_MODELS_DESTINATION}")

if(EXISTS "${DEPTHGEN_MODELS_SOURCE}/${DEPTHGEN_MODEL_FILENAME}")
  file(COPY "${DEPTHGEN_MODELS_SOURCE}/${DEPTHGEN_MODEL_FILENAME}"
    DESTINATION "${DEPTHGEN_MODELS_DESTINATION}")
elseif(DEPTHGEN_MODEL_REQUIRED)
  message(FATAL_ERROR
    "${DEPTHGEN_MODEL_FILENAME} is missing; build the depthgen_build_model target first.")
else()
  message(STATUS
    "DepthGen: ${DEPTHGEN_MODEL_FILENAME} not built yet; skipping model copy.")
endif()
