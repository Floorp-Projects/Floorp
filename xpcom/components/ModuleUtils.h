/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GenericModule_h
#define mozilla_GenericModule_h

#include "mozilla/Attributes.h"
#include "mozilla/Module.h"

#define NS_GENERIC_FACTORY_CONSTRUCTOR(_InstanceClass)                        \
static nsresult                                                               \
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
    inst = new _InstanceClass();                                              \
    if (NULL == inst) {                                                       \
        rv = NS_ERROR_OUT_OF_MEMORY;                                          \
        return rv;                                                            \
    }                                                                         \
    NS_ADDREF(inst);                                                          \
    rv = inst->QueryInterface(aIID, aResult);                                 \
    NS_RELEASE(inst);                                                         \
                                                                              \
    return rv;                                                                \
}

#define NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(_InstanceClass, _InitMethod)      \
static nsresult                                                               \
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
    inst = new _InstanceClass();                                              \
    if (NULL == inst) {                                                       \
        rv = NS_ERROR_OUT_OF_MEMORY;                                          \
        return rv;                                                            \
    }                                                                         \
    NS_ADDREF(inst);                                                          \
    rv = inst->_InitMethod();                                                 \
    if(NS_SUCCEEDED(rv)) {                                                    \
        rv = inst->QueryInterface(aIID, aResult);                             \
    }                                                                         \
    NS_RELEASE(inst);                                                         \
                                                                              \
    return rv;                                                                \
}

// 'Constructor' that uses an existing getter function that gets a singleton.
// NOTE: assumes that getter does an AddRef - so additional AddRef is not done.
#define NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(_InstanceClass, _GetterProc) \
static nsresult                                                               \
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
    inst = already_AddRefed<_InstanceClass>(_GetterProc()).get();             \
    if (NULL == inst) {                                                       \
        rv = NS_ERROR_OUT_OF_MEMORY;                                          \
        return rv;                                                            \
    }                                                                         \
    /* NS_ADDREF(inst); */                                                    \
    rv = inst->QueryInterface(aIID, aResult);                                 \
    NS_RELEASE(inst);                                                         \
                                                                              \
    return rv;                                                                \
}

#ifndef MOZILLA_INTERNAL_API

#include "nsIModule.h"
#include "nsISupportsUtils.h"

namespace mozilla {

class GenericModule MOZ_FINAL : public nsIModule
{
public:
    GenericModule(const mozilla::Module* aData)
        : mData(aData)
    {
    }

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIMODULE

private:
    const mozilla::Module* mData;
};

} // namespace mozilla

#define NS_IMPL_MOZILLA192_NSGETMODULE(module)     \
extern "C" NS_EXPORT nsresult                      \
NSGetModule(nsIComponentManager* aCompMgr,         \
            nsIFile* aLocation,                    \
            nsIModule** aResult)                   \
{                                                  \
    *aResult = new mozilla::GenericModule(module); \
    NS_ADDREF(*aResult);                           \
    return NS_OK;                                  \
}

#endif

#endif // mozilla_GenericModule_h
