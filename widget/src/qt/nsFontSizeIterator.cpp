/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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

#include "nsFontSizeIterator.h"

#include "nsFont.h"
#include "nsVoidArray.h"
 

NS_IMPL_ADDREF(nsFontSizeIterator)
NS_IMPL_RELEASE(nsFontSizeIterator)
NS_IMPL_QUERY_INTERFACE(nsFontSizeIterator, NS_GET_IID(nsIFontSizeIterator))

//----------------------------------------------------------
nsFontSizeIterator::nsFontSizeIterator() 
{
  NS_INIT_REFCNT();
  mFontInfo     = nsnull;
  mSizeIterInx  = 0;
}

//----------------------------------------------------------
nsFontSizeIterator::~nsFontSizeIterator()
{
}

///----------------------------------------------------------
//-- nsIFontNameIterator
//----------------------------------------------------------
NS_IMETHODIMP nsFontSizeIterator::Reset()
{
  mSizeIterInx = 0;
  return NS_OK;
}

//----------------------------------------------------------
NS_IMETHODIMP nsFontSizeIterator::Get( double* aFontSize  )
{
  if (nsnull != mFontInfo->mSizes && 
      mFontInfo->mSizes->Count() > 0 && 
      mSizeIterInx < mFontInfo->mSizes->Count()) {
    PRUint32 size = (PRUint32)mFontInfo->mSizes->ElementAt(mSizeIterInx);
    *aFontSize = (double)size;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}


//----------------------------------------------------------
NS_IMETHODIMP nsFontSizeIterator::Advance()
{
  if (nsnull != mFontInfo->mSizes && 
      mFontInfo->mSizes->Count() > 0 &&
      mSizeIterInx < mFontInfo->mSizes->Count()-2) {
    mSizeIterInx++;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------
NS_IMETHODIMP nsFontSizeIterator::SetFontInfo( FontInfo * aFontInfo )
{
  mFontInfo = aFontInfo;
  return NS_OK;
}
