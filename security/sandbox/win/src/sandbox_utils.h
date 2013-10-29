// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SANDBOX_UTILS_H__
#define SANDBOX_SRC_SANDBOX_UTILS_H__

#include <windows.h>
#include <string>

#include "base/basictypes.h"
#include "sandbox/win/src/nt_internals.h"

namespace sandbox {

// Returns true if the current OS is Windows XP SP2 or later.
bool IsXPSP2OrLater();

void InitObjectAttribs(const std::wstring& name,
                       ULONG attributes,
                       HANDLE root,
                       OBJECT_ATTRIBUTES* obj_attr,
                       UNICODE_STRING* uni_name);

};  // namespace sandbox

#endif  // SANDBOX_SRC_SANDBOX_UTILS_H__
