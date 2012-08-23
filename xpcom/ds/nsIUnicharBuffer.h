/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIUnicharBuffer_h___
#define nsIUnicharBuffer_h___

#include "nscore.h"
#include "nsISupports.h"

#define NS_IUNICHARBUFFER_IID \
{ 0x14cf6970, 0x93b5, 0x11d1, \
  {0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

/// Interface to a buffer that holds unicode characters
class nsIUnicharBuffer : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IUNICHARBUFFER_IID)

  NS_IMETHOD Init(uint32_t aBufferSize) = 0;
  NS_IMETHOD_(int32_t) GetLength() const = 0;
  NS_IMETHOD_(int32_t) GetBufferSize() const = 0;
  NS_IMETHOD_(PRUnichar*) GetBuffer() const = 0;
  NS_IMETHOD_(bool) Grow(int32_t aNewSize) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIUnicharBuffer, NS_IUNICHARBUFFER_IID)

/// Factory method for nsIUnicharBuffer.
extern nsresult
NS_NewUnicharBuffer(nsIUnicharBuffer** aInstancePtrResult,
                    nsISupports* aOuter,
                    uint32_t aBufferSize = 0);

#define NS_UNICHARBUFFER_CID                         \
{ /* c81fd8f0-0d6b-11d3-9331-00104ba0fd40 */         \
    0xc81fd8f0,                                      \
    0x0d6b,                                          \
    0x11d3,                                          \
    {0x93, 0x31, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

#endif /* nsIUnicharBuffer_h___ */
