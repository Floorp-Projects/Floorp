/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsComponentManagerUtils_h__
#define nsComponentManagerUtils_h__

#include "nscore.h"
#include "nsCOMPtr.h"

#include "nsIFactory.h"


nsresult CallCreateInstance(const nsCID& aClass, nsISupports* aDelegate,
                            const nsIID& aIID, void** aResult);

nsresult CallCreateInstance(const char* aContractID, nsISupports* aDelegate,
                            const nsIID& aIID, void** aResult);

nsresult CallGetClassObject(const nsCID& aClass, const nsIID& aIID,
                            void** aResult);

nsresult CallGetClassObject(const char* aContractID, const nsIID& aIID,
                            void** aResult);


class MOZ_STACK_CLASS nsCreateInstanceByCID final : public nsCOMPtr_helper
{
public:
  nsCreateInstanceByCID(const nsCID& aCID, nsresult* aErrorPtr)
    : mCID(aCID)
    , mErrorPtr(aErrorPtr)
  {
  }

  virtual nsresult NS_FASTCALL operator()(const nsIID&, void**) const
    override;

private:
  const nsCID&    mCID;
  nsresult*       mErrorPtr;
};

class MOZ_STACK_CLASS nsCreateInstanceByContractID final : public nsCOMPtr_helper
{
public:
  nsCreateInstanceByContractID(const char* aContractID, nsresult* aErrorPtr)
    : mContractID(aContractID)
    , mErrorPtr(aErrorPtr)
  {
  }

  virtual nsresult NS_FASTCALL operator()(const nsIID&, void**) const override;

private:
  const char*   mContractID;
  nsresult*     mErrorPtr;
};

class MOZ_STACK_CLASS nsCreateInstanceFromFactory final : public nsCOMPtr_helper
{
public:
  nsCreateInstanceFromFactory(nsIFactory* aFactory, nsresult* aErrorPtr)
    : mFactory(aFactory)
    , mErrorPtr(aErrorPtr)
  {
  }

  virtual nsresult NS_FASTCALL operator()(const nsIID&, void**) const override;

private:
  nsIFactory* MOZ_NON_OWNING_REF mFactory;
  nsresult*     mErrorPtr;
};


inline const nsCreateInstanceByCID
do_CreateInstance(const nsCID& aCID, nsresult* aError = 0)
{
  return nsCreateInstanceByCID(aCID, aError);
}

inline const nsCreateInstanceByContractID
do_CreateInstance(const char* aContractID, nsresult* aError = 0)
{
  return nsCreateInstanceByContractID(aContractID, aError);
}

inline const nsCreateInstanceFromFactory
do_CreateInstance(nsIFactory* aFactory, nsresult* aError = 0)
{
  return nsCreateInstanceFromFactory(aFactory, aError);
}


class MOZ_STACK_CLASS nsGetClassObjectByCID final : public nsCOMPtr_helper
{
public:
  nsGetClassObjectByCID(const nsCID& aCID, nsresult* aErrorPtr)
    : mCID(aCID)
    , mErrorPtr(aErrorPtr)
  {
  }

  virtual nsresult NS_FASTCALL operator()(const nsIID&, void**) const override;

private:
  const nsCID&    mCID;
  nsresult*       mErrorPtr;
};

class MOZ_STACK_CLASS nsGetClassObjectByContractID final : public nsCOMPtr_helper
{
public:
  nsGetClassObjectByContractID(const char* aContractID, nsresult* aErrorPtr)
    : mContractID(aContractID)
    , mErrorPtr(aErrorPtr)
  {
  }

  virtual nsresult NS_FASTCALL operator()(const nsIID&, void**) const override;

private:
  const char*   mContractID;
  nsresult*     mErrorPtr;
};

/**
 * do_GetClassObject can be used to improve performance of callers
 * that call |CreateInstance| many times.  They can cache the factory
 * and call do_CreateInstance or CallCreateInstance with the cached
 * factory rather than having the component manager retrieve it every
 * time.
 */
inline const nsGetClassObjectByCID
do_GetClassObject(const nsCID& aCID, nsresult* aError = 0)
{
  return nsGetClassObjectByCID(aCID, aError);
}

inline const nsGetClassObjectByContractID
do_GetClassObject(const char* aContractID, nsresult* aError = 0)
{
  return nsGetClassObjectByContractID(aContractID, aError);
}

// type-safe shortcuts for calling |CreateInstance|
template<class DestinationType>
inline nsresult
CallCreateInstance(const nsCID& aClass,
                   nsISupports* aDelegate,
                   DestinationType** aDestination)
{
  MOZ_ASSERT(aDestination, "null parameter");

  return CallCreateInstance(aClass, aDelegate,
                            NS_GET_TEMPLATE_IID(DestinationType),
                            reinterpret_cast<void**>(aDestination));
}

template<class DestinationType>
inline nsresult
CallCreateInstance(const nsCID& aClass, DestinationType** aDestination)
{
  MOZ_ASSERT(aDestination, "null parameter");

  return CallCreateInstance(aClass, nullptr,
                            NS_GET_TEMPLATE_IID(DestinationType),
                            reinterpret_cast<void**>(aDestination));
}

template<class DestinationType>
inline nsresult
CallCreateInstance(const char* aContractID,
                   nsISupports* aDelegate,
                   DestinationType** aDestination)
{
  MOZ_ASSERT(aContractID, "null parameter");
  MOZ_ASSERT(aDestination, "null parameter");

  return CallCreateInstance(aContractID,
                            aDelegate,
                            NS_GET_TEMPLATE_IID(DestinationType),
                            reinterpret_cast<void**>(aDestination));
}

template<class DestinationType>
inline nsresult
CallCreateInstance(const char* aContractID, DestinationType** aDestination)
{
  MOZ_ASSERT(aContractID, "null parameter");
  MOZ_ASSERT(aDestination, "null parameter");

  return CallCreateInstance(aContractID, nullptr,
                            NS_GET_TEMPLATE_IID(DestinationType),
                            reinterpret_cast<void**>(aDestination));
}

template<class DestinationType>
inline nsresult
CallCreateInstance(nsIFactory* aFactory,
                   nsISupports* aDelegate,
                   DestinationType** aDestination)
{
  MOZ_ASSERT(aFactory, "null parameter");
  MOZ_ASSERT(aDestination, "null parameter");

  return aFactory->CreateInstance(aDelegate,
                                  NS_GET_TEMPLATE_IID(DestinationType),
                                  reinterpret_cast<void**>(aDestination));
}

template<class DestinationType>
inline nsresult
CallCreateInstance(nsIFactory* aFactory, DestinationType** aDestination)
{
  MOZ_ASSERT(aFactory, "null parameter");
  MOZ_ASSERT(aDestination, "null parameter");

  return aFactory->CreateInstance(nullptr,
                                  NS_GET_TEMPLATE_IID(DestinationType),
                                  reinterpret_cast<void**>(aDestination));
}

template<class DestinationType>
inline nsresult
CallGetClassObject(const nsCID& aClass, DestinationType** aDestination)
{
  MOZ_ASSERT(aDestination, "null parameter");

  return CallGetClassObject(aClass, NS_GET_TEMPLATE_IID(DestinationType),
                            reinterpret_cast<void**>(aDestination));
}

template<class DestinationType>
inline nsresult
CallGetClassObject(const char* aContractID, DestinationType** aDestination)
{
  MOZ_ASSERT(aDestination, "null parameter");

  return CallGetClassObject(aContractID, NS_GET_TEMPLATE_IID(DestinationType),
                            reinterpret_cast<void**>(aDestination));
}

#endif /* nsComponentManagerUtils_h__ */
