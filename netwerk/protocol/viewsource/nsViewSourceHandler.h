/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsViewSourceHandler_h___
#define nsViewSourceHandler_h___

#include "nsIProtocolHandler.h"
#include "nsNetUtil.h"
#include "mozilla/Attributes.h"

class nsILoadInfo;

namespace mozilla {
namespace net {

class nsViewSourceHandler final : public nsIProtocolHandlerWithDynamicFlags,
                                  public nsIProtocolHandler {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER
  NS_DECL_NSIPROTOCOLHANDLERWITHDYNAMICFLAGS

  nsViewSourceHandler();

  // Creates a new nsViewSourceChannel to view the source of an about:srcdoc
  // URI with contents specified by srcdoc.
  MOZ_MUST_USE nsresult NewSrcdocChannel(nsIURI* aURI, nsIURI* aBaseURI,
                                         const nsAString& aSrcdoc,
                                         nsILoadInfo* aLoadInfo,
                                         nsIChannel** outChannel);

  static nsViewSourceHandler* GetInstance();

 private:
  ~nsViewSourceHandler();

  static nsViewSourceHandler* gInstance;
};

}  // namespace net
}  // namespace mozilla

#endif /* !defined( nsViewSourceHandler_h___ ) */
