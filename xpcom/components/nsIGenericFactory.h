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

#define NS_GENERIC_FACTORY_CONSTRUCTOR(_InstanceClass)                        \
static NS_IMETHODIMP                                                          \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID, void **aResult) \
{                                                                               \
    nsresult rv;                                                                \
                                                                                \
    _InstanceClass * inst;                                                      \
                                                                                \
    if (NULL == aResult) {                                                      \
        rv = NS_ERROR_NULL_POINTER;                                             \
        return rv;                                                              \
    }                                                                           \
    *aResult = NULL;                                                            \
    if (NULL != aOuter) {                                                       \
        rv = NS_ERROR_NO_AGGREGATION;                                           \
        return rv;                                                              \
    }                                                                           \
                                                                                \
    NS_NEWXPCOM(inst, _InstanceClass);                                          \
    if (NULL == inst) {                                                         \
        rv = NS_ERROR_OUT_OF_MEMORY;                                            \
        return rv;                                                              \
    }                                                                           \
    NS_ADDREF(inst);                                                            \
    rv = inst->QueryInterface(aIID, aResult);                                   \
    NS_RELEASE(inst);                                                           \
                                                                                \
    return rv;                                                                  \
}                                                                               \


#define NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(_InstanceClass, _InitMethod)        \
static NS_IMETHODIMP                                                            \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID, void **aResult) \
{                                                                               \
    nsresult rv;                                                                \
                                                                                \
    _InstanceClass * inst;                                                      \
                                                                                \
    if (NULL == aResult) {                                                      \
        rv = NS_ERROR_NULL_POINTER;                                             \
        return rv;                                                              \
    }                                                                           \
    *aResult = NULL;                                                            \
    if (NULL != aOuter) {                                                       \
        rv = NS_ERROR_NO_AGGREGATION;                                           \
        return rv;                                                              \
    }                                                                           \
                                                                                \
    NS_NEWXPCOM(inst, _InstanceClass);                                          \
    if (NULL == inst) {                                                         \
        rv = NS_ERROR_OUT_OF_MEMORY;                                            \
        return rv;                                                              \
    }                                                                           \
	rv = inst->_InitMethod();                                               \
    if(NS_FAILED(rv)) {                                                         \
        NS_DELETEXPCOM(inst);                                                   \
        return rv;                                                              \
    }                                                                           \
    NS_ADDREF(inst);                                                            \
    rv = inst->QueryInterface(aIID, aResult);                                   \
    NS_RELEASE(inst);                                                           \
                                                                                \
    return rv;                                                                  \
}                                                                               \


// if you add entries to this structure, add them at the
// END so you don't break declarations like
// { NS_MY_CID, &nsMyObjectConstructor, etc.... }
struct nsModuleComponentInfo {
    nsCID cid;
    nsIGenericFactory::ConstructorProcPtr constructor;
    const char *progid;
    const char *description;
};

#define NS_DECL_MODULE(_class)                                                \
class _class : public nsIModule {                                             \
public:                                                                       \
  _class();                                                                   \
  virtual ~_class();                                                          \
  NS_DECL_ISUPPORTS                                                           \
  NS_DECL_NSIMODULE                                                           \
};

#define NS_IMPL_MODULE_CORE(_class)                                           \
_class::_class() { NS_INIT_ISUPPORTS(); }                                     \
_class::~_class() {}

#define NS_IMPL_MODULE_GETCLASSOBJECT(_class, _table)                         \
NS_IMETHODIMP                                                                 \
_class::GetClassObject(nsIComponentManager *aCompMgr,                         \
                                const nsCID& aClass,                          \
                                const nsIID& aIID,                            \
                                void** aResult)                               \
{                                                                             \
  if (aResult == nsnull)                                                      \
    return NS_ERROR_NULL_POINTER;                                             \
                                                                              \
  *aResult = nsnull;                                                          \
                                                                              \
  nsresult rv;                                                                \
  nsIGenericFactory::ConstructorProcPtr constructor = nsnull;                 \
  nsCOMPtr<nsIGenericFactory> fact;                                           \
                                                                              \
  for (unsigned int i=0; i<(sizeof(_table) / sizeof(_table[0])); i++) {       \
    if (aClass.Equals(_table[i].cid)) {                                       \
      constructor = _table[i].constructor;                                    \
      break;                                                                  \
    }                                                                         \
  }                                                                           \
                                                                              \
  if (!constructor) return NS_ERROR_FAILURE;                                  \
  rv = NS_NewGenericFactory(getter_AddRefs(fact), constructor);               \
  if (NS_FAILED(rv)) return rv;                                               \
                                                                              \
  return fact->QueryInterface(aIID, aResult);                                 \
}

