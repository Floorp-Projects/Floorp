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

# Parses test/test-data.sha1 and writes captured file names and checksums to
# $out_files and $out_checksums as lists.
function (make_test_data_lists out_files out_checksums)
  if (NOT AOM_TEST_DATA_LIST OR NOT EXISTS "${AOM_TEST_DATA_LIST}")
    message(FATAL_ERROR "AOM_TEST_DATA_LIST (${AOM_TEST_DATA_LIST}) missing or "
            "variable empty.")
  endif ()

  # Read test-data.sha1 into $files_and_checksums. $files_and_checksums becomes
  # a list with an entry for each line from $AOM_TEST_DATA_LIST.
  file(STRINGS "${AOM_TEST_DATA_LIST}" files_and_checksums)

  # Iterate over the list of lines and split it into $checksums and $filenames.
  foreach (line ${files_and_checksums})
    string(FIND "${line}" " *" delim_pos)

    math(EXPR filename_pos "${delim_pos} + 2")
    string(SUBSTRING "${line}" 0 ${delim_pos} checksum)
    string(SUBSTRING "${line}" ${filename_pos} -1 filename)

    set(checksums ${checksums} ${checksum})
    set(filenames ${filenames} ${filename})
  endforeach ()

  if (NOT checksums OR NOT filenames)
    message(FATAL_ERROR "Parsing of ${AOM_TEST_DATA_LIST} failed.")
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
  endif ()
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
