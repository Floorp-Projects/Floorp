/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef TOOLKIT_XRE_DETECT_WIN32K_CONFLICTS_H
#define TOOLKIT_XRE_DETECT_WIN32K_CONFLICTS_H
#include <cinttypes>

// C interface for the `detect_win32k_conflicts` Rust crate

struct ConflictingMitigationStatus {
  bool caller_check;
  bool sim_exec;
  bool stack_pivot;
};

extern "C" bool detect_win32k_conflicting_mitigations(
    ConflictingMitigationStatus* aConflictingMitigationStatus);

#endif  // TOOLKIT_XRE_DETECT_WIN32K_CONFLICTS_H
