/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxUtil.h"
#include "SandboxLogging.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "mozilla/Assertions.h"

namespace mozilla {

bool
IsSingleThreaded()
{
  // This detects the thread count indirectly.  /proc/<pid>/task has a
  // subdirectory for each thread in <pid>'s thread group, and the
  // link count on the "task" directory follows Unix expectations: the
  // link from its parent, the "." link from itself, and the ".." link
  // from each subdirectory; thus, 2+N links for N threads.
  struct stat sb;
  if (stat("/proc/self/task", &sb) < 0) {
    MOZ_DIAGNOSTIC_ASSERT(false, "Couldn't access /proc/self/task!");
    return false;
  }
  MOZ_DIAGNOSTIC_ASSERT(sb.st_nlink >= 3);
  return sb.st_nlink == 3;
}

} // namespace mozilla
