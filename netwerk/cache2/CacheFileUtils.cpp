/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheFileUtils.h"
#include "LoadContextInfo.h"
#include "mozilla/Tokenizer.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include <algorithm>


namespace mozilla {
namespace net {
namespace CacheFileUtils {

namespace {

/**
 * A simple recursive descent parser for the mapping key.
 */
class KeyParser : protected Tokenizer
{
public:
  explicit KeyParser(nsACString const& aInput)
    : Tokenizer(aInput)
    // Initialize attributes to their default values
    , originAttribs(0, false)
    , isPrivate(false)
    , isAnonymous(false)
    // Initialize the cache key to a zero length by default
    , lastTag(0)
  {
  }

private:
  // Results
  NeckoOriginAttributes originAttribs;
  bool isPrivate;
  bool isAnonymous;
  nsCString idEnhance;
  nsDependentCSubstring cacheKey;

  // Keeps the last tag name, used for alphabetical sort checking
  char lastTag;

  // Classifier for the 'tag' character valid range
  static bool TagChar(const char aChar)
  {
    return aChar >= ' ' && aChar <= '~';
  }

  bool ParseTags()
  {
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
      if (!ParseValue(&originSuffix) || !originAttribs.PopulateFromSuffix(originSuffix)) {
        return false;
      }
      break;
    }
    case 'p':
      isPrivate = true;
      break;
    case 'b':
      // Leaving to be able to read and understand oldformatted entries
      originAttribs.mInBrowser = true;
      break;
    case 'a':
      isAnonymous = true;
      break;
    case 'i': {
      // Leaving to be able to read and understand oldformatted entries
      if (!ReadInteger(&originAttribs.mAppId)) {
        return false; // not a valid 32-bit integer
      }
      break;
    }
    case '~':
      if (!ParseValue(&idEnhance)) {
        return false;
      }
      break;
    default:
      if (!ParseValue()) { // skip any tag values, optional
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

  bool ParseValue(nsACString *result = nullptr)
  {
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
  already_AddRefed<LoadContextInfo> Parse()
  {
    RefPtr<LoadContextInfo> info;
    if (ParseTags()) {
      info = GetLoadContextInfo(isPrivate, isAnonymous, originAttribs);
    }

    return info.forget();
  }

  void URISpec(nsACString &result)
  {
    result.Assign(cacheKey);
  }

  void IdEnhance(nsACString &result)
  {
    result.Assign(idEnhance);
  }
};

} // namespace

already_AddRefed<nsILoadContextInfo>
ParseKey(const nsCSubstring &aKey,
         nsCSubstring *aIdEnhance,
         nsCSubstring *aURISpec)
{
  KeyParser parser(aKey);
  RefPtr<LoadContextInfo> info = parser.Parse();

  if (info) {
    if (aIdEnhance)
      parser.IdEnhance(*aIdEnhance);
    if (aURISpec)
      parser.URISpec(*aURISpec);
  }

  return info.forget();
}

void
AppendKeyPrefix(nsILoadContextInfo* aInfo, nsACString &_retval)
{
  /**
   * This key is used to salt file hashes.  When form of the key is changed
   * cache entries will fail to find on disk.
   *
   * IMPORTANT NOTE:
   * Keep the attributes list sorted according their ASCII code.
   */

  NeckoOriginAttributes const *oa = aInfo->OriginAttributesPtr();
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

void
AppendTagWithValue(nsACString & aTarget, char const aTag, nsCSubstring const & aValue)
{
  aTarget.Append(aTag);

  // First check the value string to save some memory copying
  // for cases we don't need to escape at all (most likely).
  if (!aValue.IsEmpty()) {
    if (!aValue.Contains(',')) {
      // No need to escape
      aTarget.Append(aValue);
    } else {
      nsAutoCString escapedValue(aValue);
      escapedValue.ReplaceSubstring(
        NS_LITERAL_CSTRING(","), NS_LITERAL_CSTRING(",,"));
      aTarget.Append(escapedValue);
    }
  }

  aTarget.Append(',');
}

nsresult
KeyMatchesLoadContextInfo(const nsACString &aKey, nsILoadContextInfo *aInfo,
                          bool *_retval)
{
  nsCOMPtr<nsILoadContextInfo> info = ParseKey(aKey);

  if (!info) {
    return NS_ERROR_FAILURE;
  }

  *_retval = info->Equals(aInfo);
  return NS_OK;
}

ValidityPair::ValidityPair(uint32_t aOffset, uint32_t aLen)
  : mOffset(aOffset), mLen(aLen)
{}

ValidityPair&
ValidityPair::operator=(const ValidityPair& aOther)
{
  mOffset = aOther.mOffset;
  mLen = aOther.mLen;
  return *this;
}

bool
ValidityPair::CanBeMerged(const ValidityPair& aOther) const
{
  // The pairs can be merged into a single one if the start of one of the pairs
  // is placed anywhere in the validity interval of other pair or exactly after
  // its end.
  return IsInOrFollows(aOther.mOffset) || aOther.IsInOrFollows(mOffset);
}

bool
ValidityPair::IsInOrFollows(uint32_t aOffset) const
{
  return mOffset <= aOffset && mOffset + mLen >= aOffset;
}

bool
ValidityPair::LessThan(const ValidityPair& aOther) const
{
  if (mOffset < aOther.mOffset) {
    return true;
  }

  if (mOffset == aOther.mOffset && mLen < aOther.mLen) {
    return true;
  }

  return false;
}

void
ValidityPair::Merge(const ValidityPair& aOther)
{
  MOZ_ASSERT(CanBeMerged(aOther));

  uint32_t offset = std::min(mOffset, aOther.mOffset);
  uint32_t end = std::max(mOffset + mLen, aOther.mOffset + aOther.mLen);

  mOffset = offset;
  mLen = end - offset;
}

void
ValidityMap::Log() const
{
  LOG(("ValidityMap::Log() - number of pairs: %u", mMap.Length()));
  for (uint32_t i=0; i<mMap.Length(); i++) {
    LOG(("    (%u, %u)", mMap[i].Offset() + 0, mMap[i].Len() + 0));
  }
}

uint32_t
ValidityMap::Length() const
{
  return mMap.Length();
}

void
ValidityMap::AddPair(uint32_t aOffset, uint32_t aLen)
{
  ValidityPair pair(aOffset, aLen);

  if (mMap.Length() == 0) {
    mMap.AppendElement(pair);
    return;
  }

  // Find out where to place this pair into the map, it can overlap only with
  // one preceding pair and all subsequent pairs.
  uint32_t pos = 0;
  for (pos = mMap.Length(); pos > 0; ) {
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

void
ValidityMap::Clear()
{
  mMap.Clear();
}

size_t
ValidityMap::SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  return mMap.ShallowSizeOfExcludingThis(mallocSizeOf);
}

ValidityPair&
ValidityMap::operator[](uint32_t aIdx)
{
  return mMap.ElementAt(aIdx);
}

StaticMutex DetailedCacheHitTelemetry::sLock;
uint32_t DetailedCacheHitTelemetry::sRecordCnt = 0;
DetailedCacheHitTelemetry::HitRate DetailedCacheHitTelemetry::sHRStats[kNumOfRanges];

DetailedCacheHitTelemetry::HitRate::HitRate()
{
  Reset();
}

void
DetailedCacheHitTelemetry::HitRate::AddRecord(ERecType aType)
{
  if (aType == HIT) {
    ++mHitCnt;
  } else {
    ++mMissCnt;
  }
}

uint32_t
DetailedCacheHitTelemetry::HitRate::GetHitRateBucket(uint32_t aNumOfBuckets) const
{
  uint32_t bucketIdx = (aNumOfBuckets * mHitCnt) / (mHitCnt + mMissCnt);
  if (bucketIdx == aNumOfBuckets) { // make sure 100% falls into the last bucket
    --bucketIdx;
  }

  return bucketIdx;
}

uint32_t
DetailedCacheHitTelemetry::HitRate::Count()
{
  return mHitCnt + mMissCnt;
}

void
DetailedCacheHitTelemetry::HitRate::Reset()
{
  mHitCnt = 0;
  mMissCnt = 0;
}

// static
void
DetailedCacheHitTelemetry::AddRecord(ERecType aType, TimeStamp aLoadStart)
{
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
  if (rangeIdx >= kNumOfRanges) { // The last range has no upper limit.
    rangeIdx = kNumOfRanges - 1;
  }

  uint32_t hitMissValue = 2 * rangeIdx; // 2 values per range
  if (aType == MISS) { // The order is HIT, MISS
    ++hitMissValue;
  }

  StaticMutexAutoLock lock(sLock);

  if (aType == MISS) {
    mozilla::Telemetry::AccumulateTimeDelta(
      mozilla::Telemetry::NETWORK_CACHE_V2_MISS_TIME_MS,
      aLoadStart);
  } else {
    mozilla::Telemetry::AccumulateTimeDelta(
      mozilla::Telemetry::NETWORK_CACHE_V2_HIT_TIME_MS,
      aLoadStart);
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
      uint32_t bucketOffset = sHRStats[i].GetHitRateBucket(kHitRateBuckets) *
                              kNumOfRanges;

      Telemetry::Accumulate(Telemetry::NETWORK_CACHE_HIT_RATE_PER_CACHE_SIZE,
                            bucketOffset + i);
      sHRStats[i].Reset();
    }
  }
}

} // namespace CacheFileUtils
} // namespace net
} // namespace mozilla
