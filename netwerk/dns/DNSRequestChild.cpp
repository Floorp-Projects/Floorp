/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/net/ChildDNSService.h"
#include "mozilla/net/DNSRequestChild.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/Unused.h"
#include "nsIDNSRecord.h"
#include "nsHostResolver.h"
#include "nsTArray.h"
#include "nsNetAddr.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// ChildDNSRecord:
// A simple class to provide nsIDNSRecord on the child
//-----------------------------------------------------------------------------

class ChildDNSRecord : public nsIDNSRecord
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSRECORD

  ChildDNSRecord(const DNSRecord& reply, uint16_t flags);

private:
  virtual ~ChildDNSRecord() = default;

  nsCString mCanonicalName;
  nsTArray<NetAddr> mAddresses;
  uint32_t mCurrent; // addr iterator
  uint32_t mLength;  // number of addrs
  uint16_t mFlags;
};

NS_IMPL_ISUPPORTS(ChildDNSRecord, nsIDNSRecord)

ChildDNSRecord::ChildDNSRecord(const DNSRecord& reply, uint16_t flags)
  : mCurrent(0)
  , mFlags(flags)
{
  mCanonicalName = reply.canonicalName();

  // A shame IPDL gives us no way to grab ownership of array: so copy it.
  const nsTArray<NetAddr>& addrs = reply.addrs();
  uint32_t i = 0;
  mLength = addrs.Length();
  for (; i < mLength; i++) {
    mAddresses.AppendElement(addrs[i]);
  }
}

