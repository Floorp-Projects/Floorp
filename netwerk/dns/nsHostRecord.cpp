/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHostRecord.h"
#include "TRRQuery.h"
// Put DNSLogging.h at the end to avoid LOG being overwritten by other headers.
#include "DNSLogging.h"
#include "TRRSkippedReason.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Telemetry.h"
#include "TRRService.h"

//----------------------------------------------------------------------------
// this macro filters out any flags that are not used when constructing the
// host key.  the significant flags are those that would affect the resulting
// host record (i.e., the flags that are passed down to PR_GetAddrInfoByName).
#define RES_KEY_FLAGS(_f)                                              \
  ((_f) &                                                              \
   (nsHostResolver::RES_CANON_NAME | nsHostResolver::RES_DISABLE_TRR | \
    nsIDNSService::RESOLVE_TRR_MODE_MASK | nsHostResolver::RES_IP_HINT))

#define IS_ADDR_TYPE(_type) ((_type) == nsIDNSService::RESOLVE_TYPE_DEFAULT)
#define IS_OTHER_TYPE(_type) ((_type) != nsIDNSService::RESOLVE_TYPE_DEFAULT)

//----------------------------------------------------------------------------

using namespace mozilla;
using namespace mozilla::net;

nsHostKey::nsHostKey(const nsACString& aHost, const nsACString& aTrrServer,
                     uint16_t aType, uint16_t aFlags, uint16_t aAf, bool aPb,
                     const nsACString& aOriginsuffix)
    : host(aHost),
      mTrrServer(aTrrServer),
      type(aType),
      flags(aFlags),
      af(aAf),
      pb(aPb),
      originSuffix(aOriginsuffix) {}

bool nsHostKey::operator==(const nsHostKey& other) const {
  return host == other.host && mTrrServer == other.mTrrServer &&
         type == other.type &&
         RES_KEY_FLAGS(flags) == RES_KEY_FLAGS(other.flags) && af == other.af &&
         originSuffix == other.originSuffix;
}

PLDHashNumber nsHostKey::Hash() const {
  return AddToHash(HashString(host.get()), HashString(mTrrServer.get()), type,
                   RES_KEY_FLAGS(flags), af, HashString(originSuffix.get()));
}

size_t nsHostKey::SizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t n = 0;
  n += host.SizeOfExcludingThisIfUnshared(mallocSizeOf);
  n += mTrrServer.SizeOfExcludingThisIfUnshared(mallocSizeOf);
  n += originSuffix.SizeOfExcludingThisIfUnshared(mallocSizeOf);
  return n;
}

//----------------------------------------------------------------------------
// nsHostRecord
//----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS0(nsHostRecord)

nsHostRecord::nsHostRecord(const nsHostKey& key)
    : nsHostKey(key), mTRRQuery("nsHostRecord.mTRRQuery") {}

void nsHostRecord::Invalidate() { mDoomed = true; }

void nsHostRecord::Cancel() {
  RefPtr<TRRQuery> query;
  {
    auto lock = mTRRQuery.Lock();
    query.swap(lock.ref());
  }

  if (query) {
    query->Cancel(NS_ERROR_ABORT);
  }
}

nsHostRecord::ExpirationStatus nsHostRecord::CheckExpiration(
    const mozilla::TimeStamp& now) const {
  if (!mGraceStart.IsNull() && now >= mGraceStart && !mValidEnd.IsNull() &&
      now < mValidEnd) {
    return nsHostRecord::EXP_GRACE;
  }
  if (!mValidEnd.IsNull() && now < mValidEnd) {
    return nsHostRecord::EXP_VALID;
  }

  return nsHostRecord::EXP_EXPIRED;
}

void nsHostRecord::SetExpiration(const mozilla::TimeStamp& now,
                                 unsigned int valid, unsigned int grace) {
  mValidStart = now;
  if ((valid + grace) < 60) {
    grace = 60 - valid;
    LOG(("SetExpiration: artificially bumped grace to %d\n", grace));
  }
  mGraceStart = now + TimeDuration::FromSeconds(valid);
  mValidEnd = now + TimeDuration::FromSeconds(valid + grace);
  mTtl = valid;
}

void nsHostRecord::CopyExpirationTimesAndFlagsFrom(
    const nsHostRecord* aFromHostRecord) {
  // This is used to copy information from a cache entry to a record. All
  // information necessary for HasUsableRecord needs to be copied.
  mValidStart = aFromHostRecord->mValidStart;
  mValidEnd = aFromHostRecord->mValidEnd;
  mGraceStart = aFromHostRecord->mGraceStart;
  mDoomed = aFromHostRecord->mDoomed;
}

