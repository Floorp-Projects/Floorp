/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* compile-time and runtime tests for whether to use Power ISA-specific
   extensions */

#ifndef mozilla_ppc_h_
#define mozilla_ppc_h_

// for definition of MFBT_DATA
#include "mozilla/Types.h"

namespace mozilla {
namespace ppc_private {
extern bool MFBT_DATA vmx_enabled;
extern bool MFBT_DATA vsx_enabled;
extern bool MFBT_DATA vsx3_enabled;
}  // namespace ppc_private

inline bool supports_vmx() {
#ifdef __powerpc__
  return ppc_private::vmx_enabled;
#else
  return false;
#endif
}

inline bool supports_vsx() {
#ifdef __powerpc__
  return ppc_private::vsx_enabled;
#else
  return false;
#endif
}

inline bool supports_vsx3() {
#ifdef __powerpc__
  return ppc_private::vsx3_enabled;
#else
  return false;
#endif
}

}  // namespace mozilla

#endif /* !defined(mozilla_ppc_h_) */