//-----------------------------------------------------------------------------
// ChildDNSRecord::nsIDNSRecord
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ChildDNSRecord::GetCanonicalName(nsACString &result)
{
  if (!(mFlags & nsHostResolver::RES_CANON_NAME)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  result = mCanonicalName;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSRecord::IsTRR(bool *retval)
{
  *retval = false;
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
ChildDNSRecord::GetNextAddr(uint16_t port, NetAddr *addr)
{
  if (mCurrent >= mLength) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  memcpy(addr, &mAddresses[mCurrent++], sizeof(NetAddr));

  // both Ipv4/6 use same bits for port, so safe to just use ipv4's field
  addr->inet.port = htons(port);

  return NS_OK;
}

NS_IMETHODIMP
ChildDNSRecord::GetAddresses(nsTArray<NetAddr> & aAddressArray)
{
  aAddressArray = mAddresses;
  return NS_OK;
}

// shamelessly copied from nsDNSRecord
NS_IMETHODIMP
ChildDNSRecord::GetScriptableNextAddr(uint16_t port, nsINetAddr **result)
{
  NetAddr addr;
  nsresult rv = GetNextAddr(port, &addr);
  if (NS_FAILED(rv)) return rv;

  NS_ADDREF(*result = new nsNetAddr(&addr));

  return NS_OK;
}

// also copied from nsDNSRecord
NS_IMETHODIMP
ChildDNSRecord::GetNextAddrAsString(nsACString &result)
{
  NetAddr addr;
  nsresult rv = GetNextAddr(0, &addr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  char buf[kIPv6CStrBufSize];
  if (NetAddrToString(&addr, buf, sizeof(buf))) {
    result.Assign(buf);
    return NS_OK;
  }
  NS_ERROR("NetAddrToString failed unexpectedly");
  return NS_ERROR_FAILURE; // conversion failed for some reason
}

NS_IMETHODIMP
ChildDNSRecord::HasMore(bool *result)
{
  *result = mCurrent < mLength;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSRecord::Rewind()
{
  mCurrent = 0;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSRecord::ReportUnusable(uint16_t aPort)
{
  // "We thank you for your feedback" == >/dev/null
  // TODO: we could send info back to parent.
  return NS_OK;
}

//-----------------------------------------------------------------------------
// CancelDNSRequestEvent
//-----------------------------------------------------------------------------

class CancelDNSRequestEvent : public Runnable
{
public:
  CancelDNSRequestEvent(DNSRequestChild* aDnsReq, nsresult aReason)
    : Runnable("net::CancelDNSRequestEvent")
    , mDnsRequest(aDnsReq)
    , mReasonForCancel(aReason)
  {}

  NS_IMETHOD Run() override
  {
    if (mDnsRequest->mIPCOpen) {
      // Send request to Parent process.
      mDnsRequest->SendCancelDNSRequest(mDnsRequest->mHost,
                                        mDnsRequest->mOriginAttributes,
                                        mDnsRequest->mFlags,
                                        mDnsRequest->mNetworkInterface,
                                        mReasonForCancel);
    }
    return NS_OK;
  }
private:
  RefPtr<DNSRequestChild> mDnsRequest;
  nsresult mReasonForCancel;
};

//-----------------------------------------------------------------------------
// DNSRequestChild
//-----------------------------------------------------------------------------

DNSRequestChild::DNSRequestChild(const nsACString& aHost,
                                 const OriginAttributes& aOriginAttributes,
                                 const uint32_t& aFlags,
                                 const nsACString& aNetworkInterface,
                                 nsIDNSListener *aListener,
                                 nsIEventTarget *target)
  : mListener(aListener)
  , mTarget(target)
  , mResultStatus(NS_OK)
  , mHost(aHost)
  , mOriginAttributes(aOriginAttributes)
  , mFlags(aFlags)
  , mNetworkInterface(aNetworkInterface)
  , mIPCOpen(false)
{
}

void
DNSRequestChild::StartRequest()
{
  // we can only do IPDL on the main thread
  if (!NS_IsMainThread()) {
    SystemGroup::Dispatch(
      TaskCategory::Other,
      NewRunnableMethod("net::DNSRequestChild::StartRequest",
                        this,
                        &DNSRequestChild::StartRequest));
    return;
  }

  nsCOMPtr<nsIEventTarget> systemGroupEventTarget
    = SystemGroup::EventTargetFor(TaskCategory::Other);

  gNeckoChild->SetEventTargetForActor(this, systemGroupEventTarget);

  mozilla::dom::ContentChild* cc =
    static_cast<mozilla::dom::ContentChild*>(gNeckoChild->Manager());
  if (cc->IsShuttingDown()) {
    return;
  }

  // Send request to Parent process.
  gNeckoChild->SendPDNSRequestConstructor(this, mHost, mOriginAttributes,
                                          mFlags, mNetworkInterface);
  mIPCOpen = true;

  // IPDL holds a reference until IPDL channel gets destroyed
  AddIPDLReference();
}

void
DNSRequestChild::CallOnLookupComplete()
{
  MOZ_ASSERT(mListener);
  mListener->OnLookupComplete(this, mResultRecord, mResultStatus);
}

mozilla::ipc::IPCResult
DNSRequestChild::RecvLookupCompleted(const DNSRequestResponse& reply)
{
  mIPCOpen = false;
  MOZ_ASSERT(mListener);

  switch (reply.type()) {
  case DNSRequestResponse::TDNSRecord: {
    mResultRecord = new ChildDNSRecord(reply.get_DNSRecord(), mFlags);
    break;
  }
  case DNSRequestResponse::Tnsresult: {
    mResultStatus = reply.get_nsresult();
    break;
  }
  default:
    NS_NOTREACHED("unknown type");
    return IPC_FAIL_NO_REASON(this);
  }

  MOZ_ASSERT(NS_IsMainThread());

  bool targetIsMain = false;
  if (!mTarget) {
    targetIsMain = true;
  } else {
    mTarget->IsOnCurrentThread(&targetIsMain);
  }

  if (targetIsMain) {
    CallOnLookupComplete();
  } else {
    nsCOMPtr<nsIRunnable> event =
      NewRunnableMethod("net::DNSRequestChild::CallOnLookupComplete",
                        this,
                        &DNSRequestChild::CallOnLookupComplete);
    mTarget->Dispatch(event, NS_DISPATCH_NORMAL);
  }

  Unused << Send__delete__(this);

  return IPC_OK();
}

void
DNSRequestChild::ReleaseIPDLReference()
{
  // Request is done or destroyed. Remove it from the hash table.
  RefPtr<ChildDNSService> dnsServiceChild =
    dont_AddRef(ChildDNSService::GetSingleton());
  dnsServiceChild->NotifyRequestDone(this);

  Release();
}

void
DNSRequestChild::ActorDestroy(ActorDestroyReason why)
{
  mIPCOpen = false;
}

//-----------------------------------------------------------------------------
// DNSRequestChild::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(DNSRequestChild,
                  nsICancelable)

//-----------------------------------------------------------------------------
// DNSRequestChild::nsICancelable
//-----------------------------------------------------------------------------

NS_IMETHODIMP
DNSRequestChild::Cancel(nsresult reason)
{
  if(mIPCOpen) {
    // We can only do IPDL on the main thread
    nsCOMPtr<nsIRunnable> runnable = new CancelDNSRequestEvent(this, reason);
    SystemGroup::Dispatch(TaskCategory::Other, runnable.forget());
  }
  return NS_OK;
}

//------------------------------------------------------------------------------
} // namespace net
} // namespace mozilla
