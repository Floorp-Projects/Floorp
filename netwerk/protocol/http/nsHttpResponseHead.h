/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpResponseHead_h__
#define nsHttpResponseHead_h__

#include "nsHttpHeaderArray.h"
#include "nsHttp.h"
#include "nsString.h"
#include "mozilla/RecursiveMutex.h"

#ifdef Status
/* Xlib headers insist on this for some reason... Nuke it because
   it'll override our member name */
typedef Status __StatusTmp;
#  undef Status
typedef __StatusTmp Status;
#endif

class nsIHttpHeaderVisitor;

// This needs to be forward declared here so we can include only this header
// without also including PHttpChannelParams.h
namespace IPC {
template <typename>
struct ParamTraits;
}  // namespace IPC

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// nsHttpResponseHead represents the status line and headers from an HTTP
// response.
//-----------------------------------------------------------------------------

class nsHttpResponseHead {
 public:
  nsHttpResponseHead() = default;

  nsHttpResponseHead(const nsHttpResponseHead& aOther);
  nsHttpResponseHead& operator=(const nsHttpResponseHead& aOther);

  void Enter() const MOZ_CAPABILITY_ACQUIRE(mRecursiveMutex) {
    mRecursiveMutex.Lock();
  }
  void Exit() const MOZ_CAPABILITY_RELEASE(mRecursiveMutex) {
    mRecursiveMutex.Unlock();
  }
  void AssertMutexOwned() const { mRecursiveMutex.AssertCurrentThreadIn(); }

  HttpVersion Version();
  uint16_t Status() const;
  void StatusText(nsACString& aStatusText);
  int64_t ContentLength();
  void ContentType(nsACString& aContentType) const;
  void ContentCharset(nsACString& aContentCharset);
  bool Public();
  bool Private();
  bool NoStore();
  bool NoCache();
  bool Immutable();
  /**
   * Full length of the entity. For byte-range requests, this may be larger
   * than ContentLength(), which will only represent the requested part of the
   * entity.
   */
  int64_t TotalEntitySize();

  [[nodiscard]] nsresult SetHeader(const nsACString& h, const nsACString& v,
                                   bool m = false);
  [[nodiscard]] nsresult SetHeader(const nsHttpAtom& h, const nsACString& v,
                                   bool m = false);
  [[nodiscard]] nsresult GetHeader(const nsHttpAtom& h, nsACString& v) const;
  void ClearHeader(const nsHttpAtom& h);
  void ClearHeaders();
  bool HasHeaderValue(const nsHttpAtom& h, const char* v);
  bool HasHeader(const nsHttpAtom& h) const;

  void SetContentType(const nsACString& s);
  void SetContentCharset(const nsACString& s);
  void SetContentLength(int64_t);

  // write out the response status line and headers as a single text block,
  // optionally pruning out transient headers (ie. headers that only make
  // sense the first time the response is handled).
  // Both functions append to the string supplied string.
  void Flatten(nsACString&, bool pruneTransients);
  void FlattenNetworkOriginalHeaders(nsACString& buf);

  // The next 2 functions parse flattened response head and original net
  // headers. They are used when we are reading an entry from the cache.
  //
  // To keep proper order of the original headers we MUST call
  // ParseCachedOriginalHeaders FIRST and then ParseCachedHead.
  //
  // block must be null terminated.
  [[nodiscard]] nsresult ParseCachedHead(const char* block);
  [[nodiscard]] nsresult ParseCachedOriginalHeaders(char* block);

  // parse the status line.
  nsresult ParseStatusLine(const nsACString& line);

  // parse a header line.
  [[nodiscard]] nsresult ParseHeaderLine(const nsACString& line);

  // cache validation support methods
  [[nodiscard]] nsresult ComputeFreshnessLifetime(uint32_t*);
  [[nodiscard]] nsresult ComputeCurrentAge(uint32_t now, uint32_t requestTime,
                                           uint32_t* result);
  bool MustValidate();
  bool MustValidateIfExpired();

  // return true if the response contains a valid Cache-control:
  // stale-while-revalidate and |now| is less than or equal |expiration +
  // stale-while-revalidate|.  Otherwise false.
  bool StaleWhileRevalidate(uint32_t now, uint32_t expiration);

  // returns true if the server appears to support byte range requests.
  bool IsResumable();

  // returns true if the Expires header has a value in the past relative to the
  // value of the Date header.
  bool ExpiresInPast();

  // update headers...
  void UpdateHeaders(nsHttpResponseHead* aOther);

  // reset the response head to it's initial state
  void Reset();

  [[nodiscard]] nsresult GetLastModifiedValue(uint32_t* result);

  bool operator==(const nsHttpResponseHead& aOther) const;

  // Using this function it is possible to itereate through all headers
  // automatically under one lock.
  [[nodiscard]] nsresult VisitHeaders(nsIHttpHeaderVisitor* visitor,
                                      nsHttpHeaderArray::VisitorFilter filter);
  [[nodiscard]] nsresult GetOriginalHeader(const nsHttpAtom& aHeader,
                                           nsIHttpHeaderVisitor* aVisitor);

  bool HasContentType() const;
  bool HasContentCharset();
  bool GetContentTypeOptionsHeader(nsACString& aOutput);

