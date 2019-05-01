/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheIndex.h"
#include "CacheLog.h"
#include "CacheFileUtils.h"
#include "LoadContextInfo.h"
#include "mozilla/Tokenizer.h"
#include "mozilla/Telemetry.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include <algorithm>
#include "mozilla/Unused.h"

namespace mozilla {
namespace net {
namespace CacheFileUtils {

// This designates the format for the "alt-data" metadata.
// When the format changes we need to update the version.
static uint32_t const kAltDataVersion = 1;
const char* kAltDataKey = "alt-data";

static uint32_t const kBaseDomainAccessInfoVersion = 1;

namespace {

/**
 * A simple recursive descent parser for the mapping key.
 */
class KeyParser : protected Tokenizer {
 public:
  explicit KeyParser(nsACString const& aInput)
      : Tokenizer(aInput),
        isAnonymous(false)
        // Initialize the cache key to a zero length by default
        ,
        lastTag(0) {}

 private:
  // Results
  OriginAttributes originAttribs;
  bool isAnonymous;
  nsCString idEnhance;
  nsDependentCSubstring cacheKey;

  // Keeps the last tag name, used for alphabetical sort checking
  char lastTag;

  // Classifier for the 'tag' character valid range.
  // Explicitly using unsigned char as 127 is -1 when signed and it would only
  // produce a warning.
  static bool TagChar(const char aChar) {
    unsigned char c = static_cast<unsigned char>(aChar);
    return c >= ' ' && c <= '\x7f';
  }

  bool ParseTags() {
    // Expects to be at the tag name or at the end
    if (CheckEOF()) {
      return true;
    }

    char tag;
    if (!ReadChar(&TagChar, &tag)) {
      return false;
    }

    // Check the alphabetical order, hard-fail on disobedience
    if (!(lastTag < tag || tag == ':')) {
      return false;
    }
    lastTag = tag;

    switch (tag) {
      case ':':
        // last possible tag, when present there is the cacheKey following,
        // not terminated with ',' and no need to unescape.
        cacheKey.Rebind(mCursor, mEnd - mCursor);
        return true;
      case 'O': {
        nsAutoCString originSuffix;
        if (!ParseValue(&originSuffix) ||
            !originAttribs.PopulateFromSuffix(originSuffix)) {
          return false;
        }
        break;
      }
      case 'p':
        originAttribs.SyncAttributesWithPrivateBrowsing(true);
        break;
      case 'b':
        // Leaving to be able to read and understand oldformatted entries
        originAttribs.mInIsolatedMozBrowser = true;
        break;
      case 'a':
        isAnonymous = true;
        break;
      case 'i': {
        // Leaving to be able to read and understand oldformatted entries
        uint32_t deprecatedAppId = 0;
        if (!ReadInteger(&deprecatedAppId)) {
          return false;  // not a valid 32-bit integer
        }
        break;
      }
      case '~':
        if (!ParseValue(&idEnhance)) {
          return false;
        }
        break;
      default:
        if (!ParseValue()) {  // skip any tag values, optional
          return false;
        }
        break;
    }

    // We expect a comma after every tag
    if (!CheckChar(',')) {
      return false;
    }

    // Recurse to the next tag
    return ParseTags();
  }

  bool ParseValue(nsACString* result = nullptr) {
    // If at the end, fail since we expect a comma ; value may be empty tho
    if (CheckEOF()) {
      return false;
    }

    Token t;
    while (Next(t)) {
      if (!Token::Char(',').Equals(t)) {
        if (result) {
          result->Append(t.Fragment());
        }
        continue;
      }

      if (CheckChar(',')) {
        // Two commas in a row, escaping
        if (result) {
          result->Append(',');
        }
        continue;
      }

      // We must give the comma back since the upper calls expect it
      Rollback();
      return true;
    }

    return false;
  }

