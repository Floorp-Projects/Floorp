/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsFontSizeIterator.h"
#include <windows.h>

#include "nsFont.h"
#include "nsVoidArray.h"
 

NS_IMPL_ADDREF(nsFontSizeIterator)
NS_IMPL_RELEASE(nsFontSizeIterator)
NS_IMPL_QUERY_INTERFACE(nsFontSizeIterator, nsCOMTypeInfo<nsIFontSizeIterator>::GetIID())

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
