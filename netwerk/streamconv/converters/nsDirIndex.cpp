/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDirIndex.h"

NS_IMPL_ISUPPORTS(nsDirIndex,
                  nsIDirIndex)

nsDirIndex::nsDirIndex() : mType(TYPE_UNKNOWN),
                           mSize(UINT64_MAX),
                           mLastModified(-1LL)
{
}

NS_IMETHODIMP
nsDirIndex::GetType(uint32_t* aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetType(uint32_t aType)
{
  mType = aType;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::GetContentType(nsACString& aContentType)
{
  aContentType = mContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetContentType(const nsACString& aContentType)
{
  mContentType = aContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::GetLocation(nsACString& aLocation)
{
  aLocation = mLocation;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetLocation(const nsACString& aLocation)
{
  mLocation = aLocation;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::GetDescription(nsAString& aDescription)
{
  aDescription = mDescription;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetDescription(const nsAString& aDescription)
{
  mDescription = aDescription;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::GetSize(int64_t* aSize)
{
  NS_ENSURE_ARG_POINTER(aSize);

  *aSize = mSize;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetSize(int64_t aSize)
{
  mSize = aSize;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::GetLastModified(PRTime* aLastModified)
{
  NS_ENSURE_ARG_POINTER(aLastModified);

  *aLastModified = mLastModified;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetLastModified(PRTime aLastModified)
{
  mLastModified = aLastModified;
  return NS_OK;
}
