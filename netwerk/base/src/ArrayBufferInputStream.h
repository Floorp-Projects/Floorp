/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ArrayBufferInputStream_h
#define ArrayBufferInputStream_h

#include "nsIArrayBufferInputStream.h"
#include "js/Value.h"

#define NS_ARRAYBUFFERINPUTSTREAM_CONTRACTID "@mozilla.org/io/arraybuffer-input-stream;1"
#define NS_ARRAYBUFFERINPUTSTREAM_CID                \
{ /* 3014dde6-aa1c-41db-87d0-48764a3710f6 */       \
    0x3014dde6,                                      \
    0xaa1c,                                          \
    0x41db,                                          \
    {0x87, 0xd0, 0x48, 0x76, 0x4a, 0x37, 0x10, 0xf6} \
}

class ArrayBufferInputStream : public nsIArrayBufferInputStream {
public:
  ArrayBufferInputStream();
  virtual ~ArrayBufferInputStream();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIARRAYBUFFERINPUTSTREAM
  NS_DECL_NSIINPUTSTREAM

private:
  JSRuntime* mRt;
  jsval mArrayBuffer;
  uint8_t* mBuffer; // start of actual buffer
  uint32_t mBufferLength; // length of slice
  uint32_t mOffset; // permanent offset from start of actual buffer
  uint32_t mPos; // offset from start of slice
  bool mClosed;
};

#endif // ArrayBufferInputStream_h
