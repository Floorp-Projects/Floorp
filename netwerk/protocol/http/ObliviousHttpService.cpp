/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ObliviousHttpService.h"

#include "DNSUtils.h"
#include "ObliviousHttpChannel.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS(ObliviousHttpService, nsIObliviousHttpService)

NS_IMETHODIMP
ObliviousHttpService::NewChannel(nsIURI* relayURI, nsIURI* targetURI,
                                 const nsTArray<uint8_t>& encodedConfig,
                                 nsIChannel** result) {
  nsCOMPtr<nsIChannel> innerChannel;
  nsresult rv =
      DNSUtils::CreateChannelHelper(relayURI, getter_AddRefs(innerChannel));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIHttpChannel> innerHttpChannel(do_QueryInterface(innerChannel));
  if (!innerHttpChannel) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIChannel> obliviousHttpChannel(
      new ObliviousHttpChannel(targetURI, encodedConfig, innerHttpChannel));
  obliviousHttpChannel.forget(result);
  return NS_OK;
}

}  // namespace mozilla::net
