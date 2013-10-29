// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SID_H_
#define SANDBOX_SRC_SID_H_

#include <windows.h>

namespace sandbox {

// This class is used to hold and generate SIDS.
class Sid {
 public:
  // Constructors initializing the object with the SID passed.
  // This is a converting constructor. It is not explicit.
  Sid(const SID *sid);
  Sid(WELL_KNOWN_SID_TYPE type);

  // Returns sid_.
  const SID *GetPSID() const;

 private:
  BYTE sid_[SECURITY_MAX_SID_SIZE];
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_SID_H_
