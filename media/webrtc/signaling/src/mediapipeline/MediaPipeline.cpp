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
#include "LayersLogging.h"
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
#include "mozilla/UniquePtr.h"

#include "logging.h"

// Should come from MediaEngineWebRTC.h, but that's a pain to include here
#define DEFAULT_SAMPLE_RATE 32000

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

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

  // Is this a gUM mediastream?  If so, also register the Listener directly with
  // the SourceMediaStream that's attached to the TrackUnion so we can get direct
  // unqueued (and not resampled) data
  listener_->direct_connect_ = domstream_->AddDirectListener(listener_);

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
                                             len, len + SRTP_MAX_EXPANSION));

    RUN_ON_THREAD(sts_thread_,
                  WrapRunnable(
                      RefPtr<MediaPipeline::PipelineTransport>(this),
                      &MediaPipeline::PipelineTransport::SendRtpRtcpPacket_s,
                      buf, true),
                  NS_DISPATCH_NORMAL);

    return NS_OK;
}

nsresult MediaPipeline::PipelineTransport::SendRtpRtcpPacket_s(
    nsAutoPtr<DataBuffer> data,
    bool is_rtp) {

  ASSERT_ON_THREAD(sts_thread_);
  if (!pipeline_) {
    return NS_OK;  // Detached
  }
  TransportInfo& transport = is_rtp ? pipeline_->rtp_ : pipeline_->rtcp_;

  if (!transport.send_srtp_) {
    MOZ_MTLOG(ML_DEBUG, "Couldn't write RTP/RTCP packet; SRTP not set up yet");
    return NS_OK;
  }

  MOZ_ASSERT(transport.transport_);
  NS_ENSURE_TRUE(transport.transport_, NS_ERROR_NULL_POINTER);

  // libsrtp enciphers in place, so we need a big enough buffer.
  MOZ_ASSERT(data->capacity() >= data->len() + SRTP_MAX_EXPANSION);

  int out_len;
  nsresult res;
  if (is_rtp) {
    res = transport.send_srtp_->ProtectRtp(data->data(),
                                           data->len(),
                                           data->capacity(),
                                           &out_len);
  } else {
    res = transport.send_srtp_->ProtectRtcp(data->data(),
                                            data->len(),
                                            data->capacity(),
                                            &out_len);
  }
  if (!NS_SUCCEEDED(res)) {
    return res;
  }

  // paranoia; don't have uninitialized bytes included in data->len()
  data->SetLength(out_len);

  MOZ_MTLOG(ML_DEBUG, pipeline_->description_ << " sending " <<
            (is_rtp ? "RTP" : "RTCP") << " packet");
  if (is_rtp) {
    pipeline_->increment_rtp_packets_sent(out_len);
  } else {
    pipeline_->increment_rtcp_packets_sent();
  }
  return pipeline_->SendPacket(transport.transport_, data->data(), out_len);
}

nsresult MediaPipeline::PipelineTransport::SendRtcpPacket(
    const void *data, int len) {

    nsAutoPtr<DataBuffer> buf(new DataBuffer(static_cast<const uint8_t *>(data),
                                             len, len + SRTP_MAX_EXPANSION));

    RUN_ON_THREAD(sts_thread_,
                  WrapRunnable(
                      RefPtr<MediaPipeline::PipelineTransport>(this),
                      &MediaPipeline::PipelineTransport::SendRtpRtcpPacket_s,
                      buf, false),
                  NS_DISPATCH_NORMAL);

    return NS_OK;
}

