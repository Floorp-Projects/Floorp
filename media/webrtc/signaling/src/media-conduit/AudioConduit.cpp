/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"
#include "nspr.h"

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#elif defined XP_WIN
#  include <winsock2.h>
#endif

#include "AudioConduit.h"
#include "nsCOMPtr.h"
#include "mozilla/media/MediaUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsThreadUtils.h"
#include "mozilla/Telemetry.h"
#include "mtransport/runnable_utils.h"

#include "pk11pub.h"

#include "webrtc/modules/audio_coding/codecs/builtin_audio_decoder_factory.h"
#include "webrtc/modules/audio_coding/codecs/builtin_audio_encoder_factory.h"

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/voice_engine_impl.h"
#include "webrtc/system_wrappers/include/clock.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidBridge.h"
#endif

namespace mozilla {

static const char* acLogTag = "WebrtcAudioSessionConduit";
#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG acLogTag

// 32 bytes is what WebRTC CodecInst expects
const unsigned int WebrtcAudioConduit::CODEC_PLNAME_SIZE = 32;

using LocalDirection = MediaSessionConduitLocalDirection;
/**
 * Factory Method for AudioConduit
 */
RefPtr<AudioSessionConduit> AudioSessionConduit::Create(
    RefPtr<WebRtcCallWrapper> aCall, nsCOMPtr<nsIEventTarget> aStsThread) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());

  WebrtcAudioConduit* obj = new WebrtcAudioConduit(aCall, aStsThread);
  if (obj->Init() != kMediaConduitNoError) {
    CSFLogError(LOGTAG, "%s AudioConduit Init Failed ", __FUNCTION__);
    delete obj;
    return nullptr;
  }
  CSFLogDebug(LOGTAG, "%s Successfully created AudioConduit ", __FUNCTION__);
  return obj;
}

/**
 * Destruction defines for our super-classes
 */
WebrtcAudioConduit::~WebrtcAudioConduit() {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mMutex);
  DeleteSendStream();
  DeleteRecvStream();

  DeleteChannels();

  // We don't Terminate() the VoEBase here, because the Call (owned by
  // PeerConnectionMedia) actually owns the (shared) VoEBase/VoiceEngine
  // here
  mPtrVoEBase = nullptr;
}

bool WebrtcAudioConduit::SetLocalSSRCs(
    const std::vector<unsigned int>& aSSRCs) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSSRCs.size() == 1,
             "WebrtcAudioConduit::SetLocalSSRCs accepts exactly 1 ssrc.");

  if (aSSRCs.empty()) {
    return false;
  }

  // Special case: the local SSRCs are the same - do nothing.
  if (mSendStreamConfig.rtp.ssrc == aSSRCs[0]) {
    return true;
  }
  // Update the value of the ssrcs in the config structure.
  mRecvStreamConfig.rtp.local_ssrc = aSSRCs[0];
  mSendStreamConfig.rtp.ssrc = aSSRCs[0];

  mRecvChannelProxy->SetLocalSSRC(aSSRCs[0]);

  return RecreateSendStreamIfExists();
}

std::vector<unsigned int> WebrtcAudioConduit::GetLocalSSRCs() {
  MutexAutoLock lock(mMutex);
  return std::vector<unsigned int>(1, mRecvStreamConfig.rtp.local_ssrc);
}

bool WebrtcAudioConduit::SetRemoteSSRC(unsigned int ssrc) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mRecvStreamConfig.rtp.remote_ssrc == ssrc) {
    return true;
  }
  mRecvStreamConfig.rtp.remote_ssrc = ssrc;

  return RecreateRecvStreamIfExists();
}

bool WebrtcAudioConduit::GetRemoteSSRC(unsigned int* ssrc) {
  {
    MutexAutoLock lock(mMutex);
    if (!mRecvStream) {
      return false;
    }

    const webrtc::AudioReceiveStream::Stats& stats = mRecvStream->GetStats();
    *ssrc = stats.remote_ssrc;
  }

  return true;
}

bool WebrtcAudioConduit::SetLocalCNAME(const char* cname) {
  MOZ_ASSERT(NS_IsMainThread());
  mSendChannelProxy->SetRTCP_CNAME(cname);
  return true;
}

bool WebrtcAudioConduit::SetLocalMID(const std::string& mid) {
  MOZ_ASSERT(NS_IsMainThread());
  mSendChannelProxy->SetLocalMID(mid.c_str());
  return true;
}

