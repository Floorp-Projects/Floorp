#ifndef nsIStreamLoadableDocument_h___
#define nsIStreamLoadableDocument_h___

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

  // Wrapping includes can speed up compiles (see "Large Scale C++ Software Design")
#ifndef nsISupports_h___
	#include "nsISupports.h"
#endif

class nsIInputStream;
class nsIContentViewerContainer;

#define NS_ISTREAMLOADABLEDOCUMENT_IID \
 { 0x7dae0360, 0xf907, 0x11d2,{0x81, 0xee, 0x00, 0x60, 0x08, 0x3a, 0x0b, 0xcf}}

class nsIStreamLoadableDocument : public nsISupports
	{
		public:
		  static const nsIID& GetIID() { static nsIID iid = NS_ISTREAMLOADABLEDOCUMENT_IID; return iid; }

			NS_IMETHOD LoadFromStream( nsIInputStream& xulStream,
																 nsIContentViewerContainer* aContainer,
																 const char* aCommand ) = 0;
	};

#endif // !defined(nsIStreamLoadableDocument_h___)
