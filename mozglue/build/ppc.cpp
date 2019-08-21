/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* compile-time and runtime tests for whether to use Power ISA-specific
 * extensions */

#include "ppc.h"
#include "mozilla/Unused.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(XP_LINUX)
// Use the getauxval() function if available.
// ARCH_3_00 wasn't defined until glibc 2.23, so include just in case.
#  include <sys/auxv.h>
#  ifndef PPC_FEATURE2_ARCH_3_00
#    define PPC_FEATURE2_ARCH_3_00 0x00800000
#  endif
#endif

const unsigned PPC_FLAG_VMX = 1;
const unsigned PPC_FLAG_VSX = 2;
const unsigned PPC_FLAG_VSX3 = 4;

static signed get_ppc_cpu_flags(void) {
  // This could be expensive, so cache the result.
  static signed cpu_flags = -1;

  if (cpu_flags > -1) {  // already checked
    return cpu_flags;
  }
  cpu_flags = 0;

#if defined(XP_LINUX)
  // Try getauxval().
  unsigned long int cap = getauxval(AT_HWCAP);
  unsigned long int cap2 = getauxval(AT_HWCAP2);

  if (cap & PPC_FEATURE_HAS_ALTIVEC) {
    cpu_flags |= PPC_FLAG_VMX;
  }
  if (cap & PPC_FEATURE_HAS_VSX) {
    cpu_flags |= PPC_FLAG_VSX;
  }
  if (cap2 & PPC_FEATURE2_ARCH_3_00) {
    cpu_flags |= PPC_FLAG_VSX3;
  }
#else
  // Non-Linux detection here. Currently, on systems other than Linux,
  // no CPU SIMD features will be detected.
#endif

  return cpu_flags;
}

namespace mozilla {
namespace ppc_private {
bool vmx_enabled = !!(get_ppc_cpu_flags() & PPC_FLAG_VMX);
bool vsx_enabled = !!(get_ppc_cpu_flags() & PPC_FLAG_VSX);
bool vsx3_enabled = !!(get_ppc_cpu_flags() & PPC_FLAG_VSX3);
}  // namespace ppc_private
}  // namespace mozilla
