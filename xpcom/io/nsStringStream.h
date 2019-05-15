/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStringStream_h__
#define nsStringStream_h__

#include "nsIStringStream.h"
#include "nsString.h"
#include "nsMemory.h"
#include "nsTArray.h"

/**
 * Implements:
 *   nsIStringInputStream
 *   nsIInputStream
 *   nsISeekableStream
 *   nsITellableStream
 *   nsISupportsCString
 */
#define NS_STRINGINPUTSTREAM_CONTRACTID "@mozilla.org/io/string-input-stream;1"
#define NS_STRINGINPUTSTREAM_CID                     \
  { /* 0abb0835-5000-4790-af28-61b3ba17c295 */       \
    0x0abb0835, 0x5000, 0x4790, {                    \
      0xaf, 0x28, 0x61, 0xb3, 0xba, 0x17, 0xc2, 0x95 \
    }                                                \
  }

/**
 * Factory method to get an nsInputStream from a byte buffer.  Result will
 * implement nsIStringInputStream, nsITellableStream and nsISeekableStream.
 *
 * If aAssignment is NS_ASSIGNMENT_COPY, then the resulting stream holds a copy
 * of the given buffer (aStringToRead), and the caller is free to discard
 * aStringToRead after this function returns.
 *
 * If aAssignment is NS_ASSIGNMENT_DEPEND, then the resulting stream refers
 * directly to the given buffer (aStringToRead), so the caller must ensure that
 * the buffer remains valid for the lifetime of the stream object.  Use with
 * care!!
 *
 * If aAssignment is NS_ASSIGNMENT_ADOPT, then the resulting stream refers
 * directly to the given buffer (aStringToRead) and will free aStringToRead
 * once the stream is closed.
 */
extern nsresult NS_NewByteInputStream(nsIInputStream** aStreamResult,
                                      mozilla::Span<const char> aStringToRead,
                                      nsAssignmentType aAssignment);

/**
 * Factory method to get an nsIInputStream from an nsTArray representing a byte
 * buffer.  This will take ownership of the data and empty out the nsTArray.
 *
 * Result will implement nsIStringInputStream, nsITellableStream and
 * nsISeekableStream.
 */
extern nsresult NS_NewByteInputStream(nsIInputStream** aStreamResult,
                                      nsTArray<uint8_t>&& aArray);

/**
 * Factory method to get an nsInputStream from an nsACString.  Result will
 * implement nsIStringInputStream, nsTellableStream and nsISeekableStream.
 */
extern nsresult NS_NewCStringInputStream(nsIInputStream** aStreamResult,
                                         const nsACString& aStringToRead);
extern nsresult NS_NewCStringInputStream(nsIInputStream** aStreamResult,
                                         nsCString&& aStringToRead);

#endif  // nsStringStream_h__
