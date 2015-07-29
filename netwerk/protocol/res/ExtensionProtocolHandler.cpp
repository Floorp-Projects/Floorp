/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionProtocolHandler.h"

#include "nsIAddonPolicyService.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {

NS_IMPL_QUERY_INTERFACE(ExtensionProtocolHandler, nsISubstitutingProtocolHandler,
                        nsIProtocolHandler, nsIProtocolHandlerWithDynamicFlags,
                        nsISupportsWeakReference)
NS_IMPL_ADDREF_INHERITED(ExtensionProtocolHandler, SubstitutingProtocolHandler)
NS_IMPL_RELEASE_INHERITED(ExtensionProtocolHandler, SubstitutingProtocolHandler)

nsresult
ExtensionProtocolHandler::GetFlagsForURI(nsIURI* aURI, uint32_t* aFlags)
{
  // In general a moz-extension URI is only loadable by chrome, but a whitelisted
  // subset are web-accessible. Check that whitelist.
  nsCOMPtr<nsIAddonPolicyService> aps = do_GetService("@mozilla.org/addons/policy-service;1");
  bool loadableByAnyone = false;
  if (aps) {
    nsresult rv = aps->ExtensionURILoadableByAnyone(aURI, &loadableByAnyone);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aFlags = URI_STD | URI_IS_LOCAL_RESOURCE | (loadableByAnyone ? URI_LOADABLE_BY_ANYONE : URI_DANGEROUS_TO_LOAD);
  return NS_OK;
}

} // namespace mozilla
