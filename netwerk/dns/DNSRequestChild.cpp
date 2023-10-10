/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/net/ChildDNSService.h"
#include "mozilla/net/DNSByTypeRecord.h"
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

void DNSRequestBase::SetIPCActor(DNSRequestActor* aActor) {
  mIPCActor = aActor;
}

//-----------------------------------------------------------------------------
// ChildDNSRecord:
// A simple class to provide nsIDNSRecord on the child
//-----------------------------------------------------------------------------

class ChildDNSRecord : public nsIDNSAddrRecord {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSRECORD
  NS_DECL_NSIDNSADDRRECORD

  ChildDNSRecord(const DNSRecord& reply, nsIDNSService::DNSFlags flags);

 private:
  virtual ~ChildDNSRecord() = default;

  nsCString mCanonicalName;
  nsTArray<NetAddr> mAddresses;
  uint32_t mCurrent = 0;  // addr iterator
  nsIDNSService::DNSFlags mFlags = nsIDNSService::RESOLVE_DEFAULT_FLAGS;
  double mTrrFetchDuration = 0;
  double mTrrFetchDurationNetworkOnly = 0;
  bool mIsTRR = false;
  bool mResolvedInSocketProcess = false;
  nsIRequest::TRRMode mEffectiveTRRMode = nsIRequest::TRR_DEFAULT_MODE;
  nsITRRSkipReason::value mTRRSkipReason = nsITRRSkipReason::TRR_UNSET;
  uint32_t mTTL = 0;
};

NS_IMPL_ISUPPORTS(ChildDNSRecord, nsIDNSRecord, nsIDNSAddrRecord)

ChildDNSRecord::ChildDNSRecord(const DNSRecord& reply,
                               nsIDNSService::DNSFlags flags)
    : mFlags(flags) {
  mCanonicalName = reply.canonicalName();
  mTrrFetchDuration = reply.trrFetchDuration();
  mTrrFetchDurationNetworkOnly = reply.trrFetchDurationNetworkOnly();
  mIsTRR = reply.isTRR();
  // When ChildDNSRecord is created in parent process, we know this is case that
  // DNS resolution is done in socket process.
  mResolvedInSocketProcess = XRE_IsParentProcess();
  mEffectiveTRRMode = reply.effectiveTRRMode();

  // A shame IPDL gives us no way to grab ownership of array: so copy it.
  const nsTArray<NetAddr>& addrs = reply.addrs();
  mAddresses = addrs.Clone();
  mTTL = reply.ttl();
}

