/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHostRecord_h__
#define nsHostRecord_h__

#include "mozilla/AtomicBitfields.h"
#include "mozilla/DataMutex.h"
#include "mozilla/LinkedList.h"
#include "mozilla/net/HTTPSSVC.h"
#include "nsIDNSService.h"
#include "nsIDNSByTypeRecord.h"
#include "PLDHashTable.h"
#include "TRRSkippedReason.h"

class nsHostRecord;
class nsHostResolver;

namespace mozilla {
namespace net {
class HostRecordQueue;
class TRR;
class TRRQuery;
}  // namespace net
}  // namespace mozilla

/**
 * This class is used to notify listeners when a ResolveHost operation is
 * complete. Classes that derive it must implement threadsafe nsISupports
 * to be able to use RefPtr with this class.
 */
class nsResolveHostCallback
    : public mozilla::LinkedListElement<RefPtr<nsResolveHostCallback>>,
      public nsISupports {
 public:
  /**
   * OnResolveHostComplete
   *
   * this function is called to complete a host lookup initiated by
   * nsHostResolver::ResolveHost.  it may be invoked recursively from
   * ResolveHost or on an unspecified background thread.
   *
   * NOTE: it is the responsibility of the implementor of this method
   * to handle the callback in a thread safe manner.
   *
   * @param resolver
   *        nsHostResolver object associated with this result
   * @param record
   *        the host record containing the results of the lookup
   * @param status
   *        if successful, |record| contains non-null results
   */
  virtual void OnResolveHostComplete(nsHostResolver* resolver,
                                     nsHostRecord* record, nsresult status) = 0;
  /**
   * EqualsAsyncListener
   *
   * Determines if the listener argument matches the listener member var.
   * For subclasses not implementing a member listener, should return false.
   * For subclasses having a member listener, the function should check if
   * they are the same.  Used for cases where a pointer to an object
   * implementing nsResolveHostCallback is unknown, but a pointer to
   * the original listener is known.
   *
   * @param aListener
   *        nsIDNSListener object associated with the original request
   */
  virtual bool EqualsAsyncListener(nsIDNSListener* aListener) = 0;

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf) const = 0;

 protected:
  virtual ~nsResolveHostCallback() = default;
};

struct nsHostKey {
  const nsCString host;
  const nsCString mTrrServer;
  uint16_t type = 0;
  uint16_t flags = 0;
  uint16_t af = 0;
  bool pb = false;
  const nsCString originSuffix;
  explicit nsHostKey(const nsACString& host, const nsACString& aTrrServer,
                     uint16_t type, uint16_t flags, uint16_t af, bool pb,
                     const nsACString& originSuffix);
  bool operator==(const nsHostKey& other) const;
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  PLDHashNumber Hash() const;
};

/**
 * nsHostRecord - ref counted object type stored in host resolver cache.
 */
