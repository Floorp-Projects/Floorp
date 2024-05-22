/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/DNSRequestParent.h"
#include "mozilla/net/DNSRequestChild.h"
#include "nsIDNSService.h"
#include "nsNetCID.h"
#include "nsThreadUtils.h"
#include "nsICancelable.h"
#include "nsIDNSRecord.h"
#include "nsHostResolver.h"
#include "mozilla/Unused.h"
#include "DNSAdditionalInfo.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// DNSRequestHandler::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(DNSRequestHandler, nsIDNSListener)

static void SendLookupCompletedHelper(DNSRequestActor* aActor,
                                      const DNSRequestResponse& aReply) {
  if (DNSRequestParent* parent = aActor->AsDNSRequestParent()) {
    Unused << parent->SendLookupCompleted(aReply);
  } else if (DNSRequestChild* child = aActor->AsDNSRequestChild()) {
    Unused << child->SendLookupCompleted(aReply);
  }
}

void DNSRequestHandler::DoAsyncResolve(const nsACString& hostname,
                                       const nsACString& trrServer,
                                       int32_t port, uint16_t type,
                                       const OriginAttributes& originAttributes,
                                       nsIDNSService::DNSFlags flags) {
  nsresult rv;
  mFlags = flags;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIEventTarget> main = GetMainThreadSerialEventTarget();
    nsCOMPtr<nsICancelable> unused;
    RefPtr<DNSAdditionalInfo> info;
    if (!trrServer.IsEmpty() || port != -1) {
      info = new DNSAdditionalInfo(trrServer, port);
    }
    rv = dns->AsyncResolveNative(
        hostname, static_cast<nsIDNSService::ResolveType>(type), flags, info,
        this, main, originAttributes, getter_AddRefs(unused));
  }

  if (NS_FAILED(rv) && mIPCActor->CanSend()) {
    SendLookupCompletedHelper(mIPCActor, DNSRequestResponse(rv));
  }
}

void DNSRequestHandler::OnRecvCancelDNSRequest(
    const nsCString& hostName, const nsCString& aTrrServer, const int32_t& port,
    const uint16_t& type, const OriginAttributes& originAttributes,
    const nsIDNSService::DNSFlags& flags, const nsresult& reason) {
  nsresult rv;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    RefPtr<DNSAdditionalInfo> info;
    if (!aTrrServer.IsEmpty() || port != -1) {
      info = new DNSAdditionalInfo(aTrrServer, port);
    }
    rv = dns->CancelAsyncResolveNative(
        hostName, static_cast<nsIDNSService::ResolveType>(type), flags, info,
        this, reason, originAttributes);
  }
}

bool DNSRequestHandler::OnRecvLookupCompleted(const DNSRequestResponse& reply) {
  return true;
}

//-----------------------------------------------------------------------------
// nsIDNSListener functions
//-----------------------------------------------------------------------------

NS_IMETHODIMP
DNSRequestHandler::OnLookupComplete(nsICancelable* request,
                                    nsIDNSRecord* aRecord, nsresult status) {
  if (!mIPCActor || !mIPCActor->CanSend()) {
    // nothing to do: child probably crashed
    return NS_OK;
  }

  if (NS_SUCCEEDED(status)) {
    MOZ_ASSERT(aRecord);

    nsCOMPtr<nsIDNSByTypeRecord> byTypeRec = do_QueryInterface(aRecord);
    if (byTypeRec) {
      IPCTypeRecord result;
      byTypeRec->GetResults(&result.mData);
      if (nsCOMPtr<nsIDNSHTTPSSVCRecord> rec = do_QueryInterface(aRecord)) {
        rec->GetTtl(&result.mTTL);
      }
      SendLookupCompletedHelper(mIPCActor, DNSRequestResponse(result));
      return NS_OK;
    }

    nsCOMPtr<nsIDNSAddrRecord> rec = do_QueryInterface(aRecord);
    MOZ_ASSERT(rec);
    nsAutoCString cname;
    if (mFlags & nsHostResolver::RES_CANON_NAME) {
      rec->GetCanonicalName(cname);
    }

    // Get IP addresses for hostname (use port 80 as dummy value for NetAddr)
    nsTArray<NetAddr> array;
    NetAddr addr;
    while (NS_SUCCEEDED(rec->GetNextAddr(80, &addr))) {
      array.AppendElement(addr);
    }

    double trrFetchDuration;
    rec->GetTrrFetchDuration(&trrFetchDuration);

    double trrFetchDurationNetworkOnly;
    rec->GetTrrFetchDurationNetworkOnly(&trrFetchDurationNetworkOnly);

    bool isTRR = false;
    rec->IsTRR(&isTRR);

    nsIRequest::TRRMode effectiveTRRMode = nsIRequest::TRR_DEFAULT_MODE;
    rec->GetEffectiveTRRMode(&effectiveTRRMode);

    uint32_t ttl = 0;
    rec->GetTtl(&ttl);

    SendLookupCompletedHelper(
        mIPCActor, DNSRequestResponse(DNSRecord(cname, array, trrFetchDuration,
                                                trrFetchDurationNetworkOnly,
                                                isTRR, effectiveTRRMode, ttl)));
  } else {
    SendLookupCompletedHelper(mIPCActor, DNSRequestResponse(status));
  }

  return NS_OK;
}

void DNSRequestHandler::OnIPCActorDestroy() { mIPCActor = nullptr; }

//-----------------------------------------------------------------------------
// DNSRequestParent functions
//-----------------------------------------------------------------------------

DNSRequestParent::DNSRequestParent(DNSRequestBase* aRequest)
    : DNSRequestActor(aRequest) {
  aRequest->SetIPCActor(this);
}

mozilla::ipc::IPCResult DNSRequestParent::RecvCancelDNSRequest(
    const nsCString& hostName, const nsCString& trrServer, const int32_t& port,
    const uint16_t& type, const OriginAttributes& originAttributes,
    const nsIDNSService::DNSFlags& flags, const nsresult& reason) {
  mDNSRequest->OnRecvCancelDNSRequest(hostName, trrServer, port, type,
                                      originAttributes, flags, reason);
  return IPC_OK();
}

mozilla::ipc::IPCResult DNSRequestParent::RecvLookupCompleted(
    const DNSRequestResponse& reply) {
  return mDNSRequest->OnRecvLookupCompleted(reply) ? IPC_OK()
                                                   : IPC_FAIL_NO_REASON(this);
}

void DNSRequestParent::ActorDestroy(ActorDestroyReason) {
  mDNSRequest->OnIPCActorDestroy();
  mDNSRequest = nullptr;
}

}  // namespace net
}  // namespace mozilla
