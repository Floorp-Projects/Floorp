/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is SSE.cpp
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

/* compile-time and runtime tests for whether to use SSE instructions */

#include "SSE.h"

namespace mozilla {

namespace sse_private {

#if defined(MOZILLA_SSE_HAVE_CPUID_DETECTION)

#if !defined(MOZILLA_PRESUME_MMX)
  bool mmx_enabled
    = sse_private::has_cpuid_bit(1u, sse_private::edx, (1u<<23));
#endif

#if !defined(MOZILLA_PRESUME_SSE)
  bool sse_enabled
    = sse_private::has_cpuid_bit(1u, sse_private::edx, (1u<<25));
#endif

#if !defined(MOZILLA_PRESUME_SSE2)
  bool sse2_enabled
    = sse_private::has_cpuid_bit(1u, sse_private::edx, (1u<<26));
#endif

#if !defined(MOZILLA_PRESUME_SSE3)
  bool sse3_enabled
    = sse_private::has_cpuid_bit(1u, sse_private::ecx, (1u<<0));
#endif

#if !defined(MOZILLA_PRESUME_SSSE3)
  bool ssse3_enabled
    = sse_private::has_cpuid_bit(1u, sse_private::ecx, (1u<<9));
#endif

#if !defined(MOZILLA_PRESUME_SSE4A)
  bool sse4a_enabled
    = sse_private::has_cpuid_bit(0x80000001u, sse_private::ecx, (1u<<6));
#endif

#if !defined(MOZILLA_PRESUME_SSE4_1)
  bool sse4_1_enabled
    = sse_private::has_cpuid_bit(1u, sse_private::ecx, (1u<<19));
#endif

#if !defined(MOZILLA_PRESUME_SSE4_2)
  bool sse4_2_enabled
    = sse_private::has_cpuid_bit(1u, sse_private::ecx, (1u<<20));
#endif

#endif

}

}

