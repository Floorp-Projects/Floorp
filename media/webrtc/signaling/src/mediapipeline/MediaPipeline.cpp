/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include "MediaPipeline.h"

#include <inttypes.h>
#include <math.h>

#include "AudioSegment.h"
#include "AudioConverter.h"
#include "DOMMediaStream.h"
#include "ImageContainer.h"
#include "ImageTypes.h"
#include "Layers.h"
#include "LayersLogging.h"
#include "MediaEngine.h"
#include "MediaPipelineFilter.h"
#include "MediaSegment.h"
#include "MediaStreamGraphImpl.h"
#include "MediaStreamListener.h"
#include "MediaStreamTrack.h"
#include "RemoteTrackSource.h"
#include "RtpLogger.h"
#include "VideoFrameConverter.h"
#include "VideoSegment.h"
#include "VideoStreamTrack.h"
#include "VideoUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/PeerIdentity.h"
#include "mozilla/Preferences.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Types.h"
#include "nsError.h"
#include "nsThreadUtils.h"
#include "runnable_utils.h"
#include "signaling/src/peerconnection/MediaTransportHandler.h"
#include "Tracing.h"
#include "WebrtcImageBuffer.h"
#include "webrtc/common_video/include/video_frame_buffer.h"

// Max size given stereo is 480*2*2 = 1920 (10ms of 16-bits stereo audio at
// 48KHz)
#define AUDIO_SAMPLE_BUFFER_MAX_BYTES (480 * 2 * 2)
static_assert((WEBRTC_MAX_SAMPLE_RATE / 100) * sizeof(uint16_t) * 2 <=
                  AUDIO_SAMPLE_BUFFER_MAX_BYTES,
              "AUDIO_SAMPLE_BUFFER_MAX_BYTES is not large enough");

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;

mozilla::LazyLogModule gMediaPipelineLog("MediaPipeline");

namespace mozilla {

// An async inserter for audio data, to avoid running audio codec encoders
// on the MSG/input audio thread.  Basically just bounces all the audio
// data to a single audio processing/input queue.  We could if we wanted to
// use multiple threads and a TaskQueue.
class AudioProxyThread {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioProxyThread)

  explicit AudioProxyThread(AudioSessionConduit* aConduit)
      : mConduit(aConduit),
        mTaskQueue(new TaskQueue(
            GetMediaThreadPool(MediaThreadType::WEBRTC_DECODER), "AudioProxy")),
        mAudioConverter(nullptr) {
    MOZ_ASSERT(mConduit);
    MOZ_COUNT_CTOR(AudioProxyThread);
  }

  // This function is the identity if aInputRate is supported.
  // Else, it returns a rate that is supported, that ensure no loss in audio
  // quality: the sampling rate returned is always greater to the inputed
  // sampling-rate, if they differ..
  uint32_t AppropriateSendingRateForInputRate(uint32_t aInputRate) {
    AudioSessionConduit* conduit =
        static_cast<AudioSessionConduit*>(mConduit.get());
    if (conduit->IsSamplingFreqSupported(aInputRate)) {
      return aInputRate;
    }
    if (aInputRate < 16000) {
      return 16000;
    } else if (aInputRate < 32000) {
      return 32000;
    } else if (aInputRate < 44100) {
      return 44100;
    } else {
      return 48000;
    }
  }

  // From an arbitrary AudioChunk at sampling-rate aRate, process the audio into
  // something the conduit can work with (or send silence if the track is not
  // enabled), and send the audio in 10ms chunks to the conduit.
  void InternalProcessAudioChunk(TrackRate aRate, const AudioChunk& aChunk,
                                 bool aEnabled) {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

    // Convert to interleaved 16-bits integer audio, with a maximum of two
    // channels (since the WebRTC.org code below makes the assumption that the
    // input audio is either mono or stereo), with a sample-rate rate that is
    // 16, 32, 44.1, or 48kHz.
    uint32_t outputChannels = aChunk.ChannelCount() == 1 ? 1 : 2;
    int32_t transmissionRate = AppropriateSendingRateForInputRate(aRate);

    // We take advantage of the fact that the common case (microphone directly
    // to PeerConnection, that is, a normal call), the samples are already
    // 16-bits mono, so the representation in interleaved and planar is the
    // same, and we can just use that.
    if (aEnabled && outputChannels == 1 &&
        aChunk.mBufferFormat == AUDIO_FORMAT_S16 && transmissionRate == aRate) {
      const int16_t* samples = aChunk.ChannelData<int16_t>().Elements()[0];
      PacketizeAndSend(samples, transmissionRate, outputChannels,
                       aChunk.mDuration);
      return;
    }

    uint32_t sampleCount = aChunk.mDuration * outputChannels;
    if (mInterleavedAudio.Length() < sampleCount) {
      mInterleavedAudio.SetLength(sampleCount);
    }

    if (!aEnabled || aChunk.mBufferFormat == AUDIO_FORMAT_SILENCE) {
      PodZero(mInterleavedAudio.Elements(), sampleCount);
    } else if (aChunk.mBufferFormat == AUDIO_FORMAT_FLOAT32) {
      DownmixAndInterleave(aChunk.ChannelData<float>(), aChunk.mDuration,
                           aChunk.mVolume, outputChannels,
                           mInterleavedAudio.Elements());
    } else if (aChunk.mBufferFormat == AUDIO_FORMAT_S16) {
      DownmixAndInterleave(aChunk.ChannelData<int16_t>(), aChunk.mDuration,
                           aChunk.mVolume, outputChannels,
                           mInterleavedAudio.Elements());
    }
    int16_t* inputAudio = mInterleavedAudio.Elements();
    size_t inputAudioFrameCount = aChunk.mDuration;

    AudioConfig inputConfig(AudioConfig::ChannelLayout(outputChannels), aRate,
                            AudioConfig::FORMAT_S16);
    AudioConfig outputConfig(AudioConfig::ChannelLayout(outputChannels),
                             transmissionRate, AudioConfig::FORMAT_S16);
    // Resample to an acceptable sample-rate for the sending side
    if (!mAudioConverter || mAudioConverter->InputConfig() != inputConfig ||
        mAudioConverter->OutputConfig() != outputConfig) {
      mAudioConverter = MakeUnique<AudioConverter>(inputConfig, outputConfig);
    }

    int16_t* processedAudio = nullptr;
    size_t framesProcessed =
        mAudioConverter->Process(inputAudio, inputAudioFrameCount);

    if (framesProcessed == 0) {
      // In place conversion not possible, use a buffer.
      framesProcessed = mAudioConverter->Process(mOutputAudio, inputAudio,
                                                 inputAudioFrameCount);
      processedAudio = mOutputAudio.Data();
    } else {
      processedAudio = inputAudio;
    }

    PacketizeAndSend(processedAudio, transmissionRate, outputChannels,
                     framesProcessed);
  }

  // This packetizes aAudioData in 10ms chunks and sends it.
  // aAudioData is interleaved audio data at a rate and with a channel count
  // that is appropriate to send with the conduit.
  void PacketizeAndSend(const int16_t* aAudioData, uint32_t aRate,
                        uint32_t aChannels, uint32_t aFrameCount) {
    MOZ_ASSERT(AppropriateSendingRateForInputRate(aRate) == aRate);
    MOZ_ASSERT(aChannels == 1 || aChannels == 2);
    MOZ_ASSERT(aAudioData);

    uint32_t audio_10ms = aRate / 100;

    if (!mPacketizer || mPacketizer->PacketSize() != audio_10ms ||
        mPacketizer->Channels() != aChannels) {
      // It's the right thing to drop the bit of audio still in the packetizer:
      // we don't want to send to the conduit audio that has two different
      // rates while telling it that it has a constante rate.
      mPacketizer =
          MakeUnique<AudioPacketizer<int16_t, int16_t>>(audio_10ms, aChannels);
      mPacket = MakeUnique<int16_t[]>(audio_10ms * aChannels);
    }

    mPacketizer->Input(aAudioData, aFrameCount);

    while (mPacketizer->PacketsAvailable()) {
      mPacketizer->Output(mPacket.get());
      mConduit->SendAudioFrame(mPacket.get(), mPacketizer->PacketSize(), aRate,
                               mPacketizer->Channels(), 0);
    }
  }

