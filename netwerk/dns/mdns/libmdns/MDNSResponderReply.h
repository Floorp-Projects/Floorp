/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_netwerk_dns_mdns_libmdns_MDNSResponderReply_h
#define mozilla_netwerk_dns_mdns_libmdns_MDNSResponderReply_h

#include "dns_sd.h"
#include "MDNSResponderOperator.h"
#include "mozilla/UniquePtr.h"
#include "nsIThread.h"
#include "mozilla/nsRefPtr.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

class BrowseReplyRunnable final : public nsRunnable
{
public:
  BrowseReplyRunnable(DNSServiceRef aSdRef,
                      DNSServiceFlags aFlags,
                      uint32_t aInterfaceIndex,
                      DNSServiceErrorType aErrorCode,
                      const nsACString& aServiceName,
                      const nsACString& aRegType,
                      const nsACString& aReplyDomain,
                      BrowseOperator* aContext);

  NS_IMETHODIMP Run() override;

  static void Reply(DNSServiceRef aSdRef,
                    DNSServiceFlags aFlags,
                    uint32_t aInterfaceIndex,
                    DNSServiceErrorType aErrorCode,
                    const char* aServiceName,
                    const char* aRegType,
                    const char* aReplyDomain,
                    void* aContext);

private:
  DNSServiceRef mSdRef;
  DNSServiceFlags mFlags;
  uint32_t mInterfaceIndex;
  DNSServiceErrorType mErrorCode;
  nsCString mServiceName;
  nsCString mRegType;
  nsCString mReplyDomain;
  BrowseOperator* mContext;
};

class RegisterReplyRunnable final : public nsRunnable
{
public:
  RegisterReplyRunnable(DNSServiceRef aSdRef,
                        DNSServiceFlags aFlags,
                        DNSServiceErrorType aErrorCode,
                        const nsACString& aName,
                        const nsACString& aRegType,
                        const nsACString& aDomain,
                        RegisterOperator* aContext);

  NS_IMETHODIMP Run() override;

  static void Reply(DNSServiceRef aSdRef,
                    DNSServiceFlags aFlags,
                    DNSServiceErrorType aErrorCode,
                    const char* aName,
                    const char* aRegType,
                    const char* aDomain,
                    void* aContext);

private:
  DNSServiceRef mSdRef;
  DNSServiceFlags mFlags;
  DNSServiceErrorType mErrorCode;
  nsCString mName;
  nsCString mRegType;
  nsCString mDomain;
  RegisterOperator* mContext;
};

class ResolveReplyRunnable final : public nsRunnable
{
public:
  ResolveReplyRunnable(DNSServiceRef aSdRef,
                       DNSServiceFlags aFlags,
                       uint32_t aInterfaceIndex,
                       DNSServiceErrorType aErrorCode,
                       const nsACString& aFullName,
                       const nsACString& aHostTarget,
                       uint16_t aPort,
                       uint16_t aTxtLen,
                       const unsigned char* aTxtRecord,
                       ResolveOperator* aContext);
  ~ResolveReplyRunnable();

  NS_IMETHODIMP Run() override;

  static void Reply(DNSServiceRef aSdRef,
                    DNSServiceFlags aFlags,
                    uint32_t aInterfaceIndex,
                    DNSServiceErrorType aErrorCode,
                    const char* aFullName,
                    const char* aHostTarget,
                    uint16_t aPort,
                    uint16_t aTxtLen,
                    const unsigned char* aTxtRecord,
                    void* aContext);

private:
  DNSServiceRef mSdRef;
  DNSServiceFlags mFlags;
  uint32_t mInterfaceIndex;
  DNSServiceErrorType mErrorCode;
  nsCString mFullname;
  nsCString mHosttarget;
  uint16_t mPort;
  uint16_t mTxtLen;
  UniquePtr<unsigned char> mTxtRecord;
  nsRefPtr<ResolveOperator> mContext;
};

} // namespace net
} // namespace mozilla

 #endif // mozilla_netwerk_dns_mdns_libmdns_MDNSResponderReply_h
