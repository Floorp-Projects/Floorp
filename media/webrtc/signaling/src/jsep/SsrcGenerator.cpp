/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/jsep/SsrcGenerator.h"
#include "pk11pub.h"

namespace mozilla {

bool
SsrcGenerator::GenerateSsrc(uint32_t* ssrc)
{
  do {
    SECStatus rv = PK11_GenerateRandom(
        reinterpret_cast<unsigned char*>(ssrc), sizeof(uint32_t));
    if (rv != SECSuccess) {
      return false;
    }
  } while (mSsrcs.count(*ssrc));
  mSsrcs.insert(*ssrc);

  return true;
}
} // namespace mozilla
