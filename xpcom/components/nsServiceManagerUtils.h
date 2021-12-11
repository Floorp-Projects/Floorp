/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsServiceManagerUtils_h__
#define nsServiceManagerUtils_h__

#include "nsCOMPtr.h"
#include "nsString.h"

inline nsGetServiceByCID do_GetService(const nsCID& aCID) {
  return nsGetServiceByCID(aCID);
}

inline nsGetServiceByCIDWithError do_GetService(const nsCID& aCID,
                                                nsresult* aError) {
  return nsGetServiceByCIDWithError(aCID, aError);
}

inline nsGetServiceByContractID do_GetService(const char* aContractID) {
  return nsGetServiceByContractID(aContractID);
}

inline nsGetServiceByContractIDWithError do_GetService(const char* aContractID,
                                                       nsresult* aError) {
  return nsGetServiceByContractIDWithError(aContractID, aError);
}

nsresult CallGetService(const nsCID& aClass, const nsIID& aIID, void** aResult);

nsresult CallGetService(const char* aContractID, const nsIID& aIID,
                        void** aResult);

// type-safe shortcuts for calling |GetService|
template <class DestinationType>
inline nsresult CallGetService(const nsCID& aClass,
                               DestinationType** aDestination) {
  MOZ_ASSERT(aDestination, "null parameter");

  return CallGetService(aClass, NS_GET_TEMPLATE_IID(DestinationType),
                        reinterpret_cast<void**>(aDestination));
}

template <class DestinationType>
inline nsresult CallGetService(const char* aContractID,
                               DestinationType** aDestination) {
  MOZ_ASSERT(aContractID, "null parameter");
  MOZ_ASSERT(aDestination, "null parameter");

  return CallGetService(aContractID, NS_GET_TEMPLATE_IID(DestinationType),
                        reinterpret_cast<void**>(aDestination));
}

#endif
