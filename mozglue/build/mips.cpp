/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* compile-time and runtime tests for whether to use MIPS-specific extensions */

#include "mips.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  MIPS_FLAG_LOONGSON3 = 1,
};

static unsigned get_mips_cpu_flags(void) {
  unsigned flags = 0;
  FILE* fin;

  fin = fopen("/proc/cpuinfo", "r");
  if (fin != nullptr) {
    char buf[1024];
    memset(buf, 0, sizeof(buf));
    fread(buf, sizeof(char), sizeof(buf) - 1, fin);
    fclose(fin);
    if (strstr(buf, "Loongson-3")) flags |= MIPS_FLAG_LOONGSON3;
  }
  return flags;
}

static bool check_loongson3(void) {
  // Cache a local copy so we only have to read /proc/cpuinfo once.
  static unsigned mips_cpu_flags = get_mips_cpu_flags();
  return (mips_cpu_flags & MIPS_FLAG_LOONGSON3) != 0;
}

namespace mozilla {
namespace mips_private {
bool isLoongson3 = check_loongson3();
}  // namespace mips_private
}  // namespace mozilla
