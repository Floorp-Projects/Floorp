/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPaperCUPS.h"

#include "nsString.h"

static const double POINTS_PER_HUNDREDTH_MILLIMETER = 0.0283465;

NS_IMPL_ISUPPORTS(nsPaperCUPS, nsIPaper);

NS_IMETHODIMP
nsPaperCUPS::GetName(nsAString& aName) {
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsPaperCUPS::GetWidth(double* aWidth) {
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = mInfo.width * POINTS_PER_HUNDREDTH_MILLIMETER;
  return NS_OK;
}

NS_IMETHODIMP
nsPaperCUPS::GetHeight(double* aHeight) {
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = mInfo.length * POINTS_PER_HUNDREDTH_MILLIMETER;
  return NS_OK;
}

NS_IMETHODIMP
nsPaperCUPS::GetUnwriteableMarginTop(double* aUnwriteableMarginTop) {
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginTop);
  *aUnwriteableMarginTop = mInfo.top * POINTS_PER_HUNDREDTH_MILLIMETER;
  return NS_OK;
}

NS_IMETHODIMP
nsPaperCUPS::GetUnwriteableMarginBottom(double* aUnwriteableMarginBottom) {
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginBottom);
  *aUnwriteableMarginBottom = mInfo.bottom * POINTS_PER_HUNDREDTH_MILLIMETER;
  return NS_OK;
}

NS_IMETHODIMP
nsPaperCUPS::GetUnwriteableMarginLeft(double* aUnwriteableMarginLeft) {
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginLeft);
  *aUnwriteableMarginLeft = mInfo.left * POINTS_PER_HUNDREDTH_MILLIMETER;
  return NS_OK;
}

NS_IMETHODIMP
nsPaperCUPS::GetUnwriteableMarginRight(double* aUnwriteableMarginRight) {
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginRight);
  *aUnwriteableMarginRight = mInfo.right * POINTS_PER_HUNDREDTH_MILLIMETER;
  return NS_OK;
}
