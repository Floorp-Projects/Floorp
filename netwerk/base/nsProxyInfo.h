/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsProxyInfo_h__
#define nsProxyInfo_h__

#include "nsIProxyInfo.h"
#include "nsString.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"

// Use to support QI nsIProxyInfo to nsProxyInfo
#define NS_PROXYINFO_IID                             \
  { /* ed42f751-825e-4cc2-abeb-3670711a8b85 */       \
    0xed42f751, 0x825e, 0x4cc2, {                    \
      0xab, 0xeb, 0x36, 0x70, 0x71, 0x1a, 0x8b, 0x85 \
    }                                                \
  }

namespace mozilla {
namespace net {

class ProxyInfoCloneArgs;

// This class is exposed to other classes inside Necko for fast access
// to the nsIProxyInfo attributes.
class nsProxyInfo final : public nsIProxyInfo {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PROXYINFO_IID)

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPROXYINFO

  // Cheap accessors for use within Necko
  const nsCString& Host() const { return mHost; }
  int32_t Port() const { return mPort; }
  const char* Type() const { return mType; }
  uint32_t Flags() const { return mFlags; }
  const nsCString& Username() const { return mUsername; }
  const nsCString& Password() const { return mPassword; }
  uint32_t Timeout() { return mTimeout; }
  uint32_t ResolveFlags() { return mResolveFlags; }
  const nsCString& ProxyAuthorizationHeader() const {
    return mProxyAuthorizationHeader;
  }
  const nsCString& ConnectionIsolationKey() const {
    return mConnectionIsolationKey;
  }

  bool IsDirect();
  bool IsHTTP();
  bool IsHTTPS();
  bool IsSOCKS();

  static void SerializeProxyInfo(nsProxyInfo* aProxyInfo,
                                 nsTArray<ProxyInfoCloneArgs>& aResult);
  static nsProxyInfo* DeserializeProxyInfo(
      const nsTArray<ProxyInfoCloneArgs>& aArgs);

  already_AddRefed<nsProxyInfo> CloneProxyInfoWithNewResolveFlags(
      uint32_t aResolveFlags);

 private:
  friend class nsProtocolProxyService;

  explicit nsProxyInfo(const char* type = nullptr) : mType(type) {}

  nsProxyInfo(const nsACString& aType, const nsACString& aHost, int32_t aPort,
              const nsACString& aUsername, const nsACString& aPassword,
              uint32_t aFlags, uint32_t aTimeout, uint32_t aResolveFlags,
              const nsACString& aProxyAuthorizationHeader,
              const nsACString& aConnectionIsolationKey);

  ~nsProxyInfo() { NS_IF_RELEASE(mNext); }

  const char* mType;  // pointer to statically allocated value
  nsCString mHost;
  nsCString mUsername;
  nsCString mPassword;
  nsCString mProxyAuthorizationHeader;
  nsCString mConnectionIsolationKey;
  nsCString mSourceId;
  int32_t mPort{-1};
  uint32_t mFlags{0};
  // We need to read on multiple threads, but don't need to sync on anything
  // else
  Atomic<uint32_t, Relaxed> mResolveFlags{0};
  uint32_t mTimeout{UINT32_MAX};
  nsProxyInfo* mNext{nullptr};
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsProxyInfo, NS_PROXYINFO_IID)

}  // namespace net
}  // namespace mozilla

#endif  // nsProxyInfo_h__