void WebrtcAudioConduit::SetSyncGroup(const std::string& group) {
  MOZ_ASSERT(NS_IsMainThread());
  mRecvStreamConfig.sync_group = group;
}

bool WebrtcAudioConduit::GetSendPacketTypeStats(
    webrtc::RtcpPacketTypeCounter* aPacketCounts) {
  ASSERT_ON_THREAD(mStsThread);
  if (!mSendStream) {
    return false;
  }
  return mSendChannelProxy->GetRTCPPacketTypeCounters(*aPacketCounts);
}

bool WebrtcAudioConduit::GetRecvPacketTypeStats(
    webrtc::RtcpPacketTypeCounter* aPacketCounts) {
  ASSERT_ON_THREAD(mStsThread);
  if (!mEngineReceiving) {
    return false;
  }
  return mRecvChannelProxy->GetRTCPPacketTypeCounters(*aPacketCounts);
}

bool WebrtcAudioConduit::GetRTPReceiverStats(unsigned int* jitterMs,
                                             unsigned int* cumulativeLost) {
  ASSERT_ON_THREAD(mStsThread);
  *jitterMs = 0;
  *cumulativeLost = 0;
  if (!mRecvStream) {
    return false;
  }
  auto stats = mRecvStream->GetStats();
  *jitterMs = stats.jitter_ms;
  *cumulativeLost = stats.packets_lost;
  return true;
}

bool WebrtcAudioConduit::GetRTCPReceiverReport(uint32_t* jitterMs,
                                               uint32_t* packetsReceived,
                                               uint64_t* bytesReceived,
                                               uint32_t* cumulativeLost,
                                               Maybe<double>* aOutRttSec) {
  ASSERT_ON_THREAD(mStsThread);
  double fractionLost = 0.0;
  int64_t timestampTmp = 0;
  int64_t rttMsTmp = 0;
  bool res = false;
  if (mSendChannelProxy) {
    res = mSendChannelProxy->GetRTCPReceiverStatistics(
        &timestampTmp, jitterMs, cumulativeLost, packetsReceived, bytesReceived,
        &fractionLost, &rttMsTmp);
  }

  const auto stats = mCall->Call()->GetStats();
  const auto rtt = stats.rtt_ms;
  if (rtt > static_cast<decltype(stats.rtt_ms)>(INT32_MAX)) {
    // If we get a bogus RTT we will keep using the previous RTT
#ifdef DEBUG
    CSFLogError(LOGTAG,
                "%s for AudioConduit:%p RTT is larger than the"
                " maximum size of an RTCP RTT.",
                __FUNCTION__, this);
#endif
  } else {
    if (mRttSec && rtt < 0) {
      CSFLogError(LOGTAG,
                  "%s for AudioConduit:%p RTT returned an error after "
                  " previously succeeding.",
                  __FUNCTION__, this);
      mRttSec = Nothing();
    }
    if (rtt >= 0) {
      mRttSec = Some(static_cast<DOMHighResTimeStamp>(rtt) / 1000.0);
    }
  }
  *aOutRttSec = mRttSec;
  return res;
}

bool WebrtcAudioConduit::GetRTCPSenderReport(unsigned int* packetsSent,
                                             uint64_t* bytesSent) {
  ASSERT_ON_THREAD(mStsThread);
  if (!mRecvChannelProxy) {
    return false;
  }

  webrtc::CallStatistics stats = mRecvChannelProxy->GetRTCPStatistics();
  *packetsSent = stats.rtcp_sender_packets_sent;
  *bytesSent = stats.rtcp_sender_octets_sent;
  return *packetsSent > 0 && *bytesSent > 0;
}

bool WebrtcAudioConduit::SetDtmfPayloadType(unsigned char type, int freq) {
  CSFLogInfo(LOGTAG, "%s : setting dtmf payload %d", __FUNCTION__, (int)type);
  MOZ_ASSERT(NS_IsMainThread());

  int result = mSendChannelProxy->SetSendTelephoneEventPayloadType(type, freq);
  if (result == -1) {
    CSFLogError(LOGTAG,
                "%s Failed call to SetSendTelephoneEventPayloadType(%u, %d)",
                __FUNCTION__, type, freq);
  }
  return result != -1;
}

bool WebrtcAudioConduit::InsertDTMFTone(int channel, int eventCode,
                                        bool outOfBand, int lengthMs,
                                        int attenuationDb) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mSendChannelProxy || !mDtmfEnabled) {
    return false;
  }

  int result = 0;
  if (outOfBand) {
    result = mSendChannelProxy->SendTelephoneEventOutband(eventCode, lengthMs);
  }
  return result != -1;
}

