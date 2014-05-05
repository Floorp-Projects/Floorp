/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVersionComparatorImpl.h"
#include "nsVersionComparator.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS(nsVersionComparatorImpl, nsIVersionComparator)

NS_IMETHODIMP
nsVersionComparatorImpl::Compare(const nsACString& A, const nsACString& B,
                                 int32_t *aResult)
{
  *aResult = mozilla::CompareVersions(PromiseFlatCString(A).get(),
                                      PromiseFlatCString(B).get());

  return NS_OK;
}
