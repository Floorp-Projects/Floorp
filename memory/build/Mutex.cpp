/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Mutex.h"

#if defined(XP_DARWIN)

// static
bool Mutex::SpinInKernelSpace() {
  if (__builtin_available(macOS 10.15, *)) {
    return true;
  }

  return false;
}

// static
bool Mutex::gSpinInKernelSpace = SpinInKernelSpace();

#endif  // defined(XP_DARWIN)
