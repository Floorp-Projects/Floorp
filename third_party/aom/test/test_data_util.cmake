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

set(AOM_TEST_DATA_FILE_NAMES
    "hantro_collage_w352h288.yuv"
    "hantro_odd.yuv"
    "park_joy_90p_10_420.y4m"
    "park_joy_90p_10_422.y4m"
    "park_joy_90p_10_444.y4m"
    "park_joy_90p_10_440.yuv"
    "park_joy_90p_12_420.y4m"
    "park_joy_90p_12_422.y4m"
    "park_joy_90p_12_444.y4m"
    "park_joy_90p_12_440.yuv"
    "park_joy_90p_8_420_a10-1.y4m"
    "park_joy_90p_8_420.y4m"
    "park_joy_90p_8_422.y4m"
    "park_joy_90p_8_444.y4m"
    "park_joy_90p_8_440.yuv"
    "desktop_credits.y4m"
    "niklas_1280_720_30.y4m"
    "rush_hour_444.y4m"
    "screendata.y4m"
    "niklas_640_480_30.yuv")

if (CONFIG_DECODE_PERF_TESTS AND CONFIG_AV1_ENCODER)
  set(AOM_TEST_DATA_FILE_NAMES
      ${AOM_TEST_DATA_FILE_NAMES}
      "niklas_1280_720_30.yuv")
endif ()

if (CONFIG_ENCODE_PERF_TESTS AND CONFIG_AV1_ENCODER)
  set(AOM_TEST_DATA_FILE_NAMES
      ${AOM_TEST_DATA_FILE_NAMES}
      "desktop_640_360_30.yuv"
      "kirland_640_480_30.yuv"
      "macmarcomoving_640_480_30.yuv"
      "macmarcostationary_640_480_30.yuv"
      "niklas_1280_720_30.yuv"
      "tacomanarrows_640_480_30.yuv"
      "tacomasmallcameramovement_640_480_30.yuv"
      "thaloundeskmtg_640_480_30.yuv")
endif ()

# Parses test/test-data.sha1 and writes captured file names and checksums to
# $out_files and $out_checksums as lists.
function (make_test_data_lists test_data_file out_files out_checksums)
  if (NOT test_data_file OR NOT EXISTS "${test_data_file}")
    message(FATAL_ERROR "Test info file missing or empty (${test_data_file})")
  endif ()

  # Read $test_data_file into $files_and_checksums. $files_and_checksums becomes
  # a list with an entry for each line from $test_data_file.
  file(STRINGS "${test_data_file}" files_and_checksums)

  # Iterate over the list of lines and split it into $checksums and $filenames.
  foreach (line ${files_and_checksums})
    string(FIND "${line}" " *" delim_pos)

    math(EXPR filename_pos "${delim_pos} + 2")
    string(SUBSTRING "${line}" 0 ${delim_pos} checksum)
    string(SUBSTRING "${line}" ${filename_pos} -1 filename)

    list(FIND AOM_TEST_DATA_FILE_NAMES ${filename} list_index)
    if (NOT ${list_index} EQUAL -1)
      # Include the name and checksum in output only when the file is needed.
      set(checksums ${checksums} ${checksum})
      set(filenames ${filenames} ${filename})
    endif ()
  endforeach ()

  list(LENGTH filenames num_files)
  list(LENGTH checksums num_checksums)
  if (NOT checksums OR NOT filenames OR NOT num_files EQUAL num_checksums)
    message(FATAL_ERROR "Parsing of ${test_data_file} failed.")
  endif ()

  set(${out_checksums} ${checksums} PARENT_SCOPE)
  set(${out_files} ${filenames} PARENT_SCOPE)
endfunction ()

# Appends each file name in $test_files to $test_dir and adds the result path to
# $out_path_list.
function (expand_test_file_paths test_files test_dir out_path_list)
  foreach (filename ${${test_files}})
    set(path_list ${path_list} "${test_dir}/${filename}")
  endforeach ()
  set(${out_path_list} ${path_list} PARENT_SCOPE)
endfunction ()

function (check_file local_path expected_checksum out_needs_update)
  if (EXISTS "${local_path}")
    file(SHA1 "${local_path}" file_checksum)
  else ()
    set(${out_needs_update} 1 PARENT_SCOPE)
    return ()
  endif ()

  if ("${file_checksum}" STREQUAL "${expected_checksum}")
    unset(${out_needs_update} PARENT_SCOPE)
  else ()
    set(${out_needs_update} 1 PARENT_SCOPE)
    return ()
  endif ()
  message("${local_path} up to date.")
endfunction ()

# Downloads data from $file_url, confirms that $file_checksum matches, and
# writes it to $local_path.
function (download_test_file file_url file_checksum local_path)
  message("Downloading ${file_url} ...")
  file(DOWNLOAD "${file_url}" "${local_path}"
       SHOW_PROGRESS
       EXPECTED_HASH SHA1=${file_checksum})
  message("Download of ${file_url} complete.")
endfunction ()
