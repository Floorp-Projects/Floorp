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

#ifndef nsGenericFactory_h___
#define nsGenericFactory_h___

#include "nsIGenericFactory.h"

/**
 * Most factories follow this simple pattern, so why not just use a function pointer
 * for most creation operations?
 */
class nsGenericFactory : public nsIGenericFactory {
public:
    static const nsCID& CID() { static nsCID cid = NS_GENERICFACTORY_CID; return cid; }

	nsGenericFactory(ConstructorProcPtr constructor = NULL);
	virtual ~nsGenericFactory();
	
	NS_DECL_ISUPPORTS
	
	NS_IMETHOD CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult);

	NS_IMETHOD LockFactory(PRBool aLock);

	/**
	 * Establishes the generic factory's constructor function, which will be called
	 * by CreateInstance.
	 */
    NS_IMETHOD SetConstructor(ConstructorProcPtr constructor);

	/**
	 * Establishes the generic factory's destructor function, which will be called
	 * whe the generic factory is deleted. This is used to notify the DLL that
	 * an instance of one of its generic factories is going away.
	 */
    NS_IMETHOD SetDestructor(DestructorProcPtr destructor);

	static NS_METHOD Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);
private:
	ConstructorProcPtr mConstructor;
	DestructorProcPtr mDestructor;
};

#endif /* nsGenericFactory_h___ */
