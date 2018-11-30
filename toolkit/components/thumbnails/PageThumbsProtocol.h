//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PageThumbsProtocol_h__
#define PageThumbsProtocol_h__

#include "nsIProtocolHandler.h"
#include "nsString.h"

// {5a4ae9b5-f475-48ae-9dce-0b4c1d347884}
#define PAGETHUMBSPROTOCOL_CID                       \
  {                                                  \
    0x5a4ae9b5, 0xf475, 0x48ae, {                    \
      0x9d, 0xce, 0x0b, 0x4c, 0x1d, 0x34, 0x78, 0x84 \
    }                                                \
  }

class PageThumbsProtocol final : public nsIProtocolHandler {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER

 private:
  ~PageThumbsProtocol() {}
  nsresult GetFilePathForURL(nsIURI* aURI, nsIFile** _retval);
  nsresult ParseProtocolURL(nsIURI* aURI, nsString& aParsedURL);
};

#endif /* PageThumbsProtocol_h__ */