 private:
  [[nodiscard]] nsresult SetHeader_locked(const nsHttpAtom& atom,
                                          const nsACString& h,
                                          const nsACString& v, bool m = false)
      MOZ_REQUIRES(mRecursiveMutex);
  void AssignDefaultStatusText() MOZ_REQUIRES(mRecursiveMutex);
  void ParseVersion(const char*) MOZ_REQUIRES(mRecursiveMutex);
  void ParseCacheControl(const char*) MOZ_REQUIRES(mRecursiveMutex);
  void ParsePragma(const char*) MOZ_REQUIRES(mRecursiveMutex);
  // Parses a content-length header-value as described in
  // https://fetch.spec.whatwg.org/#content-length-header
  nsresult ParseResponseContentLength(const nsACString& aHeaderStr)
      MOZ_REQUIRES(mRecursiveMutex);

  nsresult ParseStatusLine_locked(const nsACString& line)
      MOZ_REQUIRES(mRecursiveMutex);
  [[nodiscard]] nsresult ParseHeaderLine_locked(const nsACString& line,
                                                bool originalFromNetHeaders)
      MOZ_REQUIRES(mRecursiveMutex);

  // these return failure if the header does not exist.
  [[nodiscard]] nsresult ParseDateHeader(const nsHttpAtom& header,
                                         uint32_t* result) const
      MOZ_REQUIRES(mRecursiveMutex);
  [[nodiscard]] nsresult GetAgeValue(uint32_t* result);
  [[nodiscard]] nsresult GetMaxAgeValue(uint32_t* result);
  [[nodiscard]] nsresult GetStaleWhileRevalidateValue(uint32_t* result);
  [[nodiscard]] nsresult GetDateValue(uint32_t* result);
  [[nodiscard]] nsresult GetExpiresValue(uint32_t* result);

  bool ExpiresInPast_locked() const MOZ_REQUIRES(mRecursiveMutex);
  [[nodiscard]] nsresult GetAgeValue_locked(uint32_t* result) const
      MOZ_REQUIRES(mRecursiveMutex);
  [[nodiscard]] nsresult GetExpiresValue_locked(uint32_t* result) const
      MOZ_REQUIRES(mRecursiveMutex);
  [[nodiscard]] nsresult GetMaxAgeValue_locked(uint32_t* result) const
      MOZ_REQUIRES(mRecursiveMutex);
  [[nodiscard]] nsresult GetStaleWhileRevalidateValue_locked(
      uint32_t* result) const MOZ_REQUIRES(mRecursiveMutex);

  [[nodiscard]] nsresult GetDateValue_locked(uint32_t* result) const
      MOZ_REQUIRES(mRecursiveMutex) {
    return ParseDateHeader(nsHttp::Date, result);
  }

  [[nodiscard]] nsresult GetLastModifiedValue_locked(uint32_t* result) const
      MOZ_REQUIRES(mRecursiveMutex) {
    return ParseDateHeader(nsHttp::Last_Modified, result);
  }

  bool NoCache_locked() const MOZ_REQUIRES(mRecursiveMutex) {
    // We ignore Pragma: no-cache if Cache-Control is set.
    MOZ_ASSERT_IF(mCacheControlNoCache, mHasCacheControl);
    return mHasCacheControl ? mCacheControlNoCache : mPragmaNoCache;
  }

 private:
  // All members must be copy-constructable and assignable
  nsHttpHeaderArray mHeaders MOZ_GUARDED_BY(mRecursiveMutex);
  HttpVersion mVersion MOZ_GUARDED_BY(mRecursiveMutex){HttpVersion::v1_1};
  uint16_t mStatus MOZ_GUARDED_BY(mRecursiveMutex){200};
  nsCString mStatusText MOZ_GUARDED_BY(mRecursiveMutex);
  int64_t mContentLength MOZ_GUARDED_BY(mRecursiveMutex){-1};
  nsCString mContentType MOZ_GUARDED_BY(mRecursiveMutex);
  nsCString mContentCharset MOZ_GUARDED_BY(mRecursiveMutex);
  bool mHasCacheControl MOZ_GUARDED_BY(mRecursiveMutex){false};
  bool mCacheControlPublic MOZ_GUARDED_BY(mRecursiveMutex){false};
  bool mCacheControlPrivate MOZ_GUARDED_BY(mRecursiveMutex){false};
  bool mCacheControlNoStore MOZ_GUARDED_BY(mRecursiveMutex){false};
  bool mCacheControlNoCache MOZ_GUARDED_BY(mRecursiveMutex){false};
  bool mCacheControlImmutable MOZ_GUARDED_BY(mRecursiveMutex){false};
  bool mCacheControlStaleWhileRevalidateSet MOZ_GUARDED_BY(mRecursiveMutex){
      false};
  uint32_t mCacheControlStaleWhileRevalidate MOZ_GUARDED_BY(mRecursiveMutex){0};
  bool mCacheControlMaxAgeSet MOZ_GUARDED_BY(mRecursiveMutex){false};
  uint32_t mCacheControlMaxAge MOZ_GUARDED_BY(mRecursiveMutex){0};
  bool mPragmaNoCache MOZ_GUARDED_BY(mRecursiveMutex){false};

  // We are using RecursiveMutex instead of a Mutex because VisitHeader
  // function calls nsIHttpHeaderVisitor::VisitHeader while under lock.
  mutable RecursiveMutex mRecursiveMutex{"nsHttpResponseHead.mRecursiveMutex"};
  // During VisitHeader we sould not allow call to SetHeader.
  bool mInVisitHeaders MOZ_GUARDED_BY(mRecursiveMutex){false};

  friend struct IPC::ParamTraits<nsHttpResponseHead>;
};

}  // namespace net
}  // namespace mozilla

#endif  // nsHttpResponseHead_h__
