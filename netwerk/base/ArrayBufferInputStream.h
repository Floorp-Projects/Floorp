/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ArrayBufferInputStream_h
#define ArrayBufferInputStream_h

#include "nsIArrayBufferInputStream.h"
#include "js/Value.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "nsISupportsImpl.h"

#define NS_ARRAYBUFFERINPUTSTREAM_CONTRACTID \
  "@mozilla.org/io/arraybuffer-input-stream;1"
#define NS_ARRAYBUFFERINPUTSTREAM_CID                \
  { /* 3014dde6-aa1c-41db-87d0-48764a3710f6 */       \
    0x3014dde6, 0xaa1c, 0x41db, {                    \
      0x87, 0xd0, 0x48, 0x76, 0x4a, 0x37, 0x10, 0xf6 \
    }                                                \
  }

class ArrayBufferInputStream : public nsIArrayBufferInputStream {
 public:
  ArrayBufferInputStream() = default;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIARRAYBUFFERINPUTSTREAM
  NS_DECL_NSIINPUTSTREAM

 private:
  virtual ~ArrayBufferInputStream() = default;
  mozilla::UniquePtr<uint8_t[]> mArrayBuffer;
  uint32_t mBufferLength{0};
  uint32_t mPos{0};
  bool mClosed{false};
};

#endif  // ArrayBufferInputStream_h
