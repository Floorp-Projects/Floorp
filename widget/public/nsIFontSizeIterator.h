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

#ifndef nsIFontSizeIterator_h__
#define nsIFontSizeIterator_h__

#include "nsISupports.h"
class nsString;

// {285EF9B1-094A-11d3-9A87-0050046CDA96}
#define NS_IFONTSIZEITERATOR_IID \
{ 0x285ef9b1, 0x94a, 0x11d3, { 0x9a, 0x87, 0x0, 0x50, 0x4, 0x6c, 0xda, 0x96 } };

class nsIFontSizeIterator : public nsISupports
		// Font sizes are identified with doubles (e.g., for the possibility of fractional sizes from MM, etc.).
		//	|Get| and |Advance| are distinct to facility wrapping with C++ objects as standard iterators.
	{
		public:
      NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFONTSIZEITERATOR_IID)

			NS_IMETHOD Reset() = 0;
				// does not need to be called initially, returns iterator to initial state

			NS_IMETHOD Get( double* aFontSize ) = 0;
				// returns an error when no more sizes are available

			NS_IMETHOD Advance() = 0;
				// returns an error when no more sizes are available
	};


#endif
