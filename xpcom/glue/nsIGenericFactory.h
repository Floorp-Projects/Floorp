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

#ifndef nsIGenericFactory_h___
#define nsIGenericFactory_h___

#include "nsIFactory.h"

// {3bc97f01-ccdf-11d2-bab8-b548654461fc}
#define NS_GENERICFACTORY_CID \
{ 0x3bc97f01, 0xccdf, 0x11d2, { 0xba, 0xb8, 0xb5, 0x48, 0x65, 0x44, 0x61, 0xfc } }

// {3bc97f00-ccdf-11d2-bab8-b548654461fc}
#define NS_IGENERICFACTORY_IID \
{ 0x3bc97f00, 0xccdf, 0x11d2, { 0xba, 0xb8, 0xb5, 0x48, 0x65, 0x44, 0x61, 0xfc } }

/**
 * Provides a Generic nsIFactory implementation that can be used by
 * DLLs with very simple factory needs.
 */
class nsIGenericFactory : public nsIFactory {
public:
    static const nsIID& IID() { static nsIID iid = NS_IGENERICFACTORY_IID; return iid; }
    
    typedef nsresult (*ConstructorProcPtr) (nsISupports *aOuter, REFNSIID aIID, void **aResult);

	/**
	 * Establishes the generic factory's constructor function, which will be called
	 * by CreateInstance.
	 */
    NS_IMETHOD SetConstructor(ConstructorProcPtr constructor) = 0;
};

#endif /* nsIGenericFactory_h___ */
