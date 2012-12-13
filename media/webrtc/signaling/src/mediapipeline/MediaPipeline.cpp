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
#include "transportflow.h"
#include "transportlayer.h"
#include "transportlayerdtls.h"
#include "transportlayerice.h"

#include "runnable_utils.h"

using namespace mozilla;

// Logging context
MOZ_MTLOG_MODULE("mediapipeline");

namespace mozilla {

static char kDTLSExporterLabel[] = "EXTRACTOR-dtls_srtp";

nsresult MediaPipeline::Init() {
  conduit_->AttachTransport(transport_);

  MOZ_ASSERT(rtp_transport_);

  nsresult res;

  // Look to see if the transport is ready
  rtp_transport_->SignalStateChange.connect(this,
                                            &MediaPipeline::StateChange);

  if (rtp_transport_->state() == TransportLayer::TS_OPEN) {
    res = TransportReady(rtp_transport_);
    NS_ENSURE_SUCCESS(res, res);
  } else {
    if (!muxed_) {
      rtcp_transport_->SignalStateChange.connect(this,
                                                 &MediaPipeline::StateChange);

      if (rtcp_transport_->state() == TransportLayer::TS_OPEN) {
        res = TransportReady(rtcp_transport_);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  return NS_OK;
}


// Disconnect us from the transport so that we can cleanly destruct
// the pipeline on the main thread.
void MediaPipeline::DetachTransportInt() {
  transport_->Detach();
  rtp_transport_ = NULL;
  rtcp_transport_ = NULL;
}

void MediaPipeline::DetachTransport() {
  RUN_ON_THREAD(sts_thread_,
                WrapRunnable(this, &MediaPipeline::DetachTransportInt),
                NS_DISPATCH_SYNC);
}

void MediaPipeline::StateChange(TransportFlow *flow, TransportLayer::State state) {
  if (state == TransportLayer::TS_OPEN) {
    MOZ_MTLOG(PR_LOG_DEBUG, "Flow is ready");
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
  bool rtcp = !(flow == rtp_transport_);
  State *state = rtcp ? &rtcp_state_ : &rtp_state_;

  if (*state != MP_CONNECTING) {
    MOZ_MTLOG(PR_LOG_ERROR, "Transport ready for flow in wrong state:" <<
              (rtcp ? "rtcp" : "rtp"));
    return NS_ERROR_FAILURE;
  }

  nsresult res;

  MOZ_MTLOG(PR_LOG_DEBUG, "Transport ready for flow " << (rtcp ? "rtcp" : "rtp"));

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

      MOZ_MTLOG(PR_LOG_DEBUG, "Listening for packets received on " <<
                static_cast<void *>(dtls->downward()));

      dtls->downward()->SignalPacketReceived.connect(this,
                                                     &MediaPipeline::
                                                     PacketReceived);
    } else {
      MOZ_MTLOG(PR_LOG_DEBUG, "Listening for RTP packets received on " <<
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

    MOZ_MTLOG(PR_LOG_DEBUG, "Listening for RTCP packets received on " <<
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

  MOZ_MTLOG(PR_LOG_DEBUG, "Transport closed for flow " << (rtcp ? "rtcp" : "rtp"));

  NS_WARNING(
      "MediaPipeline Transport failed. This is not properly cleaned up yet");


  // TODO(ekr@rtfm.com): SECURITY: Figure out how to clean up if the
  // connection was good and now it is bad.
  // TODO(ekr@rtfm.com): Report up so that the PC knows we
  // have experienced an error.

  return NS_OK;
}


// Wrapper to send a packet on the STS thread.
nsresult MediaPipeline::SendPacket(TransportFlow *flow, const void *data,
                                   int len) {
  nsresult rv;
  nsresult res;

  rv = RUN_ON_THREAD(sts_thread_,
    WrapRunnableRet(this, &MediaPipeline::SendPacketInt, flow, data, len, &res),
    NS_DISPATCH_SYNC);

  // res is invalid unless the dispatch succeeded
  if (NS_FAILED(rv))
    return rv;

  return res;
}

nsresult MediaPipeline::SendPacketInt(TransportFlow *flow, const void *data,
                                   int len) {
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
  if (!(rtp_packets_sent_ % 1000)) {
    MOZ_MTLOG(PR_LOG_DEBUG, "RTP packet count " << static_cast<void *>(this)
              << ": " << rtp_packets_sent_);
  }
}

void MediaPipeline::increment_rtcp_packets_sent() {
  ++rtcp_packets_sent_;
  if (!(rtcp_packets_sent_ % 1000)) {
    MOZ_MTLOG(PR_LOG_DEBUG, "RTCP packet count " << static_cast<void *>(this)
              << ": " << rtcp_packets_sent_);
  }
}

void MediaPipeline::increment_rtp_packets_received() {
  ++rtp_packets_received_;
  if (!(rtp_packets_received_ % 1000)) {
    MOZ_MTLOG(PR_LOG_DEBUG, "RTP packet count " << static_cast<void *>(this)
              << ": " << rtp_packets_received_);
  }
}

void MediaPipeline::increment_rtcp_packets_received() {
  ++rtcp_packets_received_;
  if (!(rtcp_packets_received_ % 1000)) {
    MOZ_MTLOG(PR_LOG_DEBUG, "RTCP packet count " << static_cast<void *>(this)
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

  // TODO(ekr@rtfm.com): filter for DTLS here and in RtcpPacketReceived
  // TODO(ekr@rtfm.com): filter on SSRC for bundle
  increment_rtp_packets_received();

  MOZ_ASSERT(rtp_recv_srtp_);  // This should never happen

  if (direction_ == TRANSMIT) {
    // Discard any media that is being transmitted to us
    // This will be unnecessary when we have SSRC filtering.
    return;
  }

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
  // TODO(ekr@rtfm.com): Check for errors
  MOZ_MTLOG(PR_LOG_DEBUG, "Attaching pipeline to stream "
            << static_cast<void *>(stream_) <<
            " conduit type=" <<
            (conduit_->type() == MediaSessionConduit::AUDIO ?
             "audio" : "video") <<
            " hints=" << stream_->GetHintContents());

  // Force this to be a refptr so that we are holding a strong reference
  // to the media stream.
  nsRefPtr<MediaStream> stream (stream_->GetStream());
  return RUN_ON_THREAD(main_thread_, WrapRunnable(stream,
                                                  &MediaStream::AddListener,
                                                  listener_),
                       NS_DISPATCH_NORMAL);
}

nsresult MediaPipeline::PipelineTransport::SendRtpPacket(
    const void *data, int len) {
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
  int max_len = len + SRTP_MAX_EXPANSION;
  ScopedDeletePtr<unsigned char> inner_data(
      new unsigned char[max_len]);
  memcpy(inner_data, data, len);

  int out_len;
  nsresult res = pipeline_->rtp_send_srtp_->ProtectRtp(inner_data,
                                                       len,
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
  int max_len = len + SRTP_MAX_EXPANSION;
  ScopedDeletePtr<unsigned char> inner_data(
      new unsigned char[max_len]);
  memcpy(inner_data, data, len);

  int out_len;
  nsresult res = pipeline_->rtcp_send_srtp_->ProtectRtcp(inner_data,
                                                         len,
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
  if (!pipeline_)
    return;  // Detached

  MOZ_MTLOG(PR_LOG_DEBUG, "MediaPipeline::NotifyQueuedTrackChanges()");

  // Return early if we are not connected to avoid queueing stuff
  // up in the conduit
  if (pipeline_->rtp_transport_->state() != TransportLayer::TS_OPEN) {
    MOZ_MTLOG(PR_LOG_DEBUG, "Transport not ready yet, dropping packets");
    return;
  }

  // TODO(ekr@rtfm.com): For now assume that we have only one
  // track type and it's destined for us
  if (queued_media.GetType() == MediaSegment::AUDIO) {
    if (pipeline_->conduit_->type() != MediaSessionConduit::AUDIO) {
      // Ignore data in case we have a muxed stream
      return;
    }
    AudioSegment* audio = const_cast<AudioSegment *>(
        static_cast<const AudioSegment *>(&queued_media));

    AudioSegment::ChunkIterator iter(*audio);
    while(!iter.IsEnded()) {
      pipeline_->ProcessAudioChunk(static_cast<AudioSessionConduit *>
                                   (pipeline_->conduit_.get()),
                                   rate, *iter);
      iter.Next();
    }
  } else if (queued_media.GetType() == MediaSegment::VIDEO) {
#ifdef MOZILLA_INTERNAL_API
    if (pipeline_->conduit_->type() != MediaSessionConduit::VIDEO) {
      // Ignore data in case we have a muxed stream
      return;
    }
    VideoSegment* video = const_cast<VideoSegment *>(
        static_cast<const VideoSegment *>(&queued_media));

    VideoSegment::ChunkIterator iter(*video);
    while(!iter.IsEnded()) {
      pipeline_->ProcessVideoChunk(static_cast<VideoSessionConduit *>
                                   (pipeline_->conduit_.get()),
                                   rate, *iter);
      iter.Next();
    }
#endif
  } else {
    // Ignore
  }
}

void MediaPipelineTransmit::ProcessAudioChunk(AudioSessionConduit *conduit,
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
          const short* buf = static_cast<const short *>(chunk.mBuffer->Data());
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

  MOZ_MTLOG(PR_LOG_DEBUG, "Sending an audio frame");
  conduit->SendAudioFrame(samples.get(), chunk.mDuration, rate, 0);
}

#ifdef MOZILLA_INTERNAL_API
void MediaPipelineTransmit::ProcessVideoChunk(VideoSessionConduit *conduit,
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
  MOZ_MTLOG(PR_LOG_DEBUG, __FUNCTION__);

  // Force this to be a refptr so that we are holding a strong reference
  // to the media stream.
  nsRefPtr<MediaStream> stream (stream_->GetStream());
  return RUN_ON_THREAD(main_thread_, WrapRunnable(stream,
                                                  &MediaStream::AddListener,
                                                  listener_),
                       NS_DISPATCH_NORMAL);
}

void MediaPipelineReceiveAudio::PipelineListener::
NotifyPull(MediaStreamGraph* graph, StreamTime total) {
  if (!pipeline_)
    return;  // Detached

  SourceMediaStream *source =
    pipeline_->stream_->GetStream()->AsSourceStream();

  MOZ_ASSERT(source);
  if (!source) {
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
        static_cast<AudioSessionConduit*>(pipeline_->conduit_.get())->GetAudioFrame(
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

    char buf[32];
    PR_snprintf(buf, 32, "%p", source);
    source->AppendToTrack(1,  // TODO(ekr@rtfm.com): Track ID
                          &segment);
  }
}

nsresult MediaPipelineReceiveVideo::Init() {
  MOZ_MTLOG(PR_LOG_DEBUG, __FUNCTION__);

  static_cast<VideoSessionConduit *>(conduit_.get())->
      AttachRenderer(renderer_);

  return NS_OK;
}

MediaPipelineReceiveVideo::PipelineRenderer::PipelineRenderer(
    MediaPipelineReceiveVideo *pipeline) :
    pipeline_(pipeline),
    width_(640), height_(480) {

#ifdef MOZILLA_INTERNAL_API
  image_container_ = layers::LayerManager::CreateImageContainer();
  SourceMediaStream *source =
      pipeline_->stream_->GetStream()->AsSourceStream();
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
      pipeline_->stream_->GetStream()->AsSourceStream();

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

