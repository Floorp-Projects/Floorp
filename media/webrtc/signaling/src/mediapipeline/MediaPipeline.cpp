/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include "MediaPipeline.h"

#include <math.h>

#include "nspr.h"
#include <prlog.h>
#include "srtp.h"

#ifdef MOZILLA_INTERNAL_API
#include "VideoSegment.h"
#include "Layers.h"
#include "ImageTypes.h"
#include "ImageContainer.h"
#endif

#include "logging.h"
#include "nsError.h"
#include "AudioSegment.h"
#include "MediaSegment.h"
#include "databuffer.h"
#include "transportflow.h"
#include "transportlayer.h"
#include "transportlayerdtls.h"
#include "transportlayerice.h"

#include "runnable_utils.h"

using namespace mozilla;

#ifdef DEBUG
// Dial up pipeline logging in debug mode
#define MP_LOG_INFO PR_LOG_WARN
#else
#define MP_LOG_INFO PR_LOG_INFO
#endif


// Logging context
MOZ_MTLOG_MODULE("mediapipeline");

namespace mozilla {

static char kDTLSExporterLabel[] = "EXTRACTOR-dtls_srtp";

nsresult MediaPipeline::Init() {
  ASSERT_ON_THREAD(main_thread_);

  // TODO(ekr@rtfm.com): is there a way to make this async?
  nsresult ret;
  RUN_ON_THREAD(sts_thread_,
		WrapRunnableRet(this, &MediaPipeline::Init_s, &ret),
		NS_DISPATCH_SYNC);
  return ret;
}

nsresult MediaPipeline::Init_s() {
  ASSERT_ON_THREAD(sts_thread_);
  conduit_->AttachTransport(transport_);

  MOZ_ASSERT(rtp_transport_);

  nsresult res;

  // Look to see if the transport is ready
  rtp_transport_->SignalStateChange.connect(this,
                                            &MediaPipeline::StateChange);

  if (rtp_transport_->state() == TransportLayer::TS_OPEN) {
    res = TransportReady(rtp_transport_);
    if (NS_FAILED(res)) {
      MOZ_MTLOG(PR_LOG_ERROR, "Error calling TransportReady()");
      return res;
    }
  } else {
    if (!muxed_) {
      rtcp_transport_->SignalStateChange.connect(this,
                                                 &MediaPipeline::StateChange);

      if (rtcp_transport_->state() == TransportLayer::TS_OPEN) {
        res = TransportReady(rtcp_transport_);
        if (NS_FAILED(res)) {
          MOZ_MTLOG(PR_LOG_ERROR, "Error calling TransportReady()");
          return res;
        }
      }
    }
  }
  return NS_OK;
}


// Disconnect us from the transport so that we can cleanly destruct
// the pipeline on the main thread.
void MediaPipeline::DetachTransport_s() {
  ASSERT_ON_THREAD(sts_thread_);

  disconnect_all();
  transport_->Detach();
  rtp_transport_ = NULL;
  rtcp_transport_ = NULL;
}

void MediaPipeline::DetachTransport() {
  RUN_ON_THREAD(sts_thread_,
                WrapRunnable(this, &MediaPipeline::DetachTransport_s),
                NS_DISPATCH_SYNC);
}

void MediaPipeline::StateChange(TransportFlow *flow, TransportLayer::State state) {
  if (state == TransportLayer::TS_OPEN) {
    MOZ_MTLOG(MP_LOG_INFO, "Flow is ready");
    TransportReady(flow);
  } else if (state == TransportLayer::TS_CLOSED ||
             state == TransportLayer::TS_ERROR) {
    TransportFailed(flow);
  }
}

nsresult MediaPipeline::TransportReady(TransportFlow *flow) {
  nsresult rv;
  nsresult res;

  rv = RUN_ON_THREAD(sts_thread_,
    WrapRunnableRet(this, &MediaPipeline::TransportReadyInt, flow, &res),
    NS_DISPATCH_SYNC);

  // res is invalid unless the dispatch succeeded
  if (NS_FAILED(rv))
    return rv;

  return res;
}

nsresult MediaPipeline::TransportReadyInt(TransportFlow *flow) {
  MOZ_ASSERT(!description_.empty());
  bool rtcp = !(flow == rtp_transport_);
  State *state = rtcp ? &rtcp_state_ : &rtp_state_;

  if (*state != MP_CONNECTING) {
    MOZ_MTLOG(PR_LOG_ERROR, "Transport ready for flow in wrong state:" <<
	      description_ << ": " << (rtcp ? "rtcp" : "rtp"));
    return NS_ERROR_FAILURE;
  }

  nsresult res;

  MOZ_MTLOG(MP_LOG_INFO, "Transport ready for pipeline " <<
	    static_cast<void *>(this) << " flow " << description_ << ": " <<
	    (rtcp ? "rtcp" : "rtp"));

  // Now instantiate the SRTP objects
  TransportLayerDtls *dtls = static_cast<TransportLayerDtls *>(
      flow->GetLayer(TransportLayerDtls::ID()));
  MOZ_ASSERT(dtls);  // DTLS is mandatory

  uint16_t cipher_suite;
  res = dtls->GetSrtpCipher(&cipher_suite);
  if (NS_FAILED(res)) {
    MOZ_MTLOG(PR_LOG_ERROR, "Failed to negotiate DTLS-SRTP. This is an error");
    return res;
  }

  // SRTP Key Exporter as per RFC 5764 S 4.2
  unsigned char srtp_block[SRTP_TOTAL_KEY_LENGTH * 2];
  res = dtls->ExportKeyingMaterial(kDTLSExporterLabel, false, "",
                                   srtp_block, sizeof(srtp_block));
  if (NS_FAILED(res)) {
    MOZ_MTLOG(PR_LOG_ERROR, "Failed to compute DTLS-SRTP keys. This is an error");
    *state = MP_CLOSED;
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

  if (!rtcp) {
    // RTP side
    MOZ_ASSERT(!rtp_send_srtp_ && !rtp_recv_srtp_);
    rtp_send_srtp_ = SrtpFlow::Create(cipher_suite, false,
                                      write_key, SRTP_TOTAL_KEY_LENGTH);
    rtp_recv_srtp_ = SrtpFlow::Create(cipher_suite, true,
                                      read_key, SRTP_TOTAL_KEY_LENGTH);
    if (!rtp_send_srtp_ || !rtp_recv_srtp_) {
      MOZ_MTLOG(PR_LOG_ERROR, "Couldn't create SRTP flow for RTCP");
      *state = MP_CLOSED;
      return NS_ERROR_FAILURE;
    }

    // Start listening
    if (muxed_) {
      MOZ_ASSERT(!rtcp_send_srtp_ && !rtcp_recv_srtp_);
      rtcp_send_srtp_ = rtp_send_srtp_;
      rtcp_recv_srtp_ = rtp_recv_srtp_;

      MOZ_MTLOG(MP_LOG_INFO, "Listening for packets received on " <<
                static_cast<void *>(dtls->downward()));

      dtls->downward()->SignalPacketReceived.connect(this,
                                                     &MediaPipeline::
                                                     PacketReceived);
    } else {
      MOZ_MTLOG(MP_LOG_INFO, "Listening for RTP packets received on " <<
                static_cast<void *>(dtls->downward()));

      dtls->downward()->SignalPacketReceived.connect(this,
                                                     &MediaPipeline::
                                                     RtpPacketReceived);
    }
  }
  else {
    MOZ_ASSERT(!rtcp_send_srtp_ && !rtcp_recv_srtp_);
    rtcp_send_srtp_ = SrtpFlow::Create(cipher_suite, false,
                                       write_key, SRTP_TOTAL_KEY_LENGTH);
    rtcp_recv_srtp_ = SrtpFlow::Create(cipher_suite, true,
                                       read_key, SRTP_TOTAL_KEY_LENGTH);
    if (!rtcp_send_srtp_ || !rtcp_recv_srtp_) {
      MOZ_MTLOG(PR_LOG_ERROR, "Couldn't create SRTCP flow for RTCP");
      *state = MP_CLOSED;
      return NS_ERROR_FAILURE;
    }

    MOZ_MTLOG(MP_LOG_INFO, "Listening for RTCP packets received on " <<
      static_cast<void *>(dtls->downward()));

    // Start listening
    dtls->downward()->SignalPacketReceived.connect(this,
                                                  &MediaPipeline::
                                                  RtcpPacketReceived);
  }

  *state = MP_OPEN;
  return NS_OK;
}

nsresult MediaPipeline::TransportFailed(TransportFlow *flow) {
  bool rtcp = !(flow == rtp_transport_);

  State *state = rtcp ? &rtcp_state_ : &rtp_state_;

  *state = MP_CLOSED;

  MOZ_MTLOG(MP_LOG_INFO, "Transport closed for flow " << (rtcp ? "rtcp" : "rtp"));

  NS_WARNING(
      "MediaPipeline Transport failed. This is not properly cleaned up yet");


  // TODO(ekr@rtfm.com): SECURITY: Figure out how to clean up if the
  // connection was good and now it is bad.
  // TODO(ekr@rtfm.com): Report up so that the PC knows we
  // have experienced an error.

  return NS_OK;
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

    MOZ_MTLOG(PR_LOG_ERROR, "Failed write on stream");
    return NS_BASE_STREAM_CLOSED;
  }

  return NS_OK;
}

void MediaPipeline::increment_rtp_packets_sent() {
  ++rtp_packets_sent_;

  if (!(rtp_packets_sent_ % 100)) {
    MOZ_MTLOG(MP_LOG_INFO, "RTP sent packet count for " << description_
              << " Pipeline " << static_cast<void *>(this)
              << " Flow : " << static_cast<void *>(rtp_transport_)
              << ": " << rtp_packets_sent_);
  }
}

void MediaPipeline::increment_rtcp_packets_sent() {
  ++rtcp_packets_sent_;
  if (!(rtcp_packets_sent_ % 100)) {
    MOZ_MTLOG(MP_LOG_INFO, "RTCP sent packet count for " << description_
              << " Pipeline " << static_cast<void *>(this)
              << " Flow : " << static_cast<void *>(rtcp_transport_)
              << ": " << rtcp_packets_sent_);
  }
}

void MediaPipeline::increment_rtp_packets_received() {
  ++rtp_packets_received_;
  if (!(rtp_packets_received_ % 100)) {
    MOZ_MTLOG(MP_LOG_INFO, "RTP received packet count for " << description_
              << " Pipeline " << static_cast<void *>(this)
              << " Flow : " << static_cast<void *>(rtp_transport_)
              << ": " << rtp_packets_received_);
  }
}

void MediaPipeline::increment_rtcp_packets_received() {
  ++rtcp_packets_received_;
  if (!(rtcp_packets_received_ % 100)) {
    MOZ_MTLOG(MP_LOG_INFO, "RTCP received packet count for " << description_
              << " Pipeline " << static_cast<void *>(this)
              << " Flow : " << static_cast<void *>(rtcp_transport_)
              << ": " << rtcp_packets_received_);
  }
}

void MediaPipeline::RtpPacketReceived(TransportLayer *layer,
                                      const unsigned char *data,
                                      size_t len) {
  if (!transport_->pipeline()) {
    MOZ_MTLOG(PR_LOG_DEBUG, "Discarding incoming packet; transport disconnected");
    return;
  }

  if (!conduit_) {
    MOZ_MTLOG(PR_LOG_DEBUG, "Discarding incoming packet; media disconnected");
    return;
  }

  MOZ_ASSERT(rtp_recv_srtp_);  // This should never happen

  if (direction_ == TRANSMIT) {
    // Discard any media that is being transmitted to us
    // This will be unnecessary when we have SSRC filtering.
    return;
  }

  // TODO(ekr@rtfm.com): filter for DTLS here and in RtcpPacketReceived
  // TODO(ekr@rtfm.com): filter on SSRC for bundle
  increment_rtp_packets_received();

  // Make a copy rather than cast away constness
  ScopedDeletePtr<unsigned char> inner_data(
      new unsigned char[len]);
  memcpy(inner_data, data, len);
  int out_len;
  nsresult res = rtp_recv_srtp_->UnprotectRtp(inner_data,
                                              len, len, &out_len);
  if (!NS_SUCCEEDED(res))
    return;

  (void)conduit_->ReceivedRTPPacket(inner_data, out_len);  // Ignore error codes
}

void MediaPipeline::RtcpPacketReceived(TransportLayer *layer,
                                       const unsigned char *data,
                                       size_t len) {
  if (!transport_->pipeline()) {
    MOZ_MTLOG(PR_LOG_DEBUG, "Discarding incoming packet; transport disconnected");
    return;
  }

  if (!conduit_) {
    MOZ_MTLOG(PR_LOG_DEBUG, "Discarding incoming packet; media disconnected");
    return;
  }

  if (direction_ == RECEIVE) {
    // Discard any RTCP that is being transmitted to us
    // This will be unnecessary when we have SSRC filtering.
    return;
  }

  increment_rtcp_packets_received();

  MOZ_ASSERT(rtcp_recv_srtp_);  // This should never happen

  // Make a copy rather than cast away constness
  ScopedDeletePtr<unsigned char> inner_data(
      new unsigned char[len]);
  memcpy(inner_data, data, len);
  int out_len;

  nsresult res = rtcp_recv_srtp_->UnprotectRtcp(inner_data, len, len, &out_len);

  if (!NS_SUCCEEDED(res))
    return;

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
    MOZ_MTLOG(PR_LOG_DEBUG, "Discarding incoming packet; transport disconnected");
    return;
  }

