/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/UrlClassifierFeatureResult.h"

namespace mozilla {
namespace net {

UrlClassifierFeatureResult::UrlClassifierFeatureResult(
    nsIURI* aURI, nsIUrlClassifierFeature* aFeature, const nsACString& aList)
    : mURI(aURI), mFeature(aFeature), mList(aList) {}

UrlClassifierFeatureResult::~UrlClassifierFeatureResult() = default;

NS_IMETHODIMP
UrlClassifierFeatureResult::GetUri(nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aURI);
  nsCOMPtr<nsIURI> uri = mURI;
  uri.forget(aURI);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureResult::GetFeature(nsIUrlClassifierFeature** aFeature) {
  NS_ENSURE_ARG_POINTER(aFeature);
  nsCOMPtr<nsIUrlClassifierFeature> feature = mFeature;
  feature.forget(aFeature);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureResult::GetList(nsACString& aList) {
  aList = mList;
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(UrlClassifierFeatureResult)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIUrlClassifierFeatureResult)
  NS_INTERFACE_MAP_ENTRY(nsIUrlClassifierFeatureResult)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(UrlClassifierFeatureResult)
NS_IMPL_RELEASE(UrlClassifierFeatureResult)

}  // namespace net
}  // namespace mozilla