bool nsHostRecord::HasUsableResult(const mozilla::TimeStamp& now,
                                   uint16_t queryFlags) const {
  if (mDoomed) {
    return false;
  }

  return HasUsableResultInternal(now, queryFlags);
}

//----------------------------------------------------------------------------
// AddrHostRecord
//----------------------------------------------------------------------------

static size_t SizeOfResolveHostCallbackListExcludingHead(
    const mozilla::LinkedList<RefPtr<nsResolveHostCallback>>& aCallbacks,
    MallocSizeOf mallocSizeOf) {
  size_t n = aCallbacks.sizeOfExcludingThis(mallocSizeOf);

  for (const nsResolveHostCallback* t = aCallbacks.getFirst(); t;
       t = t->getNext()) {
    n += t->SizeOfIncludingThis(mallocSizeOf);
  }

  return n;
}

NS_IMPL_ISUPPORTS_INHERITED(AddrHostRecord, nsHostRecord, AddrHostRecord)

AddrHostRecord::AddrHostRecord(const nsHostKey& key) : nsHostRecord(key) {}

AddrHostRecord::~AddrHostRecord() {
  mCallbacks.clear();
  Telemetry::Accumulate(Telemetry::DNS_BLACKLIST_COUNT, mUnusableCount);
}

bool AddrHostRecord::Blocklisted(const NetAddr* aQuery) {
  addr_info_lock.AssertCurrentThreadOwns();
  LOG(("Checking unusable list for host [%s], host record [%p].\n", host.get(),
       this));

  // skip the string conversion for the common case of no blocklist
  if (!mUnusableItems.Length()) {
    return false;
  }

  char buf[kIPv6CStrBufSize];
  if (!aQuery->ToStringBuffer(buf, sizeof(buf))) {
    return false;
  }
  nsDependentCString strQuery(buf);

  for (uint32_t i = 0; i < mUnusableItems.Length(); i++) {
    if (mUnusableItems.ElementAt(i).Equals(strQuery)) {
      LOG(("Address [%s] is blocklisted for host [%s].\n", buf, host.get()));
      return true;
    }
  }

  return false;
}

void AddrHostRecord::ReportUnusable(const NetAddr* aAddress) {
  addr_info_lock.AssertCurrentThreadOwns();
  LOG(
      ("Adding address to blocklist for host [%s], host record [%p]."
       "used trr=%d\n",
       host.get(), this, mTRRSuccess));

  ++mUnusableCount;

  char buf[kIPv6CStrBufSize];
  if (aAddress->ToStringBuffer(buf, sizeof(buf))) {
    LOG(
        ("Successfully adding address [%s] to blocklist for host "
         "[%s].\n",
         buf, host.get()));
    mUnusableItems.AppendElement(nsCString(buf));
  }
}

void AddrHostRecord::ResetBlocklist() {
  addr_info_lock.AssertCurrentThreadOwns();
  LOG(("Resetting blocklist for host [%s], host record [%p].\n", host.get(),
       this));
  mUnusableItems.Clear();
}

size_t AddrHostRecord::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const {
  size_t n = mallocSizeOf(this);

  n += nsHostKey::SizeOfExcludingThis(mallocSizeOf);
  n += SizeOfResolveHostCallbackListExcludingHead(mCallbacks, mallocSizeOf);

  n += addr_info ? addr_info->SizeOfIncludingThis(mallocSizeOf) : 0;
  n += mallocSizeOf(addr.get());

  n += mUnusableItems.ShallowSizeOfExcludingThis(mallocSizeOf);
  for (size_t i = 0; i < mUnusableItems.Length(); i++) {
    n += mUnusableItems[i].SizeOfExcludingThisIfUnshared(mallocSizeOf);
  }
  return n;
}

bool AddrHostRecord::HasUsableResultInternal(const mozilla::TimeStamp& now,
                                             uint16_t queryFlags) const {
  // don't use cached negative results for high priority queries.
  if (negative && IsHighPriority(queryFlags)) {
    return false;
  }

  if (CheckExpiration(now) == EXP_EXPIRED) {
    return false;
  }

  if (negative) {
    return true;
  }

  return addr_info || addr;
}

