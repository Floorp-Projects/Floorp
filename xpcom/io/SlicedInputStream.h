/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SlicedInputStream_h
#define SlicedInputStream_h

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsICloneableInputStream.h"

// A wrapper for a slice of an underlying input stream.

class SlicedInputStream final : public nsIAsyncInputStream
                              , public nsICloneableInputStream
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  // Create an input stream whose data comes from a slice of aInputStream.  The
  // slice begins at aStart bytes beyond aInputStream's current position, and
  // extends for a maximum of aLength bytes.  If aInputStream contains fewer
  // than aStart bytes, reading from SlicedInputStream returns no data.  If
  // aInputStream contains more than aStart bytes, but fewer than aStart +
  // aLength bytes, reading from SlicedInputStream returns as many bytes as can
  // be consumed from aInputStream after reading aLength bytes.
  //
  // aInputStream should not be read from after constructing a
  // SlicedInputStream wrapper around it.

  SlicedInputStream(nsIInputStream* aInputStream,
                    uint64_t aStart, uint64_t aLength);

private:
  ~SlicedInputStream();

  nsCOMPtr<nsIInputStream> mInputStream;
  uint64_t mStart;
  uint64_t mLength;
  uint64_t mCurPos;

  bool mClosed;
};

#endif // SlicedInputStream_h
