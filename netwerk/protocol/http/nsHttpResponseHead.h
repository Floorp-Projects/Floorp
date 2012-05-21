/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpResponseHead_h__
#define nsHttpResponseHead_h__

#include "nsHttpHeaderArray.h"
#include "nsHttp.h"
#include "nsString.h"

//-----------------------------------------------------------------------------
// nsHttpResponseHead represents the status line and headers from an HTTP
// response.
//-----------------------------------------------------------------------------

class nsHttpResponseHead
{
public:
    nsHttpResponseHead() : mVersion(NS_HTTP_VERSION_1_1)
                         , mStatus(200)
                         , mContentLength(LL_MAXUINT)
                         , mCacheControlNoStore(false)
                         , mCacheControlNoCache(false)
                         , mPragmaNoCache(false) {}
    ~nsHttpResponseHead() 
    {
        Reset();
    }
    
    nsHttpHeaderArray    &Headers()        { return mHeaders; }
    nsHttpVersion         Version()        { return mVersion; }
    PRUint16              Status()         { return mStatus; }
    const nsAFlatCString &StatusText()     { return mStatusText; }
    PRInt64               ContentLength()  { return mContentLength; }
    const nsAFlatCString &ContentType()    { return mContentType; }
    const nsAFlatCString &ContentCharset() { return mContentCharset; }
    bool                  NoStore()        { return mCacheControlNoStore; }
    bool                  NoCache()        { return (mCacheControlNoCache || mPragmaNoCache); }
    /**
     * Full length of the entity. For byte-range requests, this may be larger
     * than ContentLength(), which will only represent the requested part of the
     * entity.
     */
    PRInt64               TotalEntitySize();

    const char *PeekHeader(nsHttpAtom h)            { return mHeaders.PeekHeader(h); }
    nsresult SetHeader(nsHttpAtom h, const nsACString &v, bool m=false);
    nsresult GetHeader(nsHttpAtom h, nsACString &v) { return mHeaders.GetHeader(h, v); }
    void     ClearHeader(nsHttpAtom h)              { mHeaders.ClearHeader(h); }
    void     ClearHeaders()                         { mHeaders.Clear(); }

    const char *FindHeaderValue(nsHttpAtom h, const char *v) { return mHeaders.FindHeaderValue(h, v); }
    bool        HasHeaderValue(nsHttpAtom h, const char *v) { return mHeaders.HasHeaderValue(h, v); }

    void     SetContentType(const nsACString &s)    { mContentType = s; }
    void     SetContentCharset(const nsACString &s) { mContentCharset = s; }
    void     SetContentLength(PRInt64);

    // write out the response status line and headers as a single text block,
    // optionally pruning out transient headers (ie. headers that only make
    // sense the first time the response is handled).
    void     Flatten(nsACString &, bool pruneTransients);

    // parse flattened response head. block must be null terminated. parsing is
    // destructive.
    nsresult Parse(char *block);

    // parse the status line. line must be null terminated.
    void     ParseStatusLine(const char *line);

    // parse a header line. line must be null terminated. parsing is destructive.
    nsresult ParseHeaderLine(const char *line);

    // cache validation support methods
    nsresult ComputeFreshnessLifetime(PRUint32 *);
    nsresult ComputeCurrentAge(PRUint32 now, PRUint32 requestTime, PRUint32 *result);
    bool     MustValidate();
    bool     MustValidateIfExpired();

    // returns true if the server appears to support byte range requests.
    bool     IsResumable();

    // returns true if the Expires header has a value in the past relative to the
    // value of the Date header.
    bool     ExpiresInPast();

    // update headers...
    nsresult UpdateHeaders(nsHttpHeaderArray &headers); 

    // reset the response head to it's initial state
    void     Reset();

    // these return failure if the header does not exist.
    nsresult ParseDateHeader(nsHttpAtom header, PRUint32 *result);
    nsresult GetAgeValue(PRUint32 *result);
    nsresult GetMaxAgeValue(PRUint32 *result);
    nsresult GetDateValue(PRUint32 *result)         { return ParseDateHeader(nsHttp::Date, result); }
    nsresult GetExpiresValue(PRUint32 *result);
    nsresult GetLastModifiedValue(PRUint32 *result) { return ParseDateHeader(nsHttp::Last_Modified, result); }

private:
    void     ParseVersion(const char *);
    void     ParseCacheControl(const char *);
    void     ParsePragma(const char *);

private:
    nsHttpHeaderArray mHeaders;
    nsHttpVersion     mVersion;
    PRUint16          mStatus;
    nsCString         mStatusText;
    PRInt64           mContentLength;
    nsCString         mContentType;
    nsCString         mContentCharset;
    bool              mCacheControlNoStore;
    bool              mCacheControlNoCache;
    bool              mPragmaNoCache;

    friend struct IPC::ParamTraits<nsHttpResponseHead>;
};

#endif // nsHttpResponseHead_h__
