// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SANDBOX_UTILS_H_
#define SANDBOX_SRC_SANDBOX_UTILS_H_

#include <windows.h>
#include <string>

#include "base/strings/string16.h"
#include "sandbox/win/src/nt_internals.h"

namespace sandbox {

void InitObjectAttribs(const base::string16& name,
                       ULONG attributes,
                       HANDLE root,
                       OBJECT_ATTRIBUTES* obj_attr,
                       UNICODE_STRING* uni_name,
                       SECURITY_QUALITY_OF_SERVICE* security_qos);

}  // namespace sandbox

#endif  // SANDBOX_SRC_SANDBOX_UTILS_H_