  void QueueAudioChunk(TrackRate aRate, const AudioChunk& aChunk,
                       bool aEnabled) {
    RefPtr<AudioProxyThread> self = this;
    nsresult rv = mTaskQueue->Dispatch(NS_NewRunnableFunction(
        "AudioProxyThread::QueueAudioChunk", [self, aRate, aChunk, aEnabled]() {
          self->InternalProcessAudioChunk(aRate, aChunk, aEnabled);
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

 protected:
  virtual ~AudioProxyThread() {
    // Conduits must be released on MainThread, and we might have the last
    // reference We don't need to worry about runnables still trying to access
    // the conduit, since the runnables hold a ref to AudioProxyThread.
    NS_ReleaseOnMainThreadSystemGroup("AudioProxyThread::mConduit",
                                      mConduit.forget());
    MOZ_COUNT_DTOR(AudioProxyThread);
  }

  RefPtr<AudioSessionConduit> mConduit;
  const RefPtr<TaskQueue> mTaskQueue;
  // Only accessed on mTaskQueue
  UniquePtr<AudioPacketizer<int16_t, int16_t>> mPacketizer;
  // A buffer to hold a single packet of audio.
  UniquePtr<int16_t[]> mPacket;
  nsTArray<int16_t> mInterleavedAudio;
  AlignedShortBuffer mOutputAudio;
  UniquePtr<AudioConverter> mAudioConverter;
};

MediaPipeline::MediaPipeline(const std::string& aPc,
                             MediaTransportHandler* aTransportHandler,
                             DirectionType aDirection,
                             nsCOMPtr<nsIEventTarget> aMainThread,
                             nsCOMPtr<nsIEventTarget> aStsThread,
                             RefPtr<MediaSessionConduit> aConduit)
    : mDirection(aDirection),
      mLevel(0),
      mTransportHandler(aTransportHandler),
      mConduit(aConduit),
      mMainThread(aMainThread),
      mStsThread(aStsThread),
      mTransport(new PipelineTransport(aStsThread)),
      mRtpPacketsSent(0),
      mRtcpPacketsSent(0),
      mRtpPacketsReceived(0),
      mRtcpPacketsReceived(0),
      mRtpBytesSent(0),
      mRtpBytesReceived(0),
      mPc(aPc),
      mRtpParser(webrtc::RtpHeaderParser::Create()),
      mPacketDumper(new PacketDumper(mPc)) {
  if (mDirection == DirectionType::RECEIVE) {
    mConduit->SetReceiverTransport(mTransport);
  } else {
    mConduit->SetTransmitterTransport(mTransport);
  }
}

MediaPipeline::~MediaPipeline() {
  MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
          ("Destroying MediaPipeline: %s", mDescription.c_str()));
  NS_ReleaseOnMainThreadSystemGroup("MediaPipeline::mConduit",
                                    mConduit.forget());
}

void MediaPipeline::Shutdown_m() {
  Stop();
  DetachMedia();

  RUN_ON_THREAD(mStsThread,
                WrapRunnable(RefPtr<MediaPipeline>(this),
                             &MediaPipeline::DetachTransport_s),
                NS_DISPATCH_NORMAL);
}

void MediaPipeline::DetachTransport_s() {
  ASSERT_ON_THREAD(mStsThread);

  MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
          ("%s in %s", mDescription.c_str(), __FUNCTION__));

  disconnect_all();
  mRtpState = TransportLayer::TS_NONE;
  mRtcpState = TransportLayer::TS_NONE;
  mTransportId.clear();
  mTransport->Detach();

  // Make sure any cycles are broken
  mPacketDumper = nullptr;
}

void MediaPipeline::UpdateTransport_m(const std::string& aTransportId,
                                      nsAutoPtr<MediaPipelineFilter> aFilter) {
  RUN_ON_THREAD(
      mStsThread,
      WrapRunnable(RefPtr<MediaPipeline>(this),
                   &MediaPipeline::UpdateTransport_s, aTransportId, aFilter),
      NS_DISPATCH_NORMAL);
}

void MediaPipeline::UpdateTransport_s(const std::string& aTransportId,
                                      nsAutoPtr<MediaPipelineFilter> aFilter) {
  ASSERT_ON_THREAD(mStsThread);
  if (!mSignalsConnected) {
    mTransportHandler->SignalStateChange.connect(
        this, &MediaPipeline::RtpStateChange);
    mTransportHandler->SignalRtcpStateChange.connect(
        this, &MediaPipeline::RtcpStateChange);
    mTransportHandler->SignalEncryptedSending.connect(
        this, &MediaPipeline::EncryptedPacketSending);
    mTransportHandler->SignalPacketReceived.connect(
        this, &MediaPipeline::PacketReceived);
    mSignalsConnected = true;
  }

  if (aTransportId != mTransportId) {
    mTransportId = aTransportId;
    mRtpState = mTransportHandler->GetState(mTransportId, false);
    mRtcpState = mTransportHandler->GetState(mTransportId, true);
    CheckTransportStates();
  }

  if (mFilter && aFilter) {
    // Use the new filter, but don't forget any remote SSRCs that we've learned
    // by receiving traffic.
    mFilter->Update(*aFilter);
  } else {
    mFilter = aFilter;
  }
}

void MediaPipeline::AddRIDExtension_m(size_t aExtensionId) {
  RUN_ON_THREAD(mStsThread,
                WrapRunnable(RefPtr<MediaPipeline>(this),
                             &MediaPipeline::AddRIDExtension_s, aExtensionId),
                NS_DISPATCH_NORMAL);
}

void MediaPipeline::AddRIDExtension_s(size_t aExtensionId) {
  mRtpParser->RegisterRtpHeaderExtension(webrtc::kRtpExtensionRtpStreamId,
                                         aExtensionId);
}

void MediaPipeline::AddRIDFilter_m(const std::string& aRid) {
  RUN_ON_THREAD(mStsThread,
                WrapRunnable(RefPtr<MediaPipeline>(this),
                             &MediaPipeline::AddRIDFilter_s, aRid),
                NS_DISPATCH_NORMAL);
}

void MediaPipeline::AddRIDFilter_s(const std::string& aRid) {
  mFilter = new MediaPipelineFilter;
  mFilter->AddRemoteRtpStreamId(aRid);
}

void MediaPipeline::GetContributingSourceStats(
    const nsString& aInboundRtpStreamId,
    FallibleTArray<dom::RTCRTPContributingSourceStats>& aArr) const {
  // Get the expiry from now
  DOMHighResTimeStamp expiry = RtpCSRCStats::GetExpiryFromTime(GetNow());
  for (auto info : mCsrcStats) {
    if (!info.second.Expired(expiry)) {
      RTCRTPContributingSourceStats stats;
      info.second.GetWebidlInstance(stats, aInboundRtpStreamId);
      aArr.AppendElement(stats, fallible);
    }
  }
}

void MediaPipeline::RtpStateChange(const std::string& aTransportId,
                                   TransportLayer::State aState) {
  if (mTransportId != aTransportId) {
    return;
  }
  mRtpState = aState;
  CheckTransportStates();
}

void MediaPipeline::RtcpStateChange(const std::string& aTransportId,
                                    TransportLayer::State aState) {
  if (mTransportId != aTransportId) {
    return;
  }
  mRtcpState = aState;
  CheckTransportStates();
}

void MediaPipeline::CheckTransportStates() {
  ASSERT_ON_THREAD(mStsThread);

  if (mRtpState == TransportLayer::TS_CLOSED ||
      mRtpState == TransportLayer::TS_ERROR ||
      mRtcpState == TransportLayer::TS_CLOSED ||
      mRtcpState == TransportLayer::TS_ERROR) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Warning,
            ("RTP Transport failed for pipeline %p flow %s", this,
             mDescription.c_str()));

    NS_WARNING(
        "MediaPipeline Transport failed. This is not properly cleaned up yet");
    // TODO(ekr@rtfm.com): SECURITY: Figure out how to clean up if the
    // connection was good and now it is bad.
    // TODO(ekr@rtfm.com): Report up so that the PC knows we
    // have experienced an error.
    mTransport->Detach();
    return;
  }

