/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is ShowSSEConfig.cpp.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
