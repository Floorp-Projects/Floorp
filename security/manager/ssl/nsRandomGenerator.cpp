/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRandomGenerator.h"

#include "ScopedNSSTypes.h"
#include "nsNSSComponent.h"
#include "pk11pub.h"
#include "prerror.h"
#include "secerr.h"

NS_IMPL_ISUPPORTS(nsRandomGenerator, nsIRandomGenerator)

NS_IMETHODIMP
nsRandomGenerator::GenerateRandomBytes(uint32_t aLength,
                                       uint8_t** aBuffer)
{
  NS_ENSURE_ARG_POINTER(aBuffer);
  *aBuffer = nullptr;

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mozilla::UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return NS_ERROR_FAILURE;
  }

  auto buf = static_cast<uint8_t*>(moz_xmalloc(aLength));
  if (!buf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SECStatus srv = PK11_GenerateRandomOnSlot(slot.get(), buf, aLength);
  if (srv != SECSuccess) {
    free(buf);
    return NS_ERROR_FAILURE;
  }

  *aBuffer = buf;

  return NS_OK;
}

nsRandomGenerator::~nsRandomGenerator()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  shutdown(ShutdownCalledFrom::Object);
}
