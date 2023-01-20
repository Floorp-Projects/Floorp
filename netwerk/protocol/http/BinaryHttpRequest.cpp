/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BinaryHttpRequest.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS(BinaryHttpRequest, nsIBinaryHttpRequest)

NS_IMETHODIMP BinaryHttpRequest::GetMethod(nsACString& aMethod) {
  aMethod.Assign(mMethod);
  return NS_OK;
}

NS_IMETHODIMP BinaryHttpRequest::GetScheme(nsACString& aScheme) {
  aScheme.Assign(mScheme);
  return NS_OK;
}

NS_IMETHODIMP BinaryHttpRequest::GetAuthority(nsACString& aAuthority) {
  aAuthority.Assign(mAuthority);
  return NS_OK;
}

NS_IMETHODIMP BinaryHttpRequest::GetPath(nsACString& aPath) {
  aPath.Assign(mPath);
  return NS_OK;
}

NS_IMETHODIMP BinaryHttpRequest::GetHeaderNames(
    nsTArray<nsCString>& aHeaderNames) {
  aHeaderNames.Assign(mHeaderNames);
  return NS_OK;
}

NS_IMETHODIMP BinaryHttpRequest::GetHeaderValues(
    nsTArray<nsCString>& aHeaderValues) {
  aHeaderValues.Assign(mHeaderValues);
  return NS_OK;
}

NS_IMETHODIMP BinaryHttpRequest::GetContent(nsTArray<uint8_t>& aContent) {
  aContent.Assign(mContent);
  return NS_OK;
}

}  // namespace mozilla::net
