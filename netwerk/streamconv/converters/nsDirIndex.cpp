/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDirIndex.h"
#include "nsISupportsObsolete.h"

NS_IMPL_ISUPPORTS1(nsDirIndex,
                   nsIDirIndex)

nsDirIndex::nsDirIndex() : mType(TYPE_UNKNOWN),
                           mSize(UINT64_MAX),
                           mLastModified(-1) {
}

nsDirIndex::~nsDirIndex() {}

NS_IMPL_GETSET(nsDirIndex, Type, uint32_t, mType)

// GETSET macros for modern strings would be nice...

NS_IMETHODIMP
nsDirIndex::GetContentType(char* *aContentType) {
  *aContentType = ToNewCString(mContentType);
  if (!*aContentType)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetContentType(const char* aContentType) {
  mContentType = aContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::GetLocation(char* *aLocation) {
  *aLocation = ToNewCString(mLocation);
  if (!*aLocation)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetLocation(const char* aLocation) {
  mLocation = aLocation;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::GetDescription(PRUnichar* *aDescription) {
  *aDescription = ToNewUnicode(mDescription);
  if (!*aDescription)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetDescription(const PRUnichar* aDescription) {
  mDescription.Assign(aDescription);
  return NS_OK;
}

NS_IMPL_GETSET(nsDirIndex, Size, int64_t, mSize)
NS_IMPL_GETSET(nsDirIndex, LastModified, PRTime, mLastModified)

