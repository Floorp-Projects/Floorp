/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/DNSRequestParent.h"
#include "nsIDNSService.h"
#include "nsNetCID.h"
#include "nsThreadUtils.h"
#include "nsIServiceManager.h"
#include "nsICancelable.h"
#include "nsIDNSRecord.h"
#include "nsHostResolver.h"
#include "mozilla/unused.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

DNSRequestParent::DNSRequestParent()
  : mIPCClosed(false)
{

}

DNSRequestParent::~DNSRequestParent()
{

}

void
DNSRequestParent::DoAsyncResolve(const nsACString &hostname, uint32_t flags)
{
  nsresult rv;
  mFlags = flags;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
    nsCOMPtr<nsICancelable> unused;
    rv = dns->AsyncResolve(hostname, flags, this, mainThread,
                           getter_AddRefs(unused));
  }

  if (NS_FAILED(rv) && !mIPCClosed) {
    unused << Send__delete__(this, DNSRequestResponse(rv));
  }
}

void
DNSRequestParent::ActorDestroy(ActorDestroyReason why)
{
  // We may still have refcount>0 if DNS hasn't called our OnLookupComplete
  // yet, but child process has crashed.  We must not send any more msgs
  // to child, or IPDL will kill chrome process, too.
  mIPCClosed = true;
}
//-----------------------------------------------------------------------------
// DNSRequestParent::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(DNSRequestParent,
                   nsIDNSListener)

//-----------------------------------------------------------------------------
// nsIDNSListener functions
//-----------------------------------------------------------------------------

NS_IMETHODIMP
DNSRequestParent::OnLookupComplete(nsICancelable *request,
                                   nsIDNSRecord  *rec,
                                   nsresult       status)
{
  if (mIPCClosed) {
    // nothing to do: child probably crashed
    return NS_OK;
  }

  if (NS_SUCCEEDED(status)) {
    MOZ_ASSERT(rec);

    nsAutoCString cname;
    if (mFlags & nsHostResolver::RES_CANON_NAME) {
      rec->GetCanonicalName(cname);
    }

    // Get IP addresses for hostname (use port 80 as dummy value for NetAddr)
    NetAddrArray array;
    NetAddr addr;
    while (NS_SUCCEEDED(rec->GetNextAddr(80, &addr))) {
      array.AppendElement(addr);
    }

    unused << Send__delete__(this, DNSRequestResponse(DNSRecord(cname, array)));
  } else {
    unused << Send__delete__(this, DNSRequestResponse(status));
  }

  return NS_OK;
}



}} // mozilla::net