// Returns true if the entry can be removed, or false if it should be left.
// Sets ResolveAgain true for entries being resolved right now.
bool AddrHostRecord::RemoveOrRefresh(bool aTrrToo) {
  // no need to flush TRRed names, they're not resolved "locally"
  MutexAutoLock lock(addr_info_lock);
  if (addr_info && !aTrrToo && addr_info->IsTRROrODoH()) {
    return false;
  }
  if (LoadNative()) {
    if (!onQueue()) {
      // The request has been passed to the OS resolver. The resultant DNS
      // record should be considered stale and not trusted; set a flag to
      // ensure it is called again.
      StoreResolveAgain(true);
    }
    // if onQueue is true, the host entry is already added to the cache
    // but is still pending to get resolved: just leave it in hash.
    return false;
  }
  // Already resolved; not in a pending state; remove from cache
  return true;
}

void AddrHostRecord::NotifyRetryingTrr() {
  MOZ_ASSERT(mFirstTRRSkippedReason ==
             mozilla::net::TRRSkippedReason::TRR_UNSET);

  // Save the skip reason of our first attempt for recording telemetry later.
  mFirstTRRSkippedReason = mTRRSkippedReason;
  mTRRSkippedReason = mozilla::net::TRRSkippedReason::TRR_UNSET;
}

