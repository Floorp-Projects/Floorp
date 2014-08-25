/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LinuxUtils.h"

#if defined(XP_LINUX)

#include <ctype.h>
#include <stdio.h>

#include "nsPrintfCString.h"

namespace mozilla {

void
LinuxUtils::GetThreadName(pid_t aTid, nsACString& aName)
{
  aName.Truncate();
  if (aTid <= 0) {
    return;
  }

  const size_t kBuffSize = 16; // 15 chars max + '\n'
  char buf[kBuffSize];
  nsPrintfCString path("/proc/%d/comm", aTid);
  FILE* fp = fopen(path.get(), "r");
  if (!fp) {
    // The fopen could also fail if the thread exited before we got here.
    return;
  }

  size_t len = fread(buf, 1, kBuffSize, fp);
  fclose(fp);

  // No need to strip the '\n', since isspace() includes it.
  while (len > 0 &&
         (isspace(buf[len - 1]) || isdigit(buf[len - 1]) ||
          buf[len - 1] == '#' || buf[len - 1] == '_')) {
    --len;
  }

  aName.Assign(buf, len);
}

}

#endif // XP_LINUX
