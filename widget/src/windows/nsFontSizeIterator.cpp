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
NS_IMPL_QUERY_INTERFACE(nsFontSizeIterator, nsIFontSizeIterator::GetIID())

//----------------------------------------------------------
nsFontSizeIterator::nsFontSizeIterator(nsVoidArray * aFontList) 
{
  NS_INIT_REFCNT();

  mFontList     = aFontList;
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
  if (mSizeIterInx < mFontList->Count()) {
    nsFont * font = (nsFont *)mFontList->ElementAt(mSizeIterInx);
    *aFontSize = 0.0;//font->name;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}


//----------------------------------------------------------
NS_IMETHODIMP nsFontSizeIterator::Advance()
{
  if (mSizeIterInx >= mFontList->Count()-1) {
    return NS_ERROR_FAILURE;
  }

  while (mSizeIterInx < mFontList->Count()-2) {
    mSizeIterInx++;
    nsFont * font = (nsFont *)mFontList->ElementAt(mSizeIterInx);
    if (mFontName.Equals(font->name)) {
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------
NS_IMETHODIMP nsFontSizeIterator::SetFontName( const nsString& aFontName)
{
  mFontName = aFontName;
  return NS_OK;
}
