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
#define NS_GENERICFACTORY_CID                                                 \
  { 0x3bc97f01, 0xccdf, 0x11d2,                                               \
    { 0xba, 0xb8, 0xb5, 0x48, 0x65, 0x44, 0x61, 0xfc } }

// {3bc97f00-ccdf-11d2-bab8-b548654461fc}
#define NS_IGENERICFACTORY_IID                                                \
  { 0x3bc97f00, 0xccdf, 0x11d2,                                               \
    { 0xba, 0xb8, 0xb5, 0x48, 0x65, 0x44, 0x61, 0xfc } }

#define NS_GENERICFACTORY_CONTRACTID "@mozilla.org/generic-factory;1"
#define NS_GENERICFACTORY_CLASSNAME "Generic Factory"

struct nsModuleComponentInfo; // forward declaration

/**
 * Provides a Generic nsIFactory implementation that can be used by
 * DLLs with very simple factory needs.
 */
class nsIGenericFactory : public nsIFactory {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IGENERICFACTORY_IID)
    
    NS_IMETHOD SetComponentInfo(nsModuleComponentInfo *info) = 0;
    NS_IMETHOD GetComponentInfo(nsModuleComponentInfo **infop) = 0;
};

extern NS_COM nsresult
NS_NewGenericFactory(nsIGenericFactory **result,
                     nsModuleComponentInfo *info);


////////////////////////////////////////////////////////////////////////////////
// Generic Modules
// 
// (See xpcom/sample/nsSampleModule.cpp to see how to use this.)

#include "nsIModule.h"

typedef NS_CALLBACK(NSConstructorProcPtr)(nsISupports *aOuter, REFNSIID aIID,
                                          void **aResult);

typedef NS_CALLBACK(NSRegisterSelfProcPtr)(nsIComponentManager *aCompMgr,
                                           nsIFile *aPath,
                                           const char *aRegistryLocation,
                                           const char *aComponentType,
                                           const nsModuleComponentInfo *aInfo);

typedef NS_CALLBACK(NSUnregisterSelfProcPtr)(nsIComponentManager *aCompMgr,
                                             nsIFile *aPath,
                                             const char *aRegistryLocation,
                                             const nsModuleComponentInfo *aInfo);

typedef NS_CALLBACK(NSFactoryDestructorProcPtr)(void);

typedef NS_CALLBACK(NSGetInterfacesProcPtr)(PRUint32 *countp,
                                            nsIID* **array);

typedef NS_CALLBACK(NSGetLanguageHelperProcPtr)(PRUint32 language,
                                                nsISupports **helper);

/**
 * Use this type to define a list of module component info to pass to 
 * NS_NewGenericModule. E.g.:
 *     static nsModuleComponentInfo components[] = { ... };
 * See xpcom/sample/nsSampleModule.cpp for more info.
 */
struct nsModuleComponentInfo {
    const char*                                 mDescription;
    nsCID                                       mCID;
    const char*                                 mContractID;
    NSConstructorProcPtr                        mConstructor;
    NSRegisterSelfProcPtr                       mRegisterSelfProc;
    NSUnregisterSelfProcPtr                     mUnregisterSelfProc;
    NSFactoryDestructorProcPtr                  mFactoryDestructor;
    NSGetInterfacesProcPtr                      mGetInterfacesProc;
    NSGetLanguageHelperProcPtr                  mGetLanguageHelperProc;
    nsIClassInfo **                             mClassInfoGlobal;
    PRUint32                                    mFlags;
};

typedef void (PR_CALLBACK *nsModuleDestructorProc) (nsIModule *self);
typedef nsresult (PR_CALLBACK *nsModuleConstructorProc) (nsIModule *self);

extern NS_COM nsresult
NS_NewGenericModule(const char* moduleName,
                    PRUint32 componentCount,
                    nsModuleComponentInfo* components,
                    nsModuleConstructorProc ctor,
                    nsModuleDestructorProc dtor,
                    nsIModule* *result);

#if defined(XPCOM_TRANSLATE_NSGM_ENTRY_POINT)
#  define NSGETMODULE_ENTRY_POINT(_name) _name##_NSGetModule
#  define NSGETMODULE_COMPONENTS(_name) _name##_NSGM_comps
#  define NSGETMODULE_COMPONENTS_COUNT(_name) _name##_NSGM_comp_count
#else
#  define NSGETMODULE_ENTRY_POINT(_name) NSGetModule
#  define NSGETMODULE_COMPONENTS(_name) NSGetModule_components
#  define NSGETMODULE_COMPONENTS_COUNT(_name) NSGetModule_components_count
#endif

