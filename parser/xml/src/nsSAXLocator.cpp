/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSAXLocator.h"

NS_IMPL_ISUPPORTS1(nsSAXLocator, nsISAXLocator)

nsSAXLocator::nsSAXLocator(nsString& aPublicId,
                           nsString& aSystemId,
                           PRInt32 aLineNumber,
                           PRInt32 aColumnNumber) :
  mPublicId(aPublicId),
  mSystemId(aSystemId),
  mLineNumber(aLineNumber),
  mColumnNumber(aColumnNumber)
{
}

NS_IMETHODIMP
nsSAXLocator::GetColumnNumber(PRInt32 *aColumnNumber)
{
  *aColumnNumber = mColumnNumber;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXLocator::GetLineNumber(PRInt32 *aLineNumber)
{
  *aLineNumber = mLineNumber;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXLocator::GetPublicId(nsAString &aPublicId)
{
  aPublicId = mPublicId;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXLocator::GetSystemId(nsAString &aSystemId)
{
  aSystemId = mSystemId;
  return NS_OK;
}
