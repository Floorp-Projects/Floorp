/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ODoH.h"

#include "mozilla/Base64.h"
#include "nsIURIMutator.h"
#include "ODoHService.h"
#include "TRRService.h"

namespace mozilla {
namespace net {

#undef LOG
#undef LOG_ENABLED
extern mozilla::LazyLogModule gHostResolverLog;
#define LOG(args) MOZ_LOG(gHostResolverLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() \
  MOZ_LOG_TEST(mozilla::net::gHostResolverLog, mozilla::LogLevel::Debug)

NS_IMETHODIMP
ODoH::Run() {
  if (!gODoHService) {
    RecordReason(nsHostRecord::TRR_SEND_FAILED);
    FailData(NS_ERROR_FAILURE);
    return NS_OK;
  }

  if (!gODoHService->ODoHConfigs()) {
    LOG(("ODoH::Run ODoHConfigs is not available\n"));
    if (NS_SUCCEEDED(gODoHService->UpdateODoHConfig())) {
      gODoHService->AppendPendingODoHRequest(this);
    } else {
      FailData(NS_ERROR_FAILURE);
      return NS_OK;
    }
    return NS_OK;
  }

  return TRR::Run();
}

DNSPacket* ODoH::GetOrCreateDNSPacket() {
  if (!mPacket) {
    mPacket = MakeUnique<ODoHDNSPacket>();
  }

  return mPacket.get();
}

nsresult ODoH::CreateQueryURI(nsIURI** aOutURI) {
  nsAutoCString uri;
  nsCOMPtr<nsIURI> dnsURI;
  gODoHService->GetRequestURI(uri);

  nsresult rv = NS_NewURI(getter_AddRefs(dnsURI), uri);
  if (NS_FAILED(rv)) {
    return rv;
  }

  dnsURI.forget(aOutURI);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