void AddrHostRecord::ResolveComplete() {
  if (LoadNativeUsed()) {
    if (mNativeSuccess) {
      uint32_t millis = static_cast<uint32_t>(mNativeDuration.ToMilliseconds());
      Telemetry::Accumulate(Telemetry::DNS_NATIVE_LOOKUP_TIME, millis);
    }
    AccumulateCategoricalKeyed(
        TRRService::ProviderKey(),
        mNativeSuccess ? Telemetry::LABELS_DNS_LOOKUP_DISPOSITION3::osOK
                       : Telemetry::LABELS_DNS_LOOKUP_DISPOSITION3::osFail);
  }

  if (mResolverType == DNSResolverType::ODoH) {
    // XXX(kershaw): Consider adding the failed host name into a blocklist.
    if (mTRRSuccess) {
      uint32_t millis = static_cast<uint32_t>(mTrrDuration.ToMilliseconds());
      Telemetry::Accumulate(Telemetry::DNS_ODOH_LOOKUP_TIME, millis);
    }

    if (nsHostResolver::Mode() == nsIDNSService::MODE_TRRFIRST) {
      Telemetry::Accumulate(Telemetry::ODOH_SKIP_REASON_ODOH_FIRST,
                            static_cast<uint32_t>(mTRRSkippedReason));
    }

    return;
  }

  if (mResolverType == DNSResolverType::TRR) {
    if (mTRRSuccess) {
      uint32_t millis = static_cast<uint32_t>(mTrrDuration.ToMilliseconds());
      Telemetry::Accumulate(Telemetry::DNS_TRR_LOOKUP_TIME3,
                            TRRService::ProviderKey(), millis);
    }
    AccumulateCategoricalKeyed(
        TRRService::ProviderKey(),
        mTRRSuccess ? Telemetry::LABELS_DNS_LOOKUP_DISPOSITION3::trrOK
                    : Telemetry::LABELS_DNS_LOOKUP_DISPOSITION3::trrFail);
  }

  if (nsHostResolver::Mode() == nsIDNSService::MODE_TRRFIRST) {
    MOZ_ASSERT(mTRRSkippedReason != mozilla::net::TRRSkippedReason::TRR_UNSET);

    if (StaticPrefs::network_trr_strict_native_fallback()) {
      nsAutoCString telemetryKey(TRRService::ProviderKey());

      if (mFirstTRRSkippedReason != mozilla::net::TRRSkippedReason::TRR_UNSET) {
        telemetryKey.AppendLiteral("|");
        telemetryKey.AppendInt(static_cast<uint32_t>(mFirstTRRSkippedReason));

        Telemetry::Accumulate(mTRRSuccess
                                  ? Telemetry::TRR_SKIP_REASON_RETRY_SUCCESS
                                  : Telemetry::TRR_SKIP_REASON_RETRY_FAILED,
                              TRRService::ProviderKey(),
                              static_cast<uint32_t>(mFirstTRRSkippedReason));
      }

      Telemetry::Accumulate(Telemetry::TRR_SKIP_REASON_STRICT_MODE,
                            telemetryKey,
                            static_cast<uint32_t>(mTRRSkippedReason));

      if (mTRRSuccess) {
        Telemetry::Accumulate(Telemetry::TRR_ATTEMPT_COUNT,
                              TRRService::ProviderKey(), mTrrAttempts);
      } else if (LoadNativeUsed()) {
        Telemetry::Accumulate(mNativeSuccess
                                  ? Telemetry::TRR_SKIP_REASON_NATIVE_SUCCESS
                                  : Telemetry::TRR_SKIP_REASON_NATIVE_FAILED,
                              TRRService::ProviderKey(),
                              static_cast<uint32_t>(mTRRSkippedReason));
      }
    } else {
      Telemetry::Accumulate(Telemetry::TRR_SKIP_REASON_TRR_FIRST2,
                            TRRService::ProviderKey(),
                            static_cast<uint32_t>(mTRRSkippedReason));
      if (!mTRRSuccess) {
        Telemetry::Accumulate(mNativeSuccess
                                  ? Telemetry::TRR_SKIP_REASON_NATIVE_SUCCESS
                                  : Telemetry::TRR_SKIP_REASON_NATIVE_FAILED,
                              TRRService::ProviderKey(),
                              static_cast<uint32_t>(mTRRSkippedReason));
      }

      if (IsRelevantTRRSkipReason(mTRRSkippedReason)) {
        Telemetry::Accumulate(Telemetry::TRR_RELEVANT_SKIP_REASON_TRR_FIRST,
                              TRRService::ProviderKey(),
                              static_cast<uint32_t>(mTRRSkippedReason));

        if (!mTRRSuccess && LoadNativeUsed()) {
          Telemetry::Accumulate(
              mNativeSuccess
                  ? Telemetry::TRR_RELEVANT_SKIP_REASON_NATIVE_SUCCESS
                  : Telemetry::TRR_RELEVANT_SKIP_REASON_NATIVE_FAILED,
              TRRService::ProviderKey(),
              static_cast<uint32_t>(mTRRSkippedReason));
        }
      }
    }
  }

  if (mEffectiveTRRMode == nsIRequest::TRR_FIRST_MODE) {
    if (flags & nsIDNSService::RESOLVE_DISABLE_TRR) {
      // TRR is disabled on request, which is a next-level back-off method.
      Telemetry::Accumulate(Telemetry::DNS_TRR_DISABLED3,
                            TRRService::ProviderKey(), mNativeSuccess);
    } else {
      if (mTRRSuccess) {
        AccumulateCategoricalKeyed(TRRService::ProviderKey(),
                                   Telemetry::LABELS_DNS_TRR_FIRST4::TRR);
      } else if (mNativeSuccess) {
        if (mResolverType == DNSResolverType::TRR) {
          AccumulateCategoricalKeyed(
              TRRService::ProviderKey(),
              Telemetry::LABELS_DNS_TRR_FIRST4::NativeAfterTRR);
        } else {
          AccumulateCategoricalKeyed(TRRService::ProviderKey(),
                                     Telemetry::LABELS_DNS_TRR_FIRST4::Native);
        }
      } else {
        AccumulateCategoricalKeyed(
            TRRService::ProviderKey(),
            Telemetry::LABELS_DNS_TRR_FIRST4::BothFailed);
      }
    }
  }

  switch (mEffectiveTRRMode) {
    case nsIRequest::TRR_DISABLED_MODE:
      AccumulateCategorical(Telemetry::LABELS_DNS_LOOKUP_ALGORITHM::nativeOnly);
      break;
    case nsIRequest::TRR_FIRST_MODE:
      AccumulateCategorical(Telemetry::LABELS_DNS_LOOKUP_ALGORITHM::trrFirst);
      break;
    case nsIRequest::TRR_ONLY_MODE:
      AccumulateCategorical(Telemetry::LABELS_DNS_LOOKUP_ALGORITHM::trrOnly);
      break;
    case nsIRequest::TRR_DEFAULT_MODE:
      MOZ_ASSERT_UNREACHABLE("We should not have a default value here");
      break;
  }

  if (mResolverType == DNSResolverType::TRR &&
      !StaticPrefs::network_trr_strict_native_fallback() && !mTRRSuccess &&
      mNativeSuccess && TRRService::Get()) {
    TRRService::Get()->AddToBlocklist(nsCString(host), originSuffix, pb, true);
  }
}

AddrHostRecord::DnsPriority AddrHostRecord::GetPriority(uint16_t aFlags) {
  if (IsHighPriority(aFlags)) {
    return AddrHostRecord::DNS_PRIORITY_HIGH;
  }
  if (IsMediumPriority(aFlags)) {
    return AddrHostRecord::DNS_PRIORITY_MEDIUM;
  }

  return AddrHostRecord::DNS_PRIORITY_LOW;
}

//----------------------------------------------------------------------------
// TypeHostRecord
//----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS_INHERITED(TypeHostRecord, nsHostRecord, TypeHostRecord,
                            nsIDNSTXTRecord, nsIDNSHTTPSSVCRecord)

