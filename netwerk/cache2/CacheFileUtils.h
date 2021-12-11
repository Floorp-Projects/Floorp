/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileUtils__h__
#define CacheFileUtils__h__

#include "nsError.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/TimeStamp.h"

class nsILoadContextInfo;

namespace mozilla {
namespace net {
namespace CacheFileUtils {

extern const char* kAltDataKey;

already_AddRefed<nsILoadContextInfo> ParseKey(const nsACString& aKey,
                                              nsACString* aIdEnhance = nullptr,
                                              nsACString* aURISpec = nullptr);

void AppendKeyPrefix(nsILoadContextInfo* aInfo, nsACString& _retval);

void AppendTagWithValue(nsACString& aTarget, char const aTag,
                        const nsACString& aValue);

nsresult KeyMatchesLoadContextInfo(const nsACString& aKey,
                                   nsILoadContextInfo* aInfo, bool* _retval);

class ValidityPair {
 public:
  ValidityPair(uint32_t aOffset, uint32_t aLen);

  ValidityPair& operator=(const ValidityPair& aOther) = default;

  // Returns true when two pairs can be merged, i.e. they do overlap or the one
  // ends exactly where the other begins.
  bool CanBeMerged(const ValidityPair& aOther) const;

  // Returns true when aOffset is placed anywhere in the validity interval or
  // exactly after its end.
  bool IsInOrFollows(uint32_t aOffset) const;

  // Returns true when this pair has lower offset than the other pair. In case
  // both pairs have the same offset it returns true when this pair has a
  // shorter length.
  bool LessThan(const ValidityPair& aOther) const;

  // Merges two pair into one.
  void Merge(const ValidityPair& aOther);

  uint32_t Offset() const { return mOffset; }
  uint32_t Len() const { return mLen; }

 private:
  uint32_t mOffset;
  uint32_t mLen;
};

class ValidityMap {
 public:
  // Prints pairs in the map into log.
  void Log() const;

  // Returns number of pairs in the map.
  uint32_t Length() const;

  // Adds a new pair to the map. It keeps the pairs ordered and merges pairs
  // when possible.
  void AddPair(uint32_t aOffset, uint32_t aLen);

  // Removes all pairs from the map.
  void Clear();

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  ValidityPair& operator[](uint32_t aIdx);

 private:
  nsTArray<ValidityPair> mMap;
};

class DetailedCacheHitTelemetry {
 public:
  enum ERecType { HIT = 0, MISS = 1 };

  static void AddRecord(ERecType aType, TimeStamp aLoadStart);

 private:
  class HitRate {
   public:
    HitRate();

    void AddRecord(ERecType aType);
    // Returns the bucket index that the current hit rate falls into according
    // to the given aNumOfBuckets.
    uint32_t GetHitRateBucket(uint32_t aNumOfBuckets) const;
    uint32_t Count();
    void Reset();

   private:
    uint32_t mHitCnt = 0;
    uint32_t mMissCnt = 0;
  };

  // Group the hits and misses statistics by cache files count ranges (0-5000,
  // 5001-10000, ... , 95001- )
  static const uint32_t kRangeSize = 5000;
  static const uint32_t kNumOfRanges = 20;

  // Use the same ranges to report an average hit rate. Report the hit rates
  // (and reset the counters) every kTotalSamplesReportLimit samples.
  static const uint32_t kTotalSamplesReportLimit = 1000;

  // Report hit rate for a given cache size range only if it contains
  // kHitRateSamplesReportLimit or more samples. This limit should avoid
  // reporting a biased statistics.
  static const uint32_t kHitRateSamplesReportLimit = 500;

  // All hit rates are accumulated in a single telemetry probe, so to use
  // a sane number of enumerated values the hit rate is divided into buckets
  // instead of using a percent value. This constant defines number of buckets
  // that we divide the hit rates into. I.e. we'll report ranges 0%-5%, 5%-10%,
  // 10-%15%, ...
  static const uint32_t kHitRateBuckets = 20;

  // Protects sRecordCnt, sHitStats and Telemetry::Accumulated() calls.
  static StaticMutex sLock;

  // Counter of samples that is compared against kTotalSamplesReportLimit.
  static uint32_t sRecordCnt;

  // Hit rate statistics for every cache size range.
  static HitRate sHRStats[kNumOfRanges];
};

class CachePerfStats {
 public:
  // perfStatTypes in displayRcwnStats() in toolkit/content/aboutNetworking.js
  // must match EDataType
  enum EDataType {
    IO_OPEN = 0,
    IO_READ = 1,
    IO_WRITE = 2,
    ENTRY_OPEN = 3,
    LAST = 4
  };

  static void AddValue(EDataType aType, uint32_t aValue, bool aShortOnly);
  static uint32_t GetAverage(EDataType aType, bool aFiltered);
  static uint32_t GetStdDev(EDataType aType, bool aFiltered);
  static bool IsCacheSlow();
  static void GetSlowStats(uint32_t* aSlow, uint32_t* aNotSlow);

 private:
  // This class computes average and standard deviation, it returns an
  // arithmetic avg and stddev until total number of values reaches mWeight.
  // Then it returns modified moving average computed as follows:
  //
  //   avg = (1-a)*avg + a*value
  //   avgsq = (1-a)*avgsq + a*value^2
  //   stddev = sqrt(avgsq - avg^2)
  //
  //   where
  //       avgsq is an average of the square of the values
  //       a = 1 / weight
  class MMA {
   public:
    MMA(uint32_t aTotalWeight, bool aFilter);

    void AddValue(uint32_t aValue);
    uint32_t GetAverage();
    uint32_t GetStdDev();

   private:
    uint64_t mSum;
    uint64_t mSumSq;
    uint32_t mCnt;
    uint32_t mWeight;
    bool mFilter;
  };

  class PerfData {
   public:
    PerfData();

    void AddValue(uint32_t aValue, bool aShortOnly);
    uint32_t GetAverage(bool aFiltered);
    uint32_t GetStdDev(bool aFiltered);

   private:
    // Contains filtered data (i.e. times when we think the cache and disk was
    // not busy) for a longer time.
    MMA mFilteredAvg;

    // Contains unfiltered average of few recent values.
    MMA mShortAvg;
  };

  static StaticMutex sLock;

  static PerfData sData[LAST];
  static uint32_t sCacheSlowCnt;
  static uint32_t sCacheNotSlowCnt;
};

void FreeBuffer(void* aBuf);

nsresult ParseAlternativeDataInfo(const char* aInfo, int64_t* _offset,
                                  nsACString* _type);

void BuildAlternativeDataInfo(const char* aInfo, int64_t aOffset,
                              nsACString& _retval);

class CacheFileLock final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CacheFileLock)
  CacheFileLock() = default;

  mozilla::Mutex& Lock() { return mLock; }

 private:
  ~CacheFileLock() = default;

  mozilla::Mutex mLock{"CacheFile.mLock"};
};

}  // namespace CacheFileUtils
}  // namespace net
}  // namespace mozilla

#endif