void WebrtcAudioConduit::OnRtpPacket(const webrtc::RTPHeader& aHeader,
                                     const int64_t aTimestamp,
                                     const uint32_t aJitter) {
  ASSERT_ON_THREAD(mStsThread);
  mRtpSourceObserver.OnRtpPacket(aHeader, aTimestamp, aJitter);
}

void WebrtcAudioConduit::GetRtpSources(
    const int64_t aTimeNow, nsTArray<dom::RTCRtpSourceEntry>& outSources) {
  MOZ_ASSERT(NS_IsMainThread());
  return mRtpSourceObserver.GetRtpSources(aTimeNow, outSources);
}

// test-only: inserts a CSRC entry in a RtpSourceObserver's history for
// getContributingSources mochitests
void InsertAudioLevelForContributingSource(RtpSourceObserver& observer,
                                           uint32_t aCsrcSource,
                                           int64_t aTimestamp,
                                           bool aHasAudioLevel,
                                           uint8_t aAudioLevel) {
  using EntryType = dom::RTCRtpSourceEntryType;
  auto key = RtpSourceObserver::GetKey(aCsrcSource, EntryType::Contributing);
  auto& hist = observer.mRtpSources[key];
  hist.Insert(aTimestamp, aTimestamp, aHasAudioLevel, aAudioLevel);
}

void WebrtcAudioConduit::InsertAudioLevelForContributingSource(
    uint32_t aCsrcSource, int64_t aTimestamp, bool aHasAudioLevel,
    uint8_t aAudioLevel) {
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::InsertAudioLevelForContributingSource(
      mRtpSourceObserver, aCsrcSource, aTimestamp, aHasAudioLevel, aAudioLevel);
}

/*
 * WebRTCAudioConduit Implementation
 */
