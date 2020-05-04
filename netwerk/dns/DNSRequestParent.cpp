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

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

DNSRequestHandler::DNSRequestHandler() : mFlags(0) {}

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
                                       uint16_t type,
                                       const OriginAttributes& originAttributes,
                                       uint32_t flags) {
  nsresult rv;
  mFlags = flags;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIEventTarget> main = GetMainThreadEventTarget();
    nsCOMPtr<nsICancelable> unused;
    if (type != nsIDNSService::RESOLVE_TYPE_DEFAULT) {
      rv = dns->AsyncResolveByTypeNative(hostname, type, flags, this, main,
                                         originAttributes,
                                         getter_AddRefs(unused));
    } else if (trrServer.IsEmpty()) {
      rv = dns->AsyncResolveNative(hostname, flags, this, main,
                                   originAttributes, getter_AddRefs(unused));
    } else {
      rv = dns->AsyncResolveWithTrrServerNative(hostname, trrServer, flags,
                                                this, main, originAttributes,
                                                getter_AddRefs(unused));
    }
  }

  if (NS_FAILED(rv) && mIPCActor->CanSend()) {
    SendLookupCompletedHelper(mIPCActor, DNSRequestResponse(rv));
  }
}

void DNSRequestHandler::OnRecvCancelDNSRequest(
    const nsCString& hostName, const nsCString& aTrrServer,
    const uint16_t& type, const OriginAttributes& originAttributes,
    const uint32_t& flags, const nsresult& reason) {
  nsresult rv;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    if (type != nsIDNSService::RESOLVE_TYPE_DEFAULT) {
      rv = dns->CancelAsyncResolveByTypeNative(hostName, type, flags, this,
                                               reason, originAttributes);
    } else if (aTrrServer.IsEmpty()) {
      rv = dns->CancelAsyncResolveNative(hostName, flags, this, reason,
                                         originAttributes);
    } else {
      rv = dns->CancelAsyncResolveWithTrrServerNative(
          hostName, aTrrServer, flags, this, reason, originAttributes);
    }
  }
}

bool DNSRequestHandler::OnRecvLookupCompleted(const DNSRequestResponse& reply) {
  return true;
}

//-----------------------------------------------------------------------------
// nsIDNSListener functions
//-----------------------------------------------------------------------------

NS_IMETHODIMP
DNSRequestHandler::OnLookupComplete(nsICancelable* request, nsIDNSRecord* rec,
                                    nsresult status) {
  if (!mIPCActor || !mIPCActor->CanSend()) {
    // nothing to do: child probably crashed
    return NS_OK;
  }

  if (NS_SUCCEEDED(status)) {
    MOZ_ASSERT(rec);

    nsCOMPtr<nsIDNSByTypeRecord> byTypeRec = do_QueryInterface(rec);
    if (byTypeRec) {
      IPCTypeRecord result;
      byTypeRec->GetResults(&result.mData);
      SendLookupCompletedHelper(mIPCActor, DNSRequestResponse(result));
      return NS_OK;
    }

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

    SendLookupCompletedHelper(mIPCActor,
                              DNSRequestResponse(DNSRecord(cname, array)));
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
    const nsCString& hostName, const nsCString& trrServer, const uint16_t& type,
    const OriginAttributes& originAttributes, const uint32_t& flags,
    const nsresult& reason) {
  mDNSRequest->OnRecvCancelDNSRequest(hostName, trrServer, type,
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
