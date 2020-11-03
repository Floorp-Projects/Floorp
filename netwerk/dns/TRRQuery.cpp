/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TRRQuery.h"
#include "TRR.h"

namespace mozilla {
namespace net {

#undef LOG
extern mozilla::LazyLogModule gHostResolverLog;
#define LOG(args) MOZ_LOG(gHostResolverLog, mozilla::LogLevel::Debug, args)

static already_AddRefed<AddrInfo> merge_rrset(AddrInfo* rrto,
                                              AddrInfo* rrfrom) {
  MOZ_ASSERT(rrto && rrfrom);
  // Each of the arguments are all-IPv4 or all-IPv6 hence judging
  // by the first element. This is true only for TRR resolutions.
  bool isIPv6 = rrfrom->Addresses().Length() > 0 &&
                rrfrom->Addresses()[0].raw.family == PR_AF_INET6;

  nsTArray<NetAddr> addresses = rrto->Addresses().Clone();
  for (const auto& addr : rrfrom->Addresses()) {
    if (isIPv6) {
      // rrfrom has IPv6 so it should be first
      addresses.InsertElementAt(0, addr);
    } else {
      addresses.AppendElement(addr);
    }
  }
  auto builder = rrto->Build();
  builder.SetAddresses(std::move(addresses));
  return builder.Finish();
}

void TRRQuery::Cancel() {
  MutexAutoLock trrlock(mTrrLock);
  if (mTrrA) {
    mTrrA->Cancel();
    mTrrA = nullptr;
  }
  if (mTrrAAAA) {
    mTrrAAAA->Cancel();
    mTrrAAAA = nullptr;
  }
  if (mTrrByType) {
    mTrrByType->Cancel();
    mTrrByType = nullptr;
  }
}

nsresult TRRQuery::DispatchLookup(TRR* pushedTRR) {
  mTrrStart = TimeStamp::Now();

  RefPtr<AddrHostRecord> addrRec;
  RefPtr<TypeHostRecord> typeRec;

  if (mRecord->IsAddrRecord()) {
    addrRec = do_QueryObject(mRecord);
    MOZ_ASSERT(addrRec);
  } else {
    typeRec = do_QueryObject(mRecord);
    MOZ_ASSERT(typeRec);
  }

  mTrrStart = TimeStamp::Now();
  bool madeQuery = false;

  if (addrRec) {
    mTrrAUsed = INIT;
    mTrrAAAAUsed = INIT;

    // If asking for AF_UNSPEC, issue both A and AAAA.
    // If asking for AF_INET6 or AF_INET, do only that single type
    enum TrrType rectype = (mRecord->af == AF_INET6) ? TRRTYPE_AAAA : TRRTYPE_A;

    if (pushedTRR) {
      rectype = pushedTRR->Type();
    }
    bool sendAgain;

    do {
      sendAgain = false;
      if ((TRRTYPE_AAAA == rectype) && gTRRService &&
          (gTRRService->DisableIPv6() ||
           (StaticPrefs::network_trr_skip_AAAA_when_not_supported() &&
            mHostResolver->GetNCS() &&
            mHostResolver->GetNCS()->GetIPv6() ==
                nsINetworkConnectivityService::NOT_AVAILABLE))) {
        break;
      }
      LOG(("TRR Resolve %s type %d\n", addrRec->host.get(), (int)rectype));
      RefPtr<TRR> trr;
      trr = pushedTRR ? pushedTRR : new TRR(this, mRecord, rectype);
      if (pushedTRR || NS_SUCCEEDED(gTRRService->DispatchTRRRequest(trr))) {
        MutexAutoLock trrlock(mTrrLock);
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
        madeQuery = true;
        if (!pushedTRR && (mRecord->af == AF_UNSPEC) &&
            (rectype == TRRTYPE_A)) {
          rectype = TRRTYPE_AAAA;
          sendAgain = true;
        }
      }
    } while (sendAgain);
  } else {
    typeRec->mStart = TimeStamp::Now();
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
    RefPtr<TRR> trr;
    trr = pushedTRR ? pushedTRR : new TRR(this, mRecord, rectype);
    RefPtr<TRR> trrRequest = trr;
    if (pushedTRR || NS_SUCCEEDED(gTRRService->DispatchTRRRequest(trr))) {
      MutexAutoLock trrlock(mTrrLock);
      MOZ_ASSERT(!mTrrByType);
      mTrrByType = trr;
      madeQuery = true;
    }
  }

  return madeQuery ? NS_OK : NS_ERROR_UNKNOWN_HOST;
}

AHostResolver::LookupStatus TRRQuery::CompleteLookup(
    nsHostRecord* rec, nsresult status, AddrInfo* aNewRRSet, bool pb,
    const nsACString& aOriginsuffix, nsHostRecord::TRRSkippedReason aReason) {
  if (rec != mRecord) {
    return mHostResolver->CompleteLookup(rec, status, aNewRRSet, pb,
                                         aOriginsuffix, aReason);
  }

  RefPtr<AddrInfo> newRRSet(aNewRRSet);
  bool pendingARequest = false;
  bool pendingAAAARequest = false;
  {
    MutexAutoLock trrlock(mTrrLock);
    if (newRRSet->IsTRR() == TRRTYPE_A) {
      MOZ_ASSERT(mTrrA);
      mTRRAFailReason = aReason;
      mTrrA = nullptr;
      mTrrAUsed = NS_SUCCEEDED(status) ? OK : FAILED;
    } else if (newRRSet->IsTRR() == TRRTYPE_AAAA) {
      MOZ_ASSERT(mTrrAAAA);
      mTRRAAAAFailReason = aReason;
      mTrrAAAA = nullptr;
      mTrrAAAAUsed = NS_SUCCEEDED(status) ? OK : FAILED;
    } else {
      MOZ_ASSERT(0);
    }
    if (mTrrA) {
      pendingARequest = true;
    }
    if (mTrrAAAA) {
      pendingAAAARequest = true;
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

  if (pendingARequest ||
      pendingAAAARequest) {  // There are other outstanding requests
    mFirstTRRresult = status;
    if (NS_FAILED(status)) {
      return LOOKUP_OK;  // wait for outstanding
    }

    // There's another TRR complete pending. Wait for it and keep
    // this RRset around until then.
    MOZ_ASSERT(!mFirstTRR && newRRSet);
    mFirstTRR.swap(newRRSet);  // autoPtr.swap()
    MOZ_ASSERT(mFirstTRR && !newRRSet);

    if (StaticPrefs::network_trr_wait_for_A_and_AAAA()) {
      LOG(("CompleteLookup: waiting for all responses!\n"));
      return LOOKUP_OK;
    }

    if (pendingARequest && !StaticPrefs::network_trr_early_AAAA()) {
      // This is an early AAAA with a pending A response. Allowed
      // only by pref.
      LOG(("CompleteLookup: avoiding early use of TRR AAAA!\n"));
      return LOOKUP_OK;
    }

    // we can do some callbacks with this partial result which requires
    // a deep copy
    newRRSet = mFirstTRR;

    // Increment mResolving so we wait for the next resolve too.
    rec->mResolving++;
  } else {
    // no more outstanding TRRs
    // If mFirstTRR is set, merge those addresses into current set!
    if (mFirstTRR) {
      if (NS_SUCCEEDED(status)) {
        LOG(("Merging responses"));
        newRRSet = merge_rrset(newRRSet, mFirstTRR);
      } else {
        LOG(("Will use previous response"));
        newRRSet.swap(mFirstTRR);  // transfers
        // We must use the status of the first response, otherwise we'll
        // pass an error result to the consumers.
        status = mFirstTRRresult;
      }
      mFirstTRR = nullptr;
    } else {
      if (NS_FAILED(status) && status != NS_ERROR_DEFINITIVE_UNKNOWN_HOST &&
          mFirstTRRresult == NS_ERROR_DEFINITIVE_UNKNOWN_HOST) {
        status = NS_ERROR_DEFINITIVE_UNKNOWN_HOST;
      }
    }

    if (mTRRSuccess && mHostResolver->GetNCS() &&
        (mHostResolver->GetNCS()->GetNAT64() ==
         nsINetworkConnectivityService::OK) &&
        newRRSet) {
      newRRSet = mHostResolver->GetNCS()->MapNAT64IPs(newRRSet);
    }
  }

  if (mTrrAUsed == OK) {
    AccumulateCategoricalKeyed(
        TRRService::AutoDetectedKey(),
        Telemetry::LABELS_DNS_LOOKUP_DISPOSITION2::trrAOK);
  } else if (mTrrAUsed == FAILED) {
    AccumulateCategoricalKeyed(
        TRRService::AutoDetectedKey(),
        Telemetry::LABELS_DNS_LOOKUP_DISPOSITION2::trrAFail);
  }

  if (mTrrAAAAUsed == OK) {
    AccumulateCategoricalKeyed(
        TRRService::AutoDetectedKey(),
        Telemetry::LABELS_DNS_LOOKUP_DISPOSITION2::trrAAAAOK);
  } else if (mTrrAAAAUsed == FAILED) {
    AccumulateCategoricalKeyed(
        TRRService::AutoDetectedKey(),
        Telemetry::LABELS_DNS_LOOKUP_DISPOSITION2::trrAAAAFail);
  }

  return mHostResolver->CompleteLookup(rec, status, newRRSet, pb, aOriginsuffix,
                                       aReason);
}

AHostResolver::LookupStatus TRRQuery::CompleteLookupByType(
    nsHostRecord* rec, nsresult status,
    mozilla::net::TypeRecordResultType& aResult, uint32_t aTtl, bool pb) {
  if (rec == mRecord) {
    MutexAutoLock trrlock(mTrrLock);
    mTrrByType = nullptr;
  }
  return mHostResolver->CompleteLookupByType(rec, status, aResult, aTtl, pb);
}

}  // namespace net
}  // namespace mozilla
