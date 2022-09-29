/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is temporarily needed for bug 1766432. We plan to remove it
// afterwards. Do not add new definitions here.

#ifndef mozglue_misc_MozProcessMitigationDynamicCodePolicy_h__
#define mozglue_misc_MozProcessMitigationDynamicCodePolicy_h__

#include <winnt.h>

// See bug 1766432 comment 4. We currently need to use our own definition
// for PROCESS_MITIGATION_DYNAMIC_CODE_POLICY in MinGW builds.
#if defined(__MINGW32__) || defined(__MINGW64__)

typedef struct _MOZ_PROCESS_MITIGATION_DYNAMIC_CODE_POLICY {
  __C89_NAMELESS union {
    DWORD Flags;
    __C89_NAMELESS struct {
      DWORD ProhibitDynamicCode : 1;
      DWORD AllowThreadOptOut : 1;
      DWORD AllowRemoteDowngrade : 1;
      DWORD ReservedFlags : 29;
    };
  };
} MOZ_PROCESS_MITIGATION_DYNAMIC_CODE_POLICY;

#else

using MOZ_PROCESS_MITIGATION_DYNAMIC_CODE_POLICY =
    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY;

#endif  // defined(__MINGW32__) || defined(__MINGW64__)

#endif  // mozglue_misc_MozProcessMitigationDynamicCodePolicy_h__
