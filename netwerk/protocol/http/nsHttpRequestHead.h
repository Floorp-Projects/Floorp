/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpRequestHead_h__
#define nsHttpRequestHead_h__

#include "nsHttp.h"
#include "nsHttpHeaderArray.h"
#include "nsString.h"

namespace mozilla { namespace net {

//-----------------------------------------------------------------------------
// nsHttpRequestHead represents the request line and headers from an HTTP
// request.
//-----------------------------------------------------------------------------

class nsHttpRequestHead
{
public:
    nsHttpRequestHead();
    ~nsHttpRequestHead();

    void SetMethod(const nsACString &method);
    void SetVersion(nsHttpVersion version) { mVersion = version; }
    void SetRequestURI(const nsCSubstring &s) { mRequestURI = s; }

    const nsHttpHeaderArray &Headers() const { return mHeaders; }
    nsHttpHeaderArray & Headers()          { return mHeaders; }
    const nsCString &Method()        const { return mMethod; }
    nsHttpVersion       Version()    const { return mVersion; }
    const nsCSubstring &RequestURI() const { return mRequestURI; }

    void SetHTTPS(bool val) { mHTTPS = val; }
    bool IsHTTPS() const { return mHTTPS; }

    void SetOrigin(const nsACString &scheme, const nsACString &host, int32_t port);
    const nsCString &Origin() const { return mOrigin; }

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

    bool IsSafeMethod() const;

    enum ParsedMethodType
    {
        kMethod_Custom,
        kMethod_Get,
        kMethod_Post,
        kMethod_Options,
        kMethod_Connect,
        kMethod_Head,
        kMethod_Put,
        kMethod_Trace
    };

    ParsedMethodType ParsedMethod() const { return mParsedMethod; }
    bool EqualsMethod(ParsedMethodType aType) const { return mParsedMethod == aType; }
    bool IsGet() const { return EqualsMethod(kMethod_Get); }
    bool IsPost() const { return EqualsMethod(kMethod_Post); }
    bool IsOptions() const { return EqualsMethod(kMethod_Options); }
    bool IsConnect() const { return EqualsMethod(kMethod_Connect); }
    bool IsHead() const { return EqualsMethod(kMethod_Head); }
    bool IsPut() const { return EqualsMethod(kMethod_Put); }
    bool IsTrace() const { return EqualsMethod(kMethod_Trace); }
    void ParseHeaderSet(char *buffer) { mHeaders.ParseHeaderSet(buffer); }

private:
    // All members must be copy-constructable and assignable
    nsHttpHeaderArray mHeaders;
    nsCString         mMethod;
    nsHttpVersion     mVersion;
    nsCString         mRequestURI;
    nsCString         mOrigin;
    ParsedMethodType  mParsedMethod;
    bool              mHTTPS;
};

}} // namespace mozilla::net

#endif // nsHttpRequestHead_h__
