/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRandomGenerator.h"

#include "ScopedNSSTypes.h"
#include "nsNSSComponent.h"
#include "pk11pub.h"
#include "prerror.h"
#include "secerr.h"
#include "mozilla/UniquePtrExtensions.h"

NS_IMPL_ISUPPORTS(nsRandomGenerator, nsIRandomGenerator)

NS_IMETHODIMP
nsRandomGenerator::GenerateRandomBytes(uint32_t aLength, uint8_t** aBuffer) {
  NS_ENSURE_ARG_POINTER(aBuffer);
  *aBuffer = nullptr;

  mozilla::UniqueFreePtr<uint8_t> buf(
      static_cast<uint8_t*>(moz_xmalloc(aLength)));
  nsresult rv = GenerateRandomBytesInto(buf.get(), aLength);
  NS_ENSURE_SUCCESS(rv, rv);

  *aBuffer = buf.release();
  return NS_OK;
}

NS_IMETHODIMP
nsRandomGenerator::GenerateRandomBytesInto(uint8_t* aBuffer, uint32_t aLength) {
  NS_ENSURE_ARG_POINTER(aBuffer);

  mozilla::UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return NS_ERROR_FAILURE;
  }

  SECStatus srv = PK11_GenerateRandomOnSlot(slot.get(), aBuffer, aLength);
  return srv == SECSuccess ? NS_OK : NS_ERROR_FAILURE;
}
