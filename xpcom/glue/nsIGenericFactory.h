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

#define NS_GENERICFACTORY_PROGID "component:/netscape/generic-factory"
#define NS_GENERICFACTORY_CLASSNAME "Generic Factory"
/**
 * Provides a Generic nsIFactory implementation that can be used by
 * DLLs with very simple factory needs.
 */
class nsIGenericFactory : public nsIFactory {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IGENERICFACTORY_IID; return iid; }
    
    typedef NS_CALLBACK(ConstructorProcPtr) (nsISupports *aOuter, REFNSIID aIID, void **aResult);
    typedef NS_CALLBACK(DestructorProcPtr) (void);

	/**
	 * Establishes the generic factory's constructor function, which will be called
	 * by CreateInstance.
	 */
    NS_IMETHOD SetConstructor(ConstructorProcPtr constructor) = 0;

	/**
	 * Establishes the generic factory's destructor function, which will be called
	 * whe the generic factory is deleted. This is used to notify the DLL that
	 * an instance of one of its generic factories is going away.
	 */
    NS_IMETHOD SetDestructor(DestructorProcPtr destructor) = 0;
};

extern NS_COM nsresult
NS_NewGenericFactory(nsIGenericFactory* *result,
                     nsIGenericFactory::ConstructorProcPtr constructor,
                     nsIGenericFactory::DestructorProcPtr destructor = NULL);

#define NS_GENERIC_FACTORY_CONSTRUCTOR(_InstanceClass)                          \
static NS_IMETHODIMP                                                            \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID, void **aResult) \
{                                                                               \
    nsresult rv;                                                                \
                                                                                \
    _InstanceClass * inst;                                                      \
                                                                                \
    if (NULL == aResult) {                                                      \
        rv = NS_ERROR_NULL_POINTER;                                             \
        goto done;                                                              \
    }                                                                           \
    *aResult = NULL;                                                            \
    if (NULL != aOuter) {                                                       \
        rv = NS_ERROR_NO_AGGREGATION;                                           \
        goto done;                                                              \
    }                                                                           \
                                                                                \
    NS_NEWXPCOM(inst, _InstanceClass);                                          \
    if (NULL == inst) {                                                         \
        rv = NS_ERROR_OUT_OF_MEMORY;                                            \
        goto done;                                                              \
    }                                                                           \
    NS_ADDREF(inst);                                                            \
    rv = inst->QueryInterface(aIID, aResult);                                   \
    NS_RELEASE(inst);                                                           \
                                                                                \
  done:                                                                         \
    return rv;                                                                  \
} 

#endif /* nsIGenericFactory_h___ */
