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

////////////////////////////////////////////////////////////////////////////////

#include "nsIModule.h"
#include "nsHashtable.h"

class nsGenericModule : public nsIModule
{
public:
    nsGenericModule(const char* moduleName, PRUint32 componentCount,
                    nsModuleComponentInfo* components,
                    nsModuleDestructorProc dtor);
    virtual ~nsGenericModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool                      mInitialized;
    const char*                 mModuleName;
    PRUint32                    mComponentCount;
    nsModuleComponentInfo*      mComponents;
    nsSupportsHashtable         mFactories;
    nsModuleDestructorProc      mDtor;
};

#endif /* nsGenericFactory_h___ */