  if (mRtpState == TransportLayer::TS_OPEN) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
            ("RTP Transport ready for pipeline %p flow %s", this,
             mDescription.c_str()));
  }

  if (mRtcpState == TransportLayer::TS_OPEN) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
            ("RTCP Transport ready for pipeline %p flow %s", this,
             mDescription.c_str()));
  }

  if (mRtpState == TransportLayer::TS_OPEN && mRtcpState == mRtpState) {
    mTransport->Attach(this);
    TransportReady_s();
  }
}

void MediaPipeline::SendPacket(MediaPacket&& packet) {
  ASSERT_ON_THREAD(mStsThread);
  MOZ_ASSERT(mRtpState == TransportLayer::TS_OPEN);
  MOZ_ASSERT(!mTransportId.empty());
  mTransportHandler->SendPacket(mTransportId, std::move(packet));
}

void MediaPipeline::IncrementRtpPacketsSent(int32_t aBytes) {
  ++mRtpPacketsSent;
  mRtpBytesSent += aBytes;

  if (!(mRtpPacketsSent % 100)) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
            ("RTP sent packet count for %s Pipeline %p: %u (%" PRId64 " bytes)",
             mDescription.c_str(), this, mRtpPacketsSent, mRtpBytesSent));
  }
}

void MediaPipeline::IncrementRtcpPacketsSent() {
  ++mRtcpPacketsSent;
  if (!(mRtcpPacketsSent % 100)) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
            ("RTCP sent packet count for %s Pipeline %p: %u",
             mDescription.c_str(), this, mRtcpPacketsSent));
  }
}

void MediaPipeline::IncrementRtpPacketsReceived(int32_t aBytes) {
  ++mRtpPacketsReceived;
  mRtpBytesReceived += aBytes;
  if (!(mRtpPacketsReceived % 100)) {
    MOZ_LOG(
        gMediaPipelineLog, LogLevel::Info,
        ("RTP received packet count for %s Pipeline %p: %u (%" PRId64 " bytes)",
         mDescription.c_str(), this, mRtpPacketsReceived, mRtpBytesReceived));
  }
}

void MediaPipeline::IncrementRtcpPacketsReceived() {
  ++mRtcpPacketsReceived;
  if (!(mRtcpPacketsReceived % 100)) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
            ("RTCP received packet count for %s Pipeline %p: %u",
             mDescription.c_str(), this, mRtcpPacketsReceived));
  }
}

void MediaPipeline::RtpPacketReceived(MediaPacket& packet) {
  if (mDirection == DirectionType::TRANSMIT) {
    return;
  }

  if (!mTransport->Pipeline()) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Error,
            ("Discarding incoming packet; transport disconnected"));
    return;
  }

  if (!mConduit) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
            ("Discarding incoming packet; media disconnected"));
    return;
  }

  if (!packet.len()) {
    return;
  }

  webrtc::RTPHeader header;
  if (!mRtpParser->Parse(packet.data(), packet.len(), &header, true)) {
    return;
  }

  if (mFilter && !mFilter->Filter(header)) {
    return;
  }

  // Make sure to only get the time once, and only if we need it by
  // using getTimestamp() for access
  DOMHighResTimeStamp now = 0.0;
  bool hasTime = false;

  // Remove expired RtpCSRCStats
  if (!mCsrcStats.empty()) {
    if (!hasTime) {
      now = GetNow();
      hasTime = true;
    }
    auto expiry = RtpCSRCStats::GetExpiryFromTime(now);
    for (auto p = mCsrcStats.begin(); p != mCsrcStats.end();) {
      if (p->second.Expired(expiry)) {
        p = mCsrcStats.erase(p);
        continue;
      }
      p++;
    }
  }

  // Add new RtpCSRCStats
  if (header.numCSRCs) {
    for (auto i = 0; i < header.numCSRCs; i++) {
      if (!hasTime) {
        now = GetNow();
        hasTime = true;
      }
      auto csrcInfo = mCsrcStats.find(header.arrOfCSRCs[i]);
      if (csrcInfo == mCsrcStats.end()) {
        mCsrcStats.insert(std::make_pair(
            header.arrOfCSRCs[i], RtpCSRCStats(header.arrOfCSRCs[i], now)));
      } else {
        csrcInfo->second.SetTimestamp(now);
      }
    }
  }

  MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
          ("%s received RTP packet.", mDescription.c_str()));
  IncrementRtpPacketsReceived(packet.len());
  OnRtpPacketReceived();

  RtpLogger::LogPacket(packet, true, mDescription);

  // Might be nice to pass ownership of the buffer in this case, but it is a
  // small optimization in a rare case.
  mPacketDumper->Dump(mLevel, dom::mozPacketDumpType::Srtp, false,
                      packet.encrypted_data(), packet.encrypted_len());

  mPacketDumper->Dump(mLevel, dom::mozPacketDumpType::Rtp, false, packet.data(),
                      packet.len());

  (void)mConduit->ReceivedRTPPacket(packet.data(), packet.len(),
                                    header.ssrc);  // Ignore error codes
}

void MediaPipeline::RtcpPacketReceived(MediaPacket& packet) {
  if (!mTransport->Pipeline()) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
            ("Discarding incoming packet; transport disconnected"));
    return;
  }

  if (!mConduit) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
            ("Discarding incoming packet; media disconnected"));
    return;
  }

  if (!packet.len()) {
    return;
  }

  // We do not filter RTCP. This is because a compound RTCP packet can contain
  // any collection of RTCP packets, and webrtc.org already knows how to filter
  // out what it is interested in, and what it is not. Maybe someday we should
  // have a TransportLayer that breaks up compound RTCP so we can filter them
  // individually, but I doubt that will matter much.

  MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
          ("%s received RTCP packet.", mDescription.c_str()));
  IncrementRtcpPacketsReceived();

  RtpLogger::LogPacket(packet, true, mDescription);

  // Might be nice to pass ownership of the buffer in this case, but it is a
  // small optimization in a rare case.
  mPacketDumper->Dump(mLevel, dom::mozPacketDumpType::Srtcp, false,
                      packet.encrypted_data(), packet.encrypted_len());

  mPacketDumper->Dump(mLevel, dom::mozPacketDumpType::Rtcp, false,
                      packet.data(), packet.len());

  if (StaticPrefs::media_webrtc_net_force_disable_rtcp_reception()) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
            ("%s RTCP packet forced to be dropped", mDescription.c_str()));
    return;
  }

  (void)mConduit->ReceivedRTCPPacket(packet.data(),
                                     packet.len());  // Ignore error codes
}

