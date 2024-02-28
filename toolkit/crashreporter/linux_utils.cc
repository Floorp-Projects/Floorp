/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "linux_utils.h"

bool ElfFileSoVersion(const char* mapping_name,
                      VersionComponents* version_components) {
  if (!version_components) {
    return false;
  }

  std::string path = std::string(mapping_name);
  std::string filename = path.substr(path.find_last_of('/') + 1);

  std::string dot_so_dot(".so.");
  // We found no version so just report 0
  size_t has_dot_so_dot = filename.find(dot_so_dot);
  if (has_dot_so_dot == std::string::npos) {
    return true;
  }

  std::string so_version =
      filename.substr(has_dot_so_dot + dot_so_dot.length());
  std::string tmp;
  for (std::string::iterator it = so_version.begin(); it != so_version.end();
       ++it) {
    // We can't have more than four: MAJOR.minor.release.patch
    if (version_components->size() == 4) {
      break;
    }

    char c = *it;
    if (c != '.') {
      if (isdigit(c)) {
        tmp += c;
      }

      if (std::next(it) != so_version.end()) {
        continue;
      }
    }

    if (tmp.length() > 0) {
      int t = std::stoi(tmp);  // assume tmp is < UINT32T_MAX
      if (t < 0) {
        return false;
      }

      uint32_t casted_tmp = static_cast<uint32_t>(t);
      // We have lost some information we should warn.
      if ((unsigned int)t != casted_tmp) {
        return false;
      }
      version_components->push_back(casted_tmp);
    }
    tmp = "";
  }

  return true;
}
