/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

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
                         , mContentLength(-1)
                         , mCacheControlNoStore(PR_FALSE)
                         , mCacheControlNoCache(PR_FALSE)
                         , mPragmaNoCache(PR_FALSE) {}
   ~nsHttpResponseHead() 
    {
        Reset();
    }
    
    nsHttpHeaderArray    &Headers()        { return mHeaders; }
    nsHttpVersion         Version()        { return mVersion; }
    PRUint16              Status()         { return mStatus; }
    const nsAFlatCString &StatusText()     { return mStatusText; }
    PRInt32               ContentLength()  { return mContentLength; }
    const nsAFlatCString &ContentType()    { return mContentType; }
    const nsAFlatCString &ContentCharset() { return mContentCharset; }
    PRBool                NoStore()        { return mCacheControlNoStore; }
    PRBool                NoCache()        { return (mCacheControlNoCache || mPragmaNoCache); }
    /**
     * Full length of the entity. For byte-range requests, this may be larger
     * than ContentLength(), which will only represent the requested part of the
     * entity.
     */
    PRInt32               TotalEntitySize();

    const char *PeekHeader(nsHttpAtom h)            { return mHeaders.PeekHeader(h); }
    nsresult SetHeader(nsHttpAtom h, const nsACString &v, PRBool m=PR_FALSE);
    nsresult GetHeader(nsHttpAtom h, nsACString &v) { return mHeaders.GetHeader(h, v); }
    void     ClearHeader(nsHttpAtom h)              { mHeaders.ClearHeader(h); }
    void     ClearHeaders()                         { mHeaders.Clear(); }

    void     SetContentType(const nsACString &s)    { mContentType = s; }
    void     SetContentCharset(const nsACString &s) { mContentCharset = s; }
    void     SetContentLength(PRInt32);

    // write out the response status line and headers as a single text block,
    // optionally pruning out transient headers (ie. headers that only make
    // sense the first time the response is handled).
    void     Flatten(nsACString &, PRBool pruneTransients);

    // parse flattened response head. block must be null terminated. parsing is
    // destructive.
    nsresult Parse(char *block);

    // parse the status line. line must be null terminated.
    void     ParseStatusLine(char *line);

    // parse a header line. line must be null terminated. parsing is destructive.
    void     ParseHeaderLine(char *line);

    // cache validation support methods
    nsresult ComputeFreshnessLifetime(PRUint32 *);
    nsresult ComputeCurrentAge(PRUint32 now, PRUint32 requestTime, PRUint32 *result);
    PRBool   MustValidate();
    PRBool   MustValidateIfExpired();

    // returns true if the server appears to support byte range requests.
    PRBool   IsResumable();

    // returns true if the Expires header has a value in the past relative to the
    // value of the Date header.
    PRBool   ExpiresInPast();

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
    void     ParseContentType(char *);
    void     ParseCacheControl(const char *);
    void     ParsePragma(const char *);

private:
    nsHttpHeaderArray mHeaders;
    nsHttpVersion     mVersion;
    PRUint16          mStatus;
    nsCString         mStatusText;
    PRInt32           mContentLength;
    nsCString         mContentType;
    nsCString         mContentCharset;
    PRPackedBool      mCacheControlNoStore;
    PRPackedBool      mCacheControlNoCache;
    PRPackedBool      mPragmaNoCache;
};

#endif // nsHttpResponseHead_h__
