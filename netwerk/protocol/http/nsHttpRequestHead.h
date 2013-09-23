/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpRequestHead_h__
#define nsHttpRequestHead_h__

#include "nsHttp.h"
#include "nsHttpHeaderArray.h"
#include "nsString.h"

//-----------------------------------------------------------------------------
// nsHttpRequestHead represents the request line and headers from an HTTP
// request.
//-----------------------------------------------------------------------------

class nsHttpRequestHead
{
public:
    nsHttpRequestHead() : mMethod(nsHttp::Get), mVersion(NS_HTTP_VERSION_1_1) {}

    void SetMethod(nsHttpAtom method) { mMethod = method; }
    void SetVersion(nsHttpVersion version) { mVersion = version; }
    void SetRequestURI(const nsCSubstring &s) { mRequestURI = s; }

    const nsHttpHeaderArray &Headers() const { return mHeaders; }
    nsHttpHeaderArray & Headers()          { return mHeaders; }
    nsHttpAtom          Method()     const { return mMethod; }
    nsHttpVersion       Version()    const { return mVersion; }
    const nsCSubstring &RequestURI() const { return mRequestURI; }

    const char *PeekHeader(nsHttpAtom h) const
    {
        return mHeaders.PeekHeader(h);
    }
    nsresult SetHeader(nsHttpAtom h, const nsACString &v, bool m=false) { return mHeaders.SetHeader(h, v, m); }
    nsresult GetHeader(nsHttpAtom h, nsACString &v) const
    {
        return mHeaders.GetHeader(h, v);
    }
    void ClearHeader(nsHttpAtom h)                                           { mHeaders.ClearHeader(h); }
    void ClearHeaders()                                                      { mHeaders.Clear(); }

    const char *FindHeaderValue(nsHttpAtom h, const char *v) const
    {
        return mHeaders.FindHeaderValue(h, v);
    }
    bool HasHeaderValue(nsHttpAtom h, const char *v) const
    {
      return mHeaders.HasHeaderValue(h, v);
    }

    void Flatten(nsACString &, bool pruneProxyHeaders = false);

    // Don't allow duplicate values
    nsresult SetHeaderOnce(nsHttpAtom h, const char *v, bool merge = false)
    {
        if (!merge || !HasHeaderValue(h, v))
            return mHeaders.SetHeader(h, nsDependentCString(v), merge);
        return NS_OK;
    }

private:
    // All members must be copy-constructable and assignable
    nsHttpHeaderArray mHeaders;
    nsHttpAtom        mMethod;
    nsHttpVersion     mVersion;
    nsCString         mRequestURI;
};

#endif // nsHttpRequestHead_h__
