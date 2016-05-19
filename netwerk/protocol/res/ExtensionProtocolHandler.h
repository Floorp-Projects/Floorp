/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ExtensionProtocolHandler_h___
#define ExtensionProtocolHandler_h___

#include "SubstitutingProtocolHandler.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace net {

class ExtensionProtocolHandler final : public nsISubstitutingProtocolHandler,
                                       public nsIProtocolHandlerWithDynamicFlags,
                                       public SubstitutingProtocolHandler,
                                       public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPROTOCOLHANDLERWITHDYNAMICFLAGS
  NS_FORWARD_NSIPROTOCOLHANDLER(SubstitutingProtocolHandler::)
  NS_FORWARD_NSISUBSTITUTINGPROTOCOLHANDLER(SubstitutingProtocolHandler::)

  ExtensionProtocolHandler() : SubstitutingProtocolHandler("moz-extension") {}

protected:
  ~ExtensionProtocolHandler() {}

  bool ResolveSpecialCases(const nsACString& aHost, const nsACString& aPath, nsACString& aResult) override
  {
    // Create a special about:blank-like moz-extension://foo/_blank.html for all
    // registered extensions. We can't just do this as a substitution because
    // substitutions can only match on host.
    if (SubstitutingProtocolHandler::HasSubstitution(aHost) && aPath.EqualsLiteral("/_blank.html")) {
      aResult.AssignLiteral("about:blank");
      return true;
    }

    return false;
  }

  virtual nsresult SubstituteChannel(nsIURI* uri, nsILoadInfo* aLoadInfo, nsIChannel** result) override;
};

} // namespace net
} // namespace mozilla

#endif /* ExtensionProtocolHandler_h___ */
