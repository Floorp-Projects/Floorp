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
nsDirIndex::GetContentType(char* *aContentType)
{
  NS_ENSURE_ARG_POINTER(aContentType);

  *aContentType = ToNewCString(mContentType);
  if (!*aContentType)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetContentType(const char* aContentType)
{
  mContentType = aContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::GetLocation(char* *aLocation)
{
  NS_ENSURE_ARG_POINTER(aLocation);

  *aLocation = ToNewCString(mLocation);
  if (!*aLocation)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetLocation(const char* aLocation)
{
  mLocation = aLocation;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::GetDescription(char16_t* *aDescription)
{
  NS_ENSURE_ARG_POINTER(aDescription);

  *aDescription = ToNewUnicode(mDescription);
  if (!*aDescription)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetDescription(const char16_t* aDescription)
{
  mDescription.Assign(aDescription);
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
