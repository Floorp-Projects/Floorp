/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRandomGenerator.h"
#include "pk11pub.h"

////////////////////////////////////////////////////////////////////////////////
//// nsRandomGenerator

NS_IMPL_THREADSAFE_ISUPPORTS1(nsRandomGenerator, nsIRandomGenerator)

////////////////////////////////////////////////////////////////////////////////
//// nsIRandomGenerator

/* void generateRandomBytes(in unsigned long aLength,
                            [retval, array, size_is(aLength)] out octet aBuffer) */
NS_IMETHODIMP
nsRandomGenerator::GenerateRandomBytes(PRUint32 aLength,
                                       PRUint8 **aBuffer)
{
  NS_ENSURE_ARG_POINTER(aBuffer);

  PRUint8 *buf = reinterpret_cast<PRUint8 *>(NS_Alloc(aLength));
  if (!buf)
    return NS_ERROR_OUT_OF_MEMORY;

  SECStatus srv = PK11_GenerateRandom(buf, aLength);
  if (SECSuccess != srv) {
    NS_Free(buf);
    return NS_ERROR_FAILURE;
  }

  *aBuffer = buf;

  return NS_OK;
}
