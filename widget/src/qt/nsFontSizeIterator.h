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

#ifndef __nsFontSizeIterator
#define __nsFontSizeIterator

#include "nsIFontSizeIterator.h"
#include "nsString.h"

class nsVoidArray;

typedef struct {
  nsString      mName;
  PRBool        mIsScalable;
  nsVoidArray * mSizes;
} FontInfo;


class nsFontSizeIterator: public nsIFontSizeIterator {
public:
  nsFontSizeIterator();
  virtual ~nsFontSizeIterator();

  NS_DECL_ISUPPORTS

  // nsIFontSizeIterator
	NS_IMETHOD Reset();
	NS_IMETHOD Get( double* aFontSize );
	NS_IMETHOD Advance();

  // Native impl
	NS_IMETHOD SetFontInfo( FontInfo * aFontInfo );

protected:

  FontInfo * mFontInfo;      
  PRInt32  mSizeIterInx;   // current index of iter
};

#endif
