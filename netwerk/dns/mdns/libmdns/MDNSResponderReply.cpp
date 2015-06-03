/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MDNSResponderReply.h"
#include "mozilla/Endian.h"
#include "private/pprio.h"

namespace mozilla {
namespace net {

BrowseReplyRunnable::BrowseReplyRunnable(DNSServiceRef aSdRef,
                                         DNSServiceFlags aFlags,
                                         uint32_t aInterfaceIndex,
                                         DNSServiceErrorType aErrorCode,
                                         const nsACString& aServiceName,
                                         const nsACString& aRegType,
                                         const nsACString& aReplyDomain,
                                         BrowseOperator* aContext)
  : mSdRef(aSdRef)
  , mFlags(aFlags)
  , mInterfaceIndex(aInterfaceIndex)
  , mErrorCode(aErrorCode)
  , mServiceName(aServiceName)
  , mRegType(aRegType)
  , mReplyDomain(aReplyDomain)
  , mContext(aContext)
{
}

NS_IMETHODIMP
BrowseReplyRunnable::Run()
{
  MOZ_ASSERT(mContext);
  mContext->Reply(mSdRef,
                  mFlags,
                  mInterfaceIndex,
                  mErrorCode,
                  mServiceName,
                  mRegType,
                  mReplyDomain);
  return NS_OK;
}

void
BrowseReplyRunnable::Reply(DNSServiceRef aSdRef,
                           DNSServiceFlags aFlags,
                           uint32_t aInterfaceIndex,
                           DNSServiceErrorType aErrorCode,
                           const char* aServiceName,
                           const char* aRegType,
                           const char* aReplyDomain,
                           void* aContext)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  BrowseOperator* obj(reinterpret_cast<BrowseOperator*>(aContext));
  if (!obj) {
    return;
  }

  nsCOMPtr<nsIThread> thread(obj->GetThread());
  if (!thread) {
    return;
  }

  thread->Dispatch(new BrowseReplyRunnable(aSdRef,
                                           aFlags,
                                           aInterfaceIndex,
                                           aErrorCode,
                                           nsCString(aServiceName),
                                           nsCString(aRegType),
                                           nsCString(aReplyDomain),
                                           obj),
                   NS_DISPATCH_NORMAL);
}

RegisterReplyRunnable::RegisterReplyRunnable(DNSServiceRef aSdRef,
                                             DNSServiceFlags aFlags,
                                             DNSServiceErrorType aErrorCode,
                                             const nsACString& aName,
                                             const nsACString& aRegType,
                                             const nsACString& domain,
                                             RegisterOperator* aContext)
  : mSdRef(aSdRef)
  , mFlags(aFlags)
  , mErrorCode(aErrorCode)
  , mName(aName)
  , mRegType(aRegType)
  , mDomain(domain)
  , mContext(aContext)
{
}

NS_IMETHODIMP
RegisterReplyRunnable::Run()
{
  MOZ_ASSERT(mContext);

  mContext->Reply(mSdRef,
                  mFlags,
                  mErrorCode,
                  mName,
                  mRegType,
                  mDomain);
  return NS_OK;
}

void
RegisterReplyRunnable::Reply(DNSServiceRef aSdRef,
                             DNSServiceFlags aFlags,
                             DNSServiceErrorType aErrorCode,
                             const char* aName,
                             const char* aRegType,
                             const char* domain,
                             void* aContext)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  RegisterOperator* obj(reinterpret_cast<RegisterOperator*>(aContext));
  if (!obj) {
    return;
  }

  nsCOMPtr<nsIThread> thread(obj->GetThread());
  if (!thread) {
    return;
  }

  thread->Dispatch(new RegisterReplyRunnable(aSdRef,
                                             aFlags,
                                             aErrorCode,
                                             nsCString(aName),
                                             nsCString(aRegType),
                                             nsCString(domain),
                                             obj),
                   NS_DISPATCH_NORMAL);
}

ResolveReplyRunnable::ResolveReplyRunnable(DNSServiceRef aSdRef,
                                           DNSServiceFlags aFlags,
                                           uint32_t aInterfaceIndex,
                                           DNSServiceErrorType aErrorCode,
                                           const nsACString& aFullName,
                                           const nsACString& aHostTarget,
                                           uint16_t aPort,
                                           uint16_t aTxtLen,
                                           const unsigned char* aTxtRecord,
                                           ResolveOperator* aContext)
  : mSdRef(aSdRef)
  , mFlags(aFlags)
  , mInterfaceIndex(aInterfaceIndex)
  , mErrorCode(aErrorCode)
  , mFullname(aFullName)
  , mHosttarget(aHostTarget)
  , mPort(aPort)
  , mTxtLen(aTxtLen)
  , mTxtRecord(new unsigned char[aTxtLen])
  , mContext(aContext)
{
  if (mTxtRecord) {
    memcpy(mTxtRecord.get(), aTxtRecord, aTxtLen);
  }
}

ResolveReplyRunnable::~ResolveReplyRunnable()
{
}

NS_IMETHODIMP
ResolveReplyRunnable::Run()
{
  MOZ_ASSERT(mContext);
  mContext->Reply(mSdRef,
                  mFlags,
                  mInterfaceIndex,
                  mErrorCode,
                  mFullname,
                  mHosttarget,
                  mPort,
                  mTxtLen,
                  mTxtRecord.get());
  return NS_OK;
}

void
ResolveReplyRunnable::Reply(DNSServiceRef aSdRef,
                            DNSServiceFlags aFlags,
                            uint32_t aInterfaceIndex,
                            DNSServiceErrorType aErrorCode,
                            const char* aFullName,
                            const char* aHostTarget,
                            uint16_t aPort,
                            uint16_t aTxtLen,
                            const unsigned char* aTxtRecord,
                            void* aContext)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  ResolveOperator* obj(reinterpret_cast<ResolveOperator*>(aContext));
  if (!obj) {
    return;
  }

  nsCOMPtr<nsIThread> thread(obj->GetThread());
  if (!thread) {
    return;
  }

  thread->Dispatch(new ResolveReplyRunnable(aSdRef,
                                            aFlags,
                                            aInterfaceIndex,
                                            aErrorCode,
                                            nsCString(aFullName),
                                            nsCString(aHostTarget),
                                            NativeEndian::swapFromNetworkOrder(aPort),
                                            aTxtLen,
                                            aTxtRecord,
                                            obj),
                   NS_DISPATCH_NORMAL);
}

} // namespace net
} // namespace mozilla
