/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAboutBlank.h"
#include "nsStringStream.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"

NS_IMPL_ISUPPORTS(nsAboutBlank, nsIAboutModule)

NS_IMETHODIMP
nsAboutBlank::NewChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                         nsIChannel** result) {
  NS_ENSURE_ARG_POINTER(aURI);

  nsCOMPtr<nsIInputStream> in;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(in), ""_ns);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewInputStreamChannelInternal(getter_AddRefs(channel), aURI,
                                        in.forget(), "text/html"_ns, "utf-8"_ns,
                                        aLoadInfo);
  if (NS_FAILED(rv)) return rv;

  channel.forget(result);
  return rv;
}

NS_IMETHODIMP
nsAboutBlank::GetURIFlags(nsIURI* aURI, uint32_t* result) {
  *result = nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
            nsIAboutModule::URI_CAN_LOAD_IN_CHILD |
            nsIAboutModule::MAKE_LINKABLE |
            nsIAboutModule::HIDE_FROM_ABOUTABOUT;
  return NS_OK;
}

NS_IMETHODIMP
nsAboutBlank::GetChromeURI(nsIURI* aURI, nsIURI** chromeURI) {
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult nsAboutBlank::Create(REFNSIID aIID, void** aResult) {
  RefPtr<nsAboutBlank> about = new nsAboutBlank();
  return about->QueryInterface(aIID, aResult);
}

////////////////////////////////////////////////////////////////////////////////
