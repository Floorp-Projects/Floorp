/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* compile-time and runtime tests for whether to use MIPS-specific extensions */

#ifndef mozilla_mips_h_
#define mozilla_mips_h_

// for definition of MFBT_DATA
#include "mozilla/Types.h"

namespace mozilla {

  namespace mips_private {
    extern bool MFBT_DATA isLoongson3;
  } // namespace mips_private

  inline bool supports_mmi() {
#ifdef __mips__
    return mips_private::isLoongson3;
#else
    return false;
#endif
  }

} // namespace mozilla

#endif /* !defined(mozilla_mips_h_) */
