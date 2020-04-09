/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/net/ChildDNSService.h"
#include "mozilla/net/DNSRequestChild.h"
#include "mozilla/net/DNSRequestParent.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/net/SocketProcessParent.h"
#include "mozilla/Unused.h"
#include "nsIDNSRecord.h"
#include "nsIDNSByTypeRecord.h"
#include "nsHostResolver.h"
#include "nsIOService.h"
#include "nsTArray.h"
#include "nsNetAddr.h"
#include "nsThreadUtils.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// ChildDNSRecord:
// A simple class to provide nsIDNSRecord on the child
//-----------------------------------------------------------------------------

class ChildDNSRecord : public nsIDNSRecord {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSRECORD

  ChildDNSRecord(const DNSRecord& reply, uint16_t flags);

 private:
  virtual ~ChildDNSRecord() = default;

  nsCString mCanonicalName;
  nsTArray<NetAddr> mAddresses;
  uint32_t mCurrent;  // addr iterator
  uint16_t mFlags;
};

NS_IMPL_ISUPPORTS(ChildDNSRecord, nsIDNSRecord)

ChildDNSRecord::ChildDNSRecord(const DNSRecord& reply, uint16_t flags)
    : mCurrent(0), mFlags(flags) {
  mCanonicalName = reply.canonicalName();

  // A shame IPDL gives us no way to grab ownership of array: so copy it.
  const nsTArray<NetAddr>& addrs = reply.addrs();
  mAddresses = addrs;
}

