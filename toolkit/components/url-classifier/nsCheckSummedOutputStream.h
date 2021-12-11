//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCheckSummedOutputStream_h__
#define nsCheckSummedOutputStream_h__

#include "nsIFile.h"
#include "nsIOutputStream.h"
#include "nsICryptoHash.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "../../../netwerk/base/nsBufferedStreams.h"
#include "prio.h"

class nsCheckSummedOutputStream : public nsBufferedOutputStream {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  // Size of MD5 hash in bytes
  static const uint32_t CHECKSUM_SIZE = 16;
  static const uint32_t MAX_BUFFER_SIZE = 64 * 1024;

  nsCheckSummedOutputStream() = default;

  NS_IMETHOD Finish() override;
  NS_IMETHOD Write(const char* buf, uint32_t count, uint32_t* result) override;
  NS_IMETHOD Init(nsIOutputStream* stream, uint32_t bufferSize) override;

 protected:
  virtual ~nsCheckSummedOutputStream() { nsBufferedOutputStream::Close(); }

  nsCOMPtr<nsICryptoHash> mHash;
  nsCString mCheckSum;
};

// returns a file output stream which can be QI'ed to nsIFileOutputStream.
inline nsresult NS_NewCheckSummedOutputStream(nsIOutputStream** result,
                                              nsIFile* file) {
  nsCOMPtr<nsIOutputStream> localOutFile;
  nsresult rv =
      NS_NewSafeLocalFileOutputStream(getter_AddRefs(localOutFile), file,
                                      PR_WRONLY | PR_TRUNCATE | PR_CREATE_FILE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIBufferedOutputStream> out = new nsCheckSummedOutputStream();
  rv = out->Init(localOutFile, nsCheckSummedOutputStream::MAX_BUFFER_SIZE);
  if (NS_SUCCEEDED(rv)) {
    out.forget(result);
  }
  return rv;
}

class nsCrc32CheckSumedOutputStream : public nsBufferedOutputStream {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  static const uint32_t CHECKSUM_SIZE = 4;

  nsCrc32CheckSumedOutputStream() = default;

  NS_IMETHOD Finish() override;
  NS_IMETHOD Write(const char* buf, uint32_t count, uint32_t* result) override;
  NS_IMETHOD Init(nsIOutputStream* stream, uint32_t bufferSize) override;

 protected:
  virtual ~nsCrc32CheckSumedOutputStream() { nsBufferedOutputStream::Close(); }

  uint32_t mCheckSum;
};

inline nsresult NS_NewCrc32OutputStream(
    nsIOutputStream** aResult, already_AddRefed<nsIOutputStream> aOutput,
    uint32_t aBufferSize) {
  nsCOMPtr<nsIOutputStream> out = std::move(aOutput);

  nsCOMPtr<nsIBufferedOutputStream> bufferOutput =
      new nsCrc32CheckSumedOutputStream();
  nsresult rv = bufferOutput->Init(out, aBufferSize);
  if (NS_SUCCEEDED(rv)) {
    bufferOutput.forget(aResult);
  }
  return rv;
}

#endif
