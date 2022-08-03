/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpRequestHead_h__
#define nsHttpRequestHead_h__

#include "nsHttp.h"
#include "nsHttpHeaderArray.h"
#include "nsString.h"
#include "mozilla/RecursiveMutex.h"

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
// nsHttpRequestHead represents the request line and headers from an HTTP
// request.
//-----------------------------------------------------------------------------

class nsHttpRequestHead {
 public:
  nsHttpRequestHead();
  explicit nsHttpRequestHead(const nsHttpRequestHead& aRequestHead);
  nsHttpRequestHead(nsHttpRequestHead&& aRequestHead);
  ~nsHttpRequestHead();

  nsHttpRequestHead& operator=(const nsHttpRequestHead& aRequestHead);

  // The following function is only used in HttpChannelParent to avoid
  // copying headers. If you use it be careful to do it only under
  // nsHttpRequestHead lock!!!
  const nsHttpHeaderArray& Headers() const;
  void Enter() const MOZ_CAPABILITY_ACQUIRE(mRecursiveMutex) {
    mRecursiveMutex.Lock();
  }
  void Exit() const MOZ_CAPABILITY_RELEASE(mRecursiveMutex) {
    mRecursiveMutex.Unlock();
  }

  void SetHeaders(const nsHttpHeaderArray& aHeaders);

  void SetMethod(const nsACString& method);
  void SetVersion(HttpVersion version);
  void SetRequestURI(const nsACString& s);
  void SetPath(const nsACString& s);
  uint32_t HeaderCount();

  // Using this function it is possible to itereate through all headers
  // automatically under one lock.
  [[nodiscard]] nsresult VisitHeaders(
      nsIHttpHeaderVisitor* visitor,
      nsHttpHeaderArray::VisitorFilter filter = nsHttpHeaderArray::eFilterAll);
  void Method(nsACString& aMethod);
  HttpVersion Version();
  void RequestURI(nsACString& RequestURI);
  void Path(nsACString& aPath);
  void SetHTTPS(bool val);
  bool IsHTTPS();

  void SetOrigin(const nsACString& scheme, const nsACString& host,
                 int32_t port);
  void Origin(nsACString& aOrigin);

  [[nodiscard]] nsresult SetHeader(const nsACString& h, const nsACString& v,
                                   bool m = false);
  [[nodiscard]] nsresult SetHeader(const nsHttpAtom& h, const nsACString& v,
                                   bool m = false);
  [[nodiscard]] nsresult SetHeader(const nsHttpAtom& h, const nsACString& v,
                                   bool m,
                                   nsHttpHeaderArray::HeaderVariety variety);
  [[nodiscard]] nsresult SetEmptyHeader(const nsACString& h);
  [[nodiscard]] nsresult GetHeader(const nsHttpAtom& h, nsACString& v);

  [[nodiscard]] nsresult ClearHeader(const nsHttpAtom& h);
  void ClearHeaders();

  bool HasHeaderValue(const nsHttpAtom& h, const char* v);
  // This function returns true if header is set even if it is an empty
  // header.
  bool HasHeader(const nsHttpAtom& h);
  void Flatten(nsACString&, bool pruneProxyHeaders = false);

  // Don't allow duplicate values
  [[nodiscard]] nsresult SetHeaderOnce(const nsHttpAtom& h, const char* v,
                                       bool merge = false);

  bool IsSafeMethod();

  enum ParsedMethodType {
    kMethod_Custom,
    kMethod_Get,
    kMethod_Post,
    kMethod_Options,
    kMethod_Connect,
    kMethod_Head,
    kMethod_Put,
    kMethod_Trace
  };

  static void ParseMethod(const nsCString& aRawMethod,
                          ParsedMethodType& aParsedMethod);

  ParsedMethodType ParsedMethod();
  bool EqualsMethod(ParsedMethodType aType);
  bool IsGet() { return EqualsMethod(kMethod_Get); }
  bool IsPost() { return EqualsMethod(kMethod_Post); }
  bool IsOptions() { return EqualsMethod(kMethod_Options); }
  bool IsConnect() { return EqualsMethod(kMethod_Connect); }
  bool IsHead() { return EqualsMethod(kMethod_Head); }
  bool IsPut() { return EqualsMethod(kMethod_Put); }
  bool IsTrace() { return EqualsMethod(kMethod_Trace); }
  void ParseHeaderSet(const char* buffer);

 private:
  // All members must be copy-constructable and assignable
  nsHttpHeaderArray mHeaders MOZ_GUARDED_BY(mRecursiveMutex);
  nsCString mMethod MOZ_GUARDED_BY(mRecursiveMutex){"GET"_ns};
  HttpVersion mVersion MOZ_GUARDED_BY(mRecursiveMutex){HttpVersion::v1_1};

  // mRequestURI and mPath are strings instead of an nsIURI
  // because this is used off the main thread
  // TODO: nsIURI is thread-safe now, should be fixable.
  nsCString mRequestURI MOZ_GUARDED_BY(mRecursiveMutex);
  nsCString mPath MOZ_GUARDED_BY(mRecursiveMutex);

  nsCString mOrigin MOZ_GUARDED_BY(mRecursiveMutex);
  ParsedMethodType mParsedMethod MOZ_GUARDED_BY(mRecursiveMutex){kMethod_Get};
  bool mHTTPS MOZ_GUARDED_BY(mRecursiveMutex){false};

  // We are using RecursiveMutex instead of a Mutex because VisitHeader
  // function calls nsIHttpHeaderVisitor::VisitHeader while under lock.
  mutable RecursiveMutex mRecursiveMutex MOZ_UNANNOTATED{
      "nsHttpRequestHead.mRecursiveMutex"};

  // During VisitHeader we sould not allow call to SetHeader.
  bool mInVisitHeaders MOZ_GUARDED_BY(mRecursiveMutex){false};

  friend struct IPC::ParamTraits<nsHttpRequestHead>;
};

}  // namespace net
}  // namespace mozilla

#endif  // nsHttpRequestHead_h__
