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
#include "nsIDNSByTypeRecord.h"
#include "nsIEventTarget.h"

namespace mozilla {
namespace net {

class DNSRequestChild final
  : public PDNSRequestChild
  , public nsICancelable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICANCELABLE

  DNSRequestChild(const nsACString& aHost,
                  const uint16_t& aType,
                  const OriginAttributes& aOriginAttributes,
                  const uint32_t& aFlags,
                  nsIDNSListener *aListener, nsIEventTarget *target);

  void AddIPDLReference() {
    AddRef();
  }
  void ReleaseIPDLReference();

  // Sends IPDL request to parent
  void StartRequest();
  void CallOnLookupComplete();
  void CallOnLookupByTypeComplete();

protected:
  friend class CancelDNSRequestEvent;
  friend class ChildDNSService;
  virtual ~DNSRequestChild() {}

  virtual mozilla::ipc::IPCResult RecvLookupCompleted(const DNSRequestResponse& reply) override;
  virtual void ActorDestroy(ActorDestroyReason why) override;

  nsCOMPtr<nsIDNSListener>     mListener;
  nsCOMPtr<nsIEventTarget>     mTarget;
  nsCOMPtr<nsIDNSRecord>       mResultRecord;
  nsCOMPtr<nsIDNSByTypeRecord> mResultByTypeRecords; // the result of a by-type
                                                     // query (mType must not be
                                                     // equal to
                                                     // nsIDNSService::RESOLVE_TYPE_DEFAULT
                                                     // (this is reserved for
                                                     // the standard A/AAAA query)).
  nsresult                     mResultStatus;
  nsCString                    mHost;
  uint16_t                     mType;
  const OriginAttributes       mOriginAttributes;
  uint16_t                     mFlags;
  bool                         mIPCOpen;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_DNSRequestChild_h
