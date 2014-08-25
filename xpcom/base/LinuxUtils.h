/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LinuxUtils_h
#define mozilla_LinuxUtils_h

#if defined(XP_LINUX)

#include <unistd.h>
#include "nsString.h"

namespace mozilla {

class LinuxUtils
{
public:
  // Obtain the name of a thread, omitting any numeric suffix added by a
  // thread pool library (as in, e.g., "Binder_2" or "mozStorage #1").
  // The empty string is returned on error.
  //
  // Note: if this is ever needed on kernels older than 2.6.33 (early 2010),
  // it will have to parse /proc/<pid>/status instead, because
  // /proc/<pid>/comm didn't exist before then.
  static void GetThreadName(pid_t aTid, nsACString& aName);
};

}

#endif // XP_LINUX

#endif
