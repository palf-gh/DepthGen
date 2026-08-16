if(NOT DEFINED DEPTHGEN_GIT_EXECUTABLE OR
   NOT DEFINED DEPTHGEN_ZIPDEPTH_REPOSITORY OR
   NOT DEFINED DEPTHGEN_ZIPDEPTH_COMMIT OR
   NOT DEFINED DEPTHGEN_ZIPDEPTH_SOURCE_ROOT OR
   NOT DEFINED DEPTHGEN_ZIPDEPTH_CHECKPOINT OR
   NOT DEFINED DEPTHGEN_ZIPDEPTH_CHECKPOINT_SHA256)
  message(FATAL_ERROR "ZipDepth fetch requires Git, repository, commit, source root, checkpoint, and hash.")
endif()

if(NOT EXISTS "${DEPTHGEN_ZIPDEPTH_SOURCE_ROOT}/.git")
  get_filename_component(parent "${DEPTHGEN_ZIPDEPTH_SOURCE_ROOT}" DIRECTORY)
  file(MAKE_DIRECTORY "${parent}")
  # Build systems may pre-create a byproduct's parent directory. Remove that
  # incomplete generated tree before cloning; a valid checkout is never
  # removed because it has already passed the .git guard above.
  file(REMOVE_RECURSE "${DEPTHGEN_ZIPDEPTH_SOURCE_ROOT}")
  execute_process(
    COMMAND "${DEPTHGEN_GIT_EXECUTABLE}" clone --filter=blob:none --no-checkout
      "${DEPTHGEN_ZIPDEPTH_REPOSITORY}" "${DEPTHGEN_ZIPDEPTH_SOURCE_ROOT}"
    RESULT_VARIABLE clone_result)
  if(NOT clone_result EQUAL 0)
    message(FATAL_ERROR "Failed to clone the pinned ZipDepth source.")
  endif()
endif()

execute_process(
  COMMAND "${DEPTHGEN_GIT_EXECUTABLE}" -C "${DEPTHGEN_ZIPDEPTH_SOURCE_ROOT}"
    fetch --depth 1 origin "${DEPTHGEN_ZIPDEPTH_COMMIT}"
  RESULT_VARIABLE fetch_result)
if(NOT fetch_result EQUAL 0)
  message(FATAL_ERROR "Failed to fetch pinned ZipDepth commit ${DEPTHGEN_ZIPDEPTH_COMMIT}.")
endif()
execute_process(
  COMMAND "${DEPTHGEN_GIT_EXECUTABLE}" -C "${DEPTHGEN_ZIPDEPTH_SOURCE_ROOT}"
    checkout --detach "${DEPTHGEN_ZIPDEPTH_COMMIT}"
  RESULT_VARIABLE checkout_result)
if(NOT checkout_result EQUAL 0)
  message(FATAL_ERROR "Failed to check out pinned ZipDepth commit.")
endif()
execute_process(
  COMMAND "${DEPTHGEN_GIT_EXECUTABLE}" -C "${DEPTHGEN_ZIPDEPTH_SOURCE_ROOT}" rev-parse HEAD
  OUTPUT_VARIABLE actual_commit
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE rev_parse_result)
if(NOT rev_parse_result EQUAL 0 OR NOT actual_commit STREQUAL DEPTHGEN_ZIPDEPTH_COMMIT)
  message(FATAL_ERROR "ZipDepth checkout does not match the pinned commit.")
endif()

if(NOT EXISTS "${DEPTHGEN_ZIPDEPTH_CHECKPOINT}")
  message(FATAL_ERROR "Pinned ZipDepth checkout does not contain the NPU checkpoint.")
endif()
file(SHA256 "${DEPTHGEN_ZIPDEPTH_CHECKPOINT}" checkpoint_sha256)
if(NOT checkpoint_sha256 STREQUAL DEPTHGEN_ZIPDEPTH_CHECKPOINT_SHA256)
  message(FATAL_ERROR "ZipDepth NPU checkpoint SHA-256 mismatch: ${checkpoint_sha256}")
endif()
message(STATUS "ZipDepth source and NPU checkpoint are pinned and verified.")
