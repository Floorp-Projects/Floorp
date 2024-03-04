/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "common/linux/linux_libc_support.h"
#include "linux_utils.h"

bool ElfFileSoVersion(const char* mapping_name, uint32_t* version_components) {
  if (!version_components) {
    return false;
  }

  // We found no version so just report 0
  const char* so_version = my_strstr(mapping_name, ".so.");
  if (so_version == nullptr) {
    return true;
  }

  char tmp[12];  // 11 for maximum representation of UINT32T_MAX + \0 ?
  size_t current_position = 0;
  size_t next_tmp = 0;
  tmp[0] = '\0';
  for (size_t so_version_pos = 0; so_version_pos <= my_strlen(so_version);
       ++so_version_pos) {
    // We can't have more than four: MAJOR.minor.release.patch
    if (current_position == 4) {
      break;
    }

    char c = so_version[so_version_pos];
    if (c != '.') {
      if ((c <= '9' && c >= '0')) {
        tmp[next_tmp] = c;
        tmp[next_tmp + 1] = '\0';
        ++next_tmp;
      }

      if (so_version[so_version_pos + 1] != '\0') {
        continue;
      }
    }

    if (my_strlen(tmp) > 0) {
      int t;
      if (!my_strtoui(&t, tmp)) {
        return false;
      }
      uint32_t casted_tmp = (uint32_t)t;
      version_components[current_position] = casted_tmp;
      ++current_position;
    }

    tmp[0] = '\0';
    next_tmp = 0;
  }

  return true;
}
