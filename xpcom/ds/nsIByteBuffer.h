/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIByteBuffer_h___
#define nsIByteBuffer_h___

#include "nscore.h"
#include "nsISupports.h"

class nsIInputStream;

#define NS_IBYTE_BUFFER_IID    \
{ 0xe4a6e4b0, 0x93b4, 0x11d1, \
  {0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }
#define NS_IBYTEBUFFER_IID    \
{ 0xe4a6e4b0, 0x93b4, 0x11d1, \
  {0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }
#define NS_BYTEBUFFER_CONTRACTID "@mozilla.org/byte-buffer;1"

/** Interface to a buffer that holds bytes */
class nsIByteBuffer : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IBYTEBUFFER_IID)

  NS_IMETHOD Init(uint32_t aBufferSize) = 0;

  /** @return length of buffer, i.e. how many bytes are currently in it. */
  NS_IMETHOD_(uint32_t) GetLength(void) const = 0;

  /** @return number of bytes allocated in the buffer */
  NS_IMETHOD_(uint32_t) GetBufferSize(void) const = 0;

  /** @return the buffer */
  NS_IMETHOD_(char*) GetBuffer(void) const = 0;

  /** Grow buffer to aNewSize bytes. */
  NS_IMETHOD_(bool) Grow(uint32_t aNewSize) = 0;

  /** Fill the buffer with data from aStream.  Don't grow the buffer, only
   *  read until length of buffer equals buffer size. */
  NS_IMETHOD_(int32_t) Fill(nsresult* aErrorCode, nsIInputStream* aStream,
                            uint32_t aKeep) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIByteBuffer, NS_IBYTEBUFFER_IID)

#define NS_BYTEBUFFER_CID                            \
{ /* a49d5280-0d6b-11d3-9331-00104ba0fd40 */         \
    0xa49d5280,                                      \
    0x0d6b,                                          \
    0x11d3,                                          \
    {0x93, 0x31, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

/** Create a new byte buffer using the given buffer size. */
extern nsresult 
NS_NewByteBuffer(nsIByteBuffer** aInstancePtrResult,
                 nsISupports* aOuter,
                 uint32_t aBufferSize = 0);

#endif /* nsIByteBuffer_h___ */