class nsHostRecord : public mozilla::LinkedListElement<RefPtr<nsHostRecord>>,
                     public nsHostKey,
                     public nsISupports {
  using TRRSkippedReason = mozilla::net::TRRSkippedReason;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return 0;
  }

  // Returns the TRR mode encoded by the flags
  nsIRequest::TRRMode TRRMode();

  // Records the first reason that caused TRR to be skipped or to fail.
  void RecordReason(TRRSkippedReason reason) {
    if (mTRRSkippedReason == TRRSkippedReason::TRR_UNSET) {
      mTRRSkippedReason = reason;
    }
  }

  enum DnsPriority {
    DNS_PRIORITY_LOW = nsIDNSService::RESOLVE_PRIORITY_LOW,
    DNS_PRIORITY_MEDIUM = nsIDNSService::RESOLVE_PRIORITY_MEDIUM,
    DNS_PRIORITY_HIGH,
  };

 protected:
  friend class nsHostResolver;
  friend class mozilla::net::HostRecordQueue;
  friend class mozilla::net::TRR;
  friend class mozilla::net::TRRQuery;

  explicit nsHostRecord(const nsHostKey& key);
  virtual ~nsHostRecord() = default;

  // Mark hostrecord as not usable
  void Invalidate();

  enum ExpirationStatus {
    EXP_VALID,
    EXP_GRACE,
    EXP_EXPIRED,
  };

  ExpirationStatus CheckExpiration(const mozilla::TimeStamp& now) const;

  // Convenience function for setting the timestamps above (mValidStart,
  // mValidEnd, and mGraceStart). valid and grace are durations in seconds.
  void SetExpiration(const mozilla::TimeStamp& now, unsigned int valid,
                     unsigned int grace);
  void CopyExpirationTimesAndFlagsFrom(const nsHostRecord* aFromHostRecord);

  // Checks if the record is usable (not expired and has a value)
  bool HasUsableResult(const mozilla::TimeStamp& now,
                       uint16_t queryFlags = 0) const;

  static DnsPriority GetPriority(uint16_t aFlags);

  virtual void Cancel();
  virtual bool HasUsableResultInternal(const mozilla::TimeStamp& now,
                                       uint16_t queryFlags) const = 0;
  virtual bool RefreshForNegativeResponse() const { return true; }

  mozilla::LinkedList<RefPtr<nsResolveHostCallback>> mCallbacks;

  bool IsAddrRecord() const {
    return type == nsIDNSService::RESOLVE_TYPE_DEFAULT;
  }

  // When the record began being valid. Used mainly for bookkeeping.
  mozilla::TimeStamp mValidStart;

  // When the record is no longer valid (it's time of expiration)
  mozilla::TimeStamp mValidEnd;

  // When the record enters its grace period. This must be before mValidEnd.
  // If a record is in its grace period (and not expired), it will be used
  // but a request to refresh it will be made.
  mozilla::TimeStamp mGraceStart;

  uint32_t mTtl = 0;

  // The computed TRR mode that is actually used by the request.
  // It is set in nsHostResolver::NameLookup and is based on the mode of the
  // default resolver and the TRRMode encoded in the flags.
  // The mode into account if the TRR service is disabled,
  // parental controls are on, domain matches exclusion list, etc.
  nsIRequest::TRRMode mEffectiveTRRMode = nsIRequest::TRR_DEFAULT_MODE;

  TRRSkippedReason mTRRSkippedReason = TRRSkippedReason::TRR_UNSET;
  TRRSkippedReason mFirstTRRSkippedReason = TRRSkippedReason::TRR_UNSET;
  TRRSkippedReason mTRRAFailReason = TRRSkippedReason::TRR_UNSET;
  TRRSkippedReason mTRRAAAAFailReason = TRRSkippedReason::TRR_UNSET;

  mozilla::DataMutex<RefPtr<mozilla::net::TRRQuery>> mTRRQuery;

  // counter of outstanding resolving calls
  mozilla::Atomic<int32_t> mResolving{0};

  // Number of times we've attempted TRR. Reset when we refresh.
  // TRR is attempted at most twice - first attempt and retry.
  mozilla::Atomic<int32_t> mTrrAttempts{0};

  // True if this record is a cache of a failed lookup.  Negative cache
  // entries are valid just like any other (though never for more than 60
  // seconds), but a use of that negative entry forces an asynchronous refresh.
  bool negative = false;

  // Explicitly expired
  bool mDoomed = false;
};

// b020e996-f6ab-45e5-9bf5-1da71dd0053a
#define ADDRHOSTRECORD_IID                           \
  {                                                  \
    0xb020e996, 0xf6ab, 0x45e5, {                    \
      0x9b, 0xf5, 0x1d, 0xa7, 0x1d, 0xd0, 0x05, 0x3a \
    }                                                \
  }

class AddrHostRecord final : public nsHostRecord {
  using Mutex = mozilla::Mutex;
  using DNSResolverType = mozilla::net::DNSResolverType;

 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(ADDRHOSTRECORD_IID)
  NS_DECL_ISUPPORTS_INHERITED

  /* a fully resolved host record has either a non-null |addr_info| or |addr|
   * field.  if |addr_info| is null, it implies that the |host| is an IP
   * address literal.  in which case, |addr| contains the parsed address.
   * otherwise, if |addr_info| is non-null, then it contains one or many
   * IP addresses corresponding to the given host name.  if both |addr_info|
   * and |addr| are null, then the given host has not yet been fully resolved.
   * |af| is the address family of the record we are querying for.
   */

  /* the lock protects |addr_info| and |addr_info_gencnt| because they
   * are mutable and accessed by the resolver worker thread and the
   * nsDNSService2 class.  |addr| doesn't change after it has been
   * assigned a value.  only the resolver worker thread modifies
   * nsHostRecord (and only in nsHostResolver::CompleteLookup);
   * the other threads just read it.  therefore the resolver worker
   * thread doesn't need to lock when reading |addr_info|.
   */
  Mutex addr_info_lock{"AddrHostRecord.addr_info_lock"};
  // generation count of |addr_info|
  int addr_info_gencnt = 0;
  RefPtr<mozilla::net::AddrInfo> addr_info;
  mozilla::UniquePtr<mozilla::net::NetAddr> addr;

  // hold addr_info_lock when calling the blocklist functions
  bool Blocklisted(const mozilla::net::NetAddr* query);
  void ResetBlocklist();
  void ReportUnusable(const mozilla::net::NetAddr* aAddress);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const override;

  nsIRequest::TRRMode EffectiveTRRMode() const { return mEffectiveTRRMode; }

