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

#ifndef __nsFontSizeIterator
#define __nsFontSizeIterator

#include "nsIFontSizeIterator.h"
#include "nsString.h"

class nsVoidArray;

class nsFontSizeIterator: public nsIFontSizeIterator {
public:
  nsFontSizeIterator(nsVoidArray * aFontList);
  virtual ~nsFontSizeIterator();

  NS_DECL_ISUPPORTS

  // nsIFontSizeIterator
	NS_IMETHOD Reset();
	NS_IMETHOD Get( double* aFontSize );
	NS_IMETHOD Advance();

  // Native impl
	NS_IMETHOD SetFontName( const nsString& aFontName);

protected:

  nsVoidArray * mFontList; //  this array is not owned by this object

  nsString mFontName;      // the name of the font to be looked at
  PRInt32 mSizeIterInx;    // current index of iter
};

#endif
