/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include "transportlayersrtp.h"
#include "transportlayerdtls.h"

#include "logging.h"
#include "nsError.h"
#include "mozilla/Assertions.h"
#include "transportlayerdtls.h"
#include "srtp.h"
#include "databuffer.h"
#include "nsAutoPtr.h"

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

static char kDTLSExporterLabel[] = "EXTRACTOR-dtls_srtp";

TransportLayerSrtp::TransportLayerSrtp(TransportLayerDtls& dtls)
{
  // We need to connect to the dtls layer, not the ice layer, because even
  // though the packets that DTLS decrypts don't flow through us, we do base our
  // keying information on the keying information established by the DTLS layer.
  dtls.SignalStateChange.connect(this, &TransportLayerSrtp::StateChange);

  TL_SET_STATE(dtls.state());
}

void
TransportLayerSrtp::WasInserted()
{
  // Connect to the lower layers
  if (!Setup()) {
    TL_SET_STATE(TS_ERROR);
  }
}

bool
TransportLayerSrtp::Setup()
{
  CheckThread();
  if (!downward_) {
    MOZ_MTLOG(ML_ERROR, "SRTP layer with nothing below. This is useless");
    return false;
  }

  // downward_ is the TransportLayerIce
  downward_->SignalPacketReceived.connect(this, &TransportLayerSrtp::PacketReceived);

  return true;
}

static bool
IsRtp(const unsigned char* aData, size_t aLen)
{
  if (aLen < 2)
    return false;

  // Check if this is a RTCP packet. Logic based on the types listed in
  // media/webrtc/trunk/src/modules/rtp_rtcp/source/rtp_utility.cc

  // Anything outside this range is RTP.
  if ((aData[1] < 192) || (aData[1] > 207))
    return true;

  if (aData[1] == 192) // FIR
    return false;

  if (aData[1] == 193) // NACK, but could also be RTP. This makes us sad
    return true;      // but it's how webrtc.org behaves.

  if (aData[1] == 194)
    return true;

  if (aData[1] == 195) // IJ.
    return false;

  if ((aData[1] > 195) && (aData[1] < 200)) // the > 195 is redundant
    return true;

  if ((aData[1] >= 200) && (aData[1] <= 207)) // SR, RR, SDES, BYE,
    return false;                           // APP, RTPFB, PSFB, XR

  MOZ_ASSERT(false); // Not reached, belt and suspenders.
  return true;
}

TransportResult
TransportLayerSrtp::SendPacket(const unsigned char* data, size_t len)
{
  if (len < 4) {
    MOZ_ASSERT(false);
    return TE_ERROR;
  }

  // Make copy and add some room to expand.
  nsAutoPtr<DataBuffer> buf(
    new DataBuffer(data, len, len + SRTP_MAX_EXPANSION));

  int out_len;
  nsresult res;
  if (IsRtp(data, len)) {
    MOZ_MTLOG(ML_INFO, "Attempting to protect RTP...");
    res = mSendSrtp->ProtectRtp(
      buf->data(), buf->len(), buf->capacity(), &out_len);
  } else {
    MOZ_MTLOG(ML_INFO, "Attempting to protect RTCP...");
    res = mSendSrtp->ProtectRtcp(
      buf->data(), buf->len(), buf->capacity(), &out_len);
  }

  if (NS_FAILED(res)) {
    MOZ_MTLOG(ML_ERROR,
                "Error protecting RTP/RTCP len=" << len
                << "[" << std::hex
                << buf->data()[0] << " "
                << buf->data()[1] << " "
                << buf->data()[2] << " "
                << buf->data()[3]
                << "]");
    return TE_ERROR;
  }

  // paranoia; don't have uninitialized bytes included in data->len()
  buf->SetLength(out_len);

  TransportResult bytes = downward_->SendPacket(buf->data(), buf->len());
  if (bytes == static_cast<int>(buf->len())) {
    // Whole packet was written, but the encrypted length might be different.
    // Don't confuse the caller.
    return len;
  }

  if (bytes == TE_WOULDBLOCK) {
    return TE_WOULDBLOCK;
  }

  return TE_ERROR;
}

