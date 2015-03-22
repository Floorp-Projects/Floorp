/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBinaryStream_h___
#define nsBinaryStream_h___

#include "nsCOMPtr.h"
#include "nsAString.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIStreamBufferAccess.h"

#define NS_BINARYOUTPUTSTREAM_CID        \
{ /* 86c37b9a-74e7-4672-844e-6e7dd83ba484 */         \
     0x86c37b9a,                                     \
     0x74e7,                                         \
     0x4672,                                         \
    {0x84, 0x4e, 0x6e, 0x7d, 0xd8, 0x3b, 0xa4, 0x84} \
}

#define NS_BINARYOUTPUTSTREAM_CONTRACTID "@mozilla.org/binaryoutputstream;1"

// Derive from nsIObjectOutputStream so this class can be used as a superclass
// by nsObjectOutputStream.
class nsBinaryOutputStream final : public nsIObjectOutputStream
{
public:
  nsBinaryOutputStream()
  {
  }

protected:
  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsIOutputStream methods
  NS_DECL_NSIOUTPUTSTREAM

  // nsIBinaryOutputStream methods
  NS_DECL_NSIBINARYOUTPUTSTREAM

  // nsIObjectOutputStream methods
  NS_DECL_NSIOBJECTOUTPUTSTREAM

  // Call Write(), ensuring that all proffered data is written
  nsresult WriteFully(const char* aBuf, uint32_t aCount);

  nsCOMPtr<nsIOutputStream>       mOutputStream;
  nsCOMPtr<nsIStreamBufferAccess> mBufferAccess;

private:
  // virtual dtor since subclasses call our Release()
  virtual ~nsBinaryOutputStream()
  {
  }
};

#define NS_BINARYINPUTSTREAM_CID        \
{ /* c521a612-2aad-46db-b6ab-3b821fb150b1 */       \
   0xc521a612,                                     \
   0x2aad,                                         \
   0x46db,                                         \
  {0xb6, 0xab, 0x3b, 0x82, 0x1f, 0xb1, 0x50, 0xb1} \
}

#define NS_BINARYINPUTSTREAM_CONTRACTID "@mozilla.org/binaryinputstream;1"

class nsBinaryInputStream final : public nsIObjectInputStream
{
public:
  nsBinaryInputStream()
  {
  }

protected:
  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsIInputStream methods
  NS_DECL_NSIINPUTSTREAM

  // nsIBinaryInputStream methods
  NS_DECL_NSIBINARYINPUTSTREAM

  // nsIObjectInputStream methods
  NS_DECL_NSIOBJECTINPUTSTREAM

  nsCOMPtr<nsIInputStream>        mInputStream;
  nsCOMPtr<nsIStreamBufferAccess> mBufferAccess;

private:
  // virtual dtor since subclasses call our Release()
  virtual ~nsBinaryInputStream()
  {
  }
};

#endif // nsBinaryStream_h___
