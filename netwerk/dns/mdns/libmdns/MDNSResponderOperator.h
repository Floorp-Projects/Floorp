/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_netwerk_dns_mdns_libmdns_MDNSResponderOperator_h
#define mozilla_netwerk_dns_mdns_libmdns_MDNSResponderOperator_h

#include "dns_sd.h"
#include "mozilla/Atomics.h"
#include "nsCOMPtr.h"
#include "nsIDNSServiceDiscovery.h"
#include "nsIThread.h"
#include "mozilla/nsRefPtr.h"
#include "nsString.h"

namespace mozilla {
namespace net {

class MDNSResponderOperator
{
public:
  MDNSResponderOperator();

  virtual nsresult Start();
  virtual nsresult Stop();
  void Cancel() { mIsCancelled = true; }
  nsIThread* GetThread() const { return mThread; }

protected:
  virtual ~MDNSResponderOperator();

  bool IsServing() const { return mService; }
  nsresult ResetService(DNSServiceRef aService);

private:
  class ServiceWatcher;

  DNSServiceRef mService;
  nsRefPtr<ServiceWatcher> mWatcher;
  nsCOMPtr<nsIThread> mThread; // remember caller thread for callback
  Atomic<bool> mIsCancelled;
};

class BrowseOperator final : private MDNSResponderOperator
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BrowseOperator)

  BrowseOperator(const nsACString& aServiceType,
                 nsIDNSServiceDiscoveryListener* aListener);

  nsresult Start() override;
  nsresult Stop() override;
  using MDNSResponderOperator::Cancel;
  using MDNSResponderOperator::GetThread;

  void Reply(DNSServiceRef aSdRef,
             DNSServiceFlags aFlags,
             uint32_t aInterfaceIndex,
             DNSServiceErrorType aErrorCode,
             const nsACString& aServiceName,
             const nsACString& aRegType,
             const nsACString& aReplyDomain);

private:
  ~BrowseOperator() = default;

  nsCString mServiceType;
  nsCOMPtr<nsIDNSServiceDiscoveryListener> mListener;
};

class RegisterOperator final : private MDNSResponderOperator
{
  enum { TXT_BUFFER_SIZE = 256 };

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RegisterOperator)

  RegisterOperator(nsIDNSServiceInfo* aServiceInfo,
                   nsIDNSRegistrationListener* aListener);

  nsresult Start() override;
  nsresult Stop() override;
  using MDNSResponderOperator::Cancel;
  using MDNSResponderOperator::GetThread;

  void Reply(DNSServiceRef aSdRef,
             DNSServiceFlags aFlags,
             DNSServiceErrorType aErrorCode,
             const nsACString& aName,
             const nsACString& aRegType,
             const nsACString& aDomain);

private:
  ~RegisterOperator() = default;

  nsCOMPtr<nsIDNSServiceInfo> mServiceInfo;
  nsCOMPtr<nsIDNSRegistrationListener> mListener;
};

class ResolveOperator final : private MDNSResponderOperator
{
  enum { TXT_BUFFER_SIZE = 256 };

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ResolveOperator)

  ResolveOperator(nsIDNSServiceInfo* aServiceInfo,
                  nsIDNSServiceResolveListener* aListener);

  nsresult Start() override;
  nsresult Stop() override;
  using MDNSResponderOperator::GetThread;

  void Reply(DNSServiceRef aSdRef,
             DNSServiceFlags aFlags,
             uint32_t aInterfaceIndex,
             DNSServiceErrorType aErrorCode,
             const nsACString& aFullName,
             const nsACString& aHostTarget,
             uint16_t aPort,
             uint16_t aTxtLen,
             const unsigned char* aTxtRecord);

private:
  ~ResolveOperator() = default;

  nsCOMPtr<nsIDNSServiceInfo> mServiceInfo;
  nsCOMPtr<nsIDNSServiceResolveListener> mListener;

  // hold self until callback is made.
  nsRefPtr<ResolveOperator> mDeleteProtector;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_netwerk_dns_mdns_libmdns_MDNSResponderOperator_h
