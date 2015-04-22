/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include "MediaPipeline.h"

#ifndef USE_FAKE_MEDIA_STREAMS
#include "MediaStreamGraphImpl.h"
#endif

#include <math.h>

#include "nspr.h"
#include "srtp.h"

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
#include "VideoSegment.h"
#include "Layers.h"
#include "ImageTypes.h"
#include "ImageContainer.h"
#include "VideoUtils.h"
#ifdef WEBRTC_GONK
#include "GrallocImages.h"
#include "mozilla/layers/GrallocTextureClient.h"
#endif
#endif

#include "nsError.h"
#include "AudioSegment.h"
#include "MediaSegment.h"
#include "databuffer.h"
#include "transportflow.h"
#include "transportlayer.h"
#include "transportlayerdtls.h"
#include "transportlayerice.h"
#include "runnable_utils.h"
#include "libyuv/convert.h"
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
#include "mozilla/PeerIdentity.h"
#endif
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Types.h"

#include "logging.h"

using namespace mozilla;
using namespace mozilla::gfx;

// Logging context
MOZ_MTLOG_MODULE("mediapipeline")

namespace mozilla {

static char kDTLSExporterLabel[] = "EXTRACTOR-dtls_srtp";

MediaPipeline::~MediaPipeline() {
  ASSERT_ON_THREAD(main_thread_);
  MOZ_ASSERT(!stream_);  // Check that we have shut down already.
  MOZ_MTLOG(ML_INFO, "Destroying MediaPipeline: " << description_);
}

nsresult MediaPipeline::Init() {
  ASSERT_ON_THREAD(main_thread_);

  if (direction_ == RECEIVE) {
    conduit_->SetReceiverTransport(transport_);
  } else {
    conduit_->SetTransmitterTransport(transport_);
  }

  RUN_ON_THREAD(sts_thread_,
                WrapRunnable(
                    nsRefPtr<MediaPipeline>(this),
                    &MediaPipeline::Init_s),
                NS_DISPATCH_NORMAL);

  return NS_OK;
}

nsresult MediaPipeline::Init_s() {
  ASSERT_ON_THREAD(sts_thread_);

  return AttachTransport_s();
}


// Disconnect us from the transport so that we can cleanly destruct the
// pipeline on the main thread.  ShutdownMedia_m() must have already been
// called
void MediaPipeline::ShutdownTransport_s() {
  ASSERT_ON_THREAD(sts_thread_);
  MOZ_ASSERT(!stream_); // verifies that ShutdownMedia_m() has run

  DetachTransport_s();
}

void
MediaPipeline::DetachTransport_s()
{
  ASSERT_ON_THREAD(sts_thread_);

  disconnect_all();
  transport_->Detach();
  rtp_.Detach();
  rtcp_.Detach();
}

nsresult
MediaPipeline::AttachTransport_s()
{
  ASSERT_ON_THREAD(sts_thread_);
  nsresult res;
  MOZ_ASSERT(rtp_.transport_);
  MOZ_ASSERT(rtcp_.transport_);
  res = ConnectTransport_s(rtp_);
  if (NS_FAILED(res)) {
    return res;
  }

  if (rtcp_.transport_ != rtp_.transport_) {
    res = ConnectTransport_s(rtcp_);
    if (NS_FAILED(res)) {
      return res;
    }
  }
  return NS_OK;
}

void
MediaPipeline::UpdateTransport_m(int level,
                                 RefPtr<TransportFlow> rtp_transport,
                                 RefPtr<TransportFlow> rtcp_transport,
                                 nsAutoPtr<MediaPipelineFilter> filter)
{
  RUN_ON_THREAD(sts_thread_,
                WrapRunnable(
                    this,
                    &MediaPipeline::UpdateTransport_s,
                    level,
                    rtp_transport,
                    rtcp_transport,
                    filter),
                NS_DISPATCH_NORMAL);
}

void
MediaPipeline::UpdateTransport_s(int level,
                                 RefPtr<TransportFlow> rtp_transport,
                                 RefPtr<TransportFlow> rtcp_transport,
                                 nsAutoPtr<MediaPipelineFilter> filter)
{
  bool rtcp_mux = false;
  if (!rtcp_transport) {
    rtcp_transport = rtp_transport;
    rtcp_mux = true;
  }

  if ((rtp_transport != rtp_.transport_) ||
      (rtcp_transport != rtcp_.transport_)) {
    DetachTransport_s();
    rtp_ = TransportInfo(rtp_transport, rtcp_mux ? MUX : RTP);
    rtcp_ = TransportInfo(rtcp_transport, rtcp_mux ? MUX : RTCP);
    AttachTransport_s();
  }

  level_ = level;

  if (filter_ && filter) {
    // Use the new filter, but don't forget any remote SSRCs that we've learned
    // by receiving traffic.
    filter_->Update(*filter);
  } else {
    filter_ = filter;
  }
}

void MediaPipeline::StateChange(TransportFlow *flow, TransportLayer::State state) {
  TransportInfo* info = GetTransportInfo_s(flow);
  MOZ_ASSERT(info);

  if (state == TransportLayer::TS_OPEN) {
    MOZ_MTLOG(ML_INFO, "Flow is ready");
    TransportReady_s(*info);
  } else if (state == TransportLayer::TS_CLOSED ||
             state == TransportLayer::TS_ERROR) {
    TransportFailed_s(*info);
  }
}

static bool MakeRtpTypeToStringArray(const char** array) {
  static const char* RTP_str = "RTP";
  static const char* RTCP_str = "RTCP";
  static const char* MUX_str = "RTP/RTCP mux";
  array[MediaPipeline::RTP] = RTP_str;
  array[MediaPipeline::RTCP] = RTCP_str;
  array[MediaPipeline::MUX] = MUX_str;
  return true;
}

static const char* ToString(MediaPipeline::RtpType type) {
  static const char* array[(int)MediaPipeline::MAX_RTP_TYPE] = {nullptr};
  // Dummy variable to cause init to happen only on first call
  static bool dummy = MakeRtpTypeToStringArray(array);
  (void)dummy;
  return array[type];
}

nsresult MediaPipeline::TransportReady_s(TransportInfo &info) {
  MOZ_ASSERT(!description_.empty());

  // TODO(ekr@rtfm.com): implement some kind of notification on
  // failure. bug 852665.
  if (info.state_ != MP_CONNECTING) {
    MOZ_MTLOG(ML_ERROR, "Transport ready for flow in wrong state:" <<
              description_ << ": " << ToString(info.type_));
    return NS_ERROR_FAILURE;
  }

  MOZ_MTLOG(ML_INFO, "Transport ready for pipeline " <<
            static_cast<void *>(this) << " flow " << description_ << ": " <<
            ToString(info.type_));

  // TODO(bcampen@mozilla.com): Should we disconnect from the flow on failure?
  nsresult res;

  // Now instantiate the SRTP objects
  TransportLayerDtls *dtls = static_cast<TransportLayerDtls *>(
      info.transport_->GetLayer(TransportLayerDtls::ID()));
  MOZ_ASSERT(dtls);  // DTLS is mandatory

  uint16_t cipher_suite;
  res = dtls->GetSrtpCipher(&cipher_suite);
  if (NS_FAILED(res)) {
    MOZ_MTLOG(ML_ERROR, "Failed to negotiate DTLS-SRTP. This is an error");
    info.state_ = MP_CLOSED;
    UpdateRtcpMuxState(info);
    return res;
  }

  // SRTP Key Exporter as per RFC 5764 S 4.2
  unsigned char srtp_block[SRTP_TOTAL_KEY_LENGTH * 2];
  res = dtls->ExportKeyingMaterial(kDTLSExporterLabel, false, "",
                                   srtp_block, sizeof(srtp_block));
  if (NS_FAILED(res)) {
    MOZ_MTLOG(ML_ERROR, "Failed to compute DTLS-SRTP keys. This is an error");
    info.state_ = MP_CLOSED;
    UpdateRtcpMuxState(info);
    MOZ_CRASH();  // TODO: Remove once we have enough field experience to
                  // know it doesn't happen. bug 798797. Note that the
                  // code after this never executes.
    return res;
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
         srtp_block + offset, SRTP_MASTER_SALT_LENGTH);
  offset += SRTP_MASTER_SALT_LENGTH;
  memcpy(server_write_key + SRTP_MASTER_KEY_LENGTH,
         srtp_block + offset, SRTP_MASTER_SALT_LENGTH);
  offset += SRTP_MASTER_SALT_LENGTH;
  MOZ_ASSERT(offset == sizeof(srtp_block));

  unsigned char *write_key;
  unsigned char *read_key;

  if (dtls->role() == TransportLayerDtls::CLIENT) {
    write_key = client_write_key;
    read_key = server_write_key;
  } else {
    write_key = server_write_key;
    read_key = client_write_key;
  }

  MOZ_ASSERT(!info.send_srtp_ && !info.recv_srtp_);
  info.send_srtp_ = SrtpFlow::Create(cipher_suite, false, write_key,
                                     SRTP_TOTAL_KEY_LENGTH);
  info.recv_srtp_ = SrtpFlow::Create(cipher_suite, true, read_key,
                                     SRTP_TOTAL_KEY_LENGTH);
  if (!info.send_srtp_ || !info.recv_srtp_) {
    MOZ_MTLOG(ML_ERROR, "Couldn't create SRTP flow for "
              << ToString(info.type_));
    info.state_ = MP_CLOSED;
    UpdateRtcpMuxState(info);
    return NS_ERROR_FAILURE;
  }

    MOZ_MTLOG(ML_INFO, "Listening for " << ToString(info.type_)
                       << " packets received on " <<
                       static_cast<void *>(dtls->downward()));

  switch (info.type_) {
    case RTP:
      dtls->downward()->SignalPacketReceived.connect(
          this,
          &MediaPipeline::RtpPacketReceived);
      break;
    case RTCP:
      dtls->downward()->SignalPacketReceived.connect(
          this,
          &MediaPipeline::RtcpPacketReceived);
      break;
    case MUX:
      dtls->downward()->SignalPacketReceived.connect(
          this,
          &MediaPipeline::PacketReceived);
      break;
    default:
      MOZ_CRASH();
  }

  info.state_ = MP_OPEN;
  UpdateRtcpMuxState(info);
  return NS_OK;
}

nsresult MediaPipeline::TransportFailed_s(TransportInfo &info) {
  ASSERT_ON_THREAD(sts_thread_);

  info.state_ = MP_CLOSED;
  UpdateRtcpMuxState(info);

  MOZ_MTLOG(ML_INFO, "Transport closed for flow " << ToString(info.type_));

  NS_WARNING(
      "MediaPipeline Transport failed. This is not properly cleaned up yet");

  // TODO(ekr@rtfm.com): SECURITY: Figure out how to clean up if the
  // connection was good and now it is bad.
  // TODO(ekr@rtfm.com): Report up so that the PC knows we
  // have experienced an error.

  return NS_OK;
}

void MediaPipeline::UpdateRtcpMuxState(TransportInfo &info) {
  if (info.type_ == MUX) {
    if (info.transport_ == rtcp_.transport_) {
      rtcp_.state_ = info.state_;
      if (!rtcp_.send_srtp_) {
        rtcp_.send_srtp_ = info.send_srtp_;
        rtcp_.recv_srtp_ = info.recv_srtp_;
      }
    }
  }
}

nsresult MediaPipeline::SendPacket(TransportFlow *flow, const void *data,
                                   int len) {
  ASSERT_ON_THREAD(sts_thread_);

  // Note that we bypass the DTLS layer here
  TransportLayerDtls *dtls = static_cast<TransportLayerDtls *>(
      flow->GetLayer(TransportLayerDtls::ID()));
  MOZ_ASSERT(dtls);

  TransportResult res = dtls->downward()->
      SendPacket(static_cast<const unsigned char *>(data), len);

  if (res != len) {
    // Ignore blocking indications
    if (res == TE_WOULDBLOCK)
      return NS_OK;

    MOZ_MTLOG(ML_ERROR, "Failed write on stream " << description_);
    return NS_BASE_STREAM_CLOSED;
  }

  return NS_OK;
}

void MediaPipeline::increment_rtp_packets_sent(int32_t bytes) {
  ++rtp_packets_sent_;
  rtp_bytes_sent_ += bytes;

  if (!(rtp_packets_sent_ % 100)) {
    MOZ_MTLOG(ML_INFO, "RTP sent packet count for " << description_
              << " Pipeline " << static_cast<void *>(this)
              << " Flow : " << static_cast<void *>(rtp_.transport_)
              << ": " << rtp_packets_sent_
              << " (" << rtp_bytes_sent_ << " bytes)");
  }
}

void MediaPipeline::increment_rtcp_packets_sent() {
  ++rtcp_packets_sent_;
  if (!(rtcp_packets_sent_ % 100)) {
    MOZ_MTLOG(ML_INFO, "RTCP sent packet count for " << description_
              << " Pipeline " << static_cast<void *>(this)
              << " Flow : " << static_cast<void *>(rtcp_.transport_)
              << ": " << rtcp_packets_sent_);
  }
}

void MediaPipeline::increment_rtp_packets_received(int32_t bytes) {
  ++rtp_packets_received_;
  rtp_bytes_received_ += bytes;
  if (!(rtp_packets_received_ % 100)) {
    MOZ_MTLOG(ML_INFO, "RTP received packet count for " << description_
              << " Pipeline " << static_cast<void *>(this)
              << " Flow : " << static_cast<void *>(rtp_.transport_)
              << ": " << rtp_packets_received_
              << " (" << rtp_bytes_received_ << " bytes)");
  }
}

void MediaPipeline::increment_rtcp_packets_received() {
  ++rtcp_packets_received_;
  if (!(rtcp_packets_received_ % 100)) {
    MOZ_MTLOG(ML_INFO, "RTCP received packet count for " << description_
              << " Pipeline " << static_cast<void *>(this)
              << " Flow : " << static_cast<void *>(rtcp_.transport_)
              << ": " << rtcp_packets_received_);
  }
}

void MediaPipeline::RtpPacketReceived(TransportLayer *layer,
                                      const unsigned char *data,
                                      size_t len) {
  if (!transport_->pipeline()) {
    MOZ_MTLOG(ML_ERROR, "Discarding incoming packet; transport disconnected");
    return;
  }

  if (!conduit_) {
    MOZ_MTLOG(ML_DEBUG, "Discarding incoming packet; media disconnected");
    return;
  }

  if (rtp_.state_ != MP_OPEN) {
    MOZ_MTLOG(ML_ERROR, "Discarding incoming packet; pipeline not open");
    return;
  }

  if (rtp_.transport_->state() != TransportLayer::TS_OPEN) {
    MOZ_MTLOG(ML_ERROR, "Discarding incoming packet; transport not open");
    return;
  }

  // This should never happen.
  MOZ_ASSERT(rtp_.recv_srtp_);

  if (direction_ == TRANSMIT) {
    return;
  }

  if (!len) {
    return;
  }

  // Filter out everything but RTP/RTCP
  if (data[0] < 128 || data[0] > 191) {
    return;
  }

  if (filter_) {
    webrtc::RTPHeader header;
    if (!rtp_parser_->Parse(data, len, &header) ||
        !filter_->Filter(header)) {
      return;
    }
  }

  // Make a copy rather than cast away constness
  ScopedDeletePtr<unsigned char> inner_data(
      new unsigned char[len]);
  memcpy(inner_data, data, len);
  int out_len = 0;
  nsresult res = rtp_.recv_srtp_->UnprotectRtp(inner_data,
                                               len, len, &out_len);
  if (!NS_SUCCEEDED(res)) {
    char tmp[16];

    PR_snprintf(tmp, sizeof(tmp), "%.2x %.2x %.2x %.2x",
                inner_data[0],
                inner_data[1],
                inner_data[2],
                inner_data[3]);

    MOZ_MTLOG(ML_NOTICE, "Error unprotecting RTP in " << description_
              << "len= " << len << "[" << tmp << "...]");

    return;
  }
  MOZ_MTLOG(ML_DEBUG, description_ << " received RTP packet.");
  increment_rtp_packets_received(out_len);

  (void)conduit_->ReceivedRTPPacket(inner_data, out_len);  // Ignore error codes
}

void MediaPipeline::RtcpPacketReceived(TransportLayer *layer,
                                       const unsigned char *data,
                                       size_t len) {
  if (!transport_->pipeline()) {
    MOZ_MTLOG(ML_DEBUG, "Discarding incoming packet; transport disconnected");
    return;
  }

  if (!conduit_) {
    MOZ_MTLOG(ML_DEBUG, "Discarding incoming packet; media disconnected");
    return;
  }

  if (rtcp_.state_ != MP_OPEN) {
    MOZ_MTLOG(ML_DEBUG, "Discarding incoming packet; pipeline not open");
    return;
  }

  if (rtcp_.transport_->state() != TransportLayer::TS_OPEN) {
    MOZ_MTLOG(ML_ERROR, "Discarding incoming packet; transport not open");
    return;
  }

  if (!len) {
    return;
  }

  // Filter out everything but RTP/RTCP
  if (data[0] < 128 || data[0] > 191) {
    return;
  }

  // Make a copy rather than cast away constness
  ScopedDeletePtr<unsigned char> inner_data(
      new unsigned char[len]);
  memcpy(inner_data, data, len);
  int out_len;

  nsresult res = rtcp_.recv_srtp_->UnprotectRtcp(inner_data,
                                                 len,
                                                 len,
                                                 &out_len);

  if (!NS_SUCCEEDED(res))
    return;

  // We do not filter RTCP for send pipelines, since the webrtc.org code for
  // senders already has logic to ignore RRs that do not apply.
  if (filter_ && direction_ == RECEIVE) {
    if (!filter_->FilterSenderReport(inner_data, out_len)) {
      MOZ_MTLOG(ML_NOTICE, "Dropping rtcp packet");
      return;
    }
  }

  MOZ_MTLOG(ML_DEBUG, description_ << " received RTCP packet.");
  increment_rtcp_packets_received();

  MOZ_ASSERT(rtcp_.recv_srtp_);  // This should never happen

  (void)conduit_->ReceivedRTCPPacket(inner_data, out_len);  // Ignore error codes
}

bool MediaPipeline::IsRtp(const unsigned char *data, size_t len) {
  if (len < 2)
    return false;

  // Check if this is a RTCP packet. Logic based on the types listed in
  // media/webrtc/trunk/src/modules/rtp_rtcp/source/rtp_utility.cc

  // Anything outside this range is RTP.
  if ((data[1] < 192) || (data[1] > 207))
    return true;

  if (data[1] == 192)  // FIR
    return false;

  if (data[1] == 193)  // NACK, but could also be RTP. This makes us sad
    return true;       // but it's how webrtc.org behaves.

  if (data[1] == 194)
    return true;

  if (data[1] == 195)  // IJ.
    return false;

  if ((data[1] > 195) && (data[1] < 200))  // the > 195 is redundant
    return true;

  if ((data[1] >= 200) && (data[1] <= 207))  // SR, RR, SDES, BYE,
    return false;                            // APP, RTPFB, PSFB, XR

  MOZ_ASSERT(false);  // Not reached, belt and suspenders.
  return true;
}

void MediaPipeline::PacketReceived(TransportLayer *layer,
                                   const unsigned char *data,
                                   size_t len) {
  if (!transport_->pipeline()) {
    MOZ_MTLOG(ML_DEBUG, "Discarding incoming packet; transport disconnected");
    return;
  }

  if (IsRtp(data, len)) {
    RtpPacketReceived(layer, data, len);
  } else {
    RtcpPacketReceived(layer, data, len);
  }
}

nsresult MediaPipelineTransmit::Init() {
  AttachToTrack(track_id_);

  return MediaPipeline::Init();
}

void MediaPipelineTransmit::AttachToTrack(const std::string& track_id) {
  ASSERT_ON_THREAD(main_thread_);

  description_ = pc_ + "| ";
  description_ += conduit_->type() == MediaSessionConduit::AUDIO ?
      "Transmit audio[" : "Transmit video[";
  description_ += track_id;
  description_ += "]";

  // TODO(ekr@rtfm.com): Check for errors
  MOZ_MTLOG(ML_DEBUG, "Attaching pipeline to stream "
            << static_cast<void *>(stream_) << " conduit type=" <<
            (conduit_->type() == MediaSessionConduit::AUDIO ?"audio":"video"));

  stream_->AddListener(listener_);

 // // Is this a gUM mediastream?  If so, also register the Listener directly with
 // // the SourceMediaStream that's attached to the TrackUnion so we can get direct
 // // unqueued (and not resampled) data
 // if (domstream_->AddDirectListener(listener_)) {
 //   listener_->direct_connect_ = true;
 // }

#ifndef MOZILLA_INTERNAL_API
  // this enables the unit tests that can't fiddle with principals and the like
  listener_->SetEnabled(true);
#endif
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
void MediaPipelineTransmit::UpdateSinkIdentity_m(nsIPrincipal* principal,
                                                 const PeerIdentity* sinkIdentity) {
  ASSERT_ON_THREAD(main_thread_);
  bool enableStream = principal->Subsumes(domstream_->GetPrincipal());
  if (!enableStream) {
    // first try didn't work, but there's a chance that this is still available
    // if our stream is bound to a peerIdentity, and the peer connection (our
    // sink) is bound to the same identity, then we can enable the stream
    PeerIdentity* streamIdentity = domstream_->GetPeerIdentity();
    if (sinkIdentity && streamIdentity) {
      enableStream = (*sinkIdentity == *streamIdentity);
    }
  }

  listener_->SetEnabled(enableStream);
}
#endif

nsresult MediaPipelineTransmit::TransportReady_s(TransportInfo &info) {
  ASSERT_ON_THREAD(sts_thread_);
  // Call base ready function.
  MediaPipeline::TransportReady_s(info);

  // Should not be set for a transmitter
  if (&info == &rtp_) {
    listener_->SetActive(true);
  }

  return NS_OK;
}

nsresult MediaPipelineTransmit::ReplaceTrack(DOMMediaStream *domstream,
                                             const std::string& track_id) {
  // MainThread, checked in calls we make
  MOZ_MTLOG(ML_DEBUG, "Reattaching pipeline " << description_ << " to stream "
            << static_cast<void *>(domstream->GetStream())
            << " track " << track_id << " conduit type=" <<
            (conduit_->type() == MediaSessionConduit::AUDIO ?"audio":"video"));

  if (domstream_) { // may be excessive paranoia
    DetachMediaStream();
  }
  domstream_ = domstream; // Detach clears it
  stream_ = domstream->GetStream();
  // Unsets the track id after RemoveListener() takes effect.
  listener_->UnsetTrackId(stream_->GraphImpl());
  track_id_ = track_id;
  AttachToTrack(track_id);
  return NS_OK;
}

void MediaPipeline::DisconnectTransport_s(TransportInfo &info) {
  MOZ_ASSERT(info.transport_);
  ASSERT_ON_THREAD(sts_thread_);

  info.transport_->SignalStateChange.disconnect(this);
  // We do this even if we're a transmitter, since we are still possibly
  // registered to receive RTCP.
  TransportLayerDtls *dtls = static_cast<TransportLayerDtls *>(
      info.transport_->GetLayer(TransportLayerDtls::ID()));
  MOZ_ASSERT(dtls);  // DTLS is mandatory
  MOZ_ASSERT(dtls->downward());
  dtls->downward()->SignalPacketReceived.disconnect(this);
}

nsresult MediaPipeline::ConnectTransport_s(TransportInfo &info) {
  MOZ_ASSERT(info.transport_);
  ASSERT_ON_THREAD(sts_thread_);

  // Look to see if the transport is ready
  if (info.transport_->state() == TransportLayer::TS_OPEN) {
    nsresult res = TransportReady_s(info);
    if (NS_FAILED(res)) {
      MOZ_MTLOG(ML_ERROR, "Error calling TransportReady(); res="
                << static_cast<uint32_t>(res) << " in " << __FUNCTION__);
      return res;
    }
  } else if (info.transport_->state() == TransportLayer::TS_ERROR) {
    MOZ_MTLOG(ML_ERROR, ToString(info.type_)
                        << "transport is already in error state");
    TransportFailed_s(info);
    return NS_ERROR_FAILURE;
  }

  info.transport_->SignalStateChange.connect(this,
                                             &MediaPipeline::StateChange);

  return NS_OK;
}

MediaPipeline::TransportInfo* MediaPipeline::GetTransportInfo_s(
    TransportFlow *flow) {
  ASSERT_ON_THREAD(sts_thread_);
  if (flow == rtp_.transport_) {
    return &rtp_;
  }

  if (flow == rtcp_.transport_) {
    return &rtcp_;
  }

  return nullptr;
}

nsresult MediaPipeline::PipelineTransport::SendRtpPacket(
    const void *data, int len) {

    nsAutoPtr<DataBuffer> buf(new DataBuffer(static_cast<const uint8_t *>(data),
                                             len));

    RUN_ON_THREAD(sts_thread_,
                  WrapRunnable(
                      RefPtr<MediaPipeline::PipelineTransport>(this),
                      &MediaPipeline::PipelineTransport::SendRtpPacket_s,
                      buf),
                  NS_DISPATCH_NORMAL);

    return NS_OK;
}

nsresult MediaPipeline::PipelineTransport::SendRtpPacket_s(
    nsAutoPtr<DataBuffer> data) {
  ASSERT_ON_THREAD(sts_thread_);
  if (!pipeline_)
    return NS_OK;  // Detached

  if (!pipeline_->rtp_.send_srtp_) {
    MOZ_MTLOG(ML_DEBUG, "Couldn't write RTP packet; SRTP not set up yet");
    return NS_OK;
  }

  MOZ_ASSERT(pipeline_->rtp_.transport_);
  NS_ENSURE_TRUE(pipeline_->rtp_.transport_, NS_ERROR_NULL_POINTER);

  // libsrtp enciphers in place, so we need a new, big enough
  // buffer.
  // XXX. allocates and deletes one buffer per packet sent.
  // Bug 822129
  int max_len = data->len() + SRTP_MAX_EXPANSION;
  ScopedDeletePtr<unsigned char> inner_data(
      new unsigned char[max_len]);
  memcpy(inner_data, data->data(), data->len());

  int out_len;
  nsresult res = pipeline_->rtp_.send_srtp_->ProtectRtp(inner_data,
                                                        data->len(),
                                                        max_len,
                                                        &out_len);
  if (!NS_SUCCEEDED(res))
    return res;

  MOZ_MTLOG(ML_DEBUG, pipeline_->description_ << " sending RTP packet.");
  pipeline_->increment_rtp_packets_sent(out_len);
  return pipeline_->SendPacket(pipeline_->rtp_.transport_, inner_data,
                               out_len);
}

nsresult MediaPipeline::PipelineTransport::SendRtcpPacket(
    const void *data, int len) {

    nsAutoPtr<DataBuffer> buf(new DataBuffer(static_cast<const uint8_t *>(data),
                                             len));

    RUN_ON_THREAD(sts_thread_,
                  WrapRunnable(
                      RefPtr<MediaPipeline::PipelineTransport>(this),
                      &MediaPipeline::PipelineTransport::SendRtcpPacket_s,
                      buf),
                  NS_DISPATCH_NORMAL);

    return NS_OK;
}

nsresult MediaPipeline::PipelineTransport::SendRtcpPacket_s(
    nsAutoPtr<DataBuffer> data) {
  ASSERT_ON_THREAD(sts_thread_);
  if (!pipeline_)
    return NS_OK;  // Detached

  if (!pipeline_->rtcp_.send_srtp_) {
    MOZ_MTLOG(ML_DEBUG, "Couldn't write RTCP packet; SRTCP not set up yet");
    return NS_OK;
  }

  MOZ_ASSERT(pipeline_->rtcp_.transport_);
  NS_ENSURE_TRUE(pipeline_->rtcp_.transport_, NS_ERROR_NULL_POINTER);

  // libsrtp enciphers in place, so we need a new, big enough
  // buffer.
  // XXX. allocates and deletes one buffer per packet sent.
  // Bug 822129.
  int max_len = data->len() + SRTP_MAX_EXPANSION;
  ScopedDeletePtr<unsigned char> inner_data(
      new unsigned char[max_len]);
  memcpy(inner_data, data->data(), data->len());

  int out_len;
  nsresult res = pipeline_->rtcp_.send_srtp_->ProtectRtcp(inner_data,
                                                          data->len(),
                                                          max_len,
                                                          &out_len);

  if (!NS_SUCCEEDED(res))
    return res;

  MOZ_MTLOG(ML_DEBUG, pipeline_->description_ << " sending RTCP packet.");
  pipeline_->increment_rtcp_packets_sent();
  return pipeline_->SendPacket(pipeline_->rtcp_.transport_, inner_data,
                               out_len);
}

void MediaPipelineTransmit::PipelineListener::
UnsetTrackId(MediaStreamGraphImpl* graph) {
#ifndef USE_FAKE_MEDIA_STREAMS
  class Message : public ControlMessage {
  public:
    Message(PipelineListener* listener) :
      ControlMessage(nullptr), listener_(listener) {}
    virtual void Run() override
    {
      listener_->UnsetTrackIdImpl();
    }
    nsRefPtr<PipelineListener> listener_;
  };
  graph->AppendMessage(new Message(this));
#else
  UnsetTrackIdImpl();
#endif
}
// Called if we're attached with AddDirectListener()
void MediaPipelineTransmit::PipelineListener::
NotifyRealtimeData(MediaStreamGraph* graph, TrackID tid,
                   StreamTime offset,
                   uint32_t events,
                   const MediaSegment& media) {
  MOZ_MTLOG(ML_DEBUG, "MediaPipeline::NotifyRealtimeData()");

  NewData(graph, tid, offset, events, media);
}

void MediaPipelineTransmit::PipelineListener::
NotifyQueuedTrackChanges(MediaStreamGraph* graph, TrackID tid,
                         StreamTime offset,
                         uint32_t events,
                         const MediaSegment& queued_media) {
  MOZ_MTLOG(ML_DEBUG, "MediaPipeline::NotifyQueuedTrackChanges()");

  // ignore non-direct data if we're also getting direct data
  if (!direct_connect_) {
    NewData(graph, tid, offset, events, queued_media);
  }
}

// I420 buffer size macros
#define YSIZE(x,y) ((x)*(y))
#define CRSIZE(x,y) ((((x)+1) >> 1) * (((y)+1) >> 1))
#define I420SIZE(x,y) (YSIZE((x),(y)) + 2 * CRSIZE((x),(y)))

// XXX NOTE: this code will have to change when we get support for multiple tracks of type
// in a MediaStream and especially in a PeerConnection stream.  bug 1056650
// It should be matching on the "correct" track for the pipeline, not just "any video track".

void MediaPipelineTransmit::PipelineListener::
NewData(MediaStreamGraph* graph, TrackID tid,
        StreamTime offset,
        uint32_t events,
        const MediaSegment& media) {
  if (!active_) {
    MOZ_MTLOG(ML_DEBUG, "Discarding packets because transport not ready");
    return;
  }

  if (track_id_ != TRACK_INVALID) {
    if (tid != track_id_) {
      return;
    }
  } else if (conduit_->type() !=
             (media.GetType() == MediaSegment::AUDIO ? MediaSessionConduit::AUDIO :
                                                       MediaSessionConduit::VIDEO)) {
    // Ignore data in case we have a muxed stream
    return;
  } else {
    // Don't lock during normal media flow except on first sample
    MutexAutoLock lock(mMutex);
    track_id_ = track_id_external_ = tid;
  }

  // TODO(ekr@rtfm.com): For now assume that we have only one
  // track type and it's destined for us
  // See bug 784517
  if (media.GetType() == MediaSegment::AUDIO) {
    AudioSegment* audio = const_cast<AudioSegment *>(
        static_cast<const AudioSegment *>(&media));

    AudioSegment::ChunkIterator iter(*audio);
    while(!iter.IsEnded()) {
      TrackRate rate;
#ifdef USE_FAKE_MEDIA_STREAMS
      rate = Fake_MediaStream::GraphRate();
#else
      rate = graph->GraphRate();
#endif
      ProcessAudioChunk(static_cast<AudioSessionConduit*>(conduit_.get()),
                        rate, *iter);
      iter.Next();
    }
  } else if (media.GetType() == MediaSegment::VIDEO) {
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
    VideoSegment* video = const_cast<VideoSegment *>(
        static_cast<const VideoSegment *>(&media));

    VideoSegment::ChunkIterator iter(*video);
    while(!iter.IsEnded()) {
      ProcessVideoChunk(static_cast<VideoSessionConduit*>(conduit_.get()),
                        *iter);
      iter.Next();
    }
#endif
  } else {
    // Ignore
  }
}

void MediaPipelineTransmit::PipelineListener::ProcessAudioChunk(
    AudioSessionConduit *conduit,
    TrackRate rate,
    AudioChunk& chunk) {
  // TODO(ekr@rtfm.com): Do more than one channel
  nsAutoArrayPtr<int16_t> samples(new int16_t[chunk.mDuration]);

  if (enabled_ && chunk.mBuffer) {
    switch (chunk.mBufferFormat) {
      case AUDIO_FORMAT_FLOAT32:
        {
          const float* buf = static_cast<const float *>(chunk.mChannelData[0]);
          ConvertAudioSamplesWithScale(buf, static_cast<int16_t*>(samples),
                                       chunk.mDuration, chunk.mVolume);
        }
        break;
      case AUDIO_FORMAT_S16:
        {
          const short* buf = static_cast<const short *>(chunk.mChannelData[0]);
          ConvertAudioSamplesWithScale(buf, samples, chunk.mDuration, chunk.mVolume);
        }
        break;
      case AUDIO_FORMAT_SILENCE:
        memset(samples, 0, chunk.mDuration * sizeof(samples[0]));
        break;
      default:
        MOZ_ASSERT(PR_FALSE);
        return;
        break;
    }
  } else {
    // This means silence.
    memset(samples, 0, chunk.mDuration * sizeof(samples[0]));
  }

  MOZ_ASSERT(!(rate%100)); // rate should be a multiple of 100

  // Check if the rate has changed since the last time we came through
  // I realize it may be overkill to check if the rate has changed, but
  // I believe it is possible (e.g. if we change sources) and it costs us
  // very little to handle this case

  if (samplenum_10ms_ !=  rate/100) {
    // Determine number of samples in 10 ms from the rate:
    samplenum_10ms_ = rate/100;
    // If we switch sample rates (e.g. if we switch codecs),
    // we throw away what was in the sample_10ms_buffer at the old rate
    samples_10ms_buffer_ = new int16_t[samplenum_10ms_];
    buffer_current_ = 0;
  }

  // Vars to handle the non-sunny-day case (where the audio chunks
  // we got are not multiples of 10ms OR there were samples left over
  // from the last run)
  int64_t chunk_remaining;
  int64_t tocpy;
  int16_t *samples_tmp = samples.get();

  chunk_remaining = chunk.mDuration;

  MOZ_ASSERT(chunk_remaining >= 0);

  if (buffer_current_) {
    tocpy = std::min(chunk_remaining, samplenum_10ms_ - buffer_current_);
    memcpy(&samples_10ms_buffer_[buffer_current_], samples_tmp, tocpy * sizeof(int16_t));
    buffer_current_ += tocpy;
    samples_tmp += tocpy;
    chunk_remaining -= tocpy;

    if (buffer_current_ == samplenum_10ms_) {
      // Send out the audio buffer we just finished filling
      conduit->SendAudioFrame(samples_10ms_buffer_, samplenum_10ms_, rate, 0);
      buffer_current_ = 0;
    } else {
      // We still don't have enough data to send a buffer
      return;
    }
  }

  // Now send (more) frames if there is more than 10ms of input left
  tocpy = (chunk_remaining / samplenum_10ms_) * samplenum_10ms_;
  if (tocpy > 0) {
    conduit->SendAudioFrame(samples_tmp, tocpy, rate, 0);
    samples_tmp += tocpy;
    chunk_remaining -= tocpy;
  }
  // Copy what remains for the next run

  MOZ_ASSERT(chunk_remaining < samplenum_10ms_);

  if (chunk_remaining) {
    memcpy(samples_10ms_buffer_, samples_tmp, chunk_remaining * sizeof(int16_t));
    buffer_current_ = chunk_remaining;
  }

}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
void MediaPipelineTransmit::PipelineListener::ProcessVideoChunk(
    VideoSessionConduit* conduit,
    VideoChunk& chunk) {
  layers::Image *img = chunk.mFrame.GetImage();

  // We now need to send the video frame to the other side
  if (!img) {
    // segment.AppendFrame() allows null images, which show up here as null
    return;
  }

  if (!enabled_ || chunk.mFrame.GetForceBlack()) {
    gfx::IntSize size = img->GetSize();
    uint32_t yPlaneLen = YSIZE(size.width, size.height);
    uint32_t cbcrPlaneLen = 2 * CRSIZE(size.width, size.height);
    uint32_t length = yPlaneLen + cbcrPlaneLen;

    // Send a black image.
    nsAutoArrayPtr<uint8_t> pixelData;
    pixelData = new (fallible) uint8_t[length];
    if (pixelData) {
      // YCrCb black = 0x10 0x80 0x80
      memset(pixelData, 0x10, yPlaneLen);
      // Fill Cb/Cr planes
      memset(pixelData + yPlaneLen, 0x80, cbcrPlaneLen);

      MOZ_MTLOG(ML_DEBUG, "Sending a black video frame");
      conduit->SendVideoFrame(pixelData, length, size.width, size.height,
                              mozilla::kVideoI420, 0);
    }
    return;
  }

  // We get passed duplicate frames every ~10ms even if there's no frame change!
  int32_t serial = img->GetSerial();
  if (serial == last_img_) {
    return;
  }
  last_img_ = serial;

  ImageFormat format = img->GetFormat();
#ifdef WEBRTC_GONK
  layers::GrallocImage* nativeImage = img->AsGrallocImage();
  if (nativeImage) {
    android::sp<android::GraphicBuffer> graphicBuffer = nativeImage->GetGraphicBuffer();
    int pixelFormat = graphicBuffer->getPixelFormat(); /* PixelFormat is an enum == int */
    mozilla::VideoType destFormat;
    switch (pixelFormat) {
      case HAL_PIXEL_FORMAT_YV12:
        // all android must support this
        destFormat = mozilla::kVideoYV12;
        break;
      case layers::GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP:
        destFormat = mozilla::kVideoNV21;
        break;
      case layers::GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_P:
        destFormat = mozilla::kVideoI420;
        break;
      default:
        // XXX Bug NNNNNNN
        // use http://mxr.mozilla.org/mozilla-central/source/content/media/omx/I420ColorConverterHelper.cpp
        // to convert unknown types (OEM-specific) to I420
        MOZ_MTLOG(ML_ERROR, "Un-handled GRALLOC buffer type:" << pixelFormat);
        MOZ_CRASH();
    }
    void *basePtr;
    graphicBuffer->lock(android::GraphicBuffer::USAGE_SW_READ_MASK, &basePtr);
    uint32_t width = graphicBuffer->getWidth();
    uint32_t height = graphicBuffer->getHeight();
    // XXX gralloc buffer's width and stride could be different depends on implementations.
    conduit->SendVideoFrame(static_cast<unsigned char*>(basePtr),
                            I420SIZE(width, height),
                            width,
                            height,
                            destFormat, 0);
    graphicBuffer->unlock();
  } else
#endif
  if (format == ImageFormat::PLANAR_YCBCR) {
    // Cast away constness b/c some of the accessors are non-const
    layers::PlanarYCbCrImage* yuv =
    const_cast<layers::PlanarYCbCrImage *>(
          static_cast<const layers::PlanarYCbCrImage *>(img));
    // Big-time assumption here that this is all contiguous data coming
    // from getUserMedia or other sources.
    const layers::PlanarYCbCrData *data = yuv->GetData();

    uint8_t *y = data->mYChannel;
    uint8_t *cb = data->mCbChannel;
    uint8_t *cr = data->mCrChannel;
    uint32_t width = yuv->GetSize().width;
    uint32_t height = yuv->GetSize().height;
    uint32_t length = yuv->GetDataSize();
    // NOTE: length may be rounded up or include 'other' data (see
    // YCbCrImageDataDeserializerBase::ComputeMinBufferSize())

    // SendVideoFrame only supports contiguous YCrCb 4:2:0 buffers
    // Verify it's contiguous and in the right order
    if (cb != (y + YSIZE(width, height)) ||
        cr != (cb + CRSIZE(width, height))) {
      MOZ_ASSERT(false, "Incorrect cb/cr pointers in ProcessVideoChunk()!");
      return;
    }
    if (length < I420SIZE(width, height)) {
      MOZ_ASSERT(false, "Invalid length for ProcessVideoChunk()");
      return;
    }
    // XXX Consider modifying these checks if we ever implement
    // any subclasses of PlanarYCbCrImage that allow disjoint buffers such
    // that y+3(width*height)/2 might go outside the allocation or there are
    // pads between y, cr and cb.
    // GrallocImage can have wider strides, and so in some cases
    // would encode as garbage.  If we need to encode it we'll either want to
    // modify SendVideoFrame or copy/move the data in the buffer.

    // OK, pass it on to the conduit
    MOZ_MTLOG(ML_DEBUG, "Sending a video frame");
    // Not much for us to do with an error
    conduit->SendVideoFrame(y, I420SIZE(width, height), width, height, mozilla::kVideoI420, 0);
  } else if(format == ImageFormat::CAIRO_SURFACE) {
    layers::CairoImage* rgb =
    const_cast<layers::CairoImage *>(
          static_cast<const layers::CairoImage *>(img));

    gfx::IntSize size = rgb->GetSize();
    int half_width = (size.width + 1) >> 1;
    int half_height = (size.height + 1) >> 1;
    int c_size = half_width * half_height;
    int buffer_size = YSIZE(size.width, size.height) + 2 * c_size;
    uint8* yuv = (uint8*) malloc(buffer_size); // fallible
    if (!yuv)
      return;

    int cb_offset = YSIZE(size.width, size.height);
    int cr_offset = cb_offset + c_size;
    RefPtr<gfx::SourceSurface> tempSurf = rgb->GetAsSourceSurface();
    RefPtr<gfx::DataSourceSurface> surf = tempSurf->GetDataSurface();

    switch (surf->GetFormat()) {
      case gfx::SurfaceFormat::B8G8R8A8:
      case gfx::SurfaceFormat::B8G8R8X8:
        libyuv::ARGBToI420(static_cast<uint8*>(surf->GetData()), surf->Stride(),
                           yuv, size.width,
                           yuv + cb_offset, half_width,
                           yuv + cr_offset, half_width,
                           size.width, size.height);
        break;
      case gfx::SurfaceFormat::R5G6B5:
        libyuv::RGB565ToI420(static_cast<uint8*>(surf->GetData()), surf->Stride(),
                             yuv, size.width,
                             yuv + cb_offset, half_width,
                             yuv + cr_offset, half_width,
                             size.width, size.height);
        break;
      default:
        MOZ_MTLOG(ML_ERROR, "Unsupported RGB video format");
        MOZ_ASSERT(PR_FALSE);
    }
    conduit->SendVideoFrame(yuv, buffer_size, size.width, size.height, mozilla::kVideoI420, 0);
    free(yuv);
  } else {
    MOZ_MTLOG(ML_ERROR, "Unsupported video format");
    MOZ_ASSERT(PR_FALSE);
    return;
  }
}
#endif

nsresult MediaPipelineReceiveAudio::Init() {
  ASSERT_ON_THREAD(main_thread_);
  MOZ_MTLOG(ML_DEBUG, __FUNCTION__);

  description_ = pc_ + "| Receive audio[";
  description_ += track_id_;
  description_ += "]";

  listener_->AddSelf(new AudioSegment());

  return MediaPipelineReceive::Init();
}


// Add a track and listener on the MSG thread using the MSG command queue
static void AddTrackAndListener(MediaStream* source,
                                TrackID track_id, TrackRate track_rate,
                                MediaStreamListener* listener, MediaSegment* segment,
                                const RefPtr<TrackAddedCallback>& completed,
                                bool queue_track) {
  // This both adds the listener and the track
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  class Message : public ControlMessage {
   public:
    Message(MediaStream* stream, TrackID track, TrackRate rate,
            MediaSegment* segment, MediaStreamListener* listener,
            const RefPtr<TrackAddedCallback>& completed)
      : ControlMessage(stream),
        track_id_(track),
        track_rate_(rate),
        segment_(segment),
        listener_(listener),
        completed_(completed) {}

    virtual void Run() override {
      StreamTime current_end = mStream->GetBufferEnd();
      TrackTicks current_ticks =
        mStream->TimeToTicksRoundUp(track_rate_, current_end);

      mStream->AddListenerImpl(listener_.forget());

      // Add a track 'now' to avoid possible underrun, especially if we add
      // a track "later".

      if (current_end != 0L) {
        MOZ_MTLOG(ML_DEBUG, "added track @ " << current_end <<
                  " -> " << mStream->StreamTimeToSeconds(current_end));
      }

      // To avoid assertions, we need to insert a dummy segment that covers up
      // to the "start" time for the track
      segment_->AppendNullData(current_ticks);
      if (segment_->GetType() == MediaSegment::AUDIO) {
        mStream->AsSourceStream()->AddAudioTrack(track_id_, track_rate_,
                                                 current_ticks,
                                                 static_cast<AudioSegment*>(segment_.forget()));
      } else {
        NS_ASSERTION(mStream->GraphRate() == track_rate_, "Rate mismatch");
        mStream->AsSourceStream()->AddTrack(track_id_,
                                            current_ticks, segment_.forget());
      }

      // We need to know how much has been "inserted" because we're given absolute
      // times in NotifyPull.
      completed_->TrackAdded(current_ticks);
    }
   private:
    TrackID track_id_;
    TrackRate track_rate_;
    nsAutoPtr<MediaSegment> segment_;
    nsRefPtr<MediaStreamListener> listener_;
    const RefPtr<TrackAddedCallback> completed_;
  };

  MOZ_ASSERT(listener);

  if (!queue_track) {
    // We're only queueing the initial set of tracks since they are added
    // atomically and have start time 0. When not queueing we have to add
    // the track on the MediaStreamGraph thread so it can be added with the
    // appropriate start time.
    source->GraphImpl()->AppendMessage(new Message(source, track_id, track_rate, segment, listener, completed));
    MOZ_MTLOG(ML_INFO, "Dispatched track-add for track id " << track_id <<
                       " on stream " << source);
    return;
  }
#endif
  source->AddListener(listener);
  if (segment->GetType() == MediaSegment::AUDIO) {
    source->AsSourceStream()->AddAudioTrack(track_id, track_rate, 0,
                                            static_cast<AudioSegment*>(segment),
                                            SourceMediaStream::ADDTRACK_QUEUED);
  } else {
    source->AsSourceStream()->AddTrack(track_id, 0, segment,
                                       SourceMediaStream::ADDTRACK_QUEUED);
  }
  MOZ_MTLOG(ML_INFO, "Queued track-add for track id " << track_id <<
                     " on MediaStream " << source);
}

void GenericReceiveListener::AddSelf(MediaSegment* segment) {
  RefPtr<TrackAddedCallback> callback = new GenericReceiveCallback(this);
  AddTrackAndListener(source_, track_id_, track_rate_, this, segment, callback,
                      queue_track_);
}

MediaPipelineReceiveAudio::PipelineListener::PipelineListener(
    SourceMediaStream * source, TrackID track_id,
    const RefPtr<MediaSessionConduit>& conduit, bool queue_track)
  : GenericReceiveListener(source, track_id, 16000, queue_track), // XXX rate assumption
    conduit_(conduit)
{
  MOZ_ASSERT(track_rate_%100 == 0);
}

void MediaPipelineReceiveAudio::PipelineListener::
NotifyPull(MediaStreamGraph* graph, StreamTime desired_time) {
  MOZ_ASSERT(source_);
  if (!source_) {
    MOZ_MTLOG(ML_ERROR, "NotifyPull() called from a non-SourceMediaStream");
    return;
  }

  // This comparison is done in total time to avoid accumulated roundoff errors.
  while (source_->TicksToTimeRoundDown(track_rate_, played_ticks_) <
         desired_time) {
    // TODO(ekr@rtfm.com): Is there a way to avoid mallocating here?  Or reduce the size?
    // Max size given mono is 480*2*1 = 960 (48KHz)
#define AUDIO_SAMPLE_BUFFER_MAX 1000
    MOZ_ASSERT((track_rate_/100)*sizeof(uint16_t) <= AUDIO_SAMPLE_BUFFER_MAX);

    nsRefPtr<SharedBuffer> samples = SharedBuffer::Create(AUDIO_SAMPLE_BUFFER_MAX);
    int16_t *samples_data = static_cast<int16_t *>(samples->Data());
    int samples_length;

    // This fetches 10ms of data
    MediaConduitErrorCode err =
        static_cast<AudioSessionConduit*>(conduit_.get())->GetAudioFrame(
            samples_data,
            track_rate_,
            0,  // TODO(ekr@rtfm.com): better estimate of "capture" (really playout) delay
            samples_length);

    if (err != kMediaConduitNoError) {
      // Insert silence on conduit/GIPS failure (extremely unlikely)
      MOZ_MTLOG(ML_ERROR, "Audio conduit failed (" << err
                << ") to return data @ " << played_ticks_
                << " (desired " << desired_time << " -> "
                << source_->StreamTimeToSeconds(desired_time) << ")");
      samples_length = (track_rate_/100)*sizeof(uint16_t); // if this is not enough we'll loop and provide more
      memset(samples_data, '\0', samples_length);
    }

    MOZ_ASSERT(samples_length < AUDIO_SAMPLE_BUFFER_MAX);

    MOZ_MTLOG(ML_DEBUG, "Audio conduit returned buffer of length "
              << samples_length);

    AudioSegment segment;
    nsAutoTArray<const int16_t*,1> channels;
    channels.AppendElement(samples_data);
    segment.AppendFrames(samples.forget(), channels, samples_length);

    // Handle track not actually added yet or removed/finished
    if (source_->AppendToTrack(track_id_, &segment)) {
      played_ticks_ += track_rate_/100; // 10ms in TrackTicks
    } else {
      MOZ_MTLOG(ML_ERROR, "AppendToTrack failed");
      // we can't un-read the data, but that's ok since we don't want to
      // buffer - but don't i-loop!
      return;
    }
  }
}

nsresult MediaPipelineReceiveVideo::Init() {
  ASSERT_ON_THREAD(main_thread_);
  MOZ_MTLOG(ML_DEBUG, __FUNCTION__);

  description_ = pc_ + "| Receive video[";
  description_ += track_id_;
  description_ += "]";

#if defined(MOZILLA_INTERNAL_API)
  listener_->AddSelf(new VideoSegment());
#endif

  // Always happens before we can DetachMediaStream()
  static_cast<VideoSessionConduit *>(conduit_.get())->
      AttachRenderer(renderer_);

  return MediaPipelineReceive::Init();
}

MediaPipelineReceiveVideo::PipelineListener::PipelineListener(
  SourceMediaStream* source, TrackID track_id, bool queue_track)
  : GenericReceiveListener(source, track_id, source->GraphRate(), queue_track),
    width_(640),
    height_(480),
#if defined(MOZILLA_XPCOMRT_API)
    image_(new mozilla::SimpleImageBuffer),
#elif defined(MOZILLA_INTERNAL_API)
    image_container_(),
    image_(),
#endif
    monitor_("Video PipelineListener") {
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  image_container_ = layers::LayerManager::CreateImageContainer();
#endif
}

void MediaPipelineReceiveVideo::PipelineListener::RenderVideoFrame(
    const unsigned char* buffer,
    unsigned int buffer_size,
    uint32_t time_stamp,
    int64_t render_time,
    const RefPtr<layers::Image>& video_image) {

#ifdef MOZILLA_INTERNAL_API
  ReentrantMonitorAutoEnter enter(monitor_);
#endif // MOZILLA_INTERNAL_API

#if defined(MOZILLA_XPCOMRT_API)
  if (buffer) {
    image_->SetImage(buffer, buffer_size, width_, height_);
  }
#elif defined(MOZILLA_INTERNAL_API)
  if (buffer) {
    // Create a video frame using |buffer|.
#ifdef MOZ_WIDGET_GONK
    ImageFormat format = ImageFormat::GRALLOC_PLANAR_YCBCR;
#else
    ImageFormat format = ImageFormat::PLANAR_YCBCR;
#endif
    nsRefPtr<layers::Image> image = image_container_->CreateImage(format);
    layers::PlanarYCbCrImage* yuvImage = static_cast<layers::PlanarYCbCrImage*>(image.get());
    uint8_t* frame = const_cast<uint8_t*>(static_cast<const uint8_t*> (buffer));

    layers::PlanarYCbCrData yuvData;
    yuvData.mYChannel = frame;
    yuvData.mYSize = IntSize(width_, height_);
    yuvData.mYStride = width_;
    yuvData.mCbCrStride = (width_ + 1) >> 1;
    yuvData.mCbChannel = frame + height_ * yuvData.mYStride;
    yuvData.mCrChannel = yuvData.mCbChannel + ((height_ + 1) >> 1) * yuvData.mCbCrStride;
    yuvData.mCbCrSize = IntSize((width_ + 1) >> 1, (height_ + 1) >> 1);
    yuvData.mPicX = 0;
    yuvData.mPicY = 0;
    yuvData.mPicSize = IntSize(width_, height_);
    yuvData.mStereoMode = StereoMode::MONO;

    yuvImage->SetData(yuvData);

    image_ = image.forget();
  }
#ifdef WEBRTC_GONK
  else {
    // Decoder produced video frame that can be appended to the track directly.
    MOZ_ASSERT(video_image);
    image_ = video_image;
  }
#endif // WEBRTC_GONK
#endif // MOZILLA_INTERNAL_API
}

void MediaPipelineReceiveVideo::PipelineListener::
NotifyPull(MediaStreamGraph* graph, StreamTime desired_time) {
  ReentrantMonitorAutoEnter enter(monitor_);

#if defined(MOZILLA_XPCOMRT_API)
  nsRefPtr<SimpleImageBuffer> image = image_;
#elif defined(MOZILLA_INTERNAL_API)
  nsRefPtr<layers::Image> image = image_;
  // our constructor sets track_rate_ to the graph rate
  MOZ_ASSERT(track_rate_ == source_->GraphRate());
#endif

#if defined(MOZILLA_INTERNAL_API)
  StreamTime delta = desired_time - played_ticks_;

  // Don't append if we've already provided a frame that supposedly
  // goes past the current aDesiredTime Doing so means a negative
  // delta and thus messes up handling of the graph
  if (delta > 0) {
    VideoSegment segment;
    segment.AppendFrame(image.forget(), delta, IntSize(width_, height_));
    // Handle track not actually added yet or removed/finished
    if (source_->AppendToTrack(track_id_, &segment)) {
      played_ticks_ = desired_time;
    } else {
      MOZ_MTLOG(ML_ERROR, "AppendToTrack failed");
      return;
    }
  }
#endif
#if defined(MOZILLA_XPCOMRT_API)
  // Clear the image without deleting the memory.
  // This prevents image_ from being used if it
  // does not have new content during the next NotifyPull.
  image_->SetImage(nullptr, 0, 0, 0);
#endif
}


}  // end namespace