void MediaPipeline::PacketReceived(const std::string& aTransportId,
                                   MediaPacket& packet) {
  if (mTransportId != aTransportId) {
    return;
  }

  if (!mTransport->Pipeline()) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
            ("Discarding incoming packet; transport disconnected"));
    return;
  }

  switch (packet.type()) {
    case MediaPacket::RTP:
      RtpPacketReceived(packet);
      break;
    case MediaPacket::RTCP:
      RtcpPacketReceived(packet);
      break;
    default:;
  }
}

void MediaPipeline::EncryptedPacketSending(const std::string& aTransportId,
                                           MediaPacket& aPacket) {
  if (mTransportId == aTransportId) {
    dom::mozPacketDumpType type;
    if (aPacket.type() == MediaPacket::SRTP) {
      type = dom::mozPacketDumpType::Srtp;
    } else if (aPacket.type() == MediaPacket::SRTCP) {
      type = dom::mozPacketDumpType::Srtcp;
    } else if (aPacket.type() == MediaPacket::DTLS) {
      // TODO(bug 1497936): Implement packet dump for DTLS
      return;
    } else {
      MOZ_ASSERT(false);
      return;
    }
    mPacketDumper->Dump(Level(), type, true, aPacket.data(), aPacket.len());
  }
}

class MediaPipelineTransmit::PipelineListener
    : public DirectMediaStreamTrackListener {
  friend class MediaPipelineTransmit;

 public:
  explicit PipelineListener(const RefPtr<MediaSessionConduit>& aConduit)
      : mConduit(aConduit),
        mActive(false),
        mEnabled(false),
        mDirectConnect(false) {}

  ~PipelineListener() {
    NS_ReleaseOnMainThreadSystemGroup("MediaPipeline::mConduit",
                                      mConduit.forget());
    if (mConverter) {
      mConverter->Shutdown();
    }
  }

  void SetActive(bool aActive) {
    mActive = aActive;
    if (mConverter) {
      mConverter->SetActive(aActive);
    }
  }
  void SetEnabled(bool aEnabled) { mEnabled = aEnabled; }

  // These are needed since nested classes don't have access to any particular
  // instance of the parent
  void SetAudioProxy(const RefPtr<AudioProxyThread>& aProxy) {
    mAudioProcessing = aProxy;
  }

  void SetVideoFrameConverter(const RefPtr<VideoFrameConverter>& aConverter) {
    mConverter = aConverter;
  }

  void OnVideoFrameConverted(const webrtc::VideoFrame& aVideoFrame) {
    MOZ_RELEASE_ASSERT(mConduit->type() == MediaSessionConduit::VIDEO);
    static_cast<VideoSessionConduit*>(mConduit.get())
        ->SendVideoFrame(aVideoFrame);
  }

  // Implement MediaStreamTrackListener
  void NotifyQueuedChanges(MediaStreamGraph* aGraph, StreamTime aTrackOffset,
                           const MediaSegment& aQueuedMedia) override;
  void NotifyEnabledStateChanged(bool aEnabled) override;

  // Implement DirectMediaStreamTrackListener
  void NotifyRealtimeTrackData(MediaStreamGraph* aGraph,
                               StreamTime aTrackOffset,
                               const MediaSegment& aMedia) override;
  void NotifyDirectListenerInstalled(InstallationResult aResult) override;
  void NotifyDirectListenerUninstalled() override;

 private:
  void NewData(const MediaSegment& aMedia, TrackRate aRate = 0);

  RefPtr<MediaSessionConduit> mConduit;
  RefPtr<AudioProxyThread> mAudioProcessing;
  RefPtr<VideoFrameConverter> mConverter;

  // active is true if there is a transport to send on
  mozilla::Atomic<bool> mActive;
  // enabled is true if the media access control permits sending
  // actual content; when false you get black/silence
  mozilla::Atomic<bool> mEnabled;

  // Written and read on the MediaStreamGraph thread
  bool mDirectConnect;
};

// Implements VideoConverterListener for MediaPipeline.
//
// We pass converted frames on to MediaPipelineTransmit::PipelineListener
// where they are further forwarded to VideoConduit.
// MediaPipelineTransmit calls Detach() during shutdown to ensure there is
// no cyclic dependencies between us and PipelineListener.
class MediaPipelineTransmit::VideoFrameFeeder : public VideoConverterListener {
 public:
  explicit VideoFrameFeeder(const RefPtr<PipelineListener>& aListener)
      : mMutex("VideoFrameFeeder"), mListener(aListener) {
    MOZ_COUNT_CTOR(VideoFrameFeeder);
  }

  void Detach() {
    MutexAutoLock lock(mMutex);

    mListener = nullptr;
  }

  void OnVideoFrameConverted(const webrtc::VideoFrame& aVideoFrame) override {
    MutexAutoLock lock(mMutex);

    if (!mListener) {
      return;
    }

    mListener->OnVideoFrameConverted(aVideoFrame);
  }

 protected:
  virtual ~VideoFrameFeeder() { MOZ_COUNT_DTOR(VideoFrameFeeder); }

  Mutex mMutex;  // Protects the member below.
  RefPtr<PipelineListener> mListener;
};

MediaPipelineTransmit::MediaPipelineTransmit(
    const std::string& aPc, MediaTransportHandler* aTransportHandler,
    nsCOMPtr<nsIEventTarget> aMainThread, nsCOMPtr<nsIEventTarget> aStsThread,
    bool aIsVideo, RefPtr<MediaSessionConduit> aConduit)
    : MediaPipeline(aPc, aTransportHandler, DirectionType::TRANSMIT,
                    aMainThread, aStsThread, aConduit),
      mIsVideo(aIsVideo),
      mListener(new PipelineListener(aConduit)),
      mFeeder(aIsVideo ? MakeAndAddRef<VideoFrameFeeder>(mListener)
                       : nullptr)  // For video we send frames to an
                                   // async VideoFrameConverter that
                                   // calls back to a VideoFrameFeeder
                                   // that feeds I420 frames to
                                   // VideoConduit.
      ,
      mTransmitting(false) {
  if (!IsVideo()) {
    mAudioProcessing = MakeAndAddRef<AudioProxyThread>(
        static_cast<AudioSessionConduit*>(aConduit.get()));
    mListener->SetAudioProxy(mAudioProcessing);
  } else {  // Video
    mConverter = MakeAndAddRef<VideoFrameConverter>();
    mConverter->AddListener(mFeeder);
    mListener->SetVideoFrameConverter(mConverter);
  }
}

MediaPipelineTransmit::~MediaPipelineTransmit() {
  if (mFeeder) {
    mFeeder->Detach();
  }

  MOZ_ASSERT(!mDomTrack);
}

void MediaPipeline::SetDescription_s(const std::string& description) {
  mDescription = description;
}

void MediaPipelineTransmit::SetDescription() {
  std::string description;
  description = mPc + "| ";
  description += mConduit->type() == MediaSessionConduit::AUDIO
                     ? "Transmit audio["
                     : "Transmit video[";

  if (!mDomTrack) {
    description += "no track]";
  } else {
    nsString nsTrackId;
    mDomTrack->GetId(nsTrackId);
    std::string trackId(NS_ConvertUTF16toUTF8(nsTrackId).get());
    description += trackId;
    description += "]";
  }

  RUN_ON_THREAD(
      mStsThread,
      WrapRunnable(RefPtr<MediaPipeline>(this),
                   &MediaPipelineTransmit::SetDescription_s, description),
      NS_DISPATCH_NORMAL);
}