  if (IsRtp(data, len)) {
    RtpPacketReceived(layer, data, len);
  } else {
    RtcpPacketReceived(layer, data, len);
  }
}

nsresult MediaPipelineTransmit::Init() {
  ASSERT_ON_THREAD(main_thread_);

  description_ = pc_ + "| ";
  description_ += conduit_->type() == MediaSessionConduit::AUDIO ?
      "Transmit audio" : "Transmit video";

  // TODO(ekr@rtfm.com): Check for errors
  MOZ_MTLOG(PR_LOG_DEBUG, "Attaching pipeline to stream "
            << static_cast<void *>(stream_) <<
            " conduit type=" <<
            (conduit_->type() == MediaSessionConduit::AUDIO ?
             "audio" : "video"));

  stream_->AddListener(listener_);

  return MediaPipeline::Init();
}

nsresult MediaPipelineTransmit::TransportReady(TransportFlow *flow) {
  // Call base ready function.
  MediaPipeline::TransportReady(flow);

  if (flow == rtp_transport_) {
    // TODO(ekr@rtfm.com): Move onto MSG thread.
    listener_->SetActive(true);
  }

  return NS_OK;
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
  if (!pipeline_)
    return NS_OK;  // Detached

  if (!pipeline_->rtp_send_srtp_) {
    MOZ_MTLOG(PR_LOG_DEBUG, "Couldn't write RTP packet; SRTP not set up yet");
    return NS_OK;
  }

  MOZ_ASSERT(pipeline_->rtp_transport_);
  NS_ENSURE_TRUE(pipeline_->rtp_transport_, NS_ERROR_NULL_POINTER);

  // libsrtp enciphers in place, so we need a new, big enough
  // buffer.
  // XXX. allocates and deletes one buffer per packet sent.
  // Bug 822129
  int max_len = data->len() + SRTP_MAX_EXPANSION;
  ScopedDeletePtr<unsigned char> inner_data(
      new unsigned char[max_len]);
  memcpy(inner_data, data->data(), data->len());

  int out_len;
  nsresult res = pipeline_->rtp_send_srtp_->ProtectRtp(inner_data,
                                                       data->len(),
                                                       max_len,
                                                       &out_len);
  if (!NS_SUCCEEDED(res))
    return res;

  pipeline_->increment_rtp_packets_sent();
  return pipeline_->SendPacket(pipeline_->rtp_transport_, inner_data,
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
  if (!pipeline_)
    return NS_OK;  // Detached

  if (!pipeline_->rtcp_send_srtp_) {
    MOZ_MTLOG(PR_LOG_DEBUG, "Couldn't write RTCP packet; SRTCP not set up yet");
    return NS_OK;
  }

  MOZ_ASSERT(pipeline_->rtcp_transport_);
  NS_ENSURE_TRUE(pipeline_->rtcp_transport_, NS_ERROR_NULL_POINTER);

  // libsrtp enciphers in place, so we need a new, big enough
  // buffer.
  // XXX. allocates and deletes one buffer per packet sent.
  // Bug 822129.
  int max_len = data->len() + SRTP_MAX_EXPANSION;
  ScopedDeletePtr<unsigned char> inner_data(
      new unsigned char[max_len]);
  memcpy(inner_data, data->data(), data->len());

  int out_len;
  nsresult res = pipeline_->rtcp_send_srtp_->ProtectRtcp(inner_data,
                                                         data->len(),
                                                         max_len,
                                                         &out_len);
  if (!NS_SUCCEEDED(res))
    return res;

  pipeline_->increment_rtcp_packets_sent();
  return pipeline_->SendPacket(pipeline_->rtcp_transport_, inner_data,
                               out_len);
}

void MediaPipelineTransmit::PipelineListener::
NotifyQueuedTrackChanges(MediaStreamGraph* graph, TrackID tid,
                         TrackRate rate,
                         TrackTicks offset,
                         uint32_t events,
                         const MediaSegment& queued_media) {
  MOZ_MTLOG(PR_LOG_DEBUG, "MediaPipeline::NotifyQueuedTrackChanges()");

  if (!active_) {
    MOZ_MTLOG(PR_LOG_DEBUG, "Discarding packets because transport not ready");
    return;
  }

  // TODO(ekr@rtfm.com): For now assume that we have only one
  // track type and it's destined for us
  // See bug 784517
  if (queued_media.GetType() == MediaSegment::AUDIO) {
    if (conduit_->type() != MediaSessionConduit::AUDIO) {
      // Ignore data in case we have a muxed stream
      return;
    }
    AudioSegment* audio = const_cast<AudioSegment *>(
        static_cast<const AudioSegment *>(&queued_media));

    AudioSegment::ChunkIterator iter(*audio);
    while(!iter.IsEnded()) {
      ProcessAudioChunk(static_cast<AudioSessionConduit*>(conduit_.get()),
                        rate, *iter);
      iter.Next();
    }
  } else if (queued_media.GetType() == MediaSegment::VIDEO) {
#ifdef MOZILLA_INTERNAL_API
    if (conduit_->type() != MediaSessionConduit::VIDEO) {
      // Ignore data in case we have a muxed stream
      return;
    }
    VideoSegment* video = const_cast<VideoSegment *>(
        static_cast<const VideoSegment *>(&queued_media));

    VideoSegment::ChunkIterator iter(*video);
    while(!iter.IsEnded()) {
      ProcessVideoChunk(static_cast<VideoSessionConduit*>(conduit_.get()),
                        rate, *iter);
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

  if (chunk.mBuffer) {
    switch(chunk.mBufferFormat) {
      case AUDIO_FORMAT_FLOAT32:
        MOZ_MTLOG(PR_LOG_ERROR, "Can't process audio except in 16-bit PCM yet");
        MOZ_ASSERT(PR_FALSE);
        return;
        break;
      case AUDIO_FORMAT_S16:
        {
          const short* buf = static_cast<const short *>(chunk.mBuffer->Data()) +
            chunk.mOffset;
          ConvertAudioSamplesWithScale(buf, samples, chunk.mDuration, chunk.mVolume);
        }
        break;
      default:
        MOZ_ASSERT(PR_FALSE);
        return;
        break;
    }
  } else {
    // This means silence.
    for (uint32_t i = 0; i < chunk.mDuration; ++i) {
      samples[i] = 0;
    }
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

#ifdef MOZILLA_INTERNAL_API
void MediaPipelineTransmit::PipelineListener::ProcessVideoChunk(
    VideoSessionConduit* conduit,
    TrackRate rate,
    VideoChunk& chunk) {
  // We now need to send the video frame to the other side
  layers::Image *img = chunk.mFrame.GetImage();
  if (!img) {
    // segment.AppendFrame() allows null images, which show up here as null
    return;
  }

  ImageFormat format = img->GetFormat();

  if (format != PLANAR_YCBCR) {
    MOZ_MTLOG(PR_LOG_ERROR, "Can't process non-YCBCR video");
    MOZ_ASSERT(PR_FALSE);
    return;
  }

  // Cast away constness b/c some of the accessors are non-const
  layers::PlanarYCbCrImage* yuv =
    const_cast<layers::PlanarYCbCrImage *>(
      static_cast<const layers::PlanarYCbCrImage *>(img));

  // Big-time assumption here that this is all contiguous data coming
  // from getUserMedia or other sources.
  const layers::PlanarYCbCrImage::Data *data = yuv->GetData();

  uint8_t *y = data->mYChannel;
#ifdef DEBUG
  uint8_t *cb = data->mCbChannel;
  uint8_t *cr = data->mCrChannel;
#endif
  uint32_t width = yuv->GetSize().width;
  uint32_t height = yuv->GetSize().height;
  uint32_t length = yuv->GetDataSize();

  // SendVideoFrame only supports contiguous YCrCb 4:2:0 buffers
  // Verify it's contiguous and in the right order
  MOZ_ASSERT(cb == (y + width*height) &&
             cr == (cb + width*height/4));
  // XXX Consider making this a non-debug-only check if we ever implement
  // any subclasses of PlanarYCbCrImage that allow disjoint buffers such
  // that y+3(width*height)/2 might go outside the allocation.
  // GrallocPlanarYCbCrImage can have wider strides, and so in some cases
  // would encode as garbage.  If we need to encode it we'll either want to
  // modify SendVideoFrame or copy/move the data in the buffer.

  // OK, pass it on to the conduit
  MOZ_MTLOG(PR_LOG_DEBUG, "Sending a video frame");
  // Not much for us to do with an error
  conduit->SendVideoFrame(y, length, width, height, mozilla::kVideoI420, 0);
}
#endif

nsresult MediaPipelineReceiveAudio::Init() {
  ASSERT_ON_THREAD(main_thread_);
  MOZ_MTLOG(PR_LOG_DEBUG, __FUNCTION__);

  description_ = pc_ + "| Receive audio";

  stream_->AddListener(listener_);

  return MediaPipelineReceive::Init();
}

void MediaPipelineReceiveAudio::PipelineListener::
NotifyPull(MediaStreamGraph* graph, StreamTime total) {
  MOZ_ASSERT(source_);
  if (!source_) {
    MOZ_MTLOG(PR_LOG_ERROR, "NotifyPull() called from a non-SourceMediaStream");
    return;
  }

  // "total" is absolute stream time.
  // StreamTime desired = total - played_;
  played_ = total;
  //double time_s = MediaTimeToSeconds(desired);

  // Number of 10 ms samples we need
  //int num_samples = ceil(time_s / .01f);

  // Doesn't matter what was asked for, always give 160 samples per 10 ms.
  int num_samples = 1;

  MOZ_MTLOG(PR_LOG_DEBUG, "Asking for " << num_samples << "sample from Audio Conduit");

  if (num_samples <= 0) {
    return;
  }

  while (num_samples--) {
    // TODO(ekr@rtfm.com): Is there a way to avoid mallocating here?
    nsRefPtr<SharedBuffer> samples = SharedBuffer::Create(1000);
    int samples_length;

    MediaConduitErrorCode err =
        static_cast<AudioSessionConduit*>(conduit_.get())->GetAudioFrame(
            static_cast<int16_t *>(samples->Data()),
            16000,  // Sampling rate fixed at 16 kHz for now
            0,  // TODO(ekr@rtfm.com): better estimate of capture delay
            samples_length);

    if (err != kMediaConduitNoError)
      return;

    MOZ_MTLOG(PR_LOG_DEBUG, "Audio conduit returned buffer of length " << samples_length);

    AudioSegment segment;
    segment.Init(1);
    segment.AppendFrames(samples.forget(), samples_length,
                         0, samples_length, AUDIO_FORMAT_S16);

    source_->AppendToTrack(1,  // TODO(ekr@rtfm.com): Track ID
                           &segment);
  }
}

nsresult MediaPipelineReceiveVideo::Init() {
  ASSERT_ON_THREAD(main_thread_);
  MOZ_MTLOG(PR_LOG_DEBUG, __FUNCTION__);

  description_ = pc_ + "| Receive video";

  static_cast<VideoSessionConduit *>(conduit_.get())->
      AttachRenderer(renderer_);

  return MediaPipelineReceive::Init();
}

MediaPipelineReceiveVideo::PipelineRenderer::PipelineRenderer(
    MediaPipelineReceiveVideo *pipeline) :
    pipeline_(pipeline),
    width_(640), height_(480) {

#ifdef MOZILLA_INTERNAL_API
  image_container_ = layers::LayerManager::CreateImageContainer();
  SourceMediaStream *source =
      pipeline_->stream_->AsSourceStream();
  source->AddTrack(1 /* Track ID */, 30, 0, new VideoSegment());
  source->AdvanceKnownTracksTime(STREAM_TIME_MAX);
#endif
}

void MediaPipelineReceiveVideo::PipelineRenderer::RenderVideoFrame(
    const unsigned char* buffer,
    unsigned int buffer_size,
    uint32_t time_stamp,
    int64_t render_time) {
#ifdef MOZILLA_INTERNAL_API
  SourceMediaStream *source =
      pipeline_->stream_->AsSourceStream();

  // Create a video frame and append it to the track.
  ImageFormat format = PLANAR_YCBCR;
  nsRefPtr<layers::Image> image = image_container_->CreateImage(&format, 1);

  layers::PlanarYCbCrImage* videoImage = static_cast<layers::PlanarYCbCrImage*>(image.get());
  uint8_t* frame = const_cast<uint8_t*>(static_cast<const uint8_t*> (buffer));
  const uint8_t lumaBpp = 8;
  const uint8_t chromaBpp = 4;

  layers::PlanarYCbCrImage::Data data;
  data.mYChannel = frame;
  data.mYSize = gfxIntSize(width_, height_);
  data.mYStride = width_ * lumaBpp/ 8;
  data.mCbCrStride = width_ * chromaBpp / 8;
  data.mCbChannel = frame + height_ * data.mYStride;
  data.mCrChannel = data.mCbChannel + height_ * data.mCbCrStride / 2;
  data.mCbCrSize = gfxIntSize(width_/ 2, height_/ 2);
  data.mPicX = 0;
  data.mPicY = 0;
  data.mPicSize = gfxIntSize(width_, height_);
  data.mStereoMode = STEREO_MODE_MONO;

  videoImage->SetData(data);

  VideoSegment segment;
  char buf[32];
  PR_snprintf(buf, 32, "%p", source);

  segment.AppendFrame(image.forget(), 1, gfxIntSize(width_, height_));
  source->AppendToTrack(1, &(segment));
#endif
}


}  // end namespace
