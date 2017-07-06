/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintProgressParams.h"
#include "nsReadableUtils.h"


NS_IMPL_ISUPPORTS(nsPrintProgressParams, nsIPrintProgressParams)

nsPrintProgressParams::nsPrintProgressParams()
{
}

nsPrintProgressParams::~nsPrintProgressParams()
{
}

NS_IMETHODIMP nsPrintProgressParams::GetDocTitle(char16_t * *aDocTitle)
{
  NS_ENSURE_ARG(aDocTitle);

  *aDocTitle = ToNewUnicode(mDocTitle);
  return NS_OK;
}

NS_IMETHODIMP nsPrintProgressParams::SetDocTitle(const char16_t * aDocTitle)
{
  mDocTitle = aDocTitle;
  return NS_OK;
}

NS_IMETHODIMP nsPrintProgressParams::GetDocURL(char16_t * *aDocURL)
{
  NS_ENSURE_ARG(aDocURL);

  *aDocURL = ToNewUnicode(mDocURL);
  return NS_OK;
}

NS_IMETHODIMP nsPrintProgressParams::SetDocURL(const char16_t * aDocURL)
{
  mDocURL = aDocURL;
  return NS_OK;
}

