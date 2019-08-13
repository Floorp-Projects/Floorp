/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FuzzySecurityInfo_h__
#define FuzzySecurityInfo_h__

#include "prerror.h"
#include "sslproto.h"
#include "sslt.h"

#include "nsCOMPtr.h"
#include "nsISSLSocketControl.h"
#include "nsIInterfaceRequestor.h"
#include "nsITransportSecurityInfo.h"

namespace mozilla {
namespace net {

class FuzzySecurityInfo final : public nsITransportSecurityInfo,
                                public nsIInterfaceRequestor,
                                public nsISSLSocketControl {

  public:
    FuzzySecurityInfo();

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSITRANSPORTSECURITYINFO
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSISSLSOCKETCONTROL


  protected:
    virtual ~FuzzySecurityInfo();

  private:
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;

};  // class FuzzySecurityInfo

}  // namespace net
}  // namespace mozilla

#endif  // FuzzySecurityInfo_h__
