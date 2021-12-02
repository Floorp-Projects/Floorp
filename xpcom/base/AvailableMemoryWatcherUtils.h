/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AvailableMemoryWatcherUtils_h
#define mozilla_AvailableMemoryWatcherUtils_h

#include "mozilla/Attributes.h"
#include "nsISupportsUtils.h"  // For nsresult

namespace mozilla {

struct MemoryInfo {
  unsigned long memTotal;
  unsigned long memAvailable;
};
// Check /proc/meminfo for low memory. Largely C method for reading
// /proc/meminfo.
MOZ_MAYBE_UNUSED
static nsresult ReadMemoryFile(const char* meminfoPath, MemoryInfo& aResult) {
  FILE* fd;
  if ((fd = fopen(meminfoPath, "r")) == nullptr) {
    // Meminfo somehow unreachable
    return NS_ERROR_FAILURE;
  }

  char buff[128];

  /* The first few lines of meminfo look something like this:
   * MemTotal:       65663448 kB
   * MemFree:        57368112 kB
   * MemAvailable:   61852700 kB
   * We mostly care about the available versus the total. We calculate our
   * memory thresholds using this, and when memory drops below 5% we consider
   * this to be a memory pressure event. In practice these lines aren't
   * necessarily in order, but we can simply search for MemTotal
   * and MemAvailable.
   */
  char namebuffer[20];
  while ((fgets(buff, sizeof(buff), fd)) != nullptr) {
    if (strstr(buff, "MemTotal:")) {
      sscanf(buff, "%s %lu ", namebuffer, &aResult.memTotal);
    }
    if (strstr(buff, "MemAvailable:")) {
      sscanf(buff, "%s %lu ", namebuffer, &aResult.memAvailable);
    }
  }
  fclose(fd);
  return NS_OK;
}

}  // namespace mozilla

#endif  // ifndef mozilla_AvailableMemoryWatcherUtils_h
