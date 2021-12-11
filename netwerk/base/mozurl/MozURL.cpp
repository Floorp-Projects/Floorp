/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StaticPrefs_security.h"

extern "C" {

bool Gecko_StrictFileOriginPolicy() {
  return mozilla::StaticPrefs::security_fileuri_strict_origin_policy();
}
}
