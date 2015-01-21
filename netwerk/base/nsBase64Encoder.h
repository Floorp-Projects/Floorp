/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSBASE64ENCODER_H_
#define NSBASE64ENCODER_H_

#include "nsIOutputStream.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

/**
 * A base64 encoder. Usage: Instantiate class, write to it using
 * Write(), then call Finish() to get the base64-encoded data.
 */
class nsBase64Encoder MOZ_FINAL : public nsIOutputStream {
  public:
    nsBase64Encoder() {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOUTPUTSTREAM

    nsresult Finish(nsCSubstring& _result);
  private:
    ~nsBase64Encoder() {}

    /// The data written to this stream. nsCString can deal fine with
    /// binary data.
    nsCString mData;
};

#endif