#define NS_IMPL_NSGETMODULE(_name, _components)                               \
    NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(_name, _components, nsnull, nsnull)

#define NS_IMPL_NSGETMODULE_WITH_CTOR(_name, _components, _ctor)              \
    NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(_name, _components, _ctor, nsnull)

#define NS_IMPL_NSGETMODULE_WITH_DTOR(_name, _components, _dtor)              \
    NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(_name, _components, nsnull, _dtor)

#define NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(_name, _components, _ctor, _dtor)  \
                                                                              \
PRUint32                                                                      \
   NSGETMODULE_COMPONENTS_COUNT(_name) =                                      \
           sizeof(_components) / sizeof(_components[0]);                      \
                                                                              \
                                                                              \
nsModuleComponentInfo* NSGETMODULE_COMPONENTS(_name) = (_components);         \
                                                                              \
extern "C" NS_EXPORT nsresult                                                 \
NSGETMODULE_ENTRY_POINT(_name) (nsIComponentManager *servMgr,                 \
                                nsIFile* location,                            \
                                nsIModule** result)                           \
{                                                                             \
    return NS_NewGenericModule((#_name),                                      \
                               NSGETMODULE_COMPONENTS_COUNT(_name),           \
                               NSGETMODULE_COMPONENTS(_name),                 \
                               _ctor, _dtor, result);                         \
}

////////////////////////////////////////////////////////////////////////////////

#define NS_GENERIC_FACTORY_CONSTRUCTOR(_InstanceClass)                        \
static NS_IMETHODIMP                                                          \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID,               \
                            void **aResult)                                   \
{                                                                             \
    nsresult rv;                                                              \
                                                                              \
    _InstanceClass * inst;                                                    \
                                                                              \
    *aResult = NULL;                                                          \
    if (NULL != aOuter) {                                                     \
        rv = NS_ERROR_NO_AGGREGATION;                                         \
        return rv;                                                            \
    }                                                                         \
                                                                              \
    NS_NEWXPCOM(inst, _InstanceClass);                                        \
    if (NULL == inst) {                                                       \
        rv = NS_ERROR_OUT_OF_MEMORY;                                          \
        return rv;                                                            \
    }                                                                         \
    NS_ADDREF(inst);                                                          \
    rv = inst->QueryInterface(aIID, aResult);                                 \
    NS_RELEASE(inst);                                                         \
                                                                              \
    return rv;                                                                \
}                                                                             \


#define NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(_InstanceClass, _InitMethod)      \
static NS_IMETHODIMP                                                          \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID,               \
                            void **aResult)                                   \
{                                                                             \
    nsresult rv;                                                              \
                                                                              \
    _InstanceClass * inst;                                                    \
                                                                              \
    *aResult = NULL;                                                          \
    if (NULL != aOuter) {                                                     \
        rv = NS_ERROR_NO_AGGREGATION;                                         \
        return rv;                                                            \
    }                                                                         \
                                                                              \
    NS_NEWXPCOM(inst, _InstanceClass);                                        \
    if (NULL == inst) {                                                       \
        rv = NS_ERROR_OUT_OF_MEMORY;                                          \
        return rv;                                                            \
    }                                                                         \
    NS_ADDREF(inst);                                                          \
	rv = inst->_InitMethod();                                             \
    if(NS_SUCCEEDED(rv)) {                                                    \
        rv = inst->QueryInterface(aIID, aResult);                             \
    }                                                                         \
    NS_RELEASE(inst);                                                         \
                                                                              \
    return rv;                                                                \
}                                                                             \

// 'Constructor' that uses an existing getter function that gets a singleton.
// NOTE: assumes that getter does an AddRef - so additional AddRef is not done.
#define NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(_InstanceClass, _GetterProc) \
static NS_IMETHODIMP                                                          \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID,               \
                            void **aResult)                                   \
{                                                                             \
    nsresult rv;                                                              \
                                                                              \
    _InstanceClass * inst;                                                    \
                                                                              \
    *aResult = NULL;                                                          \
    if (NULL != aOuter) {                                                     \
        rv = NS_ERROR_NO_AGGREGATION;                                         \
        return rv;                                                            \
    }                                                                         \
                                                                              \
    inst = _GetterProc();                                                     \
    if (NULL == inst) {                                                       \
        rv = NS_ERROR_OUT_OF_MEMORY;                                          \
        return rv;                                                            \
    }                                                                         \
    /* NS_ADDREF(inst); */                                                    \
    rv = inst->QueryInterface(aIID, aResult);                                 \
    NS_RELEASE(inst);                                                         \
                                                                              \
    return rv;                                                                \
}                                                                             \

#endif /* nsIGenericFactory_h___ */