 public:
  already_AddRefed<LoadContextInfo> Parse() {
    RefPtr<LoadContextInfo> info;
    if (ParseTags()) {
      info = GetLoadContextInfo(isAnonymous, originAttribs);
    }

    return info.forget();
  }

  void URISpec(nsACString& result) { result.Assign(cacheKey); }

  void IdEnhance(nsACString& result) { result.Assign(idEnhance); }
};

}  // namespace

already_AddRefed<nsILoadContextInfo> ParseKey(const nsACString& aKey,
                                              nsACString* aIdEnhance,
                                              nsACString* aURISpec) {
  KeyParser parser(aKey);
  RefPtr<LoadContextInfo> info = parser.Parse();

  if (info) {
    if (aIdEnhance) parser.IdEnhance(*aIdEnhance);
    if (aURISpec) parser.URISpec(*aURISpec);
  }

  return info.forget();
}

void AppendKeyPrefix(nsILoadContextInfo* aInfo, nsACString& _retval) {
  /**
   * This key is used to salt file hashes.  When form of the key is changed
   * cache entries will fail to find on disk.
   *
   * IMPORTANT NOTE:
   * Keep the attributes list sorted according their ASCII code.
   */

  OriginAttributes const* oa = aInfo->OriginAttributesPtr();
  nsAutoCString suffix;
  oa->CreateSuffix(suffix);
  if (!suffix.IsEmpty()) {
    AppendTagWithValue(_retval, 'O', suffix);
  }

  if (aInfo->IsAnonymous()) {
    _retval.AppendLiteral("a,");
  }

  if (aInfo->IsPrivate()) {
    _retval.AppendLiteral("p,");
  }
}

void AppendTagWithValue(nsACString& aTarget, char const aTag,
                        const nsACString& aValue) {
  aTarget.Append(aTag);

  // First check the value string to save some memory copying
  // for cases we don't need to escape at all (most likely).
  if (!aValue.IsEmpty()) {
    if (!aValue.Contains(',')) {
      // No need to escape
      aTarget.Append(aValue);
    } else {
      nsAutoCString escapedValue(aValue);
      escapedValue.ReplaceSubstring(NS_LITERAL_CSTRING(","),
                                    NS_LITERAL_CSTRING(",,"));
      aTarget.Append(escapedValue);
    }
  }

  aTarget.Append(',');
}

nsresult KeyMatchesLoadContextInfo(const nsACString& aKey,
                                   nsILoadContextInfo* aInfo, bool* _retval) {
  nsCOMPtr<nsILoadContextInfo> info = ParseKey(aKey);

  if (!info) {
    return NS_ERROR_FAILURE;
  }

  *_retval = info->Equals(aInfo);
  return NS_OK;
}

ValidityPair::ValidityPair(uint32_t aOffset, uint32_t aLen)
    : mOffset(aOffset), mLen(aLen) {}

bool ValidityPair::CanBeMerged(const ValidityPair& aOther) const {
  // The pairs can be merged into a single one if the start of one of the pairs
  // is placed anywhere in the validity interval of other pair or exactly after
  // its end.
  return IsInOrFollows(aOther.mOffset) || aOther.IsInOrFollows(mOffset);
}

bool ValidityPair::IsInOrFollows(uint32_t aOffset) const {
  return mOffset <= aOffset && mOffset + mLen >= aOffset;
}

bool ValidityPair::LessThan(const ValidityPair& aOther) const {
  if (mOffset < aOther.mOffset) {
    return true;
  }

  if (mOffset == aOther.mOffset && mLen < aOther.mLen) {
    return true;
  }

  return false;
}

void ValidityPair::Merge(const ValidityPair& aOther) {
  MOZ_ASSERT(CanBeMerged(aOther));

  uint32_t offset = std::min(mOffset, aOther.mOffset);
  uint32_t end = std::max(mOffset + mLen, aOther.mOffset + aOther.mLen);

  mOffset = offset;
  mLen = end - offset;
}

void ValidityMap::Log() const {
  LOG(("ValidityMap::Log() - number of pairs: %zu", mMap.Length()));
  for (uint32_t i = 0; i < mMap.Length(); i++) {
    LOG(("    (%u, %u)", mMap[i].Offset() + 0, mMap[i].Len() + 0));
  }
}

uint32_t ValidityMap::Length() const { return mMap.Length(); }

void ValidityMap::AddPair(uint32_t aOffset, uint32_t aLen) {
  ValidityPair pair(aOffset, aLen);

  if (mMap.Length() == 0) {
    mMap.AppendElement(pair);
    return;
  }

  // Find out where to place this pair into the map, it can overlap only with
  // one preceding pair and all subsequent pairs.
  uint32_t pos = 0;
  for (pos = mMap.Length(); pos > 0;) {
    --pos;

    if (mMap[pos].LessThan(pair)) {
      // The new pair should be either inserted after pos or merged with it.
      if (mMap[pos].CanBeMerged(pair)) {
        // Merge with the preceding pair
        mMap[pos].Merge(pair);
      } else {
        // They don't overlap, element must be placed after pos element
        ++pos;
        if (pos == mMap.Length()) {
          mMap.AppendElement(pair);
        } else {
          mMap.InsertElementAt(pos, pair);
        }
      }

      break;
    }

    if (pos == 0) {
      // The new pair should be placed in front of all existing pairs.
      mMap.InsertElementAt(0, pair);
    }
  }

  // pos now points to merged or inserted pair, check whether it overlaps with
  // subsequent pairs.
  while (pos + 1 < mMap.Length()) {
    if (mMap[pos].CanBeMerged(mMap[pos + 1])) {
      mMap[pos].Merge(mMap[pos + 1]);
      mMap.RemoveElementAt(pos + 1);
    } else {
      break;
    }
  }
}

void ValidityMap::Clear() { mMap.Clear(); }

size_t ValidityMap::SizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return mMap.ShallowSizeOfExcludingThis(mallocSizeOf);
}

ValidityPair& ValidityMap::operator[](uint32_t aIdx) {
  return mMap.ElementAt(aIdx);
}

StaticMutex DetailedCacheHitTelemetry::sLock;
uint32_t DetailedCacheHitTelemetry::sRecordCnt = 0;
DetailedCacheHitTelemetry::HitRate
    DetailedCacheHitTelemetry::sHRStats[kNumOfRanges];

DetailedCacheHitTelemetry::HitRate::HitRate() { Reset(); }

void DetailedCacheHitTelemetry::HitRate::AddRecord(ERecType aType) {
  if (aType == HIT) {
    ++mHitCnt;
  } else {
    ++mMissCnt;
  }
}

uint32_t DetailedCacheHitTelemetry::HitRate::GetHitRateBucket(
    uint32_t aNumOfBuckets) const {
  uint32_t bucketIdx = (aNumOfBuckets * mHitCnt) / (mHitCnt + mMissCnt);
  if (bucketIdx ==
      aNumOfBuckets) {  // make sure 100% falls into the last bucket
    --bucketIdx;
  }

  return bucketIdx;
}

uint32_t DetailedCacheHitTelemetry::HitRate::Count() {
  return mHitCnt + mMissCnt;
}

void DetailedCacheHitTelemetry::HitRate::Reset() {
  mHitCnt = 0;
  mMissCnt = 0;
}

// static
void DetailedCacheHitTelemetry::AddRecord(ERecType aType,
                                          TimeStamp aLoadStart) {
  bool isUpToDate = false;
  CacheIndex::IsUpToDate(&isUpToDate);
  if (!isUpToDate) {
    // Ignore the record when the entry file count might be incorrect
    return;
  }

  uint32_t entryCount;
  nsresult rv = CacheIndex::GetEntryFileCount(&entryCount);
  if (NS_FAILED(rv)) {
    return;
  }

  uint32_t rangeIdx = entryCount / kRangeSize;
  if (rangeIdx >= kNumOfRanges) {  // The last range has no upper limit.
    rangeIdx = kNumOfRanges - 1;
  }

  uint32_t hitMissValue = 2 * rangeIdx;  // 2 values per range
  if (aType == MISS) {                   // The order is HIT, MISS
    ++hitMissValue;
  }

  StaticMutexAutoLock lock(sLock);

  if (aType == MISS) {
    mozilla::Telemetry::AccumulateTimeDelta(
        mozilla::Telemetry::NETWORK_CACHE_V2_MISS_TIME_MS, aLoadStart);
  } else {
    mozilla::Telemetry::AccumulateTimeDelta(
        mozilla::Telemetry::NETWORK_CACHE_V2_HIT_TIME_MS, aLoadStart);
  }

  Telemetry::Accumulate(Telemetry::NETWORK_CACHE_HIT_MISS_STAT_PER_CACHE_SIZE,
                        hitMissValue);

  sHRStats[rangeIdx].AddRecord(aType);
  ++sRecordCnt;

  if (sRecordCnt < kTotalSamplesReportLimit) {
    return;
  }

  sRecordCnt = 0;

  for (uint32_t i = 0; i < kNumOfRanges; ++i) {
    if (sHRStats[i].Count() >= kHitRateSamplesReportLimit) {
      // The telemetry enums are grouped by buckets as follows:
      // Telemetry value : 0,1,2,3, ... ,19,20,21,22, ... ,398,399
      // Hit rate bucket : 0,0,0,0, ... , 0, 1, 1, 1, ... , 19, 19
      // Cache size range: 0,1,2,3, ... ,19, 0, 1, 2, ... , 18, 19
      uint32_t bucketOffset =
          sHRStats[i].GetHitRateBucket(kHitRateBuckets) * kNumOfRanges;

      Telemetry::Accumulate(Telemetry::NETWORK_CACHE_HIT_RATE_PER_CACHE_SIZE,
                            bucketOffset + i);
      sHRStats[i].Reset();
    }
  }
}

StaticMutex CachePerfStats::sLock;
CachePerfStats::PerfData CachePerfStats::sData[CachePerfStats::LAST];
uint32_t CachePerfStats::sCacheSlowCnt = 0;
uint32_t CachePerfStats::sCacheNotSlowCnt = 0;

CachePerfStats::MMA::MMA(uint32_t aTotalWeight, bool aFilter)
    : mSum(0), mSumSq(0), mCnt(0), mWeight(aTotalWeight), mFilter(aFilter) {}

void CachePerfStats::MMA::AddValue(uint32_t aValue) {
  if (mFilter) {
    // Filter high spikes
    uint32_t avg = GetAverage();
    uint32_t stddev = GetStdDev();
    uint32_t maxdiff = avg + (3 * stddev);
    if (avg && aValue > avg + maxdiff) {
      return;
    }
  }

  if (mCnt < mWeight) {
    // Compute arithmetic average until we have at least mWeight values
    CheckedInt<uint64_t> newSumSq = CheckedInt<uint64_t>(aValue) * aValue;
    newSumSq += mSumSq;
    if (!newSumSq.isValid()) {
      return;  // ignore this value
    }
    mSumSq = newSumSq.value();
    mSum += aValue;
    ++mCnt;
  } else {
    CheckedInt<uint64_t> newSumSq = mSumSq - mSumSq / mCnt;
    newSumSq += static_cast<uint64_t>(aValue) * aValue;
    if (!newSumSq.isValid()) {
      return;  // ignore this value
    }
    mSumSq = newSumSq.value();

    // Compute modified moving average for more values:
    // newAvg = ((weight - 1) * oldAvg + newValue) / weight
    mSum -= GetAverage();
    mSum += aValue;
  }
}

uint32_t CachePerfStats::MMA::GetAverage() {
  if (mCnt == 0) {
    return 0;
  }

  return mSum / mCnt;
}

uint32_t CachePerfStats::MMA::GetStdDev() {
  if (mCnt == 0) {
    return 0;
  }

  uint32_t avg = GetAverage();
  uint64_t avgSq = static_cast<uint64_t>(avg) * avg;
  uint64_t variance = mSumSq / mCnt;
  if (variance < avgSq) {
    // Due to rounding error when using integer data type, it can happen that
    // average of squares of the values is smaller than square of the average
    // of the values. In this case fix mSumSq.
    variance = avgSq;
    mSumSq = variance * mCnt;
  }

  variance -= avgSq;
  return sqrt(static_cast<double>(variance));
}

CachePerfStats::PerfData::PerfData()
    : mFilteredAvg(50, true), mShortAvg(3, false) {}

void CachePerfStats::PerfData::AddValue(uint32_t aValue, bool aShortOnly) {
  if (!aShortOnly) {
    mFilteredAvg.AddValue(aValue);
  }
  mShortAvg.AddValue(aValue);
}

uint32_t CachePerfStats::PerfData::GetAverage(bool aFiltered) {
  return aFiltered ? mFilteredAvg.GetAverage() : mShortAvg.GetAverage();
}

uint32_t CachePerfStats::PerfData::GetStdDev(bool aFiltered) {
  return aFiltered ? mFilteredAvg.GetStdDev() : mShortAvg.GetStdDev();
}

// static
void CachePerfStats::AddValue(EDataType aType, uint32_t aValue,
                              bool aShortOnly) {
  StaticMutexAutoLock lock(sLock);
  sData[aType].AddValue(aValue, aShortOnly);
}

// static
uint32_t CachePerfStats::GetAverage(EDataType aType, bool aFiltered) {
  StaticMutexAutoLock lock(sLock);
  return sData[aType].GetAverage(aFiltered);
}

// static
uint32_t CachePerfStats::GetStdDev(EDataType aType, bool aFiltered) {
  StaticMutexAutoLock lock(sLock);
  return sData[aType].GetStdDev(aFiltered);
}

// static
bool CachePerfStats::IsCacheSlow() {
  // Compare mShortAvg with mFilteredAvg to find out whether cache is getting
  // slower. Use only data about single IO operations because ENTRY_OPEN can be
  // affected by more factors than a slow disk.
  for (uint32_t i = 0; i < ENTRY_OPEN; ++i) {
    if (i == IO_WRITE) {
      // Skip this data type. IsCacheSlow is used for determining cache slowness
      // when opening entries. Writes have low priority and it's normal that
      // they are delayed a lot, but this doesn't necessarily affect opening
      // cache entries.
      continue;
    }

    uint32_t avgLong = sData[i].GetAverage(true);
    if (avgLong == 0) {
      // We have no perf data yet, skip this data type.
      continue;
    }
    uint32_t avgShort = sData[i].GetAverage(false);
    uint32_t stddevLong = sData[i].GetStdDev(true);
    uint32_t maxdiff = avgLong + (3 * stddevLong);

    if (avgShort > avgLong + maxdiff) {
      LOG(
          ("CachePerfStats::IsCacheSlow() - result is slow based on perf "
           "type %u [avgShort=%u, avgLong=%u, stddevLong=%u]",
           i, avgShort, avgLong, stddevLong));
      ++sCacheSlowCnt;
      return true;
    }
  }

  ++sCacheNotSlowCnt;
  return false;
}

// static
void CachePerfStats::GetSlowStats(uint32_t* aSlow, uint32_t* aNotSlow) {
  *aSlow = sCacheSlowCnt;
  *aNotSlow = sCacheNotSlowCnt;
}

void FreeBuffer(void* aBuf) {
#ifndef NS_FREE_PERMANENT_DATA
  if (CacheObserver::ShuttingDown()) {
    return;
  }
#endif

  free(aBuf);
}

nsresult ParseAlternativeDataInfo(const char* aInfo, int64_t* _offset,
                                  nsACString* _type) {
  // The format is: "1;12345,javascript/binary"
  //         <version>;<offset>,<type>
  mozilla::Tokenizer p(aInfo, nullptr, "/");
  uint32_t altDataVersion = 0;
  int64_t altDataOffset = -1;

  // The metadata format has a wrong version number.
  if (!p.ReadInteger(&altDataVersion) || altDataVersion != kAltDataVersion) {
    LOG(
        ("ParseAlternativeDataInfo() - altDataVersion=%u, "
         "expectedVersion=%u",
         altDataVersion, kAltDataVersion));
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!p.CheckChar(';') || !p.ReadInteger(&altDataOffset) ||
      !p.CheckChar(',')) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // The requested alt-data representation is not available
  if (altDataOffset < 0) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (_offset) {
    *_offset = altDataOffset;
  }

  if (_type) {
    mozilla::Unused << p.ReadUntil(Tokenizer::Token::EndOfFile(), *_type);
  }

  return NS_OK;
}

void BuildAlternativeDataInfo(const char* aInfo, int64_t aOffset,
                              nsACString& _retval) {
  _retval.Truncate();
  _retval.AppendInt(kAltDataVersion);
  _retval.Append(';');
  _retval.AppendInt(aOffset);
  _retval.Append(',');
  _retval.Append(aInfo);
}

nsresult ParseBaseDomainAccessInfo(const char* aInfo, uint32_t aTrID,
                                   const uint32_t* aSearchSiteID, bool* _found,
                                   uint16_t* _count) {
  // The format is: "1;12;339456,490536687,1964820,"
  //         <version>;<telemetry_report_ID>;<siteID>,<siteID>,
  mozilla::Tokenizer p(aInfo);
  uint32_t i = 0;
  uint16_t siteIDCnt = 0;

  // Check version and telemetry report ID
  if (!p.ReadInteger(&i) || i != kBaseDomainAccessInfoVersion ||
      !p.CheckChar(';') || !p.ReadInteger(&i) || i != aTrID ||
      !p.CheckChar(';')) {
    LOG(
        ("ParseBaseDomainAccessInfo() - cannot parse info [info=%s, version=%u,"
         " trID=%u]",
         aInfo, kBaseDomainAccessInfoVersion, aTrID));
    return NS_ERROR_NOT_AVAILABLE;
  }

  do {
    if (!p.ReadInteger(&i) || !p.CheckChar(',')) {
      LOG(
          ("ParseBaseDomainAccessInfo() - cannot parse site ID [info=%s, "
           "siteIDCnt=%d]",
           aInfo, siteIDCnt));
      return NS_ERROR_NOT_AVAILABLE;
    }

    // If aSearchSiteID was provided, we don't need the total count of IDs.
    // Just return true and don't process the rest of data.
    if (aSearchSiteID && *aSearchSiteID == i) {
      *_found = true;
      return NS_OK;
    }

    ++siteIDCnt;
  } while (!p.CheckEOF());

  if (_count) {
    *_count = siteIDCnt;
  }

  return NS_OK;
}

void BuildOrAppendBaseDomainAccessInfo(const char* aOldInfo, uint32_t aTrID,
                                       uint32_t aSiteID, nsACString& _retval) {
  if (aOldInfo) {
    _retval.Assign(aOldInfo);
  } else {
    _retval.Truncate();
    _retval.AppendInt(kBaseDomainAccessInfoVersion);
    _retval.Append(';');
    _retval.AppendInt(aTrID);
    _retval.Append(';');
  }

  _retval.AppendInt(aSiteID);
  _retval.Append(',');
}

}  // namespace CacheFileUtils
}  // namespace net
}  // namespace mozilla
