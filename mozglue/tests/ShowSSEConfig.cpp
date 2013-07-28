/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SSE.h"
#include <stdio.h>

int main()
{
  printf("CPUID detection present: %s\n",
#ifdef MOZILLA_SSE_HAVE_CPUID_DETECTION
         "yes"
#else
         "no"
#endif
        );

#ifdef MOZILLA_COMPILE_WITH_MMX
#define COMPILE_MMX_STRING "Y"
#else
#define COMPILE_MMX_STRING "-"
#endif
#ifdef MOZILLA_PRESUME_MMX
#define PRESUME_MMX_STRING "Y"
#else
#define PRESUME_MMX_STRING "-"
#endif

#ifdef MOZILLA_COMPILE_WITH_SSE
#define COMPILE_SSE_STRING "Y"
#else
#define COMPILE_SSE_STRING "-"
#endif
#ifdef MOZILLA_PRESUME_SSE
#define PRESUME_SSE_STRING "Y"
#else
#define PRESUME_SSE_STRING "-"
#endif

#ifdef MOZILLA_COMPILE_WITH_SSE2
#define COMPILE_SSE2_STRING "Y"
#else
#define COMPILE_SSE2_STRING "-"
#endif
#ifdef MOZILLA_PRESUME_SSE2
#define PRESUME_SSE2_STRING "Y"
#else
#define PRESUME_SSE2_STRING "-"
#endif

#ifdef MOZILLA_COMPILE_WITH_SSE3
#define COMPILE_SSE3_STRING "Y"
#else
#define COMPILE_SSE3_STRING "-"
#endif
#ifdef MOZILLA_PRESUME_SSE3
#define PRESUME_SSE3_STRING "Y"
#else
#define PRESUME_SSE3_STRING "-"
#endif

#ifdef MOZILLA_COMPILE_WITH_SSSE3
#define COMPILE_SSSE3_STRING "Y"
#else
#define COMPILE_SSSE3_STRING "-"
#endif
#ifdef MOZILLA_PRESUME_SSSE3
#define PRESUME_SSSE3_STRING "Y"
#else
#define PRESUME_SSSE3_STRING "-"
#endif

#ifdef MOZILLA_COMPILE_WITH_SSE4A
#define COMPILE_SSE4A_STRING "Y"
#else
#define COMPILE_SSE4A_STRING "-"
#endif
#ifdef MOZILLA_PRESUME_SSE4A
#define PRESUME_SSE4A_STRING "Y"
#else
#define PRESUME_SSE4A_STRING "-"
#endif

#ifdef MOZILLA_COMPILE_WITH_SSE4_1
#define COMPILE_SSE4_1_STRING "Y"
#else
#define COMPILE_SSE4_1_STRING "-"
#endif
#ifdef MOZILLA_PRESUME_SSE4_1
#define PRESUME_SSE4_1_STRING "Y"
#else
#define PRESUME_SSE4_1_STRING "-"
#endif

#ifdef MOZILLA_COMPILE_WITH_SSE4_2
#define COMPILE_SSE4_2_STRING "Y"
#else
#define COMPILE_SSE4_2_STRING "-"
#endif
#ifdef MOZILLA_PRESUME_SSE4_2
#define PRESUME_SSE4_2_STRING "Y"
#else
#define PRESUME_SSE4_2_STRING "-"
#endif

  printf("Feature Presume Compile Support  Use\n");
#define SHOW_INFO(featurelc_, featureuc_)                                     \
  printf(    "%7s    %1s       %1s       %1s\n",                              \
         #featurelc_,                                                         \
         PRESUME_##featureuc_##_STRING,                                       \
         COMPILE_##featureuc_##_STRING,                                       \
         (mozilla::supports_##featurelc_() ? "Y" : "-"));
  SHOW_INFO(mmx, MMX)
  SHOW_INFO(sse, SSE)
  SHOW_INFO(sse2, SSE2)
  SHOW_INFO(sse3, SSE3)
  SHOW_INFO(ssse3, SSSE3)
  SHOW_INFO(sse4a, SSE4A)
  SHOW_INFO(sse4_1, SSE4_1)
  SHOW_INFO(sse4_2, SSE4_2)
  return 0;
}
