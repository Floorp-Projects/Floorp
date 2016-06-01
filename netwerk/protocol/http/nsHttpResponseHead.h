/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpResponseHead_h__
#define nsHttpResponseHead_h__

#include "nsHttpHeaderArray.h"
#include "nsHttp.h"
#include "nsString.h"
#include "mozilla/ReentrantMonitor.h"

class nsIHttpHeaderVisitor;

// This needs to be forward declared here so we can include only this header
// without also including PHttpChannelParams.h
namespace IPC {
    template <typename> struct ParamTraits;
} // namespace IPC

namespace mozilla { namespace net {

//-----------------------------------------------------------------------------
// nsHttpResponseHead represents the status line and headers from an HTTP
// response.
//-----------------------------------------------------------------------------

class nsHttpResponseHead
{
public:
    nsHttpResponseHead() : mVersion(NS_HTTP_VERSION_1_1)
                         , mStatus(200)
                         , mContentLength(-1)
                         , mCacheControlPrivate(false)
                         , mCacheControlNoStore(false)
                         , mCacheControlNoCache(false)
                         , mCacheControlImmutable(false)
                         , mPragmaNoCache(false)
                         , mReentrantMonitor("nsHttpResponseHead.mReentrantMonitor")
                         , mInVisitHeaders(false) {}

    nsHttpResponseHead(const nsHttpResponseHead &aOther);
    nsHttpResponseHead &operator=(const nsHttpResponseHead &aOther);

    void Enter() { mReentrantMonitor.Enter(); }
    void Exit() { mReentrantMonitor.Exit(); }

    nsHttpVersion Version();
// X11's Xlib.h #defines 'Status' to 'int' on some systems!
#undef Status
    uint16_t Status();
    void StatusText(nsACString &aStatusText);
    int64_t ContentLength();
    void ContentType(nsACString &aContentType);
    void ContentCharset(nsACString &aContentCharset);
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

    nsresult SetHeader(nsHttpAtom h, const nsACString &v, bool m=false);
    nsresult GetHeader(nsHttpAtom h, nsACString &v);
    void ClearHeader(nsHttpAtom h);
    void ClearHeaders();
    bool HasHeaderValue(nsHttpAtom h, const char *v);
    bool HasHeader(nsHttpAtom h);

    void SetContentType(const nsACString &s);
    void SetContentCharset(const nsACString &s);
    void SetContentLength(int64_t);

    // write out the response status line and headers as a single text block,
    // optionally pruning out transient headers (ie. headers that only make
    // sense the first time the response is handled).
    // Both functions append to the string supplied string.
    void Flatten(nsACString &, bool pruneTransients);
    void FlattenNetworkOriginalHeaders(nsACString &buf);

    // The next 2 functions parse flattened response head and original net headers.
    // They are used when we are reading an entry from the cache.
    //
    // To keep proper order of the original headers we MUST call
    // ParseCachedOriginalHeaders FIRST and then ParseCachedHead.
    //
    // block must be null terminated. parsing is destructive.
    nsresult ParseCachedHead(char *block);
    nsresult ParseCachedOriginalHeaders(char *block);

    // parse the status line. line must be null terminated.
    void ParseStatusLine(const char *line);

    // parse a header line. line must be null terminated. parsing is destructive.
    nsresult ParseHeaderLine(const char *line);

    // cache validation support methods
    nsresult ComputeFreshnessLifetime(uint32_t *);
    nsresult ComputeCurrentAge(uint32_t now, uint32_t requestTime,
                               uint32_t *result);
    bool MustValidate();
    bool MustValidateIfExpired();

    // returns true if the server appears to support byte range requests.
    bool IsResumable();

    // returns true if the Expires header has a value in the past relative to the
    // value of the Date header.
    bool ExpiresInPast();

    // update headers...
    nsresult UpdateHeaders(nsHttpResponseHead *headers);

    // reset the response head to it's initial state
    void Reset();

    nsresult GetAgeValue(uint32_t *result);
    nsresult GetMaxAgeValue(uint32_t *result);
    nsresult GetDateValue(uint32_t *result);
    nsresult GetExpiresValue(uint32_t *result);
    nsresult GetLastModifiedValue(uint32_t *result);

    bool operator==(const nsHttpResponseHead& aOther) const;

    // Using this function it is possible to itereate through all headers
    // automatically under one lock.
    nsresult VisitHeaders(nsIHttpHeaderVisitor *visitor,
                          nsHttpHeaderArray::VisitorFilter filter);
    nsresult GetOriginalHeader(nsHttpAtom aHeader,
                               nsIHttpHeaderVisitor *aVisitor);

    bool HasContentType();
    bool HasContentCharset();
private:
    nsresult SetHeader_locked(nsHttpAtom h, const nsACString &v,
                              bool m=false);
    void AssignDefaultStatusText();
    void ParseVersion(const char *);
    void ParseCacheControl(const char *);
    void ParsePragma(const char *);

    void ParseStatusLine_locked(const char *line);
    nsresult ParseHeaderLine_locked(const char *line, bool originalFromNetHeaders);

    // these return failure if the header does not exist.
    nsresult ParseDateHeader(nsHttpAtom header, uint32_t *result) const;

    bool ExpiresInPast_locked() const;
    nsresult GetAgeValue_locked(uint32_t *result) const;
    nsresult GetExpiresValue_locked(uint32_t *result) const;
    nsresult GetMaxAgeValue_locked(uint32_t *result) const;

    nsresult GetDateValue_locked(uint32_t *result) const
    {
        return ParseDateHeader(nsHttp::Date, result);
    }

    nsresult GetLastModifiedValue_locked(uint32_t *result) const
    {
        return ParseDateHeader(nsHttp::Last_Modified, result);
    }

private:
    // All members must be copy-constructable and assignable
    nsHttpHeaderArray mHeaders;
    nsHttpVersion     mVersion;
    uint16_t          mStatus;
    nsCString         mStatusText;
    int64_t           mContentLength;
    nsCString         mContentType;
    nsCString         mContentCharset;
    bool              mCacheControlPrivate;
    bool              mCacheControlNoStore;
    bool              mCacheControlNoCache;
    bool              mCacheControlImmutable;
    bool              mPragmaNoCache;

    // We are using ReentrantMonitor instead of a Mutex because VisitHeader
    // function calls nsIHttpHeaderVisitor::VisitHeader while under lock.
    ReentrantMonitor  mReentrantMonitor;
    // During VisitHeader we sould not allow cal to SetHeader.
    bool              mInVisitHeaders;

    friend struct IPC::ParamTraits<nsHttpResponseHead>;
};

} // namespace net
} // namespace mozilla

#endif // nsHttpResponseHead_h__
