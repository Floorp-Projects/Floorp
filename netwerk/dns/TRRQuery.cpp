/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TRRQuery.h"

#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Telemetry.h"
#include "nsQueryObject.h"
#include "TRR.h"
#include "TRRService.h"
#include "ODoH.h"
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

nsresult TRRQuery::DispatchLookup(TRR* pushedTRR, bool aUseODoH) {
  if (aUseODoH && pushedTRR) {
    MOZ_ASSERT(false, "ODoH should not support push");
    return NS_ERROR_UNKNOWN_HOST;
  }

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
    // Need to dispatch TRR requests after |mTrrA| and |mTrrAAAA| are set
    // properly so as to avoid the race when CompleteLookup() is called at the
    // same time.
    nsTArray<RefPtr<TRR>> requestsToSend;
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
      if (aUseODoH) {
        trr = new ODoH(this, mRecord, rectype);
      } else {
        trr = pushedTRR ? pushedTRR : new TRR(this, mRecord, rectype);
      }

      {
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

        if (!pushedTRR) {
          requestsToSend.AppendElement(trr);
          if ((mRecord->af == AF_UNSPEC) && (rectype == TRRTYPE_A)) {
            rectype = TRRTYPE_AAAA;
            sendAgain = true;
          }
        } else {
          madeQuery = true;
        }
      }
    } while (sendAgain);

    mTRRRequestCounter = requestsToSend.Length();
    for (const auto& request : requestsToSend) {
      if (NS_SUCCEEDED(gTRRService->DispatchTRRRequest(request))) {
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
    requestsToSend.Clear();
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
    if (aUseODoH) {
      trr = new ODoH(this, mRecord, rectype);
    } else {
      trr = pushedTRR ? pushedTRR : new TRR(this, mRecord, rectype);
    }

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
    const nsACString& aOriginsuffix, nsHostRecord::TRRSkippedReason aReason,
    TRR* aTRRRequest) {
  if (rec != mRecord) {
    return mHostResolver->CompleteLookup(rec, status, aNewRRSet, pb,
                                         aOriginsuffix, aReason, aTRRRequest);
  }

  RefPtr<AddrInfo> newRRSet(aNewRRSet);
  DNSResolverType resolverType = newRRSet->ResolverType();
  {
    MutexAutoLock trrlock(mTrrLock);
    if (newRRSet->TRRType() == TRRTYPE_A) {
      MOZ_ASSERT(mTrrA);
      mTRRAFailReason = aReason;
      mTrrA = nullptr;
      mTrrAUsed = NS_SUCCEEDED(status) ? OK : FAILED;
    } else if (newRRSet->TRRType() == TRRTYPE_AAAA) {
      MOZ_ASSERT(mTrrAAAA);
      mTRRAAAAFailReason = aReason;
      mTrrAAAA = nullptr;
      mTrrAAAAUsed = NS_SUCCEEDED(status) ? OK : FAILED;
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
    mFirstTRRresult = status;
    if (NS_FAILED(status)) {
      return LOOKUP_OK;  // wait for outstanding
    }

    // There's another TRR complete pending. Wait for it and keep
    // this RRset around until then.
    MOZ_ASSERT(!mFirstTRR && newRRSet);
    mFirstTRR.swap(newRRSet);  // autoPtr.swap()
    MOZ_ASSERT(mFirstTRR && !newRRSet);

    LOG(("CompleteLookup: waiting for all responses!\n"));
    return LOOKUP_OK;
  }

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

  return mHostResolver->CompleteLookup(rec, status, newRRSet, pb, aOriginsuffix,
                                       aReason, aTRRRequest);
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
