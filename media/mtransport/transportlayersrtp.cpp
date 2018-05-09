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

static bool IsRtp(const unsigned char* data, size_t len)
{
  if (len < 2)
    return false;

  // Check if this is a RTCP packet. Logic based on the types listed in
  // media/webrtc/trunk/src/modules/rtp_rtcp/source/rtp_utility.cc

  // Anything outside this range is RTP.
  if ((data[1] < 192) || (data[1] > 207))
    return true;

  if (data[1] == 192) // FIR
    return false;

  if (data[1] == 193) // NACK, but could also be RTP. This makes us sad
    return true;      // but it's how webrtc.org behaves.

  if (data[1] == 194)
    return true;

  if (data[1] == 195) // IJ.
    return false;

  if ((data[1] > 195) && (data[1] < 200)) // the > 195 is redundant
    return true;

  if ((data[1] >= 200) && (data[1] <= 207)) // SR, RR, SDES, BYE,
    return false;                           // APP, RTPFB, PSFB, XR

  MOZ_ASSERT(false); // Not reached, belt and suspenders.
  return true;
}

TransportResult
TransportLayerSrtp::SendPacket(MediaPacket& packet)
{
  if (packet.len() < 4) {
    MOZ_ASSERT(false);
    return TE_ERROR;
  }

  MOZ_ASSERT(packet.capacity() - packet.len() >= SRTP_MAX_EXPANSION);

  int out_len;
  nsresult res;
  switch (packet.type()) {
    case MediaPacket::RTP:
      MOZ_MTLOG(ML_INFO, "Attempting to protect RTP...");
      res = mSendSrtp->ProtectRtp(packet.data(), packet.len(), packet.capacity(), &out_len);
      break;
    case MediaPacket::RTCP:
      MOZ_MTLOG(ML_INFO, "Attempting to protect RTCP...");
      res = mSendSrtp->ProtectRtcp(packet.data(), packet.len(), packet.capacity(), &out_len);
      break;
    default:
      MOZ_CRASH("SRTP layer asked to send packet that is neither RTP or RTCP");
  }

  if (NS_FAILED(res)) {
    MOZ_MTLOG(ML_ERROR,
                "Error protecting RTP/RTCP len=" << packet.len()
                << "[" << std::hex
                << packet.data()[0] << " "
                << packet.data()[1] << " "
                << packet.data()[2] << " "
                << packet.data()[3]
                << "]");
    return TE_ERROR;
  }

  size_t unencrypted_len = packet.len();
  packet.SetLength(out_len);

  TransportResult bytes = downward_->SendPacket(packet);
  if (bytes == out_len) {
    // Whole packet was written, but the encrypted length might be different.
    // Don't confuse the caller.
    return unencrypted_len;
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
TransportLayerSrtp::PacketReceived(TransportLayer* layer, MediaPacket& packet)
{
  if (state() != TS_OPEN) {
    return;
  }

  if (!packet.data()) {
    // Something ate this, probably the DTLS layer
    return;
  }

  if (packet.len() < 4) {
    return;
  }

  // not RTP/RTCP per RFC 7983
  if (packet.data()[0] <= 127 || packet.data()[0] >= 192) {
    return;
  }

  // We want to keep the encrypted packet around for packet dumping
  packet.CopyDataToEncrypted();
  int outLen;
  nsresult res;

  if (IsRtp(packet.data(), packet.len())) {
    packet.SetType(MediaPacket::RTP);
    MOZ_MTLOG(ML_INFO, "Attempting to unprotect RTP...");
    res = mRecvSrtp->UnprotectRtp(packet.data(), packet.len(), packet.len(), &outLen);
  } else {
    packet.SetType(MediaPacket::RTCP);
    MOZ_MTLOG(ML_INFO, "Attempting to unprotect RTCP...");
    res = mRecvSrtp->UnprotectRtcp(packet.data(), packet.len(), packet.len(), &outLen);
  }

  if (NS_SUCCEEDED(res)) {
    packet.SetLength(outLen);
    SignalPacketReceived(this, packet);
  } else {
    // TODO: What do we do wrt packet dumping here? Maybe signal an empty
    // packet? Signal the still-encrypted packet?
    MOZ_MTLOG(ML_ERROR,
                "Error unprotecting RTP/RTCP len=" << packet.len()
                << "[" << std::hex
                << packet.data()[0] << " "
                << packet.data()[1] << " "
                << packet.data()[2] << " "
                << packet.data()[3]
                << "]");
  }
}

} // namespace mozilla


