/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupportsImpl.h"
#include "nsIInputStreamLength.h"

class nsIInputStream;

namespace mozilla {

// This class helps to retrieve the stream's length.

class InputStreamLengthHelper final : public Runnable
                                    , public nsIInputStreamLengthCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // This is one of the 2 entry points of this class. It returns false if the
  // length cannot be taken synchronously.
  static bool
  GetSyncLength(nsIInputStream* aStream,
                int64_t* aLength);

  // This is one of the 2 entry points of this class. The callback is executed
  // asynchronously when the length is known.
  static void
  GetAsyncLength(nsIInputStream* aStream,
                 const std::function<void(int64_t aLength)>& aCallback);

private:
  NS_DECL_NSIINPUTSTREAMLENGTHCALLBACK

  InputStreamLengthHelper(nsIInputStream* aStream,
                          const std::function<void(int64_t aLength)>& aCallback);

  ~InputStreamLengthHelper();

  NS_IMETHOD
  Run() override;

  void
  ExecCallback(int64_t aLength);

  nsCOMPtr<nsIInputStream> mStream;
  std::function<void(int64_t aLength)> mCallback;
};

} // mozilla namespace
