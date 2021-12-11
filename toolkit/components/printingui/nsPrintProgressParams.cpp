/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintProgressParams.h"
#include "nsReadableUtils.h"

NS_IMPL_ISUPPORTS(nsPrintProgressParams, nsIPrintProgressParams)

nsPrintProgressParams::nsPrintProgressParams() = default;

nsPrintProgressParams::~nsPrintProgressParams() = default;

NS_IMETHODIMP
nsPrintProgressParams::GetDocTitle(nsAString& aDocTitle) {
  aDocTitle = mDocTitle;
  return NS_OK;
}

NS_IMETHODIMP
nsPrintProgressParams::SetDocTitle(const nsAString& aDocTitle) {
  mDocTitle = aDocTitle;
  return NS_OK;
}

NS_IMETHODIMP
nsPrintProgressParams::GetDocURL(nsAString& aDocURL) {
  aDocURL = mDocURL;
  return NS_OK;
}

NS_IMETHODIMP
nsPrintProgressParams::SetDocURL(const nsAString& aDocURL) {
  mDocURL = aDocURL;
  return NS_OK;
}
