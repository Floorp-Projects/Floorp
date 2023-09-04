/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TRRQuery.h"

#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Telemetry.h"
#include "nsQueryObject.h"
#include "TRR.h"
#include "TRRService.h"
// Put DNSLogging.h at the end to avoid LOG being overwritten by other headers.
#include "DNSLogging.h"

namespace mozilla {
namespace net {

static already_AddRefed<AddrInfo> merge_rrset(AddrInfo* rrto,
                                              AddrInfo* rrfrom) {
  MOZ_ASSERT(rrto && rrfrom);
  // Each of the arguments are all-IPv4 or all-IPv6 hence judging
  // by the first element. This is true only for TRR resolutions.
  bool isIPv6 = rrfrom->Addresses().Length() > 0 &&
                rrfrom->Addresses()[0].raw.family == PR_AF_INET6;

  nsTArray<NetAddr> addresses;
  if (isIPv6) {
    addresses = rrfrom->Addresses().Clone();
    addresses.AppendElements(rrto->Addresses());
  } else {
    addresses = rrto->Addresses().Clone();
    addresses.AppendElements(rrfrom->Addresses());
  }
  auto builder = rrto->Build();
  builder.SetAddresses(std::move(addresses));
  return builder.Finish();
}

void TRRQuery::Cancel(nsresult aStatus) {
  MutexAutoLock trrlock(mTrrLock);
  if (mTrrA) {
    mTrrA->Cancel(aStatus);
  }
  if (mTrrAAAA) {
    mTrrAAAA->Cancel(aStatus);
  }
  if (mTrrByType) {
    mTrrByType->Cancel(aStatus);
  }
}

void TRRQuery::MarkSendingTRR(TRR* trr, enum TrrType rectype, MutexAutoLock&) {
  if (rectype == TRRTYPE_A) {
    MOZ_ASSERT(!mTrrA);
    mTrrA = trr;
    mTrrAUsed = STARTED;
  } else if (rectype == TRRTYPE_AAAA) {
    MOZ_ASSERT(!mTrrAAAA);
    mTrrAAAA = trr;
    mTrrAAAAUsed = STARTED;
  } else {
    LOG(("TrrLookup called with bad type set: %d\n", rectype));
    MOZ_ASSERT(0);
  }
}

void TRRQuery::PrepareQuery(enum TrrType aRecType,
                            nsTArray<RefPtr<TRR>>& aRequestsToSend) {
  LOG(("TRR Resolve %s type %d\n", mRecord->host.get(), (int)aRecType));
  RefPtr<TRR> trr = new TRR(this, mRecord, aRecType);

  {
    MutexAutoLock trrlock(mTrrLock);
    MarkSendingTRR(trr, aRecType, trrlock);
    aRequestsToSend.AppendElement(trr);
  }
}

bool TRRQuery::SendQueries(nsTArray<RefPtr<TRR>>& aRequestsToSend) {
  bool madeQuery = false;
  mTRRRequestCounter = aRequestsToSend.Length();
  for (const auto& request : aRequestsToSend) {
    if (NS_SUCCEEDED(TRRService::Get()->DispatchTRRRequest(request))) {
      madeQuery = true;
    } else {
      mTRRRequestCounter--;
      MutexAutoLock trrlock(mTrrLock);
      if (request == mTrrA) {
        mTrrA = nullptr;
        mTrrAUsed = INIT;
      }
      if (request == mTrrAAAA) {
        mTrrAAAA = nullptr;
        mTrrAAAAUsed = INIT;
      }
    }
  }
  aRequestsToSend.Clear();
  return madeQuery;
}

nsresult TRRQuery::DispatchLookup(TRR* pushedTRR) {
  mTrrStart = TimeStamp::Now();

  if (!mRecord->IsAddrRecord()) {
    return DispatchByTypeLookup(pushedTRR);
  }

  RefPtr<AddrHostRecord> addrRec = do_QueryObject(mRecord);
  MOZ_ASSERT(addrRec);
  if (!addrRec) {
    return NS_ERROR_UNEXPECTED;
  }

  mTrrAUsed = INIT;
  mTrrAAAAUsed = INIT;

  // Always issue both A and AAAA.
  // When both are complete we filter out the unneeded results.
  enum TrrType rectype = (mRecord->af == AF_INET6) ? TRRTYPE_AAAA : TRRTYPE_A;

  if (pushedTRR) {
    MutexAutoLock trrlock(mTrrLock);
    rectype = pushedTRR->Type();
    MarkSendingTRR(pushedTRR, rectype, trrlock);
    return NS_OK;
  }

  // Need to dispatch TRR requests after |mTrrA| and |mTrrAAAA| are set
  // properly so as to avoid the race when CompleteLookup() is called at the
  // same time.
  nsTArray<RefPtr<TRR>> requestsToSend;
  if ((mRecord->af == AF_UNSPEC || mRecord->af == AF_INET6) &&
      !StaticPrefs::network_dns_disableIPv6()) {
    PrepareQuery(TRRTYPE_AAAA, requestsToSend);
  }
  if (mRecord->af == AF_UNSPEC || mRecord->af == AF_INET) {
    PrepareQuery(TRRTYPE_A, requestsToSend);
  }

  if (SendQueries(requestsToSend)) {
    return NS_OK;
  }

  return NS_ERROR_UNKNOWN_HOST;
}

nsresult TRRQuery::DispatchByTypeLookup(TRR* pushedTRR) {
  RefPtr<TypeHostRecord> typeRec = do_QueryObject(mRecord);
  MOZ_ASSERT(typeRec);
  if (!typeRec) {
    return NS_ERROR_UNEXPECTED;
  }

  enum TrrType rectype;

  // XXX this could use a more extensible approach.
  if (mRecord->type == nsIDNSService::RESOLVE_TYPE_TXT) {
    rectype = TRRTYPE_TXT;
  } else if (mRecord->type == nsIDNSService::RESOLVE_TYPE_HTTPSSVC) {
    rectype = TRRTYPE_HTTPSSVC;
  } else if (pushedTRR) {
    rectype = pushedTRR->Type();
  } else {
    MOZ_ASSERT(false, "Not an expected request type");
    return NS_ERROR_UNKNOWN_HOST;
  }

  LOG(("TRR Resolve %s type %d\n", typeRec->host.get(), (int)rectype));
  RefPtr<TRR> trr = pushedTRR ? pushedTRR : new TRR(this, mRecord, rectype);

  if (pushedTRR || NS_SUCCEEDED(TRRService::Get()->DispatchTRRRequest(trr))) {
    MutexAutoLock trrlock(mTrrLock);
    MOZ_ASSERT(!mTrrByType);
    mTrrByType = trr;
    return NS_OK;
  }

  return NS_ERROR_UNKNOWN_HOST;
}

AHostResolver::LookupStatus TRRQuery::CompleteLookup(
    nsHostRecord* rec, nsresult status, AddrInfo* aNewRRSet, bool pb,
    const nsACString& aOriginsuffix, nsHostRecord::TRRSkippedReason aReason,
    TRR* aTRRRequest) {
  if (rec != mRecord) {
    LOG(("TRRQuery::CompleteLookup - Pushed record. Go to resolver"));
    return mHostResolver->CompleteLookup(rec, status, aNewRRSet, pb,
                                         aOriginsuffix, aReason, aTRRRequest);
  }

  LOG(("TRRQuery::CompleteLookup > host: %s", rec->host.get()));

  RefPtr<AddrInfo> newRRSet(aNewRRSet);
  DNSResolverType resolverType = newRRSet->ResolverType();
  {
    MutexAutoLock trrlock(mTrrLock);
    if (newRRSet->TRRType() == TRRTYPE_A) {
      MOZ_ASSERT(mTrrA);
      mTRRAFailReason = aReason;
      mTrrA = nullptr;
      mTrrAUsed = NS_SUCCEEDED(status) ? OK : FAILED;
      MOZ_ASSERT(!mAddrInfoA);
      mAddrInfoA = newRRSet;
      mAResult = status;
      LOG(("A query status: 0x%x", static_cast<uint32_t>(status)));
    } else if (newRRSet->TRRType() == TRRTYPE_AAAA) {
      MOZ_ASSERT(mTrrAAAA);
      mTRRAAAAFailReason = aReason;
      mTrrAAAA = nullptr;
      mTrrAAAAUsed = NS_SUCCEEDED(status) ? OK : FAILED;
      MOZ_ASSERT(!mAddrInfoAAAA);
      mAddrInfoAAAA = newRRSet;
      mAAAAResult = status;
      LOG(("AAAA query status: 0x%x", static_cast<uint32_t>(status)));
    } else {
      MOZ_ASSERT(0);
    }
  }

  if (NS_SUCCEEDED(status)) {
    mTRRSuccess++;
    if (mTRRSuccess == 1) {
      // Store the duration on first succesful TRR response.  We
      // don't know that there will be a second response nor can we
      // tell which of two has useful data.
      mTrrDuration = TimeStamp::Now() - mTrrStart;
    }
  }

  bool pendingRequest = false;
  if (mTRRRequestCounter) {
    mTRRRequestCounter--;
    pendingRequest = (mTRRRequestCounter != 0);
  } else {
    MOZ_DIAGNOSTIC_ASSERT(false, "Request counter is messed up");
  }
  if (pendingRequest) {  // There are other outstanding requests
    LOG(("CompleteLookup: waiting for all responses!\n"));
    return LOOKUP_OK;
  }

  if (mRecord->af == AF_UNSPEC) {
    // merge successful records
    if (mTrrAUsed == OK) {
      LOG(("Have A response"));
      newRRSet = mAddrInfoA;
      status = mAResult;
      if (mTrrAAAAUsed == OK) {
        LOG(("Merging A and AAAA responses"));
        newRRSet = merge_rrset(newRRSet, mAddrInfoAAAA);
      }
    } else {
      newRRSet = mAddrInfoAAAA;
      status = mAAAAResult;
    }

    if (NS_FAILED(status) && (mAAAAResult == NS_ERROR_DEFINITIVE_UNKNOWN_HOST ||
                              mAResult == NS_ERROR_DEFINITIVE_UNKNOWN_HOST)) {
      status = NS_ERROR_DEFINITIVE_UNKNOWN_HOST;
    }
  } else {
    // If this is a failed AAAA request, but the server only has a A record,
    // then we should not fallback to Do53. Instead we also send a A request
    // and return NS_ERROR_DEFINITIVE_UNKNOWN_HOST if that succeeds.
    if (NS_FAILED(status) && status != NS_ERROR_DEFINITIVE_UNKNOWN_HOST &&
        (mTrrAUsed == INIT || mTrrAAAAUsed == INIT)) {
      if (newRRSet->TRRType() == TRRTYPE_A) {
        LOG(("A lookup failed. Checking if AAAA record exists"));
        nsTArray<RefPtr<TRR>> requestsToSend;
        PrepareQuery(TRRTYPE_AAAA, requestsToSend);
        if (SendQueries(requestsToSend)) {
          LOG(("Sent AAAA request"));
          return LOOKUP_OK;
        }
      } else if (newRRSet->TRRType() == TRRTYPE_AAAA) {
        LOG(("AAAA lookup failed. Checking if A record exists"));
        nsTArray<RefPtr<TRR>> requestsToSend;
        PrepareQuery(TRRTYPE_A, requestsToSend);
        if (SendQueries(requestsToSend)) {
          LOG(("Sent A request"));
          return LOOKUP_OK;
        }
      } else {
        MOZ_ASSERT(false, "Unexpected family");
      }
    }
    bool otherSucceeded =
        mRecord->af == AF_INET6 ? mTrrAUsed == OK : mTrrAAAAUsed == OK;
    LOG(("TRRQuery::CompleteLookup other request succeeded"));

    if (mRecord->af == AF_INET) {
      // return only A record
      newRRSet = mAddrInfoA;
      status = mAResult;
      if (NS_FAILED(status) &&
          (otherSucceeded || mAAAAResult == NS_ERROR_DEFINITIVE_UNKNOWN_HOST)) {
        LOG(("status set to NS_ERROR_DEFINITIVE_UNKNOWN_HOST"));
        status = NS_ERROR_DEFINITIVE_UNKNOWN_HOST;
      }

    } else if (mRecord->af == AF_INET6) {
      // return only AAAA record
      newRRSet = mAddrInfoAAAA;
      status = mAAAAResult;

      if (NS_FAILED(status) &&
          (otherSucceeded || mAResult == NS_ERROR_DEFINITIVE_UNKNOWN_HOST)) {
        LOG(("status set to NS_ERROR_DEFINITIVE_UNKNOWN_HOST"));
        status = NS_ERROR_DEFINITIVE_UNKNOWN_HOST;
      }

    } else {
      MOZ_ASSERT(false, "Unexpected AF");
      return LOOKUP_OK;
    }

    // If this record failed, but there is a record for the other AF
    // we prevent fallback to the native resolver.
  }

  if (mTRRSuccess && mHostResolver->GetNCS() &&
      (mHostResolver->GetNCS()->GetNAT64() ==
       nsINetworkConnectivityService::OK) &&
      newRRSet) {
    newRRSet = mHostResolver->GetNCS()->MapNAT64IPs(newRRSet);
  }

  if (resolverType == DNSResolverType::TRR) {
    if (mTrrAUsed == OK) {
      AccumulateCategoricalKeyed(
          TRRService::ProviderKey(),
          Telemetry::LABELS_DNS_LOOKUP_DISPOSITION3::trrAOK);
    } else if (mTrrAUsed == FAILED) {
      AccumulateCategoricalKeyed(
          TRRService::ProviderKey(),
          Telemetry::LABELS_DNS_LOOKUP_DISPOSITION3::trrAFail);
    }

    if (mTrrAAAAUsed == OK) {
      AccumulateCategoricalKeyed(
          TRRService::ProviderKey(),
          Telemetry::LABELS_DNS_LOOKUP_DISPOSITION3::trrAAAAOK);
    } else if (mTrrAAAAUsed == FAILED) {
      AccumulateCategoricalKeyed(
          TRRService::ProviderKey(),
          Telemetry::LABELS_DNS_LOOKUP_DISPOSITION3::trrAAAAFail);
    }
  }

  mAddrInfoAAAA = nullptr;
  mAddrInfoA = nullptr;

  MOZ_DIAGNOSTIC_ASSERT(!mCalledCompleteLookup,
                        "must not call CompleteLookup more than once");
  mCalledCompleteLookup = true;
  return mHostResolver->CompleteLookup(rec, status, newRRSet, pb, aOriginsuffix,
                                       aReason, aTRRRequest);
}

AHostResolver::LookupStatus TRRQuery::CompleteLookupByType(
    nsHostRecord* rec, nsresult status,
    mozilla::net::TypeRecordResultType& aResult,
    mozilla::net::TRRSkippedReason aReason, uint32_t aTtl, bool pb) {
  if (rec != mRecord) {
    LOG(("TRRQuery::CompleteLookup - Pushed record. Go to resolver"));
    return mHostResolver->CompleteLookupByType(rec, status, aResult, aReason,
                                               aTtl, pb);
  }

  {
    MutexAutoLock trrlock(mTrrLock);
    mTrrByType = nullptr;
  }

  // Unlike the address record, we store the duration regardless of the status.
  mTrrDuration = TimeStamp::Now() - mTrrStart;

  MOZ_DIAGNOSTIC_ASSERT(!mCalledCompleteLookup,
                        "must not call CompleteLookup more than once");
  mCalledCompleteLookup = true;
  return mHostResolver->CompleteLookupByType(rec, status, aResult, aReason,
                                             aTtl, pb);
}

}  // namespace net
}  // namespace mozilla
