/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSupportsArrayEnumerator.h"
#include "nsISupportsArray.h"

nsSupportsArrayEnumerator::nsSupportsArrayEnumerator(nsISupportsArray* array)
  : mArray(array)
  , mCursor(0)
{
  NS_ASSERTION(array, "null array");
  NS_ADDREF(mArray);
}

nsSupportsArrayEnumerator::~nsSupportsArrayEnumerator()
{
  NS_RELEASE(mArray);
}

NS_IMPL_ISUPPORTS(nsSupportsArrayEnumerator, nsIBidirectionalEnumerator,
                  nsIEnumerator)

NS_IMETHODIMP
nsSupportsArrayEnumerator::First()
{
  mCursor = 0;
  uint32_t cnt;
  nsresult rv = mArray->Count(&cnt);
  if (NS_FAILED(rv)) {
    return rv;
  }
  int32_t end = (int32_t)cnt;
  if (mCursor < end) {
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP
nsSupportsArrayEnumerator::Next()
{
  uint32_t cnt;
  nsresult rv = mArray->Count(&cnt);
  if (NS_FAILED(rv)) {
    return rv;
  }
  int32_t end = (int32_t)cnt;
  if (mCursor < end) { // don't count upward forever
    mCursor++;
  }
  if (mCursor < end) {
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP
nsSupportsArrayEnumerator::CurrentItem(nsISupports** aItem)
{
  NS_ASSERTION(aItem, "null out parameter");
  uint32_t cnt;
  nsresult rv = mArray->Count(&cnt);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (mCursor >= 0 && mCursor < (int32_t)cnt) {
    return mArray->GetElementAt(mCursor, aItem);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSupportsArrayEnumerator::IsDone()
{
  uint32_t cnt;
  nsresult rv = mArray->Count(&cnt);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // XXX This is completely incompatible with the meaning of nsresult.
  // NS_ENUMERATOR_FALSE is defined to be 1.  (bug 778111)
  return (mCursor >= 0 && mCursor < (int32_t)cnt)
    ? (nsresult)NS_ENUMERATOR_FALSE : NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsSupportsArrayEnumerator::Last()
{
  uint32_t cnt;
  nsresult rv = mArray->Count(&cnt);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mCursor = cnt - 1;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsArrayEnumerator::Prev()
{
  if (mCursor >= 0) {
    --mCursor;
  }
  if (mCursor >= 0) {
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE;
  }
}