void MediaPipelineTransmit::Stop() {
  ASSERT_ON_THREAD(mMainThread);

  if (!mDomTrack || !mTransmitting) {
    return;
  }

  mTransmitting = false;

  if (mDomTrack->AsAudioStreamTrack()) {
    mDomTrack->RemoveDirectListener(mListener);
    mDomTrack->RemoveListener(mListener);
  } else if (mDomTrack->AsVideoStreamTrack()) {
    mDomTrack->RemoveDirectListener(mListener);
    mDomTrack->RemoveListener(mListener);
  } else {
    MOZ_ASSERT(false, "Unknown track type");
  }

  mConduit->StopTransmitting();
}

bool MediaPipelineTransmit::Transmitting() const {
  ASSERT_ON_THREAD(mMainThread);

  return mTransmitting;
}

void MediaPipelineTransmit::Start() {
  ASSERT_ON_THREAD(mMainThread);

  if (!mDomTrack || mTransmitting) {
    return;
  }

  mTransmitting = true;

  mConduit->StartTransmitting();

  // TODO(ekr@rtfm.com): Check for errors
  MOZ_LOG(
      gMediaPipelineLog, LogLevel::Debug,
      ("Attaching pipeline to track %p conduit type=%s", this,
       (mConduit->type() == MediaSessionConduit::AUDIO ? "audio" : "video")));

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  // With full duplex we don't risk having audio come in late to the MSG
  // so we won't need a direct listener.
  const bool enableDirectListener =
      !Preferences::GetBool("media.navigator.audio.full_duplex", false);
#else
  const bool enableDirectListener = true;
#endif

  if (mDomTrack->AsAudioStreamTrack()) {
    if (enableDirectListener) {
      // Register the Listener directly with the source if we can.
      // We also register it as a non-direct listener so we fall back to that
      // if installing the direct listener fails. As a direct listener we get
      // access to direct unqueued (and not resampled) data.
      mDomTrack->AddDirectListener(mListener);
    }
    mDomTrack->AddListener(mListener);
  } else if (mDomTrack->AsVideoStreamTrack()) {
    mDomTrack->AddDirectListener(mListener);
    mDomTrack->AddListener(mListener);
  } else {
    MOZ_ASSERT(false, "Unknown track type");
  }
}

bool MediaPipelineTransmit::IsVideo() const { return mIsVideo; }

void MediaPipelineTransmit::UpdateSinkIdentity_m(
    const MediaStreamTrack* aTrack, nsIPrincipal* aPrincipal,
    const PeerIdentity* aSinkIdentity) {
  ASSERT_ON_THREAD(mMainThread);

  if (aTrack != nullptr && aTrack != mDomTrack) {
    // If a track is specified, then it might not be for this pipeline,
    // since we receive notifications for all tracks on the PC.
    // nullptr means that the PeerIdentity has changed and shall be applied
    // to all tracks of the PC.
    return;
  }

  if (!mDomTrack) {
    // Nothing to do here
    return;
  }

  bool enableTrack = aPrincipal->Subsumes(mDomTrack->GetPrincipal());
  if (!enableTrack) {
    // first try didn't work, but there's a chance that this is still available
    // if our track is bound to a peerIdentity, and the peer connection (our
    // sink) is bound to the same identity, then we can enable the track.
    const PeerIdentity* trackIdentity = mDomTrack->GetPeerIdentity();
    if (aSinkIdentity && trackIdentity) {
      enableTrack = (*aSinkIdentity == *trackIdentity);
    }
  }

  mListener->SetEnabled(enableTrack);
}

void MediaPipelineTransmit::DetachMedia() {
  ASSERT_ON_THREAD(mMainThread);
  mDomTrack = nullptr;
  // Let the listener be destroyed with the pipeline (or later).
}

void MediaPipelineTransmit::TransportReady_s() {
  ASSERT_ON_THREAD(mStsThread);
  // Call base ready function.
  MediaPipeline::TransportReady_s();
  mListener->SetActive(true);
}

nsresult MediaPipelineTransmit::SetTrack(MediaStreamTrack* aDomTrack) {
  // MainThread, checked in calls we make
  if (aDomTrack) {
    nsString nsTrackId;
    aDomTrack->GetId(nsTrackId);
    std::string track_id(NS_ConvertUTF16toUTF8(nsTrackId).get());
    MOZ_LOG(
        gMediaPipelineLog, LogLevel::Debug,
        ("Reattaching pipeline to track %p track %s conduit type: %s",
         &aDomTrack, track_id.c_str(),
         (mConduit->type() == MediaSessionConduit::AUDIO ? "audio" : "video")));
  }

  RefPtr<dom::MediaStreamTrack> oldTrack = mDomTrack;
  bool wasTransmitting = oldTrack && mTransmitting;
  Stop();
  mDomTrack = aDomTrack;
  SetDescription();

  if (wasTransmitting) {
    Start();
  }
  return NS_OK;
}

nsresult MediaPipeline::PipelineTransport::SendRtpPacket(const uint8_t* aData,
                                                         size_t aLen) {
  nsAutoPtr<MediaPacket> packet(new MediaPacket);
  packet->Copy(aData, aLen, aLen + SRTP_MAX_EXPANSION);
  packet->SetType(MediaPacket::RTP);

  RUN_ON_THREAD(
      mStsThread,
      WrapRunnable(RefPtr<MediaPipeline::PipelineTransport>(this),
                   &MediaPipeline::PipelineTransport::SendRtpRtcpPacket_s,
                   packet),
      NS_DISPATCH_NORMAL);

  return NS_OK;
}

void MediaPipeline::PipelineTransport::SendRtpRtcpPacket_s(
    nsAutoPtr<MediaPacket> aPacket) {
  bool isRtp = aPacket->type() == MediaPacket::RTP;

  ASSERT_ON_THREAD(mStsThread);
  if (!mPipeline) {
    return;  // Detached
  }

  if (isRtp && mPipeline->mRtpState != TransportLayer::TS_OPEN) {
    return;
  }

  if (!isRtp && mPipeline->mRtcpState != TransportLayer::TS_OPEN) {
    return;
  }

  MediaPacket packet(std::move(*aPacket));
  packet.sdp_level() = Some(mPipeline->Level());

  if (RtpLogger::IsPacketLoggingOn()) {
    RtpLogger::LogPacket(packet, false, mPipeline->mDescription);
  }

  if (isRtp) {
    mPipeline->mPacketDumper->Dump(mPipeline->Level(),
                                   dom::mozPacketDumpType::Rtp, true,
                                   packet.data(), packet.len());
    mPipeline->IncrementRtpPacketsSent(packet.len());
  } else {
    mPipeline->mPacketDumper->Dump(mPipeline->Level(),
                                   dom::mozPacketDumpType::Rtcp, true,
                                   packet.data(), packet.len());
    mPipeline->IncrementRtcpPacketsSent();
  }

  MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
          ("%s sending %s packet", mPipeline->mDescription.c_str(),
           (isRtp ? "RTP" : "RTCP")));

  mPipeline->SendPacket(std::move(packet));
}

