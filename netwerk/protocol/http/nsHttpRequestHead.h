/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpRequestHead_h__
#define nsHttpRequestHead_h__

#include "nsHttp.h"
#include "nsHttpHeaderArray.h"
#include "nsString.h"
#include "mozilla/ReentrantMonitor.h"

class nsIHttpHeaderVisitor;

namespace mozilla { namespace net {

class nsHttpHandler;
class HttpBaseChannel;

//-----------------------------------------------------------------------------
// nsHttpRequestHead represents the request line and headers from an HTTP
// request.
//-----------------------------------------------------------------------------

class nsHttpRequestHead
{
public:
    nsHttpRequestHead();
    ~nsHttpRequestHead();

    // The following function is only used in HttpChannelParent to avoid
    // copying headers. If you use it be careful to do it only under
    // nsHttpRequestHead lock!!!
    const nsHttpHeaderArray &Headers() const;
    void Enter() { mReentrantMonitor.Enter(); }
    void Exit() { mReentrantMonitor.Exit(); }

    void SetHeaders(const nsHttpHeaderArray& aHeaders);

    void SetMethod(const nsACString &method);
    void SetVersion(nsHttpVersion version);
    void SetRequestURI(const nsCSubstring &s);
    void SetPath(const nsCSubstring &s);
    uint32_t HeaderCount();

    // Using this function it is possible to itereate through all headers
    // automatically under one lock.
    MOZ_MUST_USE nsresult
    VisitHeaders(nsIHttpHeaderVisitor *visitor,
                 nsHttpHeaderArray::VisitorFilter filter =
                     nsHttpHeaderArray::eFilterAll);
    void Method(nsACString &aMethod);
    nsHttpVersion Version();
    void RequestURI(nsACString &RequestURI);
    void Path(nsACString &aPath);
    void SetHTTPS(bool val);
    bool IsHTTPS();

    void SetOrigin(const nsACString &scheme, const nsACString &host,
                   int32_t port);
    void Origin(nsACString &aOrigin);

    MOZ_MUST_USE nsresult SetHeader(const nsACString &h, const nsACString &v,
                                    bool m=false);
    MOZ_MUST_USE nsresult SetHeader(nsHttpAtom h, const nsACString &v,
                                    bool m=false);
    MOZ_MUST_USE nsresult SetHeader(nsHttpAtom h, const nsACString &v, bool m,
                                    nsHttpHeaderArray::HeaderVariety variety);
    MOZ_MUST_USE nsresult SetEmptyHeader(const nsACString &h);
    MOZ_MUST_USE nsresult GetHeader(nsHttpAtom h, nsACString &v);

    MOZ_MUST_USE nsresult ClearHeader(nsHttpAtom h);
    void ClearHeaders();

    bool HasHeaderValue(nsHttpAtom h, const char *v);
    // This function returns true if header is set even if it is an empty
    // header.
    bool HasHeader(nsHttpAtom h);
    void Flatten(nsACString &, bool pruneProxyHeaders = false);

    // Don't allow duplicate values
    MOZ_MUST_USE nsresult SetHeaderOnce(nsHttpAtom h, const char *v,
                                        bool merge = false);

    bool IsSafeMethod();

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

    ParsedMethodType ParsedMethod();
    bool EqualsMethod(ParsedMethodType aType);
    bool IsGet() { return EqualsMethod(kMethod_Get); }
    bool IsPost() { return EqualsMethod(kMethod_Post); }
    bool IsOptions() { return EqualsMethod(kMethod_Options); }
    bool IsConnect() { return EqualsMethod(kMethod_Connect); }
    bool IsHead() { return EqualsMethod(kMethod_Head); }
    bool IsPut() { return EqualsMethod(kMethod_Put); }
    bool IsTrace() { return EqualsMethod(kMethod_Trace); }
    void ParseHeaderSet(const char *buffer);

private:
    friend class mozilla::net::nsHttpHandler;
    friend class mozilla::net::HttpBaseChannel;
    // This function is designed to only be called during the
    // creation of this object, and doesn't grab the lock of the
    // object before setting the header.  Please do not use it if
    // you are not sure why you need it.
    MOZ_MUST_USE nsresult SetHeaderNonThreadSafe(nsHttpAtom h,
                                                 const nsACString &v,
                                                 bool m,
                                                 nsHttpHeaderArray::HeaderVariety variety);

#ifdef DEBUG
    // This helper class is used to enable calling the SetHeaderNonThreadSafe() function from
    // the callers that we want.
    class MOZ_RAII AutoEnableCallingSetHeaderNonThreadSafe final
    {
    public:
      explicit AutoEnableCallingSetHeaderNonThreadSafe(nsHttpRequestHead* aSelf)
        : mSelf(aSelf)
      {
        mSelf->mCanCallSetHeaderNonThreadSafe = true;
      }
      ~AutoEnableCallingSetHeaderNonThreadSafe()
      {
        mSelf->mCanCallSetHeaderNonThreadSafe = true;
      }

    private:
      nsHttpRequestHead* mSelf;
    };
#endif

private:
    // All members must be copy-constructable and assignable
    nsHttpHeaderArray mHeaders;
    nsCString         mMethod;
    nsHttpVersion     mVersion;

    // mRequestURI and mPath are strings instead of an nsIURI
    // because this is used off the main thread
    nsCString         mRequestURI;
    nsCString         mPath;

    nsCString         mOrigin;
    ParsedMethodType  mParsedMethod;
    bool              mHTTPS;

    // We are using ReentrantMonitor instead of a Mutex because VisitHeader
    // function calls nsIHttpHeaderVisitor::VisitHeader while under lock.
    ReentrantMonitor  mReentrantMonitor;

    // During VisitHeader we sould not allow cal to SetHeader.
    bool mInVisitHeaders;

#ifdef DEBUG
    // Calls to SetHeaderNonThreadSafe can either be made through HttpBaseChannel::Init()
    // or through the SetHeader method.
    bool mCanCallSetHeaderNonThreadSafe;
#endif
};

} // namespace net
} // namespace mozilla

#endif // nsHttpRequestHead_h__