//-----------------------------------------------------------------------------
// ChildDNSRecord::nsIDNSRecord
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ChildDNSRecord::GetCanonicalName(nsACString& result) {
  if (!(mFlags & nsHostResolver::RES_CANON_NAME)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  result = mCanonicalName;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSRecord::IsTRR(bool* retval) {
  *retval = false;
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
ChildDNSRecord::GetTrrFetchDuration(double* aTime) {
  *aTime = 0;
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
ChildDNSRecord::GetTrrFetchDurationNetworkOnly(double* aTime) {
  *aTime = 0;
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
ChildDNSRecord::GetNextAddr(uint16_t port, NetAddr* addr) {
  if (mCurrent >= mAddresses.Length()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  memcpy(addr, &mAddresses[mCurrent++], sizeof(NetAddr));

  // both Ipv4/6 use same bits for port, so safe to just use ipv4's field
  addr->inet.port = htons(port);

  return NS_OK;
}

NS_IMETHODIMP
ChildDNSRecord::GetAddresses(nsTArray<NetAddr>& aAddressArray) {
  aAddressArray = mAddresses;
  return NS_OK;
}

// shamelessly copied from nsDNSRecord
NS_IMETHODIMP
ChildDNSRecord::GetScriptableNextAddr(uint16_t port, nsINetAddr** result) {
  NetAddr addr;
  nsresult rv = GetNextAddr(port, &addr);
  if (NS_FAILED(rv)) return rv;

  RefPtr<nsNetAddr> netaddr = new nsNetAddr(&addr);
  netaddr.forget(result);

  return NS_OK;
}

// also copied from nsDNSRecord
NS_IMETHODIMP
ChildDNSRecord::GetNextAddrAsString(nsACString& result) {
  NetAddr addr;
  nsresult rv = GetNextAddr(0, &addr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  char buf[kIPv6CStrBufSize];
  if (NetAddrToString(&addr, buf, sizeof(buf))) {
    result.Assign(buf);
    return NS_OK;
  }
  NS_ERROR("NetAddrToString failed unexpectedly");
  return NS_ERROR_FAILURE;  // conversion failed for some reason
}

NS_IMETHODIMP
ChildDNSRecord::HasMore(bool* result) {
  *result = mCurrent < mAddresses.Length();
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSRecord::Rewind() {
  mCurrent = 0;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSRecord::ReportUnusable(uint16_t aPort) {
  // "We thank you for your feedback" == >/dev/null
  // TODO: we could send info back to parent.
  return NS_OK;
}

class ChildDNSByTypeRecord : public nsIDNSByTypeRecord {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_SAFE_NSIDNSRECORD(((nsIDNSRecord*)nullptr))
  NS_DECL_NSIDNSBYTYPERECORD

  explicit ChildDNSByTypeRecord(const nsTArray<nsCString>& reply);

 private:
  virtual ~ChildDNSByTypeRecord() = default;

  nsTArray<nsCString> mRecords;
};

NS_IMPL_ISUPPORTS(ChildDNSByTypeRecord, nsIDNSByTypeRecord, nsIDNSRecord)

ChildDNSByTypeRecord::ChildDNSByTypeRecord(const nsTArray<nsCString>& reply) {
  mRecords = reply;
}

NS_IMETHODIMP
ChildDNSByTypeRecord::GetRecords(nsTArray<nsCString>& aRecords) {
  aRecords = mRecords;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSByTypeRecord::GetRecordsAsOneString(nsACString& aRecords) {
  // deep copy
  for (uint32_t i = 0; i < mRecords.Length(); i++) {
    aRecords.Append(mRecords[i]);
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// DNSRequestSender
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(DNSRequestSender, nsICancelable)

DNSRequestSender::DNSRequestSender(
    const nsACString& aHost, const nsACString& aTrrServer,
    const uint16_t& aType, const OriginAttributes& aOriginAttributes,
    const uint32_t& aFlags, nsIDNSListener* aListener, nsIEventTarget* target)
    : mListener(aListener),
      mTarget(target),
      mResultStatus(NS_OK),
      mHost(aHost),
      mTrrServer(aTrrServer),
      mType(aType),
      mOriginAttributes(aOriginAttributes),
      mFlags(aFlags) {}

void DNSRequestSender::OnRecvCancelDNSRequest(
    const nsCString& hostName, const nsCString& trrServer, const uint16_t& type,
    const OriginAttributes& originAttributes, const uint32_t& flags,
    const nsresult& reason) {}

NS_IMETHODIMP
DNSRequestSender::Cancel(nsresult reason) {
  if (!mIPCActor) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mIPCActor->CanSend()) {
    // We can only do IPDL on the main thread
    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
        "net::CancelDNSRequestEvent",
        [actor(mIPCActor), host(mHost), trrServer(mTrrServer), type(mType),
         originAttributes(mOriginAttributes), flags(mFlags), reason]() {
          if (!actor->CanSend()) {
            return;
          }

          if (DNSRequestChild* child = actor->AsDNSRequestChild()) {
            Unused << child->SendCancelDNSRequest(
                host, trrServer, type, originAttributes, flags, reason);
          } else if (DNSRequestParent* parent = actor->AsDNSRequestParent()) {
            Unused << parent->SendCancelDNSRequest(
                host, trrServer, type, originAttributes, flags, reason);
          }
        });
    SchedulerGroup::Dispatch(TaskCategory::Other, runnable.forget());
  }
  return NS_OK;
}

void DNSRequestSender::StartRequest() {
  // we can only do IPDL on the main thread
  if (!NS_IsMainThread()) {
    SchedulerGroup::Dispatch(
        TaskCategory::Other,
        NewRunnableMethod("net::DNSRequestSender::StartRequest", this,
                          &DNSRequestSender::StartRequest));
    return;
  }

  if (DNSRequestChild* child = mIPCActor->AsDNSRequestChild()) {
    if (XRE_IsContentProcess()) {
      mozilla::dom::ContentChild* cc =
          static_cast<mozilla::dom::ContentChild*>(gNeckoChild->Manager());
      if (cc->IsShuttingDown()) {
        return;
      }

      // Send request to Parent process.
      gNeckoChild->SendPDNSRequestConstructor(child, mHost, mTrrServer, mType,
                                              mOriginAttributes, mFlags);
    } else if (XRE_IsSocketProcess()) {
      // DNS resolution is done in the parent process. Send a DNS request to
      // parent process.
      MOZ_ASSERT(!nsIOService::UseSocketProcess());

      SocketProcessChild* socketProcessChild =
          SocketProcessChild::GetSingleton();
      if (!socketProcessChild->CanSend()) {
        return;
      }

      socketProcessChild->SendPDNSRequestConstructor(
          child, mHost, mTrrServer, mType, mOriginAttributes, mFlags);
    } else {
      MOZ_ASSERT(false, "Wrong process");
      return;
    }
  } else if (DNSRequestParent* parent = mIPCActor->AsDNSRequestParent()) {
    // DNS resolution is done in the socket process. Send a DNS request to
    // socket process.
    MOZ_ASSERT(nsIOService::UseSocketProcess());

    RefPtr<DNSRequestParent> requestParent = parent;
    RefPtr<DNSRequestSender> self = this;
    auto task = [requestParent, self]() {
      Unused << SocketProcessParent::GetSingleton()->SendPDNSRequestConstructor(
          requestParent, self->mHost, self->mTrrServer, self->mType,
          self->mOriginAttributes, self->mFlags);
    };
    if (!gIOService->SocketProcessReady()) {
      gIOService->CallOrWaitForSocketProcess(std::move(task));
      return;
    }

    task();
  }
}

void DNSRequestSender::CallOnLookupComplete() {
  MOZ_ASSERT(mListener);
  mListener->OnLookupComplete(this, mResultRecord, mResultStatus);
}

bool DNSRequestSender::OnRecvLookupCompleted(const DNSRequestResponse& reply) {
  MOZ_ASSERT(mListener);

  switch (reply.type()) {
    case DNSRequestResponse::TDNSRecord: {
      mResultRecord = new ChildDNSRecord(reply.get_DNSRecord(), mFlags);
      break;
    }
    case DNSRequestResponse::Tnsresult: {
      mResultStatus = reply.get_nsresult();
      break;
    }
    case DNSRequestResponse::TArrayOfnsCString: {
      MOZ_ASSERT(mType != nsIDNSService::RESOLVE_TYPE_DEFAULT);
      mResultRecord = new ChildDNSByTypeRecord(reply.get_ArrayOfnsCString());
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("unknown type");
      return false;
  }

  MOZ_ASSERT(NS_IsMainThread());

  bool targetIsMain = false;
  if (!mTarget) {
    targetIsMain = true;
  } else {
    mTarget->IsOnCurrentThread(&targetIsMain);
  }

  if (targetIsMain) {
    CallOnLookupComplete();
  } else {
    nsCOMPtr<nsIRunnable> event =
        NewRunnableMethod("net::DNSRequestSender::CallOnLookupComplete", this,
                          &DNSRequestSender::CallOnLookupComplete);
    mTarget->Dispatch(event, NS_DISPATCH_NORMAL);
  }

  if (DNSRequestChild* child = mIPCActor->AsDNSRequestChild()) {
    Unused << child->Send__delete__(child);
  } else if (DNSRequestParent* parent = mIPCActor->AsDNSRequestParent()) {
    Unused << parent->Send__delete__(parent);
  }

  return true;
}

void DNSRequestSender::OnIPCActorDestroy() {
  // Request is done or destroyed. Remove it from the hash table.
  RefPtr<ChildDNSService> dnsServiceChild =
      dont_AddRef(ChildDNSService::GetSingleton());
  dnsServiceChild->NotifyRequestDone(this);

  mIPCActor = nullptr;
}

//-----------------------------------------------------------------------------
// DNSRequestChild
//-----------------------------------------------------------------------------

mozilla::ipc::IPCResult DNSRequestChild::RecvCancelDNSRequest(
    const nsCString& hostName, const nsCString& trrServer, const uint16_t& type,
    const OriginAttributes& originAttributes, const uint32_t& flags,
    const nsresult& reason) {
  mDNSRequest->OnRecvCancelDNSRequest(hostName, trrServer, type,
                                      originAttributes, flags, reason);
  return IPC_OK();
}

mozilla::ipc::IPCResult DNSRequestChild::RecvLookupCompleted(
    const DNSRequestResponse& reply) {
  return mDNSRequest->OnRecvLookupCompleted(reply) ? IPC_OK()
                                                   : IPC_FAIL_NO_REASON(this);
}

void DNSRequestChild::ActorDestroy(ActorDestroyReason) {
  mDNSRequest->OnIPCActorDestroy();
  mDNSRequest = nullptr;
}

//------------------------------------------------------------------------------
}  // namespace net
}  // namespace mozilla
