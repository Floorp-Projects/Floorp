#ifndef nsIDocStreamLoaderFactory_h___
#define nsIDocStreamLoaderFactory_h___

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
class nsIContentViewer;
class nsIContentViewerContainer;

 /* 9188bc80-f92e-11d2-81ef-0060083a0bcf */
#define NS_IDOCSTREAMLOADERFACTORY_IID \
 { 0x9188bc80, 0xf92e, 0x11d2,{0x81, 0xef, 0x00, 0x60, 0x08, 0x3a, 0x0b, 0xcf}}

class nsIDocStreamLoaderFactory : public nsISupports
	{
		public:
		  static const nsIID& GetIID() { static nsIID iid = NS_IDOCSTREAMLOADERFACTORY_IID; return iid; }

			NS_IMETHOD CreateInstance( nsIInputStream& aInputStream,
																 const char* aContentType,
																 const char* aCommand,
																 nsIContentViewerContainer* aContainer,
																 nsISupports* aExtraInfo,
																 nsIContentViewer** aDocViewer ) = 0;
	};

#endif // !defined(nsIDocStreamLoaderFactory_h___)
