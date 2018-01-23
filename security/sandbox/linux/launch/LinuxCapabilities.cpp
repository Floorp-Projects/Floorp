/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LinuxCapabilities.h"

#include <unistd.h>
#include <sys/syscall.h>

namespace mozilla {

bool
LinuxCapabilities::GetCurrent() {
  __user_cap_header_struct header = { _LINUX_CAPABILITY_VERSION_3, 0 };
  return syscall(__NR_capget, &header, &mBits) == 0
    && header.version == _LINUX_CAPABILITY_VERSION_3;
}

bool
LinuxCapabilities::SetCurrentRaw() const {
  __user_cap_header_struct header = { _LINUX_CAPABILITY_VERSION_3, 0 };
  return syscall(__NR_capset, &header, &mBits) == 0
    && header.version == _LINUX_CAPABILITY_VERSION_3;
}

} // namespace mozilla