MediaConduitErrorCode WebrtcAudioConduit::Init() {
  CSFLogDebug(LOGTAG, "%s this=%p", __FUNCTION__, this);
  MOZ_ASSERT(NS_IsMainThread());

  if (!(mPtrVoEBase = webrtc::VoEBase::GetInterface(GetVoiceEngine()))) {
    CSFLogError(LOGTAG, "%s Unable to initialize VoEBase", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  CreateChannels();

  CSFLogDebug(LOGTAG, "%s AudioSessionConduit Initialization Done (%p)",
              __FUNCTION__, this);
  return kMediaConduitNoError;
}

// AudioSessionConduit Implementation
MediaConduitErrorCode WebrtcAudioConduit::SetTransmitterTransport(
    RefPtr<TransportInterface> aTransport) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mTransmitterTransport = aTransport;
  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::SetReceiverTransport(
    RefPtr<TransportInterface> aTransport) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mReceiverTransport = aTransport;
  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::ConfigureSendMediaCodec(
    const AudioCodecConfig* codecConfig) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());

  MediaConduitErrorCode condError = kMediaConduitNoError;

  {
    // validate codec param
    if ((condError = ValidateCodecConfig(codecConfig, true)) !=
        kMediaConduitNoError) {
      return condError;
    }
  }

  condError = StopTransmitting();
  if (condError != kMediaConduitNoError) {
    return condError;
  }

  if (!CodecConfigToWebRTCCodec(codecConfig, mSendStreamConfig)) {
    CSFLogError(LOGTAG, "%s CodecConfig to WebRTC Codec Failed ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  mDtmfEnabled = codecConfig->mDtmfEnabled;

  condError = StartTransmitting();
  if (condError != kMediaConduitNoError) {
    return condError;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::ConfigureRecvMediaCodecs(
    const std::vector<UniquePtr<AudioCodecConfig>>& codecConfigList) {
  MOZ_ASSERT(NS_IsMainThread());

  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  MediaConduitErrorCode condError = kMediaConduitNoError;
  bool success = false;

  // Are we receiving already? If so, stop receiving and playout
  // since we can't apply new recv codec when the engine is playing.
  condError = StopReceiving();
  if (condError != kMediaConduitNoError) {
    return condError;
  }

  if (codecConfigList.empty()) {
    CSFLogError(LOGTAG, "%s Zero number of codecs to configure", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  // Try Applying the codecs in the list.
  // We succeed if at least one codec was applied and reception was
  // started successfully.
  mRecvStreamConfig.decoder_factory = mCall->mDecoderFactory;
  mRecvStreamConfig.decoder_map.clear();
  for (const auto& codec : codecConfigList) {
    // if the codec param is invalid or diplicate, return error
    if ((condError = ValidateCodecConfig(codec.get(), false)) !=
        kMediaConduitNoError) {
      return condError;
    }

    webrtc::SdpAudioFormat::Parameters parameters;
    if (codec->mName == "opus") {
      if (codec->mChannels == 2) {
        parameters = {{"stereo", "1"}};
      }
      if (codec->mFECEnabled) {
        parameters["useinbandfec"] = "1";
      }
      if (codec->mMaxPlaybackRate) {
        std::ostringstream o;
        o << codec->mMaxPlaybackRate;
        parameters["maxplaybackrate"] = o.str();
      }
    }

    webrtc::SdpAudioFormat format(codec->mName, codec->mFreq, codec->mChannels,
                                  parameters);
    mRecvStreamConfig.decoder_map.emplace(codec->mType, format);

    mRecvStreamConfig.voe_channel_id = mRecvChannel;
    success = true;
  }  // end for

  mRecvSSRC = mRecvStreamConfig.rtp.remote_ssrc;

  if (!success) {
    CSFLogError(LOGTAG, "%s Setting Receive Codec Failed ", __FUNCTION__);
    return kMediaConduitInvalidReceiveCodec;
  }

  // If we are here, at least one codec should have been set
  {
    MutexAutoLock lock(mMutex);
    DeleteRecvStream();
    condError = StartReceivingLocked();
    if (condError != kMediaConduitNoError) {
      return condError;
    }
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::SetLocalRTPExtensions(
    LocalDirection aDirection, const RtpExtList& extensions) {
  MOZ_ASSERT(NS_IsMainThread());
  CSFLogDebug(LOGTAG, "%s direction: %s", __FUNCTION__,
              MediaSessionConduit::LocalDirectionToString(aDirection).c_str());

  bool isSend = aDirection == LocalDirection::kSend;
  RtpExtList filteredExtensions;

  int ssrcAudioLevelId = -1;
  int csrcAudioLevelId = -1;
  int midId = -1;

  for (const auto& extension : extensions) {
    // ssrc-audio-level RTP header extension
    if (extension.uri == webrtc::RtpExtension::kAudioLevelUri) {
      ssrcAudioLevelId = extension.id;
      filteredExtensions.push_back(
          webrtc::RtpExtension(extension.uri, extension.id));
    }

    // csrc-audio-level RTP header extension
    if (extension.uri == webrtc::RtpExtension::kCsrcAudioLevelUri) {
      if (isSend) {
        CSFLogError(LOGTAG,
                    "%s SetSendAudioLevelIndicationStatus Failed"
                    " can not send CSRC audio levels.",
                    __FUNCTION__);
        return kMediaConduitMalformedArgument;
      }
      csrcAudioLevelId = extension.id;
      filteredExtensions.push_back(
          webrtc::RtpExtension(extension.uri, extension.id));
    }

    // MID RTP header extension
    if (extension.uri == webrtc::RtpExtension::kMIdUri) {
      if (!isSend) {
        // TODO(bug 1405495): Why do we error out for csrc-audio-level, but not
        // mid?
        continue;
      }
      midId = extension.id;
      filteredExtensions.push_back(
          webrtc::RtpExtension(extension.uri, extension.id));
    }
  }

  auto& currentExtensions = isSend ? mSendStreamConfig.rtp.extensions
                                   : mRecvStreamConfig.rtp.extensions;
  if (filteredExtensions == currentExtensions) {
    return kMediaConduitNoError;
  }

  currentExtensions = filteredExtensions;

  if (isSend) {
    mSendChannelProxy->SetSendAudioLevelIndicationStatus(ssrcAudioLevelId != -1,
                                                         ssrcAudioLevelId);
    mSendChannelProxy->SetSendMIDStatus(midId != -1, midId);
  } else {
    mRecvChannelProxy->SetReceiveAudioLevelIndicationStatus(
        ssrcAudioLevelId != -1, ssrcAudioLevelId);
    mRecvChannelProxy->SetReceiveCsrcAudioLevelIndicationStatus(
        csrcAudioLevelId != -1, csrcAudioLevelId);
    // TODO(bug 1405495): recv mid support
  }

  if (isSend) {
    RecreateSendStreamIfExists();
  } else {
    RecreateRecvStreamIfExists();
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::SendAudioFrame(
    const int16_t audio_data[],
    int32_t lengthSamples,  // per channel
    int32_t samplingFreqHz, uint32_t channels, int32_t capture_delay) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  // Following checks need to be performed
  // 1. Non null audio buffer pointer,
  // 2. invalid sampling frequency -  less than 0 or unsupported ones
  // 3. Appropriate Sample Length for 10 ms audio-frame. This represents
  //    block size the VoiceEngine feeds into encoder for passed in audio-frame
  //    Ex: for 16000 sampling rate , valid block-length is 160
  //    Similarly for 32000 sampling rate, valid block length is 320
  //    We do the check by the verify modular operator below to be zero

  if (!audio_data || (lengthSamples <= 0) ||
      (IsSamplingFreqSupported(samplingFreqHz) == false) ||
      ((lengthSamples % (samplingFreqHz / 100) != 0))) {
    CSFLogError(LOGTAG, "%s Invalid Parameters ", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  // validate capture time
  if (capture_delay < 0) {
    CSFLogError(LOGTAG, "%s Invalid Capture Delay ", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  // if transmission is not started .. conduit cannot insert frames
  if (!mEngineTransmitting) {
    CSFLogError(LOGTAG, "%s Engine not transmitting ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  // Insert the samples
  mPtrVoEBase->audio_transport()->PushCaptureData(
      mSendChannel, audio_data,
      sizeof(audio_data[0]) * 8,  // bits
      samplingFreqHz, channels, lengthSamples);
  // we should be good here
  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::GetAudioFrame(int16_t speechData[],
                                                        int32_t samplingFreqHz,
                                                        int32_t capture_delay,
                                                        int& lengthSamples) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);

  // validate params
  if (!speechData) {
    CSFLogError(LOGTAG, "%s Null Audio Buffer Pointer", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  // Validate sample length
  if (GetNum10msSamplesForFrequency(samplingFreqHz) == 0) {
    CSFLogError(LOGTAG, "%s Invalid Sampling Frequency ", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  // validate capture time
  if (capture_delay < 0) {
    CSFLogError(LOGTAG, "%s Invalid Capture Delay ", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  // Conduit should have reception enabled before we ask for decoded
  // samples
  if (!mEngineReceiving) {
    CSFLogError(LOGTAG, "%s Engine not Receiving ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  int lengthSamplesAllowed = lengthSamples;
  lengthSamples = 0;  // output paramter

  mRecvChannelProxy->GetAudioFrameWithInfo(samplingFreqHz, &mAudioFrame);
  // XXX Annoying, have to copy to our buffers -- refactor?
  lengthSamples = mAudioFrame.samples_per_channel_ * mAudioFrame.num_channels_;
  MOZ_RELEASE_ASSERT(lengthSamples <= lengthSamplesAllowed);
  PodCopy(speechData, mAudioFrame.data(), lengthSamples);

  CSFLogDebug(LOGTAG, "%s GetAudioFrame:Got samples: length %d ", __FUNCTION__,
              lengthSamples);
  return kMediaConduitNoError;
}

// Transport Layer Callbacks
MediaConduitErrorCode WebrtcAudioConduit::ReceivedRTPPacket(const void* data,
                                                            int len,
                                                            uint32_t ssrc) {
  ASSERT_ON_THREAD(mStsThread);

  // Handle the unknown ssrc (and ssrc-not-signaled case).
  // We can't just do this here; it has to happen on MainThread :-(
  // We also don't want to drop the packet, nor stall this thread, so we hold
  // the packet (and any following) for inserting once the SSRC is set.

  // capture packet for insertion after ssrc is set -- do this before
  // sending the runnable, since it may pull from this.  Since it
  // dispatches back to us, it's less critial to do this here, but doesn't
  // hurt.
  if (mRtpPacketQueue.IsQueueActive()) {
    mRtpPacketQueue.Enqueue(data, len);
    return kMediaConduitNoError;
  }

  if (mRecvSSRC != ssrc) {
    // a new switch needs to be done
    // any queued packets are from a previous switch that hasn't completed
    // yet; drop them and only process the latest SSRC
    mRtpPacketQueue.Clear();
    mRtpPacketQueue.Enqueue(data, len);

    CSFLogDebug(LOGTAG, "%s: switching from SSRC %u to %u", __FUNCTION__,
                static_cast<uint32_t>(mRecvSSRC), ssrc);

    // we "switch" here immediately, but buffer until the queue is released
    mRecvSSRC = ssrc;

    // Ensure lamba captures refs
    RefPtr<WebrtcAudioConduit> self = this;
    nsCOMPtr<nsIThread> thread;
    if (NS_WARN_IF(NS_FAILED(NS_GetCurrentThread(getter_AddRefs(thread))))) {
      return kMediaConduitRTPProcessingFailed;
    }
    NS_DispatchToMainThread(
        media::NewRunnableFrom([self, thread, ssrc]() mutable {
          self->SetRemoteSSRC(ssrc);
          // We want to unblock the queued packets on the original thread
          thread->Dispatch(media::NewRunnableFrom([self, ssrc]() mutable {
                             if (ssrc == self->mRecvSSRC) {
                               // SSRC is set; insert queued packets
                               self->mRtpPacketQueue.DequeueAll(self);
                             }
                             // else this is an intermediate switch; another is
                             // in-flight
                             return NS_OK;
                           }),
                           NS_DISPATCH_NORMAL);
          return NS_OK;
        }));
    return kMediaConduitNoError;
  }

  CSFLogVerbose(LOGTAG, "%s: seq# %u, Len %d, SSRC %u (0x%x) ", __FUNCTION__,
                (uint16_t)ntohs(((uint16_t*)data)[1]), len,
                (uint32_t)ntohl(((uint32_t*)data)[2]),
                (uint32_t)ntohl(((uint32_t*)data)[2]));

  if (DeliverPacket(data, len) != kMediaConduitNoError) {
    CSFLogError(LOGTAG, "%s RTP Processing Failed", __FUNCTION__);
    return kMediaConduitRTPProcessingFailed;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::ReceivedRTCPPacket(const void* data,
                                                             int len) {
  CSFLogDebug(LOGTAG, "%s : channel %d", __FUNCTION__, mRecvChannel);
  ASSERT_ON_THREAD(mStsThread);

  if (DeliverPacket(data, len) != kMediaConduitNoError) {
    CSFLogError(LOGTAG, "%s RTCP Processing Failed", __FUNCTION__);
    return kMediaConduitRTPProcessingFailed;
  }
  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::StopTransmitting() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  return StopTransmittingLocked();
}

MediaConduitErrorCode WebrtcAudioConduit::StartTransmitting() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  return StartTransmittingLocked();
}

MediaConduitErrorCode WebrtcAudioConduit::StopReceiving() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  return StopReceivingLocked();
}

MediaConduitErrorCode WebrtcAudioConduit::StartReceiving() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  return StartReceivingLocked();
}

MediaConduitErrorCode WebrtcAudioConduit::StopTransmittingLocked() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mEngineTransmitting) {
    MOZ_ASSERT(mSendStream);
    CSFLogDebug(LOGTAG, "%s Engine Already Sending. Attemping to Stop ",
                __FUNCTION__);
    mSendStream->Stop();
    mEngineTransmitting = false;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::StartTransmittingLocked() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mEngineTransmitting) {
    return kMediaConduitNoError;
  }

  if (!mSendStream) {
    CreateSendStream();
  }

  mCall->Call()->SignalChannelNetworkState(webrtc::MediaType::AUDIO,
                                           webrtc::kNetworkUp);
  mSendStream->Start();
  mEngineTransmitting = true;

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::StopReceivingLocked() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mEngineReceiving) {
    MOZ_ASSERT(mRecvStream);
    mRecvStream->Stop();
    mEngineReceiving = false;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::StartReceivingLocked() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mEngineReceiving) {
    return kMediaConduitNoError;
  }

  if (!mRecvStream) {
    CreateRecvStream();
  }

  mCall->Call()->SignalChannelNetworkState(webrtc::MediaType::AUDIO,
                                           webrtc::kNetworkUp);
  mRecvStream->Start();
  mEngineReceiving = true;

  return kMediaConduitNoError;
}

// WebRTC::RTP Callback Implementation
// Called on AudioGUM or MSG thread
bool WebrtcAudioConduit::SendRtp(const uint8_t* data, size_t len,
                                 const webrtc::PacketOptions& options) {
  CSFLogDebug(LOGTAG, "%s: len %lu", __FUNCTION__, (unsigned long)len);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // XXX(pkerr) - the PacketOptions are being ignored. This parameter was added
  // along with the Call API update in the webrtc.org codebase. The only field
  // in it is the packet_id, which is used when the header extension for
  // TransportSequenceNumber is being used, which we don't.
  (void)options;
  if (mTransmitterTransport &&
      (mTransmitterTransport->SendRtpPacket(data, len) == NS_OK)) {
    CSFLogDebug(LOGTAG, "%s Sent RTP Packet ", __FUNCTION__);
    return true;
  }
  CSFLogError(LOGTAG, "%s RTP Packet Send Failed ", __FUNCTION__);
  return false;
}

// Called on WebRTC Process thread and perhaps others
bool WebrtcAudioConduit::SendRtcp(const uint8_t* data, size_t len) {
  CSFLogDebug(LOGTAG, "%s : len %lu, first rtcp = %u ", __FUNCTION__,
              (unsigned long)len, static_cast<unsigned>(data[1]));

  // We come here if we have only one pipeline/conduit setup,
  // such as for unidirectional streams.
  // We also end up here if we are receiving
  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if (mReceiverTransport &&
      mReceiverTransport->SendRtcpPacket(data, len) == NS_OK) {
    // Might be a sender report, might be a receiver report, we don't know.
    CSFLogDebug(LOGTAG, "%s Sent RTCP Packet ", __FUNCTION__);
    return true;
  }
  if (mTransmitterTransport &&
      (mTransmitterTransport->SendRtcpPacket(data, len) == NS_OK)) {
    CSFLogDebug(LOGTAG, "%s Sent RTCP Packet (sender report) ", __FUNCTION__);
    return true;
  }
  CSFLogError(LOGTAG, "%s RTCP Packet Send Failed ", __FUNCTION__);
  return false;
}

/**
 * Converts between CodecConfig to WebRTC Codec Structure.
 */

bool WebrtcAudioConduit::CodecConfigToWebRTCCodec(
    const AudioCodecConfig* codecInfo,
    webrtc::AudioSendStream::Config& config) {
  config.encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();

  webrtc::SdpAudioFormat::Parameters parameters;
  if (codecInfo->mName == "opus") {
    if (codecInfo->mChannels == 2) {
      parameters["stereo"] = "1";
    }
    if (codecInfo->mFECEnabled) {
      parameters["useinbandfec"] = "1";
    }
    if (codecInfo->mMaxPlaybackRate) {
      std::ostringstream o;
      o << codecInfo->mMaxPlaybackRate;
      parameters["maxplaybackrate"] = o.str();
    }
  }

  webrtc::SdpAudioFormat format(codecInfo->mName, codecInfo->mFreq,
                                codecInfo->mChannels, parameters);
  webrtc::AudioSendStream::Config::SendCodecSpec spec(codecInfo->mType, format);
  config.send_codec_spec = spec;

  return true;
}

/**
 *  Supported Sampling Frequencies.
 */
bool WebrtcAudioConduit::IsSamplingFreqSupported(int freq) const {
  return GetNum10msSamplesForFrequency(freq) != 0;
}

/* Return block-length of 10 ms audio frame in number of samples */
unsigned int WebrtcAudioConduit::GetNum10msSamplesForFrequency(
    int samplingFreqHz) const {
  switch (samplingFreqHz) {
    case 16000:
      return 160;  // 160 samples
    case 32000:
      return 320;  // 320 samples
    case 44100:
      return 441;  // 441 samples
    case 48000:
      return 480;  // 480 samples
    default:
      return 0;  // invalid or unsupported
  }
}

/**
 * Perform validation on the codecConfig to be applied.
 * Verifies if the codec is already applied.
 */
MediaConduitErrorCode WebrtcAudioConduit::ValidateCodecConfig(
    const AudioCodecConfig* codecInfo, bool send) {
  if (!codecInfo) {
    CSFLogError(LOGTAG, "%s Null CodecConfig ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  if ((codecInfo->mName.empty()) ||
      (codecInfo->mName.length() >= CODEC_PLNAME_SIZE)) {
    CSFLogError(LOGTAG, "%s Invalid Payload Name Length ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  // Only mono or stereo channels supported
  if ((codecInfo->mChannels != 1) && (codecInfo->mChannels != 2)) {
    CSFLogError(LOGTAG, "%s Channel Unsupported ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  return kMediaConduitNoError;
}

void WebrtcAudioConduit::DeleteSendStream() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();
  if (mSendStream) {
    mSendStream->Stop();
    mEngineTransmitting = false;
    mCall->Call()->DestroyAudioSendStream(mSendStream);
    mSendStream = nullptr;
  }
  // Destroying the stream unregisters the transport
  mSendChannelProxy->RegisterTransport(nullptr);
}

MediaConduitErrorCode WebrtcAudioConduit::CreateSendStream() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  mSendStream = mCall->Call()->CreateAudioSendStream(mSendStreamConfig);
  if (!mSendStream) {
    return kMediaConduitUnknownError;
  }

  return kMediaConduitNoError;
}

void WebrtcAudioConduit::DeleteRecvStream() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();
  if (mRecvStream) {
    mRecvStream->Stop();
    mEngineReceiving = false;
    mCall->Call()->DestroyAudioReceiveStream(mRecvStream);
    mRecvStream = nullptr;
  }
  // Destroying the stream unregisters the transport
  mRecvChannelProxy->RegisterTransport(nullptr);
}

MediaConduitErrorCode WebrtcAudioConduit::CreateRecvStream() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  mRecvStreamConfig.rtcp_send_transport = this;
  mRecvStream = mCall->Call()->CreateAudioReceiveStream(mRecvStreamConfig);
  if (!mRecvStream) {
    return kMediaConduitUnknownError;
  }

  return kMediaConduitNoError;
}

bool WebrtcAudioConduit::RecreateSendStreamIfExists() {
  MutexAutoLock lock(mMutex);
  bool wasTransmitting = mEngineTransmitting;
  bool hadSendStream = mSendStream;
  DeleteSendStream();

  if (wasTransmitting) {
    if (StartTransmittingLocked() != kMediaConduitNoError) {
      return false;
    }
  } else if (hadSendStream) {
    if (CreateSendStream() != kMediaConduitNoError) {
      return false;
    }
  }
  return true;
}

bool WebrtcAudioConduit::RecreateRecvStreamIfExists() {
  MutexAutoLock lock(mMutex);
  bool wasReceiving = mEngineReceiving;
  bool hadRecvStream = mRecvStream;
  DeleteRecvStream();

  if (wasReceiving) {
    if (StartReceivingLocked() != kMediaConduitNoError) {
      return false;
    }
  } else if (hadRecvStream) {
    if (CreateRecvStream() != kMediaConduitNoError) {
      return false;
    }
  }
  return true;
}

MediaConduitErrorCode WebrtcAudioConduit::DeliverPacket(const void* data,
                                                        int len) {
  // Bug 1499796 - we need to get passed the time the packet was received
  webrtc::PacketReceiver::DeliveryStatus status =
      mCall->Call()->Receiver()->DeliverPacket(
          webrtc::MediaType::AUDIO, static_cast<const uint8_t*>(data), len,
          webrtc::PacketTime());

  if (status != webrtc::PacketReceiver::DELIVERY_OK) {
    CSFLogError(LOGTAG, "%s DeliverPacket Failed, %d", __FUNCTION__, status);
    return kMediaConduitRTPProcessingFailed;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::CreateChannels() {
  MOZ_ASSERT(NS_IsMainThread());

  if ((mRecvChannel = mPtrVoEBase->CreateChannel()) == -1) {
    CSFLogError(LOGTAG, "%s VoiceEngine Channel creation failed", __FUNCTION__);
    return kMediaConduitChannelError;
  }
  mRecvStreamConfig.voe_channel_id = mRecvChannel;

  if ((mSendChannel = mPtrVoEBase->CreateChannel()) == -1) {
    CSFLogError(LOGTAG, "%s VoiceEngine Channel creation failed", __FUNCTION__);
    return kMediaConduitChannelError;
  }
  mSendStreamConfig.voe_channel_id = mSendChannel;

  webrtc::VoiceEngineImpl* vei;
  vei = static_cast<webrtc::VoiceEngineImpl*>(GetVoiceEngine());
  mRecvChannelProxy = vei->GetChannelProxy(mRecvChannel);
  if (!mRecvChannelProxy) {
    CSFLogError(LOGTAG, "%s VoiceEngine Send ChannelProxy creation failed",
                __FUNCTION__);
    return kMediaConduitChannelError;
  }

  mRecvChannelProxy->SetRtpPacketObserver(this);
  mRecvChannelProxy->RegisterTransport(this);

  mSendChannelProxy = vei->GetChannelProxy(mSendChannel);
  if (!mSendChannelProxy) {
    CSFLogError(LOGTAG, "%s VoiceEngine ChannelProxy creation failed",
                __FUNCTION__);
    return kMediaConduitChannelError;
  }
  mSendChannelProxy->SetRtpPacketObserver(this);
  mSendChannelProxy->RegisterTransport(this);

  return kMediaConduitNoError;
}

void WebrtcAudioConduit::DeleteChannels() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mSendChannel != -1) {
    mSendChannelProxy = nullptr;
    mPtrVoEBase->DeleteChannel(mSendChannel);
    mSendChannel = -1;
  }

  if (mRecvChannel != -1) {
    mRecvChannelProxy = nullptr;
    mPtrVoEBase->DeleteChannel(mRecvChannel);
    mRecvChannel = -1;
  }
}

}  // namespace mozilla