 private:
  friend class nsHostResolver;
  friend class mozilla::net::HostRecordQueue;
  friend class mozilla::net::TRR;
  friend class mozilla::net::TRRQuery;

  explicit AddrHostRecord(const nsHostKey& key);
  ~AddrHostRecord();

  // Checks if the record is usable (not expired and has a value)
  bool HasUsableResultInternal(const mozilla::TimeStamp& now,
                               uint16_t queryFlags) const override;

  bool RemoveOrRefresh(bool aTrrToo);  // Mark records currently being resolved
                                       // as needed to resolve again.

  // Saves the skip reason of a first-attempt TRR lookup and clears
  // it to prepare for a retry attempt.
  void NotifyRetryingTrr();
  void ResolveComplete();

  static DnsPriority GetPriority(uint16_t aFlags);

  // true if pending and on the queue (not yet given to getaddrinfo())
  bool onQueue() { return LoadNative() && isInList(); }

  // When the lookups of this record started and their durations
  mozilla::TimeStamp mTrrStart;
  mozilla::TimeStamp mNativeStart;
  mozilla::TimeDuration mTrrDuration;
  mozilla::TimeDuration mNativeDuration;

  // TRR or ODoH was used on this record
  mozilla::Atomic<DNSResolverType> mResolverType{DNSResolverType::Native};
  uint8_t mTRRSuccess = 0;     // number of successful TRR responses
  uint8_t mNativeSuccess = 0;  // number of native lookup responses

  // clang-format off
  MOZ_ATOMIC_BITFIELDS(mAtomicBitfields, 8, (
    // true if this record is being resolved "natively", which means that
    // it is either on the pending queue or owned by one of the worker threads.
    (uint16_t, Native, 1),
    (uint16_t, NativeUsed, 1),
    // true if off queue and contributing to mActiveAnyThreadCount
    (uint16_t, UsingAnyThread, 1),
    (uint16_t, GetTtl, 1),
    (uint16_t, ResolveAgain, 1)
  ))
  // clang-format on

  // The number of times ReportUnusable() has been called in the record's
  // lifetime.
  uint32_t mUnusableCount = 0;

  // a list of addresses associated with this record that have been reported
  // as unusable. the list is kept as a set of strings to make it independent
  // of gencnt.
  nsTArray<nsCString> mUnusableItems;
};

NS_DEFINE_STATIC_IID_ACCESSOR(AddrHostRecord, ADDRHOSTRECORD_IID)

// 77b786a7-04be-44f2-987c-ab8aa96676e0
#define TYPEHOSTRECORD_IID                           \
  {                                                  \
    0x77b786a7, 0x04be, 0x44f2, {                    \
      0x98, 0x7c, 0xab, 0x8a, 0xa9, 0x66, 0x76, 0xe0 \
    }                                                \
  }

class TypeHostRecord final : public nsHostRecord,
                             public nsIDNSTXTRecord,
                             public nsIDNSHTTPSSVCRecord,
                             public mozilla::net::DNSHTTPSSVCRecordBase {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(TYPEHOSTRECORD_IID)
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDNSTXTRECORD
  NS_DECL_NSIDNSHTTPSSVCRECORD

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const override;
  uint32_t GetType();
  mozilla::net::TypeRecordResultType GetResults();

 private:
  friend class nsHostResolver;
  friend class mozilla::net::TRRQuery;

  explicit TypeHostRecord(const nsHostKey& key);
  ~TypeHostRecord();

  // Checks if the record is usable (not expired and has a value)
  bool HasUsableResultInternal(const mozilla::TimeStamp& now,
                               uint16_t queryFlags) const override;
  bool RefreshForNegativeResponse() const override;

  mozilla::net::TypeRecordResultType mResults = AsVariant(mozilla::Nothing());
  mozilla::Mutex mResultsLock{"TypeHostRecord.mResultsLock"};

  // When the lookups of this record started (for telemetry).
  mozilla::TimeStamp mStart;
  bool mAllRecordsExcluded = false;
};

NS_DEFINE_STATIC_IID_ACCESSOR(TypeHostRecord, TYPEHOSTRECORD_IID)

static inline bool IsHighPriority(uint16_t flags) {
  return !(flags & (nsHostRecord::DNS_PRIORITY_LOW |
                    nsHostRecord::DNS_PRIORITY_MEDIUM));
}

static inline bool IsMediumPriority(uint16_t flags) {
  return flags & nsHostRecord::DNS_PRIORITY_MEDIUM;
}

static inline bool IsLowPriority(uint16_t flags) {
  return flags & nsHostRecord::DNS_PRIORITY_LOW;
}

#endif  // nsHostRecord_h__