void MediaPipelineTransmit::PipelineListener::
UnsetTrackId(MediaStreamGraphImpl* graph) {
#ifndef USE_FAKE_MEDIA_STREAMS
  class Message : public ControlMessage {
  public:
    explicit Message(PipelineListener* listener) :
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
                         const MediaSegment& queued_media,
                         MediaStream* aInputStream,
                         TrackID aInputTrackID) {
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

  if (conduit_->type() !=
      (media.GetType() == MediaSegment::AUDIO ? MediaSessionConduit::AUDIO :
                                                MediaSessionConduit::VIDEO)) {
    // Ignore data of wrong kind in case we have a muxed stream
    return;
  }

  if (track_id_ == TRACK_INVALID) {
    // Don't lock during normal media flow except on first sample
    MutexAutoLock lock(mMutex);
    track_id_ = track_id_external_ = tid;
  } else if (tid != track_id_) {
    return;
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

  // Convert to interleaved, 16-bits integer audio, with a maximum of two
  // channels (since the WebRTC.org code below makes the assumption that the
  // input audio is either mono or stereo).
  uint32_t outputChannels = chunk.ChannelCount() == 1 ? 1 : 2;
  const int16_t* samples = nullptr;
  nsAutoArrayPtr<int16_t> convertedSamples;

  // If this track is not enabled, simply ignore the data in the chunk.
  if (!enabled_) {
    chunk.mBufferFormat = AUDIO_FORMAT_SILENCE;
  }

  // We take advantage of the fact that the common case (microphone directly to
  // PeerConnection, that is, a normal call), the samples are already 16-bits
  // mono, so the representation in interleaved and planar is the same, and we
  // can just use that.
  if (outputChannels == 1 && chunk.mBufferFormat == AUDIO_FORMAT_S16) {
    samples = chunk.ChannelData<int16_t>().Elements()[0];
  } else {
    convertedSamples = new int16_t[chunk.mDuration * outputChannels];

    switch (chunk.mBufferFormat) {
        case AUDIO_FORMAT_FLOAT32:
          DownmixAndInterleave(chunk.ChannelData<float>(),
                               chunk.mDuration, chunk.mVolume, outputChannels,
                               convertedSamples.get());
          break;
        case AUDIO_FORMAT_S16:
          DownmixAndInterleave(chunk.ChannelData<int16_t>(),
                               chunk.mDuration, chunk.mVolume, outputChannels,
                               convertedSamples.get());
          break;
        case AUDIO_FORMAT_SILENCE:
          PodZero(convertedSamples.get(), chunk.mDuration * outputChannels);
          break;
    }
    samples = convertedSamples.get();
  }

  MOZ_ASSERT(!(rate%100)); // rate should be a multiple of 100

  // Check if the rate or the number of channels has changed since the last time
  // we came through. I realize it may be overkill to check if the rate has
  // changed, but I believe it is possible (e.g. if we change sources) and it
  // costs us very little to handle this case.

  uint32_t audio_10ms = rate / 100;

  if (!packetizer_ ||
      packetizer_->PacketSize() != audio_10ms ||
      packetizer_->Channels() != outputChannels) {
    // It's ok to drop the audio still in the packetizer here.
    packetizer_ = new AudioPacketizer<int16_t, int16_t>(audio_10ms, outputChannels);
   }

  packetizer_->Input(samples, chunk.mDuration);

  while (packetizer_->PacketsAvailable()) {
    uint32_t samplesPerPacket = packetizer_->PacketSize() *
                                packetizer_->Channels();

    // We know that webrtc.org's code going to copy the samples down the line,
    // so we can just use a stack buffer here instead of malloc-ing.
    // Max size given stereo is 480*2*2 = 1920 (10ms of 16-bits stereo audio at
    // 48KHz)
    const size_t AUDIO_SAMPLE_BUFFER_MAX = 1920;
    int16_t packet[AUDIO_SAMPLE_BUFFER_MAX];

    packetizer_->Output(packet);
    conduit->SendAudioFrame(packet,
                            samplesPerPacket,
                            rate, 0);
  }
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
void MediaPipelineTransmit::PipelineListener::ProcessVideoChunk(
    VideoSessionConduit* conduit,
    VideoChunk& chunk) {
  Image *img = chunk.mFrame.GetImage();

  // We now need to send the video frame to the other side
  if (!img) {
    // segment.AppendFrame() allows null images, which show up here as null
    return;
  }

  if (!enabled_ || chunk.mFrame.GetForceBlack()) {
    IntSize size = img->GetSize();
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
  GrallocImage* nativeImage = img->AsGrallocImage();
  if (nativeImage) {
    android::sp<android::GraphicBuffer> graphicBuffer = nativeImage->GetGraphicBuffer();
    int pixelFormat = graphicBuffer->getPixelFormat(); /* PixelFormat is an enum == int */
    mozilla::VideoType destFormat;
    switch (pixelFormat) {
      case HAL_PIXEL_FORMAT_YV12:
        // all android must support this
        destFormat = mozilla::kVideoYV12;
        break;
      case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP:
        destFormat = mozilla::kVideoNV21;
        break;
      case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_P:
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
    return;
  } else
#endif
  if (format == ImageFormat::PLANAR_YCBCR) {
    // Cast away constness b/c some of the accessors are non-const
    PlanarYCbCrImage* yuv = const_cast<PlanarYCbCrImage *>(
        static_cast<const PlanarYCbCrImage *>(img));

    const PlanarYCbCrData *data = yuv->GetData();
    if (data) {
      uint8_t *y = data->mYChannel;
      uint8_t *cb = data->mCbChannel;
      uint8_t *cr = data->mCrChannel;
      uint32_t width = yuv->GetSize().width;
      uint32_t height = yuv->GetSize().height;
      uint32_t length = yuv->GetDataSize();
      // NOTE: length may be rounded up or include 'other' data (see
      // YCbCrImageDataDeserializerBase::ComputeMinBufferSize())

      // XXX Consider modifying these checks if we ever implement
      // any subclasses of PlanarYCbCrImage that allow disjoint buffers such
      // that y+3(width*height)/2 might go outside the allocation or there are
      // pads between y, cr and cb.
      // GrallocImage can have wider strides, and so in some cases
      // would encode as garbage.  If we need to encode it we'll either want to
      // modify SendVideoFrame or copy/move the data in the buffer.
      if (cb == (y + YSIZE(width, height)) &&
          cr == (cb + CRSIZE(width, height)) &&
          length >= I420SIZE(width, height)) {
        MOZ_MTLOG(ML_DEBUG, "Sending an I420 video frame");
        conduit->SendVideoFrame(y, I420SIZE(width, height), width, height, mozilla::kVideoI420, 0);
        return;
      } else {
        MOZ_MTLOG(ML_ERROR, "Unsupported PlanarYCbCrImage format: "
                            "width=" << width << ", height=" << height << ", y=" << y
                            << "\n  Expected: cb=y+" << YSIZE(width, height)
                                        << ", cr=y+" << YSIZE(width, height)
                                                      + CRSIZE(width, height)
                            << "\n  Observed: cb=y+" << cb - y
                                        << ", cr=y+" << cr - y
                            << "\n            ystride=" << data->mYStride
                                        << ", yskip=" << data->mYSkip
                            << "\n            cbcrstride=" << data->mCbCrStride
                                        << ", cbskip=" << data->mCbSkip
                                        << ", crskip=" << data->mCrSkip
                            << "\n            ywidth=" << data->mYSize.width
                                        << ", yheight=" << data->mYSize.height
                            << "\n            cbcrwidth=" << data->mCbCrSize.width
                                        << ", cbcrheight=" << data->mCbCrSize.height);
        NS_ASSERTION(false, "Unsupported PlanarYCbCrImage format");
      }
    }
  }

  RefPtr<SourceSurface> surf = img->GetAsSourceSurface();
  if (!surf) {
    MOZ_MTLOG(ML_ERROR, "Getting surface from " << Stringify(format) << " image failed");
    return;
  }

  RefPtr<DataSourceSurface> data = surf->GetDataSurface();
  if (!data) {
    MOZ_MTLOG(ML_ERROR, "Getting data surface from " << Stringify(format)
                        << " image with " << Stringify(surf->GetType()) << "("
                        << Stringify(surf->GetFormat()) << ") surface failed");
    return;
  }

  IntSize size = img->GetSize();
  int half_width = (size.width + 1) >> 1;
  int half_height = (size.height + 1) >> 1;
  int c_size = half_width * half_height;
  int buffer_size = YSIZE(size.width, size.height) + 2 * c_size;
  UniquePtr<uint8[]> yuv_scoped(new (fallible) uint8[buffer_size]);
  if (!yuv_scoped)
    return;
  uint8* yuv = yuv_scoped.get();

  DataSourceSurface::ScopedMap map(data, DataSourceSurface::READ);
  if (!map.IsMapped()) {
    MOZ_MTLOG(ML_ERROR, "Reading DataSourceSurface from " << Stringify(format)
                        << " image with " << Stringify(surf->GetType()) << "("
                        << Stringify(surf->GetFormat()) << ") surface failed");
    return;
  }

  int rv;
  int cb_offset = YSIZE(size.width, size.height);
  int cr_offset = cb_offset + c_size;
  switch (surf->GetFormat()) {
    case SurfaceFormat::B8G8R8A8:
    case SurfaceFormat::B8G8R8X8:
      rv = libyuv::ARGBToI420(static_cast<uint8*>(map.GetData()),
                              map.GetStride(),
                              yuv, size.width,
                              yuv + cb_offset, half_width,
                              yuv + cr_offset, half_width,
                              size.width, size.height);
      break;
    case SurfaceFormat::R5G6B5:
      rv = libyuv::RGB565ToI420(static_cast<uint8*>(map.GetData()),
                                map.GetStride(),
                                yuv, size.width,
                                yuv + cb_offset, half_width,
                                yuv + cr_offset, half_width,
                                size.width, size.height);
      break;
    default:
      MOZ_MTLOG(ML_ERROR, "Unsupported RGB video format" << Stringify(surf->GetFormat()));
      MOZ_ASSERT(PR_FALSE);
      return;
  }
  if (rv != 0) {
    MOZ_MTLOG(ML_ERROR, Stringify(surf->GetFormat()) << " to I420 conversion failed");
    return;
  }
  MOZ_MTLOG(ML_DEBUG, "Sending an I420 video frame converted from " <<
                      Stringify(surf->GetFormat()));
  conduit->SendVideoFrame(yuv, buffer_size, size.width, size.height, mozilla::kVideoI420, 0);
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
  : GenericReceiveListener(source, track_id, DEFAULT_SAMPLE_RATE, queue_track), // XXX rate assumption
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
    // Max size given stereo is 480*2*2 = 1920 (48KHz)
    const size_t AUDIO_SAMPLE_BUFFER_MAX = 1920;
    MOZ_ASSERT((track_rate_/100)*sizeof(uint16_t) * 2 <= AUDIO_SAMPLE_BUFFER_MAX);

    int16_t scratch_buffer[AUDIO_SAMPLE_BUFFER_MAX];

    int samples_length;

    // This fetches 10ms of data, either mono or stereo
    MediaConduitErrorCode err =
        static_cast<AudioSessionConduit*>(conduit_.get())->GetAudioFrame(
            scratch_buffer,
            track_rate_,
            0,  // TODO(ekr@rtfm.com): better estimate of "capture" (really playout) delay
            samples_length);

    if (err != kMediaConduitNoError) {
      // Insert silence on conduit/GIPS failure (extremely unlikely)
      MOZ_MTLOG(ML_ERROR, "Audio conduit failed (" << err
                << ") to return data @ " << played_ticks_
                << " (desired " << desired_time << " -> "
                << source_->StreamTimeToSeconds(desired_time) << ")");
      samples_length = track_rate_/100; // if this is not enough we'll loop and provide more
      PodArrayZero(scratch_buffer);
    }

    MOZ_ASSERT(samples_length * sizeof(uint16_t) < AUDIO_SAMPLE_BUFFER_MAX);

    MOZ_MTLOG(ML_DEBUG, "Audio conduit returned buffer of length "
              << samples_length);

    nsRefPtr<SharedBuffer> samples = SharedBuffer::Create(samples_length * sizeof(uint16_t));
    int16_t *samples_data = static_cast<int16_t *>(samples->Data());
    AudioSegment segment;
    // We derive the number of channels of the stream from the number of samples
    // the AudioConduit gives us, considering it gives us packets of 10ms and we
    // know the rate.
    uint32_t channelCount = samples_length / (track_rate_ / 100);
    nsAutoTArray<int16_t*,2> channels;
    nsAutoTArray<const int16_t*,2> outputChannels;
    size_t frames = samples_length / channelCount;

    channels.SetLength(channelCount);

    size_t offset = 0;
    for (size_t i = 0; i < channelCount; i++) {
      channels[i] = samples_data + offset;
      offset += frames;
    }

    DeinterleaveAndConvertBuffer(scratch_buffer,
                                 frames,
                                 channelCount,
                                 channels.Elements());

    outputChannels.AppendElements(channels);

    segment.AppendFrames(samples.forget(), outputChannels, frames);

    // Handle track not actually added yet or removed/finished
    if (source_->AppendToTrack(track_id_, &segment)) {
      played_ticks_ += frames;
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
    image_(new SimpleImageBuffer),
#elif defined(MOZILLA_INTERNAL_API)
    image_container_(),
    image_(),
#endif
    monitor_("Video PipelineListener") {
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  image_container_ = LayerManager::CreateImageContainer();
#endif
}

void MediaPipelineReceiveVideo::PipelineListener::RenderVideoFrame(
    const unsigned char* buffer,
    unsigned int buffer_size,
    uint32_t time_stamp,
    int64_t render_time,
    const RefPtr<Image>& video_image) {

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
    nsRefPtr<Image> image = image_container_->CreateImage(format);
    PlanarYCbCrImage* yuvImage = static_cast<PlanarYCbCrImage*>(image.get());
    uint8_t* frame = const_cast<uint8_t*>(static_cast<const uint8_t*> (buffer));

    PlanarYCbCrData yuvData;
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
  nsRefPtr<Image> image = image_;
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
