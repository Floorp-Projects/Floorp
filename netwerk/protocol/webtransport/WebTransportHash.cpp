/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransportHash.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS(WebTransportHash, nsIWebTransportHash);

NS_IMETHODIMP
WebTransportHash::GetValue(nsTArray<uint8_t>& aValue) {
  aValue.Clear();
  aValue.AppendElements(mValue);
  return NS_OK;
}

NS_IMETHODIMP
WebTransportHash::GetAlgorithm(nsACString& algorithm) {
  algorithm.Assign(mAlgorithm);
  return NS_OK;
}
}  // namespace mozilla::net