nsresult MediaPipeline::PipelineTransport::SendRtcpPacket(const uint8_t* aData,
                                                          size_t aLen) {
  nsAutoPtr<MediaPacket> packet(new MediaPacket);
  packet->Copy(aData, aLen, aLen + SRTP_MAX_EXPANSION);
  packet->SetType(MediaPacket::RTCP);

  RUN_ON_THREAD(
      mStsThread,
      WrapRunnable(RefPtr<MediaPipeline::PipelineTransport>(this),
                   &MediaPipeline::PipelineTransport::SendRtpRtcpPacket_s,
                   packet),
      NS_DISPATCH_NORMAL);

  return NS_OK;
}

// Called if we're attached with AddDirectListener()
void MediaPipelineTransmit::PipelineListener::NotifyRealtimeTrackData(
    MediaStreamGraph* aGraph, StreamTime aOffset, const MediaSegment& aMedia) {
  MOZ_LOG(
      gMediaPipelineLog, LogLevel::Debug,
      ("MediaPipeline::NotifyRealtimeTrackData() listener=%p, offset=%" PRId64
       ", duration=%" PRId64,
       this, aOffset, aMedia.GetDuration()));
  TRACE_COMMENT("%s",
                aMedia.GetType() == MediaSegment::VIDEO ? "Video" : "Audio");
  NewData(aMedia, aGraph->GraphRate());
}

void MediaPipelineTransmit::PipelineListener::NotifyQueuedChanges(
    MediaStreamGraph* aGraph, StreamTime aOffset,
    const MediaSegment& aQueuedMedia) {
  MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
          ("MediaPipeline::NotifyQueuedChanges()"));

  if (aQueuedMedia.GetType() == MediaSegment::VIDEO) {
    // We always get video from the direct listener.
    return;
  }

  TRACE_AUDIO_CALLBACK_COMMENT("Audio");

  if (mDirectConnect) {
    // ignore non-direct data if we're also getting direct data
    return;
  }

  size_t rate;
  if (aGraph) {
    rate = aGraph->GraphRate();
  } else {
    // When running tests, graph may be null. In that case use a default.
    rate = 16000;
  }
  NewData(aQueuedMedia, rate);
}

void MediaPipelineTransmit::PipelineListener::NotifyEnabledStateChanged(
    bool aEnabled) {
  if (mConduit->type() != MediaSessionConduit::VIDEO) {
    return;
  }
  MOZ_ASSERT(mConverter);
  mConverter->SetTrackEnabled(aEnabled);
}

void MediaPipelineTransmit::PipelineListener::NotifyDirectListenerInstalled(
    InstallationResult aResult) {
  MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
          ("MediaPipeline::NotifyDirectListenerInstalled() listener=%p,"
           " result=%d",
           this, static_cast<int32_t>(aResult)));

  mDirectConnect = InstallationResult::SUCCESS == aResult;
}

void MediaPipelineTransmit::PipelineListener::
    NotifyDirectListenerUninstalled() {
  MOZ_LOG(
      gMediaPipelineLog, LogLevel::Info,
      ("MediaPipeline::NotifyDirectListenerUninstalled() listener=%p", this));

  mDirectConnect = false;
}

void MediaPipelineTransmit::PipelineListener::NewData(
    const MediaSegment& aMedia, TrackRate aRate /* = 0 */) {
  if (mConduit->type() != (aMedia.GetType() == MediaSegment::AUDIO
                               ? MediaSessionConduit::AUDIO
                               : MediaSessionConduit::VIDEO)) {
    MOZ_ASSERT(false,
               "The media type should always be correct since the "
               "listener is locked to a specific track");
    return;
  }

  // TODO(ekr@rtfm.com): For now assume that we have only one
  // track type and it's destined for us
  // See bug 784517
  if (aMedia.GetType() == MediaSegment::AUDIO) {
    MOZ_RELEASE_ASSERT(aRate > 0);

    if (!mActive) {
      MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
              ("Discarding audio packets because transport not ready"));
      return;
    }

    const AudioSegment* audio = static_cast<const AudioSegment*>(&aMedia);
    for (AudioSegment::ConstChunkIterator iter(*audio); !iter.IsEnded();
         iter.Next()) {
      mAudioProcessing->QueueAudioChunk(aRate, *iter, mEnabled);
    }
  } else {
    const VideoSegment* video = static_cast<const VideoSegment*>(&aMedia);

    for (VideoSegment::ConstChunkIterator iter(*video); !iter.IsEnded();
         iter.Next()) {
      mConverter->QueueVideoChunk(*iter, !mEnabled);
    }
  }
}

class GenericReceiveListener : public MediaStreamTrackListener {
 public:
  explicit GenericReceiveListener(dom::MediaStreamTrack* aTrack)
      : mTrackSource(new nsMainThreadPtrHolder<RemoteTrackSource>(
            "GenericReceiveListener::mTrackSource",
            &static_cast<RemoteTrackSource&>(aTrack->GetSource()))),
        mTrackId(aTrack->GetTrackID()),
        mSource(mTrackSource->mStream),
        mIsAudio(aTrack->AsAudioStreamTrack()),
        mPrincipalHandle(PRINCIPAL_HANDLE_NONE),
        mListening(false),
        mMaybeTrackNeedsUnmute(true) {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
    MOZ_DIAGNOSTIC_ASSERT(mSource, "Must be used with a SourceMediaStream");
  }

  virtual ~GenericReceiveListener() = default;

  void AddTrackToSource(uint32_t aRate = 0) {
    MOZ_ASSERT_IF(mIsAudio, aRate != 0);

    if (mIsAudio) {
      mSource->AddAudioTrack(mTrackId, aRate, new AudioSegment());
    } else {
      mSource->AddTrack(mTrackId, new VideoSegment());
    }
    MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
            ("GenericReceiveListener added %s track %d to stream %p",
             mIsAudio ? "audio" : "video", mTrackId, mSource.get()));

    mSource->AddTrackListener(this, mTrackId);
  }

  void AddSelf() {
    if (mListening) {
      return;
    }
    mListening = true;
    mMaybeTrackNeedsUnmute = true;
    if (mIsAudio && !mSource->IsDestroyed()) {
      mSource->SetPullingEnabled(mTrackId, true);
    }
  }

  void RemoveSelf() {
    if (!mListening) {
      return;
    }
    mListening = false;
    if (mIsAudio && !mSource->IsDestroyed()) {
      mSource->SetPullingEnabled(mTrackId, false);
    }
  }

  void OnRtpReceived() {
    if (mMaybeTrackNeedsUnmute) {
      mMaybeTrackNeedsUnmute = false;
      NS_DispatchToMainThread(
          NewRunnableMethod("GenericReceiveListener::OnRtpReceived_m", this,
                            &GenericReceiveListener::OnRtpReceived_m));
    }
  }

  void OnRtpReceived_m() {
    if (mListening) {
      mTrackSource->SetMuted(false);
    }
  }

  void EndTrack() {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
            ("GenericReceiveListener ending track"));

    if (!mSource->IsDestroyed()) {
      // This breaks the cycle with the SourceMediaStream
      mSource->RemoveTrackListener(this, mTrackId);
      mSource->EndTrack(mTrackId);
      mSource->Destroy();
    }

    NS_DispatchToMainThread(NewRunnableMethod("RemoteTrackSource::ForceEnded",
                                              mTrackSource.get(),
                                              &RemoteTrackSource::ForceEnded));
  }

  // Must be called on the main thread
  void SetPrincipalHandle_m(const PrincipalHandle& aPrincipalHandle) {
    class Message : public ControlMessage {
     public:
      Message(GenericReceiveListener* aListener,
              const PrincipalHandle& aPrincipalHandle)
          : ControlMessage(nullptr),
            mListener(aListener),
            mPrincipalHandle(aPrincipalHandle) {}

      void Run() override {
        mListener->SetPrincipalHandle_msg(mPrincipalHandle);
      }

      const RefPtr<GenericReceiveListener> mListener;
      PrincipalHandle mPrincipalHandle;
    };

    mSource->GraphImpl()->AppendMessage(
        MakeUnique<Message>(this, aPrincipalHandle));
  }

  // Must be called on the MediaStreamGraph thread
  void SetPrincipalHandle_msg(const PrincipalHandle& aPrincipalHandle) {
    mPrincipalHandle = aPrincipalHandle;
  }

 protected:
  const nsMainThreadPtrHandle<RemoteTrackSource> mTrackSource;
  const TrackID mTrackId;
  const RefPtr<SourceMediaStream> mSource;
  const bool mIsAudio;
  PrincipalHandle mPrincipalHandle;
  bool mListening;
  Atomic<bool> mMaybeTrackNeedsUnmute;
};

