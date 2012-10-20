//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsILocalFile.h"
#include "nsCRT.h"
#include "nsIFile.h"
#include "nsISupportsImpl.h"
#include "nsCheckSummedOutputStream.h"

////////////////////////////////////////////////////////////////////////////////
// nsCheckSummedOutputStream

NS_IMPL_ISUPPORTS_INHERITED3(nsCheckSummedOutputStream,
                             nsSafeFileOutputStream,
                             nsISafeOutputStream,
                             nsIOutputStream,
                             nsIFileOutputStream)

NS_IMETHODIMP
nsCheckSummedOutputStream::Init(nsIFile* file, int32_t ioFlags, int32_t perm,
                                int32_t behaviorFlags)
{
  nsresult rv;
  mHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mHash->Init(nsICryptoHash::MD5);
  NS_ENSURE_SUCCESS(rv, rv);

  return nsSafeFileOutputStream::Init(file, ioFlags, perm, behaviorFlags);
}

NS_IMETHODIMP
nsCheckSummedOutputStream::Finish()
{
  nsresult rv = mHash->Finish(false, mCheckSum);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t written;
  rv = nsSafeFileOutputStream::Write(reinterpret_cast<const char*>(mCheckSum.BeginReading()),
                                     mCheckSum.Length(), &written);
  NS_ASSERTION(written == mCheckSum.Length(), "Error writing stream checksum");
  NS_ENSURE_SUCCESS(rv, rv);

  return nsSafeFileOutputStream::Finish();
}

NS_IMETHODIMP
nsCheckSummedOutputStream::Write(const char *buf, uint32_t count, uint32_t *result)
{
  nsresult rv = mHash->Update(reinterpret_cast<const uint8_t*>(buf), count);
  NS_ENSURE_SUCCESS(rv, rv);

  return nsSafeFileOutputStream::Write(buf, count, result);
}

////////////////////////////////////////////////////////////////////////////////