TypeHostRecord::TypeHostRecord(const nsHostKey& key)
    : nsHostRecord(key), DNSHTTPSSVCRecordBase(key.host) {}

TypeHostRecord::~TypeHostRecord() { mCallbacks.clear(); }

bool TypeHostRecord::HasUsableResultInternal(const mozilla::TimeStamp& now,
                                             uint16_t queryFlags) const {
  if (CheckExpiration(now) == EXP_EXPIRED) {
    return false;
  }

  if (negative) {
    return true;
  }

  return !mResults.is<Nothing>();
}

bool TypeHostRecord::RefreshForNegativeResponse() const { return false; }

NS_IMETHODIMP TypeHostRecord::GetRecords(CopyableTArray<nsCString>& aRecords) {
  // deep copy
  MutexAutoLock lock(mResultsLock);

  if (!mResults.is<TypeRecordTxt>()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aRecords = mResults.as<CopyableTArray<nsCString>>();
  return NS_OK;
}

NS_IMETHODIMP TypeHostRecord::GetRecordsAsOneString(nsACString& aRecords) {
  // deep copy
  MutexAutoLock lock(mResultsLock);

  if (!mResults.is<TypeRecordTxt>()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  auto& results = mResults.as<CopyableTArray<nsCString>>();
  for (uint32_t i = 0; i < results.Length(); i++) {
    aRecords.Append(results[i]);
  }
  return NS_OK;
}

size_t TypeHostRecord::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const {
  size_t n = mallocSizeOf(this);

  n += nsHostKey::SizeOfExcludingThis(mallocSizeOf);
  n += SizeOfResolveHostCallbackListExcludingHead(mCallbacks, mallocSizeOf);

  return n;
}

uint32_t TypeHostRecord::GetType() {
  MutexAutoLock lock(mResultsLock);

  return mResults.match(
      [](TypeRecordEmpty&) {
        MOZ_ASSERT(false, "This should never be the case");
        return nsIDNSService::RESOLVE_TYPE_DEFAULT;
      },
      [](TypeRecordTxt&) { return nsIDNSService::RESOLVE_TYPE_TXT; },
      [](TypeRecordHTTPSSVC&) { return nsIDNSService::RESOLVE_TYPE_HTTPSSVC; });
}

TypeRecordResultType TypeHostRecord::GetResults() {
  MutexAutoLock lock(mResultsLock);
  return mResults;
}

NS_IMETHODIMP
TypeHostRecord::GetRecords(nsTArray<RefPtr<nsISVCBRecord>>& aRecords) {
  MutexAutoLock lock(mResultsLock);
  if (!mResults.is<TypeRecordHTTPSSVC>()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto& results = mResults.as<TypeRecordHTTPSSVC>();

  for (const SVCB& r : results) {
    RefPtr<nsISVCBRecord> rec = new mozilla::net::SVCBRecord(r);
    aRecords.AppendElement(rec);
  }

  return NS_OK;
}

NS_IMETHODIMP
TypeHostRecord::GetServiceModeRecord(bool aNoHttp2, bool aNoHttp3,
                                     nsISVCBRecord** aRecord) {
  MutexAutoLock lock(mResultsLock);
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
TypeHostRecord::GetAllRecordsWithEchConfig(
    bool aNoHttp2, bool aNoHttp3, bool* aAllRecordsHaveEchConfig,
    bool* aAllRecordsInH3ExcludedList,
    nsTArray<RefPtr<nsISVCBRecord>>& aResult) {
  MutexAutoLock lock(mResultsLock);
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
TypeHostRecord::GetHasIPAddresses(bool* aResult) {
  NS_ENSURE_ARG(aResult);
  MutexAutoLock lock(mResultsLock);

  if (!mResults.is<TypeRecordHTTPSSVC>()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto& results = mResults.as<TypeRecordHTTPSSVC>();
  *aResult = HasIPAddressesInternal(results);
  return NS_OK;
}

NS_IMETHODIMP
TypeHostRecord::GetAllRecordsExcluded(bool* aResult) {
  NS_ENSURE_ARG(aResult);
  MutexAutoLock lock(mResultsLock);

  if (!mResults.is<TypeRecordHTTPSSVC>()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aResult = mAllRecordsExcluded;
  return NS_OK;
}

NS_IMETHODIMP
TypeHostRecord::GetTtl(uint32_t* aResult) {
  NS_ENSURE_ARG(aResult);
  *aResult = mTtl;
  return NS_OK;
}