//-----------------------------------------------------------------------------
// ChildDNSRecord::nsIDNSAddrRecord
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
  *retval = mIsTRR;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSRecord::ResolvedInSocketProcess(bool* retval) {
  *retval = mResolvedInSocketProcess;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSRecord::GetTrrFetchDuration(double* aTime) {
  *aTime = mTrrFetchDuration;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSRecord::GetTrrFetchDurationNetworkOnly(double* aTime) {
  *aTime = mTrrFetchDurationNetworkOnly;
  return NS_OK;
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
  aAddressArray = mAddresses.Clone();
  return NS_OK;
}

// shamelessly copied from nsDNSRecord
NS_IMETHODIMP
ChildDNSRecord::GetScriptableNextAddr(uint16_t port, nsINetAddr** result) {
  NetAddr addr;
  nsresult rv = GetNextAddr(port, &addr);
  if (NS_FAILED(rv)) {
    return rv;
  }

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
  if (addr.ToStringBuffer(buf, sizeof(buf))) {
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

NS_IMETHODIMP
ChildDNSRecord::GetEffectiveTRRMode(nsIRequest::TRRMode* aMode) {
  *aMode = mEffectiveTRRMode;
  return NS_OK;
}

NS_IMETHODIMP ChildDNSRecord::GetTrrSkipReason(
    nsITRRSkipReason::value* aTrrSkipReason) {
  *aTrrSkipReason = mTRRSkipReason;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSRecord::GetTtl(uint32_t* aTtl) {
  *aTtl = mTTL;
  return NS_OK;
}

class ChildDNSByTypeRecord : public nsIDNSByTypeRecord,
                             public nsIDNSTXTRecord,
                             public nsIDNSHTTPSSVCRecord,
                             public DNSHTTPSSVCRecordBase {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSRECORD
  NS_DECL_NSIDNSBYTYPERECORD
  NS_DECL_NSIDNSTXTRECORD
  NS_DECL_NSIDNSHTTPSSVCRECORD

  explicit ChildDNSByTypeRecord(const TypeRecordResultType& reply,
                                const nsACString& aHost, uint32_t aTTL);

 private:
  virtual ~ChildDNSByTypeRecord() = default;

  TypeRecordResultType mResults = AsVariant(mozilla::Nothing());
  bool mAllRecordsExcluded = false;
  uint32_t mTTL = 0;
};

NS_IMPL_ISUPPORTS(ChildDNSByTypeRecord, nsIDNSByTypeRecord, nsIDNSRecord,
                  nsIDNSTXTRecord, nsIDNSHTTPSSVCRecord)

ChildDNSByTypeRecord::ChildDNSByTypeRecord(const TypeRecordResultType& reply,
                                           const nsACString& aHost,
                                           uint32_t aTTL)
    : DNSHTTPSSVCRecordBase(aHost) {
  mResults = reply;
  mTTL = aTTL;
}

NS_IMETHODIMP
ChildDNSByTypeRecord::GetType(uint32_t* aType) {
  *aType = mResults.match(
      [](TypeRecordEmpty&) {
        MOZ_ASSERT(false, "This should never be the case");
        return nsIDNSService::RESOLVE_TYPE_DEFAULT;
      },
      [](TypeRecordTxt&) { return nsIDNSService::RESOLVE_TYPE_TXT; },
      [](TypeRecordHTTPSSVC&) { return nsIDNSService::RESOLVE_TYPE_HTTPSSVC; });
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSByTypeRecord::GetRecords(CopyableTArray<nsCString>& aRecords) {
  if (!mResults.is<TypeRecordTxt>()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aRecords = mResults.as<CopyableTArray<nsCString>>();
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSByTypeRecord::GetRecordsAsOneString(nsACString& aRecords) {
  // deep copy
  if (!mResults.is<TypeRecordTxt>()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  auto& results = mResults.as<CopyableTArray<nsCString>>();
  for (uint32_t i = 0; i < results.Length(); i++) {
    aRecords.Append(results[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSByTypeRecord::GetRecords(nsTArray<RefPtr<nsISVCBRecord>>& aRecords) {
  if (!mResults.is<TypeRecordHTTPSSVC>()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto& results = mResults.as<TypeRecordHTTPSSVC>();

  for (const SVCB& r : results) {
    RefPtr<nsISVCBRecord> rec = new SVCBRecord(r);
    aRecords.AppendElement(rec);
  }
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSByTypeRecord::GetServiceModeRecord(bool aNoHttp2, bool aNoHttp3,
                                           nsISVCBRecord** aRecord) {
  if (!mResults.is<TypeRecordHTTPSSVC>()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto& results = mResults.as<TypeRecordHTTPSSVC>();
  nsCOMPtr<nsISVCBRecord> result = GetServiceModeRecordInternal(
      aNoHttp2, aNoHttp3, results, mAllRecordsExcluded);
  if (!result) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  result.forget(aRecord);
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSByTypeRecord::GetAllRecordsWithEchConfig(
    bool aNoHttp2, bool aNoHttp3, bool* aAllRecordsHaveEchConfig,
    bool* aAllRecordsInH3ExcludedList,
    nsTArray<RefPtr<nsISVCBRecord>>& aResult) {
  if (!mResults.is<TypeRecordHTTPSSVC>()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto& records = mResults.as<TypeRecordHTTPSSVC>();
  GetAllRecordsWithEchConfigInternal(aNoHttp2, aNoHttp3, records,
                                     aAllRecordsHaveEchConfig,
                                     aAllRecordsInH3ExcludedList, aResult);
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSByTypeRecord::GetHasIPAddresses(bool* aResult) {
  NS_ENSURE_ARG(aResult);

  if (!mResults.is<TypeRecordHTTPSSVC>()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto& results = mResults.as<TypeRecordHTTPSSVC>();
  *aResult = HasIPAddressesInternal(results);
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSByTypeRecord::GetAllRecordsExcluded(bool* aResult) {
  NS_ENSURE_ARG(aResult);

  if (!mResults.is<TypeRecordHTTPSSVC>()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aResult = mAllRecordsExcluded;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSByTypeRecord::GetResults(mozilla::net::TypeRecordResultType* aResults) {
  *aResults = mResults;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSByTypeRecord::GetTtl(uint32_t* aResult) {
  *aResult = mTTL;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// DNSRequestSender
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(DNSRequestSender, nsICancelable)

DNSRequestSender::DNSRequestSender(const nsACString& aHost,
                                   const nsACString& aTrrServer, int32_t aPort,
                                   const uint16_t& aType,
                                   const OriginAttributes& aOriginAttributes,
                                   const nsIDNSService::DNSFlags& aFlags,
                                   nsIDNSListener* aListener,
                                   nsIEventTarget* target)
    : mListener(aListener),
      mTarget(target),
      mResultStatus(NS_OK),
      mHost(aHost),
      mTrrServer(aTrrServer),
      mPort(aPort),
      mType(aType),
      mOriginAttributes(aOriginAttributes),
      mFlags(aFlags) {}

void DNSRequestSender::OnRecvCancelDNSRequest(
    const nsCString& hostName, const nsCString& trrServer, const int32_t& port,
    const uint16_t& type, const OriginAttributes& originAttributes,
    const nsIDNSService::DNSFlags& flags, const nsresult& reason) {}

NS_IMETHODIMP
DNSRequestSender::Cancel(nsresult reason) {
  if (!mIPCActor) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mIPCActor->CanSend()) {
    // We can only do IPDL on the main thread
    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
        "net::CancelDNSRequestEvent",
        [actor(mIPCActor), host(mHost), trrServer(mTrrServer), port(mPort),
         type(mType), originAttributes(mOriginAttributes), flags(mFlags),
         reason]() {
          if (!actor->CanSend()) {
            return;
          }

          if (DNSRequestChild* child = actor->AsDNSRequestChild()) {
            Unused << child->SendCancelDNSRequest(
                host, trrServer, port, type, originAttributes, flags, reason);
          } else if (DNSRequestParent* parent = actor->AsDNSRequestParent()) {
            Unused << parent->SendCancelDNSRequest(
                host, trrServer, port, type, originAttributes, flags, reason);
          }
        });
    SchedulerGroup::Dispatch(runnable.forget());
  }
  return NS_OK;
}

void DNSRequestSender::StartRequest() {
  // we can only do IPDL on the main thread
  if (!NS_IsMainThread()) {
    SchedulerGroup::Dispatch(
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
      gNeckoChild->SendPDNSRequestConstructor(child, mHost, mTrrServer, mPort,
                                              mType, mOriginAttributes, mFlags);
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
          child, mHost, mTrrServer, mPort, mType, mOriginAttributes, mFlags);
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
          requestParent, self->mHost, self->mTrrServer, self->mPort,
          self->mType, self->mOriginAttributes, self->mFlags);
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
    case DNSRequestResponse::TIPCTypeRecord: {
      MOZ_ASSERT(mType != nsIDNSService::RESOLVE_TYPE_DEFAULT);
      mResultRecord =
          new ChildDNSByTypeRecord(reply.get_IPCTypeRecord().mData, mHost,
                                   reply.get_IPCTypeRecord().mTTL);
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
    Unused << mozilla::net::DNSRequestChild::Send__delete__(child);
  } else if (DNSRequestParent* parent = mIPCActor->AsDNSRequestParent()) {
    Unused << mozilla::net::DNSRequestParent::Send__delete__(parent);
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

DNSRequestChild::DNSRequestChild(DNSRequestBase* aRequest)
    : DNSRequestActor(aRequest) {
  aRequest->SetIPCActor(this);
}

mozilla::ipc::IPCResult DNSRequestChild::RecvCancelDNSRequest(
    const nsCString& hostName, const nsCString& trrServer, const int32_t& port,
    const uint16_t& type, const OriginAttributes& originAttributes,
    const nsIDNSService::DNSFlags& flags, const nsresult& reason) {
  mDNSRequest->OnRecvCancelDNSRequest(hostName, trrServer, port, type,
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
