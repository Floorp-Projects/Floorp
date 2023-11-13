/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:ts=4 sw=2 sts=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoViewContentProtocolHandler.h"
#include "GeckoViewContentChannel.h"
#include "nsStandardURL.h"
#include "nsURLHelper.h"
#include "nsIURIMutator.h"

#include "nsNetUtil.h"

#include "mozilla/ResultExtensions.h"

//-----------------------------------------------------------------------------

nsresult GeckoViewContentProtocolHandler::Init() { return NS_OK; }

NS_IMPL_ISUPPORTS(GeckoViewContentProtocolHandler, nsIProtocolHandler,
                  nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// nsIProtocolHandler methods:

NS_IMETHODIMP
GeckoViewContentProtocolHandler::GetScheme(nsACString& result) {
  result.AssignLiteral("content");
  return NS_OK;
}

NS_IMETHODIMP
GeckoViewContentProtocolHandler::NewChannel(nsIURI* uri, nsILoadInfo* aLoadInfo,
                                            nsIChannel** result) {
  nsresult rv;
  RefPtr<GeckoViewContentChannel> chan = new GeckoViewContentChannel(uri);

  rv = chan->SetLoadInfo(aLoadInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }

  chan.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
GeckoViewContentProtocolHandler::AllowPort(int32_t port, const char* scheme,
                                           bool* result) {
  // don't override anything.
  *result = false;
  return NS_OK;
}
