/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsSupportsArrayEnumerator.h"
#include "nsISupportsArray.h"

nsSupportsArrayEnumerator::nsSupportsArrayEnumerator(nsISupportsArray* array)
  : mArray(array), mCursor(0)
{
  NS_INIT_REFCNT();
  NS_ASSERTION(array, "null array");
  NS_ADDREF(mArray);
}

nsSupportsArrayEnumerator::~nsSupportsArrayEnumerator()
{
  NS_RELEASE(mArray);
}

NS_IMPL_ISUPPORTS2(nsSupportsArrayEnumerator, nsIBidirectionalEnumerator, nsIEnumerator)

NS_IMETHODIMP
nsSupportsArrayEnumerator::First()
{
  mCursor = 0;
  PRUint32 cnt;
  nsresult rv = mArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  PRInt32 end = (PRInt32)cnt;
  if (mCursor < end)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSupportsArrayEnumerator::Next()
{
  PRUint32 cnt;
  nsresult rv = mArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  PRInt32 end = (PRInt32)cnt;
  if (mCursor < end)   // don't count upward forever
    mCursor++;
  if (mCursor < end)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSupportsArrayEnumerator::CurrentItem(nsISupports **aItem)
{
  NS_ASSERTION(aItem, "null out parameter");
  PRUint32 cnt;
  nsresult rv = mArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  if (mCursor >= 0 && mCursor < (PRInt32)cnt) {
    *aItem = mArray->ElementAt(mCursor);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSupportsArrayEnumerator::IsDone()
{
  PRUint32 cnt;
  nsresult rv = mArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  return (mCursor >= 0 && mCursor < (PRInt32)cnt)
    ? NS_ENUMERATOR_FALSE : NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsSupportsArrayEnumerator::Last()
{
  PRUint32 cnt;
  nsresult rv = mArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  mCursor = cnt - 1;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsArrayEnumerator::Prev()
{
  if (mCursor >= 0)
    --mCursor;
  if (mCursor >= 0)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////

NS_COM nsresult
NS_NewISupportsArrayEnumerator(nsISupportsArray* array,
                               nsIBidirectionalEnumerator* *aInstancePtrResult)
{
  if (aInstancePtrResult == 0)
    return NS_ERROR_NULL_POINTER;
  nsSupportsArrayEnumerator* e = new nsSupportsArrayEnumerator(array);
  if (e == 0)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(e);
  *aInstancePtrResult = e;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