void
TransportLayerSrtp::StateChange(TransportLayer* layer, State state)
{
  if (state == TS_OPEN) {
    TransportLayerDtls* dtls = static_cast<TransportLayerDtls*>(layer);
    MOZ_ASSERT(dtls); // DTLS is mandatory

    uint16_t cipher_suite;
    nsresult res = dtls->GetSrtpCipher(&cipher_suite);
    if (NS_FAILED(res)) {
      MOZ_MTLOG(ML_ERROR, "Failed to negotiate DTLS-SRTP. This is an error");
      TL_SET_STATE(TS_ERROR);
      return;
    }

    // SRTP Key Exporter as per RFC 5764 S 4.2
    unsigned char srtp_block[SRTP_TOTAL_KEY_LENGTH * 2];
    res = dtls->ExportKeyingMaterial(
      kDTLSExporterLabel, false, "", srtp_block, sizeof(srtp_block));
    if (NS_FAILED(res)) {
      MOZ_MTLOG(ML_ERROR, "Failed to compute DTLS-SRTP keys. This is an error");
      TL_SET_STATE(TS_ERROR);
      return;
    }

    // Slice and dice as per RFC 5764 S 4.2
    unsigned char client_write_key[SRTP_TOTAL_KEY_LENGTH];
    unsigned char server_write_key[SRTP_TOTAL_KEY_LENGTH];
    int offset = 0;
    memcpy(client_write_key, srtp_block + offset, SRTP_MASTER_KEY_LENGTH);
    offset += SRTP_MASTER_KEY_LENGTH;
    memcpy(server_write_key, srtp_block + offset, SRTP_MASTER_KEY_LENGTH);
    offset += SRTP_MASTER_KEY_LENGTH;
    memcpy(client_write_key + SRTP_MASTER_KEY_LENGTH,
           srtp_block + offset,
           SRTP_MASTER_SALT_LENGTH);
    offset += SRTP_MASTER_SALT_LENGTH;
    memcpy(server_write_key + SRTP_MASTER_KEY_LENGTH,
           srtp_block + offset,
           SRTP_MASTER_SALT_LENGTH);
    offset += SRTP_MASTER_SALT_LENGTH;
    MOZ_ASSERT(offset == sizeof(srtp_block));

    unsigned char* write_key;
    unsigned char* read_key;

    if (dtls->role() == TransportLayerDtls::CLIENT) {
      write_key = client_write_key;
      read_key = server_write_key;
    } else {
      write_key = server_write_key;
      read_key = client_write_key;
    }

    MOZ_ASSERT(!mSendSrtp && !mRecvSrtp);
    mSendSrtp =
      SrtpFlow::Create(cipher_suite, false, write_key, SRTP_TOTAL_KEY_LENGTH);
    mRecvSrtp =
      SrtpFlow::Create(cipher_suite, true, read_key, SRTP_TOTAL_KEY_LENGTH);
    if (!mSendSrtp || !mRecvSrtp) {
      MOZ_MTLOG(ML_ERROR, "Couldn't create SRTP flow.");
      TL_SET_STATE(TS_ERROR);
      return;
    }

    MOZ_MTLOG(ML_INFO, "Created SRTP flow!");
  }

  TL_SET_STATE(state);
}

void
TransportLayerSrtp::PacketReceived(TransportLayer* layer,
                                   const unsigned char *data,
                                   size_t len)
{
  if (state() != TS_OPEN) {
    return;
  }

  if (len < 4) {
    return;
  }

  // not RTP/RTCP per RFC 7983
  if (data[0] <= 127 || data[0] >= 192) {
    return;
  }

  // Make a copy rather than cast away constness
  auto innerData = MakeUnique<unsigned char[]>(len);
  memcpy(innerData.get(), data, len);
  int outLen;
  nsresult res;

  if (IsRtp(innerData.get(), len)) {
    MOZ_MTLOG(ML_INFO, "Attempting to unprotect RTP...");
    res = mRecvSrtp->UnprotectRtp(innerData.get(), len, len, &outLen);
  } else {
    MOZ_MTLOG(ML_INFO, "Attempting to unprotect RTCP...");
    res = mRecvSrtp->UnprotectRtcp(innerData.get(), len, len, &outLen);
  }

  if (NS_SUCCEEDED(res)) {
    SignalPacketReceived(this, innerData.get(), outLen);
  } else {
    MOZ_MTLOG(ML_ERROR,
                "Error unprotecting RTP/RTCP len=" << len
                << "[" << std::hex
                << innerData[0] << " "
                << innerData[1] << " "
                << innerData[2] << " "
                << innerData[3]
                << "]");
  }
}

} // namespace mozilla


