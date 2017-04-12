##
## Copyright (c) 2017, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
if (NOT AOM_ROOT OR NOT AOM_CONFIG_DIR)
  message(FATAL_ERROR "AOM_ROOT AND AOM_CONFIG_DIR must be defined.")
endif ()

set(AOM_TEST_DATA_LIST "${AOM_ROOT}/test/test-data.sha1")
set(AOM_TEST_DATA_URL "http://downloads.webmproject.org/test_data/libvpx")
set(AOM_TEST_DATA_PATH "$ENV{LIBAOM_TEST_DATA_PATH}")

include("${AOM_ROOT}/test/test_data_util.cmake")

if (${AOM_TEST_DATA_PATH} STREQUAL "")
  message(WARNING "Writing test data to ${AOM_CONFIG_DIR}, set "
          "$LIBAOM_TEST_DATA_PATH in your environment to avoid this warning.")
  set(AOM_TEST_DATA_PATH "${AOM_CONFIG_DIR}")
endif ()

if (NOT EXISTS "${AOM_TEST_DATA_PATH}")
  file(MAKE_DIRECTORY "${AOM_TEST_DATA_PATH}")
endif ()

make_test_data_lists("AOM_TEST_DATA_FILES" "AOM_TEST_DATA_CHECKSUMS")
expand_test_file_paths("AOM_TEST_DATA_FILES" "${AOM_TEST_DATA_PATH}"
                       "AOM_TEST_DATA_FILE_PATHS")
expand_test_file_paths("AOM_TEST_DATA_FILES" "${AOM_TEST_DATA_URL}"
                       "AOM_TEST_DATA_URLS")
list(LENGTH AOM_TEST_DATA_FILES num_files)
math(EXPR num_files "${num_files} - 1")

foreach (file_num RANGE ${num_files})
  list(GET AOM_TEST_DATA_FILES ${file_num} filename)
  list(GET AOM_TEST_DATA_CHECKSUMS ${file_num} checksum)
  list(GET AOM_TEST_DATA_FILE_PATHS ${file_num} filepath)
  list(GET AOM_TEST_DATA_URLS ${file_num} url)

  check_file("${filepath}" "${checksum}" "needs_download")
  if (needs_download)
    download_test_file("${url}" "${checksum}" "${filepath}")
  endif ()
endforeach ()