MediaPipelineReceive::MediaPipelineReceive(
    const std::string& aPc, MediaTransportHandler* aTransportHandler,
    nsCOMPtr<nsIEventTarget> aMainThread, nsCOMPtr<nsIEventTarget> aStsThread,
    RefPtr<MediaSessionConduit> aConduit)
    : MediaPipeline(aPc, aTransportHandler, DirectionType::RECEIVE, aMainThread,
                    aStsThread, aConduit) {}

MediaPipelineReceive::~MediaPipelineReceive() {}

class MediaPipelineReceiveAudio::PipelineListener
    : public GenericReceiveListener {
 public:
  PipelineListener(dom::MediaStreamTrack* aTrack,
                   const RefPtr<MediaSessionConduit>& aConduit)
      : GenericReceiveListener(aTrack),
        mConduit(aConduit),
        // AudioSession conduit only supports 16, 32, 44.1 and 48kHz
        // This is an artificial limitation, it would however require more
        // changes to support any rates. If the sampling rate is not-supported,
        // we will use 48kHz instead.
        mRate(static_cast<AudioSessionConduit*>(mConduit.get())
                      ->IsSamplingFreqSupported(mSource->GraphRate())
                  ? mSource->GraphRate()
                  : WEBRTC_MAX_SAMPLE_RATE),
        mTaskQueue(
            new TaskQueue(GetMediaThreadPool(MediaThreadType::WEBRTC_DECODER),
                          "AudioPipelineListener")),
        mPlayedTicks(0) {
    AddTrackToSource(mRate);
  }

  // Implement MediaStreamTrackListener
  void NotifyPull(MediaStreamGraph* aGraph, StreamTime aEndOfAppendedData,
                  StreamTime aDesiredTime) override {
    NotifyPullImpl(aDesiredTime);
  }

 private:
  ~PipelineListener() {
    NS_ReleaseOnMainThreadSystemGroup("MediaPipeline::mConduit",
                                      mConduit.forget());
  }

  void NotifyPullImpl(StreamTime aDesiredTime) {
    TRACE_AUDIO_CALLBACK_COMMENT("Track %i", mTrackId);
    uint32_t samplesPer10ms = mRate / 100;

    // mSource's rate is not necessarily the same as the graph rate, since there
    // are sample-rate constraints on the inbound audio: only 16, 32, 44.1 and
    // 48kHz are supported. The audio frames we get here is going to be
    // resampled when inserted into the graph.
    TrackTicks desired = mSource->TimeToTicksRoundUp(mRate, aDesiredTime);
    TrackTicks framesNeeded = desired - mPlayedTicks;

    while (framesNeeded >= 0) {
      const int scratchBufferLength =
          AUDIO_SAMPLE_BUFFER_MAX_BYTES / sizeof(int16_t);
      int16_t scratchBuffer[scratchBufferLength];

      int samplesLength = scratchBufferLength;

      // This fetches 10ms of data, either mono or stereo
      MediaConduitErrorCode err =
          static_cast<AudioSessionConduit*>(mConduit.get())
              ->GetAudioFrame(scratchBuffer, mRate,
                              0,  // TODO(ekr@rtfm.com): better estimate of
                                  // "capture" (really playout) delay
                              samplesLength);

      if (err != kMediaConduitNoError) {
        // Insert silence on conduit/GIPS failure (extremely unlikely)
        MOZ_LOG(gMediaPipelineLog, LogLevel::Error,
                ("Audio conduit failed (%d) to return data @ %" PRId64
                 " (desired %" PRId64 " -> %f)",
                 err, mPlayedTicks, aDesiredTime,
                 mSource->StreamTimeToSeconds(aDesiredTime)));
        // if this is not enough we'll loop and provide more
        samplesLength = samplesPer10ms;
        PodArrayZero(scratchBuffer);
      }

      MOZ_RELEASE_ASSERT(samplesLength <= scratchBufferLength);

      MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
              ("Audio conduit returned buffer of length %u", samplesLength));

      RefPtr<SharedBuffer> samples =
          SharedBuffer::Create(samplesLength * sizeof(uint16_t));
      int16_t* samplesData = static_cast<int16_t*>(samples->Data());
      AudioSegment segment;
      // We derive the number of channels of the stream from the number of
      // samples the AudioConduit gives us, considering it gives us packets of
      // 10ms and we know the rate.
      uint32_t channelCount = samplesLength / samplesPer10ms;
      AutoTArray<int16_t*, 2> channels;
      AutoTArray<const int16_t*, 2> outputChannels;
      size_t frames = samplesLength / channelCount;

      channels.SetLength(channelCount);

      size_t offset = 0;
      for (size_t i = 0; i < channelCount; i++) {
        channels[i] = samplesData + offset;
        offset += frames;
      }

      DeinterleaveAndConvertBuffer(scratchBuffer, frames, channelCount,
                                   channels.Elements());

      outputChannels.AppendElements(channels);

      segment.AppendFrames(samples.forget(), outputChannels, frames,
                           mPrincipalHandle);

      // Handle track not actually added yet or removed/finished
      if (mSource->AppendToTrack(mTrackId, &segment)) {
        framesNeeded -= frames;
        mPlayedTicks += frames;
      } else {
        MOZ_LOG(gMediaPipelineLog, LogLevel::Error, ("AppendToTrack failed"));
        // we can't un-read the data, but that's ok since we don't want to
        // buffer - but don't i-loop!
        break;
      }
    }
  }

  RefPtr<MediaSessionConduit> mConduit;
  // This conduit's sampling rate. This is either 16, 32, 44.1 or 48kHz, and
  // tries to be the same as the graph rate. If the graph rate is higher than
  // 48kHz, mRate is capped to 48kHz. If mRate does not match the graph rate,
  // audio is resampled to the graph rate.
  const TrackRate mRate;
  const RefPtr<TaskQueue> mTaskQueue;
  TrackTicks mPlayedTicks;
};

MediaPipelineReceiveAudio::MediaPipelineReceiveAudio(
    const std::string& aPc, MediaTransportHandler* aTransportHandler,
    nsCOMPtr<nsIEventTarget> aMainThread, nsCOMPtr<nsIEventTarget> aStsThread,
    RefPtr<AudioSessionConduit> aConduit, dom::MediaStreamTrack* aTrack)
    : MediaPipelineReceive(aPc, aTransportHandler, aMainThread, aStsThread,
                           aConduit),
      mListener(aTrack ? new PipelineListener(aTrack, mConduit) : nullptr) {
  mDescription = mPc + "| Receive audio";
}

void MediaPipelineReceiveAudio::DetachMedia() {
  ASSERT_ON_THREAD(mMainThread);
  if (mListener) {
    mListener->EndTrack();
  }
}

