/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DNSRequestChild_h
#define mozilla_net_DNSRequestChild_h

#include "mozilla/net/PDNSRequestChild.h"
#include "nsICancelable.h"
#include "nsIDNSRecord.h"
#include "nsIDNSListener.h"
#include "nsIEventTarget.h"

namespace mozilla {
namespace net {

class DNSRequestChild
  : public PDNSRequestChild
  , public nsICancelable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICANCELABLE

  DNSRequestChild(const nsCString& aHost, const uint32_t& aFlags,
                  nsIDNSListener *aListener, nsIEventTarget *target);

  void AddIPDLReference() {
    AddRef();
  }
  void ReleaseIPDLReference() {
    // we don't need an 'mIPCOpen' variable until/unless we add calls that might
    // try to send IPDL msgs to parent after ReleaseIPDLReference is called
    // (when IPDL channel torn down).
    Release();
  }

  // Sends IPDL request to parent
  void StartRequest();
  void CallOnLookupComplete();

private:
  virtual ~DNSRequestChild() {}

  virtual bool Recv__delete__(const DNSRequestResponse& reply) MOZ_OVERRIDE;

  nsCOMPtr<nsIDNSListener>  mListener;
  nsCOMPtr<nsIEventTarget>  mTarget;
  nsCOMPtr<nsIDNSRecord>    mResultRecord;
  nsresult                  mResultStatus;
  nsCString                 mHost;
  uint16_t                  mFlags;
};

} // namespace net
} // namespace mozilla
#endif // mozilla_net_DNSRequestChild_h
