// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sid.h"

#include "base/logging.h"

namespace sandbox {

Sid::Sid(const SID *sid) {
  ::CopySid(SECURITY_MAX_SID_SIZE, sid_, const_cast<SID*>(sid));
};

Sid::Sid(WELL_KNOWN_SID_TYPE type) {
  DWORD size_sid = SECURITY_MAX_SID_SIZE;
  BOOL result = ::CreateWellKnownSid(type, NULL, sid_, &size_sid);
  DCHECK(result);
  DBG_UNREFERENCED_LOCAL_VARIABLE(result);
}

const SID *Sid::GetPSID() const {
  return reinterpret_cast<SID*>(const_cast<BYTE*>(sid_));
}

}  // namespace sandbox
