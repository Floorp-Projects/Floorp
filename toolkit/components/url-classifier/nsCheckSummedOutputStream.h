//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCheckSummedOutputStream_h__
#define nsCheckSummedOutputStream_h__

#include "nsILocalFile.h"
#include "nsIFile.h"
#include "nsIOutputStream.h"
#include "nsICryptoHash.h"
#include "nsNetCID.h"
#include "nsString.h"
#include "../../../netwerk/base/src/nsFileStreams.h"
#include "nsToolkitCompsCID.h"

class nsCheckSummedOutputStream : public nsSafeFileOutputStream
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // Size of MD5 hash in bytes
  static const uint32_t CHECKSUM_SIZE = 16;

  nsCheckSummedOutputStream() {}

  NS_IMETHOD Finish();
  NS_IMETHOD Write(const char *buf, uint32_t count, uint32_t *result);
  NS_IMETHOD Init(nsIFile* file, int32_t ioFlags, int32_t perm, int32_t behaviorFlags);

protected:
  virtual ~nsCheckSummedOutputStream() { nsSafeFileOutputStream::Close(); }

  nsCOMPtr<nsICryptoHash> mHash;
  nsAutoCString mCheckSum;
};

// returns a file output stream which can be QI'ed to nsIFileOutputStream.
inline nsresult
NS_NewCheckSummedOutputStream(nsIOutputStream **result,
                              nsIFile         *file,
                              int32_t         ioFlags       = -1,
                              int32_t         perm          = -1,
                              int32_t         behaviorFlags =  0)
{
    nsCOMPtr<nsIFileOutputStream> out = new nsCheckSummedOutputStream();
    nsresult rv = out->Init(file, ioFlags, perm, behaviorFlags);
    if (NS_SUCCEEDED(rv))
      NS_ADDREF(*result = out);  // cannot use nsCOMPtr::swap
    return rv;
}

#endif