void MediaPipelineReceiveAudio::SetPrincipalHandle_m(
    const PrincipalHandle& aPrincipalHandle) {
  if (mListener) {
    mListener->SetPrincipalHandle_m(aPrincipalHandle);
  }
}

void MediaPipelineReceiveAudio::Start() {
  mConduit->StartReceiving();
  if (mListener) {
    mListener->AddSelf();
  }
}

void MediaPipelineReceiveAudio::Stop() {
  if (mListener) {
    mListener->RemoveSelf();
  }
  mConduit->StopReceiving();
}

void MediaPipelineReceiveAudio::OnRtpPacketReceived() {
  if (mListener) {
    mListener->OnRtpReceived();
  }
}

class MediaPipelineReceiveVideo::PipelineListener
    : public GenericReceiveListener {
 public:
  explicit PipelineListener(dom::MediaStreamTrack* aTrack)
      : GenericReceiveListener(aTrack),
        mImageContainer(
            LayerManager::CreateImageContainer(ImageContainer::ASYNCHRONOUS)) {
    AddTrackToSource();
  }

  void RenderVideoFrame(const webrtc::VideoFrameBuffer& aBuffer,
                        uint32_t aTimeStamp, int64_t aRenderTime) {
    RefPtr<Image> image;
    if (aBuffer.type() == webrtc::VideoFrameBuffer::Type::kNative) {
      // We assume that only native handles are used with the
      // WebrtcMediaDataDecoderCodec decoder.
      const ImageBuffer* imageBuffer =
          static_cast<const ImageBuffer*>(&aBuffer);
      image = imageBuffer->GetNativeImage();
    } else {
      MOZ_ASSERT(aBuffer.type() == webrtc::VideoFrameBuffer::Type::kI420);
      rtc::scoped_refptr<const webrtc::I420BufferInterface> i420 =
          aBuffer.GetI420();

      MOZ_ASSERT(i420->DataY());
      // Create a video frame using |buffer|.
      RefPtr<PlanarYCbCrImage> yuvImage =
          mImageContainer->CreatePlanarYCbCrImage();

      PlanarYCbCrData yuvData;
      yuvData.mYChannel = const_cast<uint8_t*>(i420->DataY());
      yuvData.mYSize = IntSize(i420->width(), i420->height());
      yuvData.mYStride = i420->StrideY();
      MOZ_ASSERT(i420->StrideU() == i420->StrideV());
      yuvData.mCbCrStride = i420->StrideU();
      yuvData.mCbChannel = const_cast<uint8_t*>(i420->DataU());
      yuvData.mCrChannel = const_cast<uint8_t*>(i420->DataV());
      yuvData.mCbCrSize =
          IntSize((i420->width() + 1) >> 1, (i420->height() + 1) >> 1);
      yuvData.mPicX = 0;
      yuvData.mPicY = 0;
      yuvData.mPicSize = IntSize(i420->width(), i420->height());
      yuvData.mStereoMode = StereoMode::MONO;
      // This isn't the best default.
      yuvData.mYUVColorSpace = gfx::YUVColorSpace::BT601;

      if (!yuvImage->CopyData(yuvData)) {
        MOZ_ASSERT(false);
        return;
      }

      image = yuvImage.forget();
    }

    VideoSegment segment;
    auto size = image->GetSize();
    segment.AppendFrame(image.forget(), size, mPrincipalHandle);
    mSource->AppendToTrack(mTrackId, &segment);
  }

 private:
  RefPtr<layers::ImageContainer> mImageContainer;
};

class MediaPipelineReceiveVideo::PipelineRenderer
    : public mozilla::VideoRenderer {
 public:
  explicit PipelineRenderer(MediaPipelineReceiveVideo* aPipeline)
      : mPipeline(aPipeline) {}

  void Detach() { mPipeline = nullptr; }

  // Implement VideoRenderer
  void FrameSizeChange(unsigned int aWidth, unsigned int aHeight) override {}
  void RenderVideoFrame(const webrtc::VideoFrameBuffer& aBuffer,
                        uint32_t aTimeStamp, int64_t aRenderTime) override {
    mPipeline->mListener->RenderVideoFrame(aBuffer, aTimeStamp, aRenderTime);
  }

 private:
  MediaPipelineReceiveVideo* mPipeline;  // Raw pointer to avoid cycles
};

MediaPipelineReceiveVideo::MediaPipelineReceiveVideo(
    const std::string& aPc, MediaTransportHandler* aTransportHandler,
    nsCOMPtr<nsIEventTarget> aMainThread, nsCOMPtr<nsIEventTarget> aStsThread,
    RefPtr<VideoSessionConduit> aConduit, dom::MediaStreamTrack* aTrack)
    : MediaPipelineReceive(aPc, aTransportHandler, aMainThread, aStsThread,
                           aConduit),
      mRenderer(new PipelineRenderer(this)),
      mListener(aTrack ? new PipelineListener(aTrack) : nullptr) {
  mDescription = mPc + "| Receive video";
  aConduit->AttachRenderer(mRenderer);
}

void MediaPipelineReceiveVideo::DetachMedia() {
  ASSERT_ON_THREAD(mMainThread);

  // stop generating video and thus stop invoking the PipelineRenderer
  // and PipelineListener - the renderer has a raw ptr to the Pipeline to
  // avoid cycles, and the render callbacks are invoked from a different
  // thread so simple null-checks would cause TSAN bugs without locks.
  static_cast<VideoSessionConduit*>(mConduit.get())->DetachRenderer();
  if (mListener) {
    mListener->EndTrack();
  }
}

void MediaPipelineReceiveVideo::SetPrincipalHandle_m(
    const PrincipalHandle& aPrincipalHandle) {
  if (mListener) {
    mListener->SetPrincipalHandle_m(aPrincipalHandle);
  }
}

void MediaPipelineReceiveVideo::Start() {
  mConduit->StartReceiving();
  if (mListener) {
    mListener->AddSelf();
  }
}

void MediaPipelineReceiveVideo::Stop() {
  if (mListener) {
    mListener->RemoveSelf();
  }
  mConduit->StopReceiving();
}

void MediaPipelineReceiveVideo::OnRtpPacketReceived() {
  if (mListener) {
    mListener->OnRtpReceived();
  }
}

DOMHighResTimeStamp MediaPipeline::GetNow() {
  return webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();
}

DOMHighResTimeStamp MediaPipeline::RtpCSRCStats::GetExpiryFromTime(
    const DOMHighResTimeStamp aTime) {
  // DOMHighResTimeStamp is a unit measured in ms
  return aTime - EXPIRY_TIME_MILLISECONDS;
}

MediaPipeline::RtpCSRCStats::RtpCSRCStats(const uint32_t aCsrc,
                                          const DOMHighResTimeStamp aTime)
    : mCsrc(aCsrc), mTimestamp(aTime) {}

void MediaPipeline::RtpCSRCStats::GetWebidlInstance(
    dom::RTCRTPContributingSourceStats& aWebidlObj,
    const nsString& aInboundRtpStreamId) const {
  nsString statId = NS_LITERAL_STRING("csrc_") + aInboundRtpStreamId;
  statId.AppendLiteral("_");
  statId.AppendInt(mCsrc);
  aWebidlObj.mId.Construct(statId);
  aWebidlObj.mType.Construct(RTCStatsType::Csrc);
  aWebidlObj.mTimestamp.Construct(mTimestamp);
  aWebidlObj.mContributorSsrc.Construct(mCsrc);
  aWebidlObj.mInboundRtpStreamId.Construct(aInboundRtpStreamId);
}

}  // namespace mozilla
