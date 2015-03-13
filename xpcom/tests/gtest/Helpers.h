/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __Helpers_h
#define __Helpers_h

#include "nsIAsyncOutputStream.h"
#include "nsString.h"
#include <stdint.h>

class nsIInputStream;
class nsIOutputStream;
template <class T> class nsTArray;

namespace testing {

void
CreateData(uint32_t aNumBytes, nsTArray<char>& aDataOut);

void
Write(nsIOutputStream* aStream, const nsTArray<char>& aData, uint32_t aOffset,
      uint32_t aNumBytes);

void
WriteAllAndClose(nsIOutputStream* aStream, const nsTArray<char>& aData);

void
ConsumeAndValidateStream(nsIInputStream* aStream,
                         const nsTArray<char>& aExpectedData);

void
ConsumeAndValidateStream(nsIInputStream* aStream,
                         const nsACString& aExpectedData);

class OutputStreamCallback MOZ_FINAL : public nsIOutputStreamCallback
{
public:
  OutputStreamCallback();

  bool Called() const { return mCalled; }

private:
  ~OutputStreamCallback();

  bool mCalled;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAMCALLBACK
};

} // namespace testing

#endif // __Helpers_h