#define NS_IMPL_MODULE_REGISTERSELF(_class, _table)                           \
NS_IMETHODIMP                                                                 \
_class::RegisterSelf(nsIComponentManager *aCompMgr,                           \
                     nsIFileSpec* aPath,                                      \
                     const char* registryLocation,                            \
                     const char* componentType)                               \
{                                                                             \
    nsresult rv = NS_OK;                                                      \
    size_t i;                                                                 \
  for (i=0; i<(sizeof(_table) / sizeof(_table[0])); i++) {                    \
    rv = aCompMgr->RegisterComponentSpec(_table[i].cid,                       \
                                         _table[i].description,               \
                                         _table[i].progid,                    \
                                         aPath,                               \
                                         PR_TRUE, PR_TRUE);                   \
  }                                                                           \
  return rv;                                                                  \
}

#define NS_IMPL_MODULE_UNREGISTERSELF(_class, _table)                         \
NS_IMETHODIMP                                                                 \
_class::UnregisterSelf(nsIComponentManager *aCompMgr,                         \
                     nsIFileSpec* aPath,                                      \
                     const char* registryLocation)                            \
{                                                                             \
    nsresult rv = NS_OK; \
    size_t i; \
  for (i=0; i<(sizeof(_table) / sizeof(_table[0])); i++) {       \
    rv = aCompMgr->UnregisterComponentSpec(_table[i].cid, aPath);             \
  }                                                                           \
  return rv;                                                                  \
}

#define NS_IMPL_MODULE_CANUNLOAD(_class)                                      \
NS_IMETHODIMP                                                                 \
_class::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)          \
{                                                                             \
    if (!okToUnload)                                                          \
        return NS_ERROR_INVALID_POINTER;                                      \
                                                                              \
    *okToUnload = PR_FALSE;                                                   \
    return NS_ERROR_FAILURE;                                                  \
}

#define NS_IMPL_NSGETMODULE(_class)                                           \
static _class *g##_class;                                                     \
extern "C" NS_EXPORT nsresult                                                 \
NSGetModule(nsIComponentManager *servMgr,                                     \
            nsIFileSpec* location,                                            \
            nsIModule** aResult)                                              \
{                                                                             \
    nsresult rv = NS_OK;                                                      \
                                                                              \
    if (!aResult) return NS_ERROR_NULL_POINTER;                               \
    if (!g##_class) {                                                         \
      g##_class = new _class;                                                 \
      if (!g##_class) return NS_ERROR_OUT_OF_MEMORY;                          \
    }                                                                         \
                                                                              \
    NS_ADDREF(g##_class);                                                     \
    if (g##_class)                                                            \
      rv = g##_class->QueryInterface(NS_GET_IID(nsIModule),                   \
                                   (void **)aResult);                         \
    NS_RELEASE(g##_class);                                                    \
    return rv;                                                                \
}


#define NS_IMPL_MODULE(_module, _table) \
    NS_DECL_MODULE(_module)  \
    NS_IMPL_MODULE_CORE(_module) \
    NS_IMPL_ISUPPORTS1(_module, nsIModule) \
    NS_IMPL_MODULE_GETCLASSOBJECT(_module, _table) \
    NS_IMPL_MODULE_REGISTERSELF(_module, _table) \
    NS_IMPL_MODULE_UNREGISTERSELF(_module, _table) \
    NS_IMPL_MODULE_CANUNLOAD(_module)



// how to use the NS_IMPL_MODULE:
// define your static constructors:
// 
// NS_GENERIC_FACTORY_CONSTRUCTOR(nsMyObject1)
// NS_GENERIC_FACTORY_CONSTRUCTOR(nsMyObject2)
//
// define your array of component information:
// static nsModuleComponentInfo components[] =
// {
//   { NS_MYOBJECT1_CID, &nsMyObject1Constructor, NS_MYOBJECT1_PROGID, },
//   { NS_MYOBJECT2_CID, &nsMyObject2Constructor, NS_MYOBJECT2_PROGID, },
//   { NS_MYOBJECT2_CID, &nsMyObject2Constructor, NS_MYoBJECT2_PROGID2, },
// };
//
// NS_IMPL_MODULE(nsMyModule, components)
// NS_IMPL_NSGETMODULE(nsMyModule)



#endif /* nsIGenericFactory_h___ */
