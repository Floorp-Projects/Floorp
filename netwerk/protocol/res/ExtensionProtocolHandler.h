/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ExtensionProtocolHandler_h___
#define ExtensionProtocolHandler_h___

#include "SubstitutingProtocolHandler.h"
#include "nsWeakReference.h"

namespace mozilla {

class ExtensionProtocolHandler final : public nsISubstitutingProtocolHandler,
                                       public mozilla::SubstitutingProtocolHandler,
                                       public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIPROTOCOLHANDLER(mozilla::SubstitutingProtocolHandler::)
  NS_FORWARD_NSISUBSTITUTINGPROTOCOLHANDLER(mozilla::SubstitutingProtocolHandler::)

  // In general a moz-extension URI is only loadable by chrome, but a whitelisted
  // subset are web-accessible (see nsIAddonPolicyService).
  ExtensionProtocolHandler()
    : SubstitutingProtocolHandler("moz-extension", URI_STD | URI_DANGEROUS_TO_LOAD | URI_IS_LOCAL_RESOURCE)
  {}

protected:
  ~ExtensionProtocolHandler() {}
};

} // namespace mozilla

#endif /* ExtensionProtocolHandler_h___ */
