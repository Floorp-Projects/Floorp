/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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
#define NS_BYTEBUFFER_CLASSNAME "Byte Buffer"

/** Interface to a buffer that holds bytes */
class nsIByteBuffer : public nsISupports {
public:
	static const nsIID& GetIID() { static nsIID iid = NS_IBYTEBUFFER_IID; return iid; }

  NS_IMETHOD Init(PRUint32 aBufferSize) = 0;

  /** @return length of buffer, i.e. how many bytes are currently in it. */
  NS_IMETHOD_(PRUint32) GetLength(void) const = 0;

  /** @return number of bytes allocated in the buffer */
  NS_IMETHOD_(PRUint32) GetBufferSize(void) const = 0;

  /** @return the buffer */
  NS_IMETHOD_(char*) GetBuffer(void) const = 0;

  /** Grow buffer to aNewSize bytes. */
  NS_IMETHOD_(PRBool) Grow(PRUint32 aNewSize) = 0;

  /** Fill the buffer with data from aStream.  Don't grow the buffer, only
   *  read until length of buffer equals buffer size. */
  NS_IMETHOD_(PRInt32) Fill(nsresult* aErrorCode, nsIInputStream* aStream,
                            PRUint32 aKeep) = 0;
};

#define NS_BYTEBUFFER_CID                            \
{ /* a49d5280-0d6b-11d3-9331-00104ba0fd40 */         \
    0xa49d5280,                                      \
    0x0d6b,                                          \
    0x11d3,                                          \
    {0x93, 0x31, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

/** Create a new byte buffer using the given buffer size. */
extern NS_COM nsresult 
NS_NewByteBuffer(nsIByteBuffer** aInstancePtrResult,
                 nsISupports* aOuter,
                 PRUint32 aBufferSize = 0);

#endif /* nsIByteBuffer_h___ */

