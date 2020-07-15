/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPaper.h"

NS_IMPL_ISUPPORTS(nsPaper, nsIPaper);

NS_IMETHODIMP
nsPaper::GetName(nsAString& aName) {
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsPaper::GetWidth(double* aWidth) {
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = mWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsPaper::GetHeight(double* aHeight) {
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsPaper::GetUnwriteableMarginTop(double* aUnwriteableMarginTop) {
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginTop);
  *aUnwriteableMarginTop = mUnwriteableMarginTop;
  return NS_OK;
}

NS_IMETHODIMP
nsPaper::GetUnwriteableMarginBottom(double* aUnwriteableMarginBottom) {
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginBottom);
  *aUnwriteableMarginBottom = mUnwriteableMarginBottom;
  return NS_OK;
}

NS_IMETHODIMP
nsPaper::GetUnwriteableMarginLeft(double* aUnwriteableMarginLeft) {
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginLeft);
  *aUnwriteableMarginLeft = mUnwriteableMarginLeft;
  return NS_OK;
}

NS_IMETHODIMP
nsPaper::GetUnwriteableMarginRight(double* aUnwriteableMarginRight) {
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginRight);
  *aUnwriteableMarginRight = mUnwriteableMarginRight;
  return NS_OK;
}
