/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ___nsscriptableinputstream___h_
#define ___nsscriptableinputstream___h_

#include "nsIScriptableInputStream.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

#define NS_SCRIPTABLEINPUTSTREAM_CID               \
  {                                                \
    0x7225c040, 0xa9bf, 0x11d3, {                  \
      0xa1, 0x97, 0x0, 0x50, 0x4, 0x1c, 0xaf, 0x44 \
    }                                              \
  }

#define NS_SCRIPTABLEINPUTSTREAM_CONTRACTID \
  "@mozilla.org/scriptableinputstream;1"

class nsScriptableInputStream final : public nsIScriptableInputStream {
 public:
  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsIScriptableInputStream methods
  NS_DECL_NSISCRIPTABLEINPUTSTREAM

  // nsScriptableInputStream methods
  nsScriptableInputStream() = default;

  static nsresult Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

 private:
  ~nsScriptableInputStream() = default;

  nsresult ReadHelper(char* aBuffer, uint32_t aCount);

  nsCOMPtr<nsIInputStream> mInputStream;
};

#endif  // ___nsscriptableinputstream___h_
