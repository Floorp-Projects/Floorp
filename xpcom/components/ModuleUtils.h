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
    nsRefPtr<_InstanceClass> inst;                                            \
                                                                              \
    *aResult = nullptr;                                                       \
    if (nullptr != aOuter) {                                                  \
        return NS_ERROR_NO_AGGREGATION;                                       \
    }                                                                         \
                                                                              \
    inst = new _InstanceClass();                                              \
    return inst->QueryInterface(aIID, aResult);                               \
}

#define NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(_InstanceClass, _InitMethod)      \
static nsresult                                                               \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID,               \
                            void **aResult)                                   \
{                                                                             \
    nsresult rv;                                                              \
                                                                              \
    nsRefPtr<_InstanceClass> inst;                                            \
                                                                              \
    *aResult = nullptr;                                                       \
    if (nullptr != aOuter) {                                                  \
        return NS_ERROR_NO_AGGREGATION;                                       \
    }                                                                         \
                                                                              \
    inst = new _InstanceClass();                                              \
    rv = inst->_InitMethod();                                                 \
    if(NS_SUCCEEDED(rv)) {                                                    \
        rv = inst->QueryInterface(aIID, aResult);                             \
    }                                                                         \
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
    nsRefPtr<_InstanceClass> inst;                                            \
                                                                              \
    *aResult = nullptr;                                                       \
    if (nullptr != aOuter) {                                                  \
        return NS_ERROR_NO_AGGREGATION;                                       \
    }                                                                         \
                                                                              \
    inst = already_AddRefed<_InstanceClass>(_GetterProc());                   \
    if (nullptr == inst) {                                                    \
        return NS_ERROR_OUT_OF_MEMORY;                                        \
    }                                                                         \
    return inst->QueryInterface(aIID, aResult);                               \
}

#ifndef MOZILLA_INTERNAL_API

#include "nsIModule.h"
#include "nsISupportsUtils.h"

namespace mozilla {

class GenericModule final : public nsIModule
{
  ~GenericModule() {}

public:
  explicit GenericModule(const mozilla::Module* aData) : mData(aData) {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMODULE

private:
  const mozilla::Module* mData;
};

} // namespace mozilla

#endif

#endif // mozilla_GenericModule_h
