/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicharBuffer_h__
#define nsUnicharBuffer_h__

#include "nsIUnicharBuffer.h"
#include "mozilla/Attributes.h"

class UnicharBufferImpl MOZ_FINAL : public nsIUnicharBuffer {
public:
  UnicharBufferImpl();

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  NS_DECL_ISUPPORTS
  NS_IMETHOD Init(uint32_t aBufferSize);
  NS_IMETHOD_(int32_t) GetLength() const;
  NS_IMETHOD_(int32_t) GetBufferSize() const;
  NS_IMETHOD_(PRUnichar*) GetBuffer() const;
  NS_IMETHOD_(bool) Grow(int32_t aNewSize);

  PRUnichar* mBuffer;
  uint32_t mSpace;
  uint32_t mLength;

private:
  ~UnicharBufferImpl();
};

#endif // nsUnicharBuffer_h__
