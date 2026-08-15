if(NOT DEFINED DEPTHGEN_DOWNLOAD_URL OR NOT DEFINED DEPTHGEN_DOWNLOAD_DESTINATION OR
   NOT DEFINED DEPTHGEN_DOWNLOAD_SHA256)
  message(FATAL_ERROR "Verified download requires URL, destination, and SHA-256.")
endif()
get_filename_component(destination_directory "${DEPTHGEN_DOWNLOAD_DESTINATION}" DIRECTORY)
file(MAKE_DIRECTORY "${destination_directory}")
file(DOWNLOAD "${DEPTHGEN_DOWNLOAD_URL}" "${DEPTHGEN_DOWNLOAD_DESTINATION}"
  EXPECTED_HASH "SHA256=${DEPTHGEN_DOWNLOAD_SHA256}"
  STATUS status
  SHOW_PROGRESS)
list(GET status 0 status_code)
if(NOT status_code EQUAL 0)
  file(REMOVE "${DEPTHGEN_DOWNLOAD_DESTINATION}")
  list(GET status 1 status_message)
  message(FATAL_ERROR "Verified DepthGen model download failed: ${status_message}")
endif()
