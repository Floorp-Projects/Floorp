/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef __nsFontRetrieverService
#define __nsFontRetrieverService

#include "nsIFontRetrieverService.h"
#include "nsIFontNameIterator.h"

class nsVoidArray;
class nsFontSizeIterator;

class nsFontRetrieverService: public nsIFontRetrieverService, 
                              public nsIFontNameIterator
{
public:
  nsFontRetrieverService();
  virtual ~nsFontRetrieverService();

  NS_DECL_ISUPPORTS

  // nsIFontRetrieverService
  NS_IMETHOD CreateFontNameIterator( nsIFontNameIterator** aIterator );

  NS_IMETHOD CreateFontSizeIterator( const nsString & aFontName, nsIFontSizeIterator** aIterator );
  NS_IMETHOD IsFontScalable( const nsString & aFontName, PRBool* aResult );

  // nsIFontNameIterator

  NS_IMETHOD Reset();
  NS_IMETHOD Get( nsString* aFontName );
  NS_IMETHOD Advance();



protected:
  NS_IMETHOD LoadFontList();

  nsVoidArray * mFontList;

  PRInt32 mNameIterInx;

  nsFontSizeIterator * mSizeIter;
};

#endif
