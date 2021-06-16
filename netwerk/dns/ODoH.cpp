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
// Put DNSLogging.h at the end to avoid LOG being overwritten by other headers.
#include "DNSLogging.h"

namespace mozilla {
namespace net {

NS_IMETHODIMP
ODoH::Run() {
  if (!gODoHService) {
    RecordReason(TRRSkippedReason::TRR_SEND_FAILED);
    FailData(NS_ERROR_FAILURE);
    return NS_OK;
  }

  if (!gODoHService->ODoHConfigs()) {
    LOG(("ODoH::Run ODoHConfigs is not available\n"));
    if (NS_SUCCEEDED(gODoHService->UpdateODoHConfig())) {
      gODoHService->AppendPendingODoHRequest(this);
    } else {
      RecordReason(TRRSkippedReason::ODOH_UPDATE_KEY_FAILED);
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

void ODoH::HandleTimeout() {
  // If this request is still in the pending queue, it means we can't get the
  // ODoHConfigs within the timeout.
  if (gODoHService->RemovePendingODoHRequest(this)) {
    RecordReason(TRRSkippedReason::ODOH_KEY_NOT_AVAILABLE);
  }

  TRR::HandleTimeout();
}

void ODoH::HandleEncodeError(nsresult aStatusCode) {
  MOZ_ASSERT(NS_FAILED(aStatusCode));

  DNSPacketStatus status = mPacket->PacketStatus();
  MOZ_ASSERT(status != DNSPacketStatus::Success);

  if (status == DNSPacketStatus::KeyNotAvailable) {
    RecordReason(TRRSkippedReason::ODOH_KEY_NOT_AVAILABLE);
  } else if (status == DNSPacketStatus::KeyNotUsable) {
    RecordReason(TRRSkippedReason::ODOH_KEY_NOT_USABLE);
  } else if (status == DNSPacketStatus::EncryptError) {
    RecordReason(TRRSkippedReason::ODOH_ENCRYPTION_FAILED);
  } else {
    MOZ_ASSERT_UNREACHABLE("Unexpected status code.");
  }
}

void ODoH::HandleDecodeError(nsresult aStatusCode) {
  MOZ_ASSERT(NS_FAILED(aStatusCode));

  DNSPacketStatus status = mPacket->PacketStatus();
  MOZ_ASSERT(status != DNSPacketStatus::Success);

  if (status == DNSPacketStatus::DecryptError) {
    RecordReason(TRRSkippedReason::ODOH_DECRYPTION_FAILED);
  }

  TRR::HandleDecodeError(aStatusCode);
}

}  // namespace net
}  // namespace mozilla
