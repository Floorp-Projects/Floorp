/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIFontNameIterator_h__
#define nsIFontNameIterator_h__

#include "nsISupports.h"
class nsString;

// {CEEB39D1-0949-11d3-9A87-0050046CDA96}
#define NS_IFONTNAMEITERATOR_IID \
{ 0xceeb39d1, 0x949, 0x11d3, { 0x9a, 0x87, 0x0, 0x50, 0x4, 0x6c, 0xda, 0x96 } };

class nsIFontNameIterator : public nsISupports
		// Fonts are identified by strings, |Get| and |Advance| are distinct to facility wrapping
		//	with C++ objects as standard iterators.
	{
		public:
      NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFONTNAMEITERATOR_IID)

			NS_IMETHOD Reset() = 0;
				// does not need to be called initially, returns iterator to initial state

		  NS_IMETHOD Get( nsString* aFontName ) = 0;
				// returns an error when no more names are available

			NS_IMETHOD Advance() = 0;
				// returns an error when no more names are available
	};


#endif
