/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPaperMargin.h"

NS_IMPL_ISUPPORTS(nsPaperMargin, nsIPaperMargin)

NS_IMETHODIMP
nsPaperMargin::GetTop(double* aTop) {
  *aTop = mMargin.top;
  return NS_OK;
}

NS_IMETHODIMP
nsPaperMargin::GetRight(double* aRight) {
  *aRight = mMargin.right;
  return NS_OK;
}

NS_IMETHODIMP
nsPaperMargin::GetBottom(double* aBottom) {
  *aBottom = mMargin.bottom;
  return NS_OK;
}

NS_IMETHODIMP
nsPaperMargin::GetLeft(double* aLeft) {
  *aLeft = mMargin.left;
  return NS_OK;
}
