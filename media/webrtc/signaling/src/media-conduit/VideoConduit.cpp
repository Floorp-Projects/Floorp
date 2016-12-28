/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"
#include "nspr.h"
#include "plstr.h"

#include "AudioConduit.h"
#include "LoadManager.h"
#include "VideoConduit.h"
#include "YuvStamper.h"
#include "mozilla/TemplateLib.h"
#include "mozilla/media/MediaUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIPrefBranch.h"
#include "nsIGfxInfo.h"
#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"

#include "nsThreadUtils.h"

#include "pk11pub.h"

#include "webrtc/common_types.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"

#include "mozilla/Unused.h"

#if defined(MOZ_WIDGET_ANDROID) && defined(MOZILLA_INTERNAL_API)
#include "AndroidJNIWrapper.h"
#include "VideoEngine.h"
#endif

#include "GmpVideoCodec.h"
#ifdef MOZ_WEBRTC_OMX
#include "OMXCodecWrapper.h"
#include "OMXVideoCodec.h"
#endif

#ifdef MOZ_WEBRTC_MEDIACODEC
#include "MediaCodecVideoCodec.h"
#endif
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
#include "WebrtcGmpVideoCodec.h"
#endif

// for ntohs
#ifdef _MSC_VER
#include "Winsock2.h"
#else
#include <netinet/in.h>
#endif

#include <algorithm>
#include <math.h>
#include <cinttypes>

#define DEFAULT_VIDEO_MAX_FRAMERATE 30
#define INVALID_RTP_PAYLOAD 255 // valid payload types are 0 to 127

namespace mozilla {

static const char* logTag = "WebrtcVideoSessionConduit";

static const int kUlpFecPayloadType = 123;
static const int kRedPayloadType = 122;
static const int kNullPayloadType = -1;
static const char* kUlpFecPayloadName = "ulpfec";
static const char* kRedPayloadName = "red";

// Convert (SI) kilobits/sec to (SI) bits/sec
#define KBPS(kbps) kbps * 1000
const uint32_t WebrtcVideoConduit::kDefaultMinBitrate_bps =  KBPS(200);
const uint32_t WebrtcVideoConduit::kDefaultStartBitrate_bps = KBPS(300);
const uint32_t WebrtcVideoConduit::kDefaultMaxBitrate_bps = KBPS(2000);

// 32 bytes is what WebRTC CodecInst expects
const unsigned int WebrtcVideoConduit::CODEC_PLNAME_SIZE = 32;
static const unsigned int kViEMinCodecBitrate = 30;

template<typename T>
T MinIgnoreZero(const T& a, const T& b)
{
  return std::min(a? a:b, b? b:a);
}

void
WebrtcVideoConduit::StreamStatistics::Update(const double aFrameRate,
                                             const double aBitrate)
{
  mFrameRate.Push(aFrameRate);
  mBitrate.Push(aBitrate);
}

bool
WebrtcVideoConduit::StreamStatistics::GetVideoStreamStats(
    double& aOutFrMean, double& aOutFrStdDev, double& aOutBrMean,
    double& aOutBrStdDev) const
{
  if (mFrameRate.NumDataValues() && mBitrate.NumDataValues()) {
    aOutFrMean = mFrameRate.Mean();
    aOutFrStdDev = mFrameRate.StandardDeviation();
    aOutBrMean = mBitrate.Mean();
    aOutBrStdDev = mBitrate.StandardDeviation();
    return true;
  }
  return false;
};

void
WebrtcVideoConduit::SendStreamStatistics::DroppedFrames(
  uint32_t& aOutDroppedFrames) const
{
      aOutDroppedFrames = mDroppedFrames;
};

void
WebrtcVideoConduit::SendStreamStatistics::Update(
  const webrtc::VideoSendStream::Stats& aStats)
{
  CSFLogVerbose(logTag, "SendStreamStatistics::Update %s", __FUNCTION__);
  StreamStatistics::Update(aStats.encode_frame_rate, aStats.media_bitrate_bps);
  if (!aStats.substreams.empty()) {
    const webrtc::FrameCounts& fc =
      aStats.substreams.begin()->second.frame_counts;
    mDroppedFrames = mSentFrames - fc.key_frames + fc.delta_frames;
  } else {
    CSFLogVerbose(logTag, "%s aStats.substreams is empty", __FUNCTION__);
  }
};

void
WebrtcVideoConduit::ReceiveStreamStatistics::DiscardedPackets(
  uint32_t& aOutDiscPackets) const
{
  aOutDiscPackets = mDiscardedPackets;
};

void
WebrtcVideoConduit::ReceiveStreamStatistics::Update(
  const webrtc::VideoReceiveStream::Stats& aStats)
{
  CSFLogVerbose(logTag, "%s ", __FUNCTION__);
  StreamStatistics::Update(aStats.decode_frame_rate, aStats.total_bitrate_bps);
  mDiscardedPackets = aStats.discarded_packets;
};

/**
 * Factory Method for VideoConduit
 */
RefPtr<VideoSessionConduit>
VideoSessionConduit::Create(RefPtr<WebRtcCallWrapper> aCall)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  NS_ASSERTION(aCall, "missing required parameter: aCall");
  CSFLogVerbose(logTag, "%s", __FUNCTION__);

  if (!aCall) {
    return nullptr;
  }

  nsAutoPtr<WebrtcVideoConduit> obj(new WebrtcVideoConduit(aCall));
  if(obj->Init() != kMediaConduitNoError) {
    CSFLogError(logTag, "%s VideoConduit Init Failed ", __FUNCTION__);
    return nullptr;
  }
  CSFLogVerbose(logTag, "%s Successfully created VideoConduit ", __FUNCTION__);
  return obj.forget();
}

WebrtcVideoConduit::WebrtcVideoConduit(RefPtr<WebRtcCallWrapper> aCall)
  : mTransportMonitor("WebrtcVideoConduit")
  , mRenderer(nullptr)
  , mEngineTransmitting(false)
  , mEngineReceiving(false)
  , mCapId(-1)
  , mCodecMutex("VideoConduit codec db")
  , mInReconfig(false)
  , mLastWidth(0)
  , mLastHeight(0) // initializing as 0 forces a check for reconfig at start
  , mSendingWidth(0)
  , mSendingHeight(0)
  , mReceivingWidth(0)
  , mReceivingHeight(0)
  , mSendingFramerate(DEFAULT_VIDEO_MAX_FRAMERATE)
  , mLastFramerateTenths(DEFAULT_VIDEO_MAX_FRAMERATE * 10)
  , mNumReceivingStreams(1)
  , mVideoLatencyTestEnable(false)
  , mVideoLatencyAvg(0)
  , mMinBitrate(0)
  , mStartBitrate(0)
  , mPrefMaxBitrate(0)
  , mNegotiatedMaxBitrate(0)
  , mMinBitrateEstimate(0)
  , mCodecMode(webrtc::kRealtimeVideo)
  , mCall(aCall) // refcounted store of the call object
  , mSendStream(nullptr)
  , mSendStreamConfig(this) // 'this' is stored but not  dereferenced in the constructor.
  , mRecvStream(nullptr)
  , mRecvStreamConfig(this) // 'this' is stored but not  dereferenced in the constructor.
  , mSendCodecPlugin(nullptr)
  , mRecvCodecPlugin(nullptr)
  , mVideoStatsTimer(do_CreateInstance(NS_TIMER_CONTRACTID))
{
  mRecvStreamConfig.renderer = this;

  // Video Stats Callback
  nsTimerCallbackFunc callback = [](nsITimer* aTimer, void* aClosure) {
    CSFLogDebug(logTag, "StreamStats polling scheduled for VideoConduit: %p", aClosure);
    auto self = static_cast<WebrtcVideoConduit*>(aClosure);
    MutexAutoLock lock(self->mCodecMutex);
    if (self->mEngineTransmitting && self->mSendStream) {
      self->mSendStreamStats.Update(self->mSendStream->GetStats());
    }
    if (self->mEngineReceiving && self->mRecvStream) {
      self->mRecvStreamStats.Update(self->mRecvStream->GetStats());
    }
  };
  mVideoStatsTimer->InitWithFuncCallback(
    callback, this, 1000, nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP);
}

WebrtcVideoConduit::~WebrtcVideoConduit()
{
  CSFLogDebug(logTag, "%s ", __FUNCTION__);
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  if (mVideoStatsTimer) {
    CSFLogDebug(logTag, "canceling StreamStats for VideoConduit: %p", this);
    MutexAutoLock lock(mCodecMutex);
    CSFLogDebug(logTag, "StreamStats cancelled for VideoConduit: %p", this);
    mVideoStatsTimer->Cancel();
  }

  // Release AudioConduit first by dropping reference on MainThread, where it expects to be
  SyncTo(nullptr);
  Destroy();
}

void
WebrtcVideoConduit::AddLocalRTPExtensions(
  const std::vector<webrtc::RtpExtension> & aExtensions)
{
  auto& extList = mSendStreamConfig.rtp.extensions;
  std::remove_if(extList.begin(), extList.end(), [&](const webrtc::RtpExtension & i) {
    return std::find(aExtensions.begin(), aExtensions.end(),i) != aExtensions.end();
  });
  extList.insert(extList.end(), aExtensions.begin(), aExtensions.end());
}

std::vector<webrtc::RtpExtension>
WebrtcVideoConduit::GetLocalRTPExtensions() const
{
  return mSendStreamConfig.rtp.extensions;
}

bool WebrtcVideoConduit::SetLocalSSRCs(const std::vector<unsigned int> & aSSRCs)
{
  // Special case: the local SSRCs are the same - do nothing.
  if (mSendStreamConfig.rtp.ssrcs == aSSRCs) {
    return true;
  }

  // Update the value of the ssrcs in the config structure.
  mSendStreamConfig.rtp.ssrcs = aSSRCs;

  bool wasTransmitting = mEngineTransmitting;
  if (StopTransmitting() != kMediaConduitNoError) {
    return false;
  }

  if (wasTransmitting) {
    DeleteSendStream();
    if (StartTransmitting() != kMediaConduitNoError) {
      return false;
    }
  }

  return true;
}

std::vector<unsigned int>
WebrtcVideoConduit::GetLocalSSRCs() const
{
  return mSendStreamConfig.rtp.ssrcs;
}

bool
WebrtcVideoConduit::SetLocalCNAME(const char* cname)
{
  mSendStreamConfig.rtp.c_name = cname;
  return true;
}

MediaConduitErrorCode
WebrtcVideoConduit::ConfigureCodecMode(webrtc::VideoCodecMode mode)
{
  CSFLogVerbose(logTag, "%s ", __FUNCTION__);
  if (mode == webrtc::VideoCodecMode::kRealtimeVideo ||
      mode == webrtc::VideoCodecMode::kScreensharing) {
    mCodecMode = mode;
    return kMediaConduitNoError;
  }

  return kMediaConduitMalformedArgument;
}

webrtc::VideoEncoder::EncoderType
PayloadNameToEncoderType(const std::string& name)
{
  if ("VP8" == name) {
    return webrtc::VideoEncoder::EncoderType::kVp8;
  } else if ("VP9" == name) {
    return webrtc::VideoEncoder::EncoderType::kVp9;
  } else if ("H264" == name) {
    return webrtc::VideoEncoder::EncoderType::kH264;
  }

  return webrtc::VideoEncoder::EncoderType::kUnsupportedCodec;
}

void
WebrtcVideoConduit::DeleteSendStream()
{
  if (mSendStream) {

    if (mLoadManager && mSendStream->LoadStateObserver()) {
      mLoadManager->RemoveObserver(mSendStream->LoadStateObserver());
    }

    mCall->Call()->DestroyVideoSendStream(mSendStream);
    mSendStream = nullptr;
    mEncoder = nullptr;
  }
}

MediaConduitErrorCode
WebrtcVideoConduit::CreateSendStream()
{
  webrtc::VideoEncoder::EncoderType encoder_type =
    PayloadNameToEncoderType(mSendStreamConfig.encoder_settings.payload_name);
  if (encoder_type == webrtc::VideoEncoder::EncoderType::kUnsupportedCodec) {
    return kMediaConduitInvalidSendCodec;
  }

  nsAutoPtr<webrtc::VideoEncoder> encoder(
    CreateEncoder(encoder_type, mEncoderConfig.StreamCount() > 0));
  if (!encoder) {
    return kMediaConduitInvalidSendCodec;
  }

  mSendStreamConfig.encoder_settings.encoder = encoder.get();

  MOZ_ASSERT(mSendStreamConfig.rtp.ssrcs.size() == mEncoderConfig.StreamCount(),
             "Each video substream must have a corresponding ssrc.");

  auto cfg = mEncoderConfig.GenerateConfig();
  if (cfg.streams.empty()) {
    MOZ_CRASH("mEncoderConfig.GenerateConfig().streams.empty() == true, there are no configured streams!");
  }

  mSendStream = mCall->Call()->CreateVideoSendStream(mSendStreamConfig, cfg);

  if (!mSendStream) {
    return kMediaConduitVideoSendStreamError;
  }

  mEncoder = encoder;

  if (mLoadManager && mSendStream->LoadStateObserver()) {
    mLoadManager->AddObserver(mSendStream->LoadStateObserver());
  }

  return kMediaConduitNoError;
}

webrtc::VideoDecoder::DecoderType
PayloadNameToDecoderType(const std::string& name)
{
  if ("VP8" == name) {
    return webrtc::VideoDecoder::DecoderType::kVp8;
  } else if ("VP9" == name) {
    return webrtc::VideoDecoder::DecoderType::kVp9;
  } else if ("H264" == name) {
    return webrtc::VideoDecoder::DecoderType::kH264;
  }

  return webrtc::VideoDecoder::DecoderType::kUnsupportedCodec;
}

void
WebrtcVideoConduit::DeleteRecvStream()
{
  if (mRecvStream) {
    mCall->Call()->DestroyVideoReceiveStream(mRecvStream);
    mRecvStream = nullptr;
    mDecoders.clear();
  }
}

MediaConduitErrorCode
WebrtcVideoConduit::CreateRecvStream()
{
  webrtc::VideoReceiveStream::Decoder decoder_desc;
  std::unique_ptr<webrtc::VideoDecoder> decoder;
  webrtc::VideoDecoder::DecoderType decoder_type;

  for (auto& config : mRecvCodecList) {
    decoder_type = PayloadNameToDecoderType(config->mName);
    if (decoder_type == webrtc::VideoDecoder::DecoderType::kUnsupportedCodec) {
      CSFLogError(logTag, "%s Unknown decoder type: %s", __FUNCTION__,
                  config->mName.c_str());
      continue;
    }

    decoder.reset(CreateDecoder(decoder_type));

    if (!decoder) {
      // This really should never happen unless something went wrong
      // in the negotiation code
      NS_ASSERTION(decoder, "Failed to create video decoder");
      CSFLogError(logTag, "Failed to create decoder of type %s (%d)",
                  config->mName.c_str(), decoder_type);
      // don't stop
      continue;
    }

    decoder_desc.decoder = decoder.get();
    mDecoders.push_back(std::move(decoder));
    decoder_desc.payload_name = config->mName;
    decoder_desc.payload_type = config->mType;
    mRecvStreamConfig.decoders.push_back(decoder_desc);
  }

  mRecvStream = mCall->Call()->CreateVideoReceiveStream(mRecvStreamConfig);

  if (!mRecvStream) {
    mDecoders.clear();
    return kMediaConduitUnknownError;
  }

  return kMediaConduitNoError;
}

/**
 * Note: Setting the send-codec on the Video Engine will restart the encoder,
 * sets up new SSRC and reset RTP_RTCP module with the new codec setting.
 *
 * Note: this is called from MainThread, and the codec settings are read on
 * videoframe delivery threads (i.e in SendVideoFrame().  With
 * renegotiation/reconfiguration, this now needs a lock!  Alternatively
 * changes could be queued until the next frame is delivered using an
 * Atomic pointer and swaps.
 */

MediaConduitErrorCode
WebrtcVideoConduit::ConfigureSendMediaCodec(const VideoCodecConfig* codecConfig)
{
  CSFLogDebug(logTag, "%s for %s", __FUNCTION__,
    codecConfig ? codecConfig->mName.c_str() : "<null>");

  MediaConduitErrorCode condError = kMediaConduitNoError;

  // validate basic params
  if ((condError = ValidateCodecConfig(codecConfig, true)) != kMediaConduitNoError) {
    return condError;
  }

  condError = StopTransmitting();
  if (condError != kMediaConduitNoError) {
    return condError;
  }

  mSendStreamConfig.encoder_settings.payload_name = codecConfig->mName;
  mSendStreamConfig.encoder_settings.payload_type = codecConfig->mType;
  mSendStreamConfig.rtp.rtcp_mode = webrtc::RtcpMode::kCompound;
  mSendStreamConfig.rtp.max_packet_size = kVideoMtu;
  mSendStreamConfig.overuse_callback = mLoadManager.get();

  size_t streamCount = std::min(codecConfig->mSimulcastEncodings.size(),
                                (size_t)webrtc::kMaxSimulcastStreams);
  CSFLogDebug(logTag, "%s for VideoConduit:%p stream count:%d", __FUNCTION__,
              this, static_cast<int>(streamCount));

  mSendingFramerate = 0;
  mEncoderConfig.ClearStreams();

  unsigned short width = 320;
  unsigned short height = 240;
  int max_framerate;
  if (codecConfig->mEncodingConstraints.maxFps > 0) {
    max_framerate = codecConfig->mEncodingConstraints.maxFps;
  } else {
    max_framerate = DEFAULT_VIDEO_MAX_FRAMERATE;
  }

  // width/height will be overridden on the first frame; they must be 'sane' for
  // SetSendCodec()

  if (mSendingWidth != 0) {
    // We're already in a call and are reconfiguring (perhaps due to
    // ReplaceTrack).  Set to match the last frame we sent.

    // We could also set mLastWidth to 0, to force immediate reconfig -
    // more expensive, but perhaps less risk of missing something.  Really
    // on ReplaceTrack we should just call ConfigureCodecMode(), and if the
    // mode changed, we re-configure.
    width = mSendingWidth;
    height = mSendingHeight;
    max_framerate = mSendingFramerate;
  }
  mSendingFramerate = std::max(mSendingFramerate,
                               static_cast<unsigned int>(max_framerate));

  // So we can comply with b=TIAS/b=AS/maxbr=X when input resolution changes
  // XXX not sure this is right!
  // XXX mNegotiatedMaxBitrate = MinIgnoreZero(mPrefMaxBitrate, codecConfig->maxBitrate);

  for (size_t idx = streamCount - 1; streamCount > 0; idx--, streamCount--) {
    webrtc::VideoStream video_stream;
    VideoEncoderConfigBuilder::SimulcastStreamConfig simulcast_config;
    // Stream dimensions must be divisable by 2^(n-1), where n is the number of layers.
    // Each lower resolution layer is 1/2^(n-1) of the size of largest layer,
    // where n is the number of the layer

    // width/height will be overridden on the first frame; they must be 'sane' for
    // SetSendCodec()
    video_stream.width = width >> idx;
    video_stream.height = height >> idx;
    video_stream.max_framerate = max_framerate;
    auto& simulcastEncoding = codecConfig->mSimulcastEncodings[idx];
    // leave vector temporal_layer_thresholds_bps empty
    video_stream.temporal_layer_thresholds_bps.clear();
    video_stream.max_bitrate_bps = MinIgnoreZero(simulcastEncoding.constraints.maxBr,
                                                 kDefaultMaxBitrate_bps);
    video_stream.max_bitrate_bps = MinIgnoreZero((int) mPrefMaxBitrate*1000,
                                                 video_stream.max_bitrate_bps);
    video_stream.min_bitrate_bps = (mMinBitrate ? mMinBitrate : kDefaultMinBitrate_bps);
    if (video_stream.min_bitrate_bps > video_stream.max_bitrate_bps) {
      video_stream.min_bitrate_bps = video_stream.max_bitrate_bps;
    }
    video_stream.target_bitrate_bps = (mStartBitrate ? mStartBitrate : kDefaultStartBitrate_bps);
    if (video_stream.target_bitrate_bps > video_stream.max_bitrate_bps) {
      video_stream.target_bitrate_bps = video_stream.max_bitrate_bps;
    }
    if (video_stream.target_bitrate_bps < video_stream.min_bitrate_bps) {
      video_stream.target_bitrate_bps = video_stream.min_bitrate_bps;
    }

    video_stream.max_qp = kQpMax;
    video_stream.SetRid(simulcastEncoding.rid);
    simulcast_config.jsScaleDownBy = simulcastEncoding.constraints.scaleDownBy;
    simulcast_config.jsMaxBitrate = simulcastEncoding.constraints.maxBr;

    if (codecConfig->mName == "H264") {
      if (codecConfig->mEncodingConstraints.maxMbps > 0) {
        // Not supported yet!
        CSFLogError(logTag, "%s H.264 max_mbps not supported yet", __FUNCTION__);
      }
    }
    mEncoderConfig.AddStream(video_stream, simulcast_config);
  }

  if (codecConfig->mName == "H264") {
#ifdef MOZ_WEBRTC_OMX
    mEncoderConfig.SetResolutionDivisor(16);
#else
    mEncoderConfig.SetResolutionDivisor(1);
#endif
    mEncoderSpecificH264 = webrtc::VideoEncoder::GetDefaultH264Settings();
    mEncoderSpecificH264.profile_byte = codecConfig->mProfile;
    mEncoderSpecificH264.constraints = codecConfig->mConstraints;
    mEncoderSpecificH264.level = codecConfig->mLevel;
    mEncoderSpecificH264.packetizationMode = codecConfig->mPacketizationMode;
    mEncoderSpecificH264.scaleDownBy = codecConfig->mEncodingConstraints.scaleDownBy;

    // XXX parse the encoded SPS/PPS data
    // paranoia
    mEncoderSpecificH264.spsData = nullptr;
    mEncoderSpecificH264.spsLen = 0;
    mEncoderSpecificH264.ppsData = nullptr;
    mEncoderSpecificH264.ppsLen = 0;

    mEncoderConfig.SetEncoderSpecificSettings(&mEncoderSpecificH264);
  } else {
    mEncoderConfig.SetEncoderSpecificSettings(nullptr);
    mEncoderConfig.SetResolutionDivisor(1);
  }

  mEncoderConfig.SetContentType(mCodecMode == webrtc::kRealtimeVideo ?
    webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo :
    webrtc::VideoEncoderConfig::ContentType::kScreen);
  // for the GMP H.264 encoder/decoder!!
  mEncoderConfig.SetMinTransmitBitrateBps(0);

  // See Bug 1297058, enabling FEC when basic NACK is to be enabled in H.264 is problematic
  if (codecConfig->RtcpFbFECIsSet() &&
      !(codecConfig->mName == "H264" && codecConfig->RtcpFbNackIsSet(""))) {
    mSendStreamConfig.rtp.fec.ulpfec_payload_type = kUlpFecPayloadType;
    mSendStreamConfig.rtp.fec.red_payload_type = kRedPayloadType;
    mSendStreamConfig.rtp.fec.red_rtx_payload_type = kNullPayloadType;
  }

  mSendStreamConfig.rtp.nack.rtp_history_ms =
    codecConfig->RtcpFbNackIsSet("") ? 1000 : 0;

  {
    MutexAutoLock lock(mCodecMutex);
    // Copy the applied config for future reference.
    mCurSendCodecConfig = new VideoCodecConfig(*codecConfig);
  }

  return condError;
}

bool
WebrtcVideoConduit::SetRemoteSSRC(unsigned int ssrc)
{
  mRecvStreamConfig.rtp.remote_ssrc = ssrc;
  unsigned int current_ssrc;

  if (!GetRemoteSSRC(&current_ssrc)) {
    return false;
  }

  if (current_ssrc == ssrc || !mEngineReceiving) {
    return true;
  }

  if (StopReceiving() != kMediaConduitNoError) {
    return false;
  }

  DeleteRecvStream();
  return (StartReceiving() == kMediaConduitNoError);
}

bool
WebrtcVideoConduit::GetRemoteSSRC(unsigned int* ssrc)
{
  {
    MutexAutoLock lock(mCodecMutex);
    if (!mRecvStream) {
      return false;
    }

    const webrtc::VideoReceiveStream::Stats& stats = mRecvStream->GetStats();
    *ssrc = stats.ssrc;
  }

  return true;
}

bool
WebrtcVideoConduit::GetVideoEncoderStats(double* framerateMean,
                                         double* framerateStdDev,
                                         double* bitrateMean,
                                         double* bitrateStdDev,
                                         uint32_t* droppedFrames)
{
  {
    MutexAutoLock lock(mCodecMutex);
    if (!mEngineTransmitting || !mSendStream) {
      return false;
    }
    mSendStreamStats.GetVideoStreamStats(*framerateMean, *framerateStdDev,
      *bitrateMean, *bitrateStdDev);
    mSendStreamStats.DroppedFrames(*droppedFrames);
    return true;
  }
}

bool
WebrtcVideoConduit::GetVideoDecoderStats(double* framerateMean,
                                         double* framerateStdDev,
                                         double* bitrateMean,
                                         double* bitrateStdDev,
                                         uint32_t* discardedPackets)
{
  {
    MutexAutoLock lock(mCodecMutex);
    if (!mEngineReceiving || !mRecvStream) {
      return false;
    }
    mRecvStreamStats.GetVideoStreamStats(*framerateMean, *framerateStdDev,
      *bitrateMean, *bitrateStdDev);
    mRecvStreamStats.DiscardedPackets(*discardedPackets);
    return true;
  }
}

bool
WebrtcVideoConduit::GetAVStats(int32_t* jitterBufferDelayMs,
                               int32_t* playoutBufferDelayMs,
                               int32_t* avSyncOffsetMs)
{
  return false;
}

bool
WebrtcVideoConduit::GetRTPStats(unsigned int* jitterMs,
                                unsigned int* cumulativeLost)
{
  CSFLogVerbose(logTag, "%s for VideoConduit:%p", __FUNCTION__, this);
  {
    MutexAutoLock lock(mCodecMutex);
    if (!mRecvStream) {
      return false;
    }

    const webrtc::VideoReceiveStream::Stats& stats = mRecvStream->GetStats();
    *jitterMs = stats.rtcp_stats.jitter;
    *cumulativeLost = stats.rtcp_stats.cumulative_lost;
  }
  return true;
}

bool WebrtcVideoConduit::GetRTCPReceiverReport(DOMHighResTimeStamp* timestamp,
                                               uint32_t* jitterMs,
                                               uint32_t* packetsReceived,
                                               uint64_t* bytesReceived,
                                               uint32_t* cumulativeLost,
                                               int32_t* rttMs)
{
  {
    CSFLogVerbose(logTag, "%s for VideoConduit:%p", __FUNCTION__, this);
    MutexAutoLock lock(mCodecMutex);
    if (!mRecvStream) {
      return false;
    }

    const webrtc::VideoReceiveStream::Stats &stats = mRecvStream->GetStats();
    *jitterMs = stats.rtcp_stats.jitter;
    *cumulativeLost = stats.rtcp_stats.cumulative_lost;
    *bytesReceived = stats.rtp_stats.MediaPayloadBytes();
    *packetsReceived = stats.rtp_stats.transmitted.packets;
    // Note: timestamp is not correct per the spec... should be time the rtcp
    // was received (remote) or sent (local)
    *timestamp = webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();
    int64_t rtt = mRecvStream->GetRtt();
    if (rtt >= 0) {
      *rttMs = rtt;
    }
  }
  return true;
}

bool
WebrtcVideoConduit::GetRTCPSenderReport(DOMHighResTimeStamp* timestamp,
                                        unsigned int* packetsSent,
                                        uint64_t* bytesSent)
{
  {
    CSFLogVerbose(logTag, "%s for VideoConduit:%p", __FUNCTION__, this);
    MutexAutoLock lock(mCodecMutex);
    if (!mSendStream) {
      return false;
    }

    const webrtc::VideoSendStream::Stats& stats = mSendStream->GetStats();
    *packetsSent = 0;
    for (auto entry: stats.substreams){
      *packetsSent += entry.second.rtp_stats.transmitted.packets;
      // NG -- per https://www.w3.org/TR/webrtc-stats/ this is only payload bytes
      *bytesSent += entry.second.rtp_stats.MediaPayloadBytes();
    }
    // Note: timestamp is not correct per the spec... should be time the rtcp
    // was received (remote) or sent (local)
    *timestamp = webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();
    return true;
  }
  return false;
}

MediaConduitErrorCode
WebrtcVideoConduit::InitMain()
{
#if defined(MOZILLA_INTERNAL_API)
  // already know we must be on MainThread barring unit test weirdness
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
  if (!NS_WARN_IF(NS_FAILED(rv))) {
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);

    if (branch) {
      int32_t temp;
      Unused <<  NS_WARN_IF(NS_FAILED(branch->GetBoolPref("media.video.test_latency",
                                                          &mVideoLatencyTestEnable)));
      Unused << NS_WARN_IF(NS_FAILED(branch->GetBoolPref("media.video.test_latency",
                                                         &mVideoLatencyTestEnable)));
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref(
            "media.peerconnection.video.min_bitrate", &temp))))
      {
         if (temp >= 0) {
            mMinBitrate = temp;
         }
      }
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref(
            "media.peerconnection.video.start_bitrate", &temp))))
      {
         if (temp >= 0) {
         mStartBitrate = temp;
         }
      }
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref(
            "media.peerconnection.video.max_bitrate", &temp))))
      {
        if (temp >= 0) {
          mPrefMaxBitrate = temp;
          mNegotiatedMaxBitrate = temp; // simplifies logic in SelectBitrate (don't have to do two limit tests)
        }
      }
      if (mMinBitrate != 0 && mMinBitrate < kViEMinCodecBitrate) {
        mMinBitrate = kViEMinCodecBitrate;
      }
      if (mStartBitrate < mMinBitrate) {
        mStartBitrate = mMinBitrate;
      }
      if (mPrefMaxBitrate && mStartBitrate > mPrefMaxBitrate) {
        mStartBitrate = mPrefMaxBitrate;
      }
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref(
            "media.peerconnection.video.min_bitrate_estimate", &temp))))
      {
        if (temp >= 0) {
          mMinBitrateEstimate = temp;
        }
      }
      bool use_loadmanager = false;
      if (!NS_WARN_IF(NS_FAILED(branch->GetBoolPref(
            "media.navigator.load_adapt", &use_loadmanager))))
      {
        if (use_loadmanager) {
          mLoadManager = LoadManagerBuild();
        }
      }
    }
  }
#ifdef MOZ_WIDGET_ANDROID
  // get the JVM
  JavaVM *jvm = jsjni_GetVM();

  if (mozilla::camera::VideoEngine::SetAndroidObjects(jvm) != 0) {
    CSFLogError(logTag,  "%s: could not set Android objects", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }
#endif
#endif
  return kMediaConduitNoError;
}

/**
 * Performs initialization of the MANDATORY components of the Video Engine
 */
MediaConduitErrorCode
WebrtcVideoConduit::Init()
{
  CSFLogDebug(logTag, "%s this=%p", __FUNCTION__, this);
  MediaConduitErrorCode result;
  // Run code that must run on MainThread first
  MOZ_ASSERT(NS_IsMainThread());
  result = InitMain();
  if (result != kMediaConduitNoError) {
    return result;
  }

  CSFLogError(logTag, "%s Initialization Done", __FUNCTION__);
  return kMediaConduitNoError;
}

void
WebrtcVideoConduit::Destroy()
{
  // We can't delete the VideoEngine until all these are released!
  // And we can't use a Scoped ptr, since the order is arbitrary

  DeleteSendStream();
  DeleteRecvStream();
}

void
WebrtcVideoConduit::SyncTo(WebrtcAudioConduit* aConduit)
{
  CSFLogDebug(logTag, "%s Synced to %p", __FUNCTION__, aConduit);
  {
    MutexAutoLock lock(mCodecMutex);

    if (!mRecvStream) {
      CSFLogError(logTag, "SyncTo called with no receive stream");
      return;
    }

    if (aConduit) {
      mRecvStream->SetSyncChannel(aConduit->GetVoiceEngine(),
                                  aConduit->GetChannel());
    } else if (mSyncedTo) {
      mRecvStream->SetSyncChannel(mSyncedTo->GetVoiceEngine(), -1);
    }
  }

  mSyncedTo = aConduit;
}

MediaConduitErrorCode
WebrtcVideoConduit::AttachRenderer(RefPtr<mozilla::VideoRenderer> aVideoRenderer)
{
  CSFLogDebug(logTag, "%s", __FUNCTION__);

  // null renderer
  if (!aVideoRenderer) {
    CSFLogError(logTag, "%s NULL Renderer", __FUNCTION__);
    MOZ_ASSERT(false);
    return kMediaConduitInvalidRenderer;
  }

  // This function is called only from main, so we only need to protect against
  // modifying mRenderer while any webrtc.org code is trying to use it.
  {
    ReentrantMonitorAutoEnter enter(mTransportMonitor);
    mRenderer = aVideoRenderer;
    // Make sure the renderer knows the resolution
    mRenderer->FrameSizeChange(mReceivingWidth,
                               mReceivingHeight,
                               mNumReceivingStreams);
  }

  return kMediaConduitNoError;
}

void
WebrtcVideoConduit::DetachRenderer()
{
  {
    ReentrantMonitorAutoEnter enter(mTransportMonitor);
    if (mRenderer) {
      mRenderer = nullptr;
    }
  }
}

MediaConduitErrorCode
WebrtcVideoConduit::SetTransmitterTransport(
  RefPtr<TransportInterface> aTransport)
{
  CSFLogDebug(logTag, "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mTransmitterTransport = aTransport;
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::SetReceiverTransport(RefPtr<TransportInterface> aTransport)
{
  CSFLogDebug(logTag, "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mReceiverTransport = aTransport;
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::ConfigureRecvMediaCodecs(
  const std::vector<VideoCodecConfig* >& codecConfigList)
{
  CSFLogDebug(logTag, "%s ", __FUNCTION__);
  MediaConduitErrorCode condError = kMediaConduitNoError;
  std::string payloadName;

  condError = StopReceiving();
  if (condError != kMediaConduitNoError) {
    return condError;
  }

  if (codecConfigList.empty()) {
    CSFLogError(logTag, "%s Zero number of codecs to configure", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  webrtc::KeyFrameRequestMethod kf_request_method;
  bool kf_request_enabled = false;
  bool use_nack_basic = false;
  bool use_tmmbr = false;
  bool use_remb = false;
  bool use_fec = false;
  int ulpfec_payload_type = kNullPayloadType;
  int red_payload_type = kNullPayloadType;
  bool configuredH264 = false;
  // XXX(pkerr) - can I reuse matching decoders?
  mRecvStreamConfig.decoders.clear();
  mRecvStreamConfig.rtp.rtx.clear();

  // Try Applying the codecs in the list
  // we treat as success if at least one codec was applied and reception was
  // started successfully.
  for (const auto& codec_config : codecConfigList) {
    // if the codec param is invalid or duplicate, return error
    if ((condError = ValidateCodecConfig(codec_config, false))
        != kMediaConduitNoError) {
      CSFLogError(logTag, "%s Invalid config for %s decoder: %i", __FUNCTION__,
                  codec_config->mName.c_str(), condError);
      continue;
    }

    if (codec_config->mName == "H264") {
      // TODO(bug 1200768): We can only handle configuring one recv H264 codec
      if (configuredH264) {
        continue;
      }
      configuredH264 = true;
    }

    if (codec_config->mName == kUlpFecPayloadName) {
      ulpfec_payload_type = codec_config->mType;
      continue;
    }

    if (codec_config->mName == kRedPayloadName) {
      red_payload_type = codec_config->mType;
      continue;
    }

    // Check for the keyframe request type: PLI is preferred
    // over FIR, and FIR is preferred over none.
    if (codec_config->RtcpFbNackIsSet("pli")) {
      kf_request_enabled = true;
      kf_request_method = webrtc::kKeyFrameReqPliRtcp;
    } else if (!kf_request_enabled && codec_config->RtcpFbCcmIsSet("fir")) {
      kf_request_enabled = true;
      kf_request_method = webrtc::kKeyFrameReqFirRtcp;
    }

    use_nack_basic = codec_config->RtcpFbNackIsSet("");
    use_tmmbr = codec_config->RtcpFbCcmIsSet("tmmbr");
    use_remb = codec_config->RtcpFbRembIsSet();
    use_fec = codec_config->RtcpFbFECIsSet();

    CopyCodecToDB(codec_config);
  }

  mRecvStreamConfig.rtp.rtcp_mode = webrtc::RtcpMode::kCompound;
  mRecvStreamConfig.rtp.nack.rtp_history_ms = use_nack_basic ? 1000 : 0;
  mRecvStreamConfig.rtp.remb = use_remb;
  mRecvStreamConfig.rtp.tmmbr = use_tmmbr;

  if (use_fec) {
    mRecvStreamConfig.rtp.fec.ulpfec_payload_type = ulpfec_payload_type;
    mRecvStreamConfig.rtp.fec.red_payload_type = red_payload_type;
    mRecvStreamConfig.rtp.fec.red_rtx_payload_type = -1;
  }

  // FIXME(jesup) - Bug 1325447 -- SSRCs configured here are a problem.
  // 0 isn't allowed.  Would be best to ask for a random SSRC from the RTP code.
  // Would need to call rtp_sender.cc -- GenerateSSRC(), which isn't exposed.  It's called on
  // collision, or when we decide to send.  it should be called on receiver creation.
  // Here, we're generating the SSRC value - but this causes ssrc_forced in set in rtp_sender,
  // which locks us into the SSRC - even a collision won't change it!!!
  auto ssrc = mRecvStreamConfig.rtp.remote_ssrc;
  do {
    SECStatus rv = PK11_GenerateRandom(reinterpret_cast<unsigned char*>(&ssrc), sizeof(ssrc));
    if (rv != SECSuccess) {
      return kMediaConduitUnknownError;
    }
  } while (ssrc == mRecvStreamConfig.rtp.remote_ssrc);

  //DEBUG(pkerr)  mRecvStreamConfig.rtp.local_ssrc = ssrc;
  mRecvStreamConfig.rtp.local_ssrc = 1;

  return StartReceiving();
  return kMediaConduitNoError;
}

webrtc::VideoDecoder*
WebrtcVideoConduit::CreateDecoder(webrtc::VideoDecoder::DecoderType aType)
{
  webrtc::VideoDecoder* decoder = nullptr;

  if (aType == webrtc::VideoDecoder::kH264) {
    // get an external decoder
#ifdef MOZ_WEBRTC_OMX
    decoder = OMXVideoCodec::CreateDecoder(OMXVideoCodec::CodecType::CODEC_H264);
#else
    decoder = GmpVideoCodec::CreateDecoder();
#endif
    if (decoder) {
      mRecvCodecPlugin = static_cast<WebrtcVideoDecoder*>(decoder);
    }
#ifdef MOZ_WEBRTC_MEDIACODEC
  } else if (aType == webrtc::VideoDecoder::kVp8) {
    bool enabled = false;
    // attempt to get a decoder
#ifdef MOZILLA_INTERNAL_API
    enabled = mozilla::Preferences::GetBool(
                  "media.navigator.hardware.vp8_decode.acceleration_enabled", false);
#endif
    if (enabled) {
      nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
      if (gfxInfo) {
        int32_t status;
        nsCString discardFailureId;

        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(
                             nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_DECODE,
                             discardFailureId, &status))) {

          if (status != nsIGfxInfo::FEATURE_STATUS_OK) {
            NS_WARNING("VP8 decoder hardware is not whitelisted: disabling.\n");
          } else {
            decoder = MediaCodecVideoCodec::CreateDecoder(
                                        MediaCodecVideoCodec::CodecType::CODEC_VP8);
          }
        }
      }
    }

    // Use a software VP8 decoder as a fallback.
    if (!decoder) {
      decoder = webrtc::VideoDecoder::Create(aType);
    }
#endif
  } else {
    decoder = webrtc::VideoDecoder::Create(aType);
  }

  return decoder;
}

webrtc::VideoEncoder*
WebrtcVideoConduit::CreateEncoder(webrtc::VideoEncoder::EncoderType aType,
                                  bool enable_simulcast)
{
  webrtc::VideoEncoder* encoder = nullptr;
  if (aType == webrtc::VideoEncoder::kH264) {
    // get an external encoder
#ifdef MOZ_WEBRTC_OMX
    encoder = OMXVideoCodec::CreateEncoder(OMXVideoCodec::CodecType::CODEC_H264);
#else
    encoder = GmpVideoCodec::CreateEncoder();
#endif
    if (encoder) {
      mSendCodecPlugin = static_cast<WebrtcVideoEncoder*>(encoder);
    }
#ifdef MOZ_WEBRTC_MEDIACODEC
  } else if (aType == webrtc::VideoEncoder::kVp8) {
    bool enabled = false;
    // attempt to get a encoder
#ifdef MOZILLA_INTERNAL_API
    enabled = mozilla::Preferences::GetBool(
                  "media.navigator.hardware.vp8_encode.acceleration_enabled", false);
#endif
    if (enabled) {
      nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
      if (gfxInfo) {
        int32_t status;
        nsCString discardFailureId;

        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(
                         nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_ENCODE,
                         discardFailureId, &status))) {

          if (status != nsIGfxInfo::FEATURE_STATUS_OK) {
            NS_WARNING("VP8 encoder hardware is not whitelisted: disabling.\n");
          } else {
            encoder = MediaCodecVideoCodec::CreateEncoder(
                                        MediaCodecVideoCodec::CodecType::CODEC_VP8);
          }
        }
      }
    }

    // Use a software VP8 encoder as a fallback.
    if (!encoder) {
      encoder = webrtc::VideoEncoder::Create(aType, enable_simulcast);
    }
#endif
  } else {
    encoder = webrtc::VideoEncoder::Create(aType, enable_simulcast);
  }

  return encoder;
}

struct ResolutionAndBitrateLimits
{
  uint32_t resolution_in_mb;
  uint32_t min_bitrate_bps;
  uint32_t start_bitrate_bps;
  uint32_t max_bitrate_bps;
};

#define MB_OF(w,h) ((unsigned int)((((w+15)>>4))*((unsigned int)((h+15)>>4))))
// For now, try to set the max rates well above the knee in the curve.
// Chosen somewhat arbitrarily; it's hard to find good data oriented for
// realtime interactive/talking-head recording.  These rates assume
// 30fps.

// XXX Populate this based on a pref (which we should consider sorting because
// people won't assume they need to).
static ResolutionAndBitrateLimits kResolutionAndBitrateLimits[] = {
  {MB_OF(1920, 1200), KBPS(1500), KBPS(2000), KBPS(10000)}, // >HD (3K, 4K, etc)
  {MB_OF(1280, 720), KBPS(1200), KBPS(1500), KBPS(5000)}, // HD ~1080-1200
  {MB_OF(800, 480), KBPS(600), KBPS(800), KBPS(2500)}, // HD ~720
  {tl::Max<MB_OF(400, 240), MB_OF(352, 288)>::value, KBPS(200), KBPS(300), KBPS(1300)}, // VGA, WVGA
  {MB_OF(176, 144), KBPS(100), KBPS(150), KBPS(500)}, // WQVGA, CIF
  {0 , KBPS(40), KBPS(80), KBPS(250)} // QCIF and below
};

void
WebrtcVideoConduit::SelectBitrates(
  unsigned short width, unsigned short height, uint32_t cap,
  int32_t aLastFramerateTenths,
  webrtc::VideoStream& aVideoStream)
{
  int& out_min = aVideoStream.min_bitrate_bps;
  int& out_start = aVideoStream.target_bitrate_bps;
  int& out_max = aVideoStream.max_bitrate_bps;
  // max bandwidth should be proportional (not linearly!) to resolution, and
  // proportional (perhaps linearly, or close) to current frame rate.
  unsigned int fs = MB_OF(width, height);

  for (ResolutionAndBitrateLimits resAndLimits : kResolutionAndBitrateLimits) {
    if (fs > resAndLimits.resolution_in_mb &&
        // pick the highest range where at least start rate is within cap
        // (or if we're at the end of the array).
        (!cap || resAndLimits.start_bitrate_bps <= cap ||
         resAndLimits.resolution_in_mb == 0)) {
      out_min = static_cast<int>(MinIgnoreZero(resAndLimits.min_bitrate_bps, cap));
      out_start = static_cast<int>(MinIgnoreZero(resAndLimits.start_bitrate_bps, cap));
      out_max = static_cast<int>(MinIgnoreZero(resAndLimits.max_bitrate_bps, cap));
      break;
    }
  }

  // mLastFramerateTenths is scaled by *10
  double framerate = std::min((aLastFramerateTenths / 10.), 60.0);
  MOZ_ASSERT(framerate > 0);
  // Now linear reduction/increase based on fps (max 60fps i.e. doubling)
  if (framerate >= 10) {
    out_min = out_min * (framerate / 30);
    out_start = out_start * (framerate / 30);
    out_max = std::max(static_cast<unsigned int>(out_max * (framerate / 30)), cap);
  } else {
    // At low framerates, don't reduce bandwidth as much - cut slope to 1/2.
    // Mostly this would be ultra-low-light situations/mobile or screensharing.
    out_min = out_min * ((10 - (framerate / 2)) / 30);
    out_start = out_start * ((10 - (framerate / 2)) / 30);
    out_max = std::max(static_cast<unsigned int>(out_max * ((10 - (framerate / 2)) / 30)), cap);
  }

  if (mMinBitrate && mMinBitrate > out_min) {
    out_min = mMinBitrate;
  }
  // If we try to set a minimum bitrate that is too low, ViE will reject it.
  out_min = std::max(static_cast<int>(kViEMinCodecBitrate), out_min);
  if (mStartBitrate && mStartBitrate > out_start) {
    out_start = mStartBitrate;
  }
  out_start = std::max(out_start, out_min);

  // Note: mNegotiatedMaxBitrate is the max transport bitrate - it applies to
  // a single codec encoding, but should also apply to the sum of all
  // simulcast layers in this encoding!  So sum(layers.maxBitrate) <=
  // mNegotiatedMaxBitrate
  // Note that out_max already has had mPrefMaxBitrate applied to it
  out_max = MinIgnoreZero((int)mNegotiatedMaxBitrate, out_max);

  MOZ_ASSERT(mPrefMaxBitrate == 0 || out_max <= mPrefMaxBitrate);
}

template <class t>
static void
ConstrainPreservingAspectRatioExact(uint32_t max_fs, t* width, t* height)
{
  // We could try to pick a better starting divisor, but it won't make any real
  // performance difference.
  for (size_t d = 1; d < std::min(*width, *height); ++d) {
    if ((*width % d) || (*height % d)) {
      continue; // Not divisible
    }

    if (((*width) * (*height)) / (d * d) <= max_fs) {
      *width /= d;
      *height /= d;
      return;
    }
  }

  *width = 0;
  *height = 0;
}

template <class t>
static void
ConstrainPreservingAspectRatio(uint16_t max_width, uint16_t max_height,
                               t* width, t* height)
{
  if (((*width) <= max_width) && ((*height) <= max_height)) {
    return;
  }

  if ((*width) * max_height > max_width * (*height)) {
    (*height) = max_width * (*height) / (*width);
    (*width) = max_width;
  } else {
    (*width) = max_height * (*width) / (*height);
    (*height) = max_height;
  }
}

// XXX we need to figure out how to feed back changes in preferred capture
// resolution to the getUserMedia source.
// Returns boolean if we've submitted an async change (and took ownership
// of *frame's data)
bool
WebrtcVideoConduit::SelectSendResolution(unsigned short width,
                                         unsigned short height,
                                         webrtc::VideoFrame* frame) // may be null
{
  mCodecMutex.AssertCurrentThreadOwns();
  // XXX This will do bandwidth-resolution adaptation as well - bug 877954

  mLastWidth = width;
  mLastHeight = height;
  // Enforce constraints
  if (mCurSendCodecConfig) {
    uint16_t max_width = mCurSendCodecConfig->mEncodingConstraints.maxWidth;
    uint16_t max_height = mCurSendCodecConfig->mEncodingConstraints.maxHeight;
    if (max_width || max_height) {
      max_width = max_width ? max_width : UINT16_MAX;
      max_height = max_height ? max_height : UINT16_MAX;
      ConstrainPreservingAspectRatio(max_width, max_height, &width, &height);
    }

    // Limit resolution to max-fs while keeping same aspect ratio as the
    // incoming image.
    if (mCurSendCodecConfig->mEncodingConstraints.maxFs) {
      uint32_t max_fs = mCurSendCodecConfig->mEncodingConstraints.maxFs;
      unsigned int cur_fs, mb_width, mb_height, mb_max;

      // Could we make this simpler by picking the larger of width and height,
      // calculating a max for just that value based on the scale parameter,
      // and then let ConstrainPreservingAspectRatio do the rest?
      mb_width = (width + 15) >> 4;
      mb_height = (height + 15) >> 4;

      cur_fs = mb_width * mb_height;

      // Limit resolution to max_fs, but don't scale up.
      if (cur_fs > max_fs) {
        double scale_ratio;

        scale_ratio = sqrt((double)max_fs / (double)cur_fs);

        mb_width = mb_width * scale_ratio;
        mb_height = mb_height * scale_ratio;

        // Adjust mb_width and mb_height if they were truncated to zero.
        if (mb_width == 0) {
          mb_width = 1;
          mb_height = std::min(mb_height, max_fs);
        }
        if (mb_height == 0) {
          mb_height = 1;
          mb_width = std::min(mb_width, max_fs);
        }
      }

      // Limit width/height seperately to limit effect of extreme aspect ratios.
      mb_max = (unsigned)sqrt(8 * (double)max_fs);

      max_width = 16 * std::min(mb_width, mb_max);
      max_height = 16 * std::min(mb_height, mb_max);
      ConstrainPreservingAspectRatio(max_width, max_height, &width, &height);
    }
  }


  // Adapt to getUserMedia resolution changes
  // check if we need to reconfigure the sending resolution.
  bool changed = false;
  if (mSendingWidth != width || mSendingHeight != height) {
    CSFLogDebug(logTag, "%s: resolution changing to %ux%u (from %ux%u)",
                __FUNCTION__, width, height, mSendingWidth, mSendingHeight);
    // This will avoid us continually retrying this operation if it fails.
    // If the resolution changes, we'll try again.  In the meantime, we'll
    // keep using the old size in the encoder.
    mSendingWidth = width;
    mSendingHeight = height;
    changed = true;
  }

  // uses mSendingWidth/Height
  unsigned int framerate = SelectSendFrameRate(mSendingFramerate);
  if (mSendingFramerate != framerate) {
    CSFLogDebug(logTag, "%s: framerate changing to %u (from %u)",
                __FUNCTION__, framerate, mSendingFramerate);
    mSendingFramerate = framerate;
    changed = true;
  }

  if (changed) {
    // On a resolution change, bounce this to the correct thread to
    // re-configure (same as used for Init().  Do *not* block the calling
    // thread since that may be the MSG thread.

    // MUST run on the same thread as Init()/etc
    if (!NS_IsMainThread()) {
      // Note: on *initial* config (first frame), best would be to drop
      // frames until the config is done, then encode the most recent frame
      // provided and continue from there.  We don't do this, but we do drop
      // all frames while in the process of a reconfig and then encode the
      // frame that started the reconfig, which is close.  There may be
      // barely perceptible glitch in the video due to the dropped frame(s).
      mInReconfig = true;

      // We can't pass a UniquePtr<> or unique_ptr<> to a lambda directly
      webrtc::VideoFrame* new_frame = nullptr;
      if (frame) {
        new_frame = new webrtc::VideoFrame();
        // the internal buffer pointer is refcounted, so we don't have 2 copies here
        new_frame->ShallowCopy(*frame);
      }
      RefPtr<WebrtcVideoConduit> self(this);
      RefPtr<Runnable> webrtc_runnable =
        media::NewRunnableFrom([self, width, height, new_frame]() -> nsresult {
            UniquePtr<webrtc::VideoFrame> local_frame(new_frame); // Simplify cleanup

            MutexAutoLock lock(self->mCodecMutex);
            return self->ReconfigureSendCodec(width, height, new_frame);
          });
      // new_frame now owned by lambda
      CSFLogDebug(logTag, "%s: proxying lambda to WebRTC thread for reconfig (width %u/%u, height %u/%u",
                  __FUNCTION__, width, mLastWidth, height, mLastHeight);
      NS_DispatchToMainThread(webrtc_runnable.forget());
      if (new_frame) {
        return true; // queued it
      }
    } else {
      // already on the right thread
      ReconfigureSendCodec(width, height, frame);
    }
  }
  return false;
}

nsresult
WebrtcVideoConduit::ReconfigureSendCodec(unsigned short width,
                                         unsigned short height,
                                         webrtc::VideoFrame* frame)
{
  mCodecMutex.AssertCurrentThreadOwns();

  if (!mEncoderConfig.StreamCount()) {
    CSFLogError(logTag, "%s: No VideoStreams configured", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  mEncoderConfig.ForEachStream(
    [&](webrtc::VideoStream& video_stream,
        VideoEncoderConfigBuilder::SimulcastStreamConfig& simStream,
        const size_t index)
  {
    mInReconfig = false;

    CSFLogDebug(logTag,
                "%s: Requesting resolution change to %ux%u (from %ux%u), jsScaleDownBy=%f",
                __FUNCTION__, width, height, (unsigned int)video_stream.width,
                (unsigned int)video_stream.height, simStream.jsScaleDownBy);

    MOZ_ASSERT(simStream.jsScaleDownBy >= 1.0);
    uint32_t new_width = (width / simStream.jsScaleDownBy);
    uint32_t new_height = (height / simStream.jsScaleDownBy);
    video_stream.width = width;
    video_stream.height = height;
    video_stream.max_framerate = mSendingFramerate;
    unsigned int out_min, out_start, out_max;
    SelectBitrates(video_stream.width, video_stream.height, 0,
                   mLastFramerateTenths, video_stream);
    CSFLogVerbose(logTag, "%s: new_width=%" PRIu32 " new_height=%" PRIu32,
                  __FUNCTION__, new_width, new_height);
    if (new_width != video_stream.width || new_height != video_stream.height) {
      if (mEncoderConfig.StreamCount() == 1) {
        CSFLogVerbose(logTag, "%s: ConstrainPreservingAspectRatio", __FUNCTION__);
        // Use less strict scaling in unicast. That way 320x240 / 3 = 106x79.
        ConstrainPreservingAspectRatio(new_width, new_height,
                                       &video_stream.width, &video_stream.height);
      } else {
        CSFLogVerbose(logTag, "%s: ConstrainPreservingAspectRatioExact", __FUNCTION__);
        // webrtc.org supposedly won't tolerate simulcast unless every stream
        // is exactly the same aspect ratio. 320x240 / 3 = 80x60.
        ConstrainPreservingAspectRatioExact(new_width * new_height,
                                            &video_stream.width, &video_stream.height);
      }
    }

    CSFLogDebug(
      logTag, "%s: Encoder resolution changed to %ux%u @ %ufps, bitrate %u:%u",
      __FUNCTION__, video_stream.width, video_stream.height, mSendingFramerate,
      video_stream.min_bitrate_bps, video_stream.max_bitrate_bps);
  });
  if (!mSendStream->ReconfigureVideoEncoder(mEncoderConfig.GenerateConfig())) {
    CSFLogError(logTag, "%s: ReconfigureVideoEncoder failed", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }
  if (frame) {
    // XXX I really don't like doing this from MainThread...
    mSendStream->Input()->IncomingCapturedFrame(*frame);
    CSFLogDebug(logTag, "%s Inserted a frame from reconfig lambda", __FUNCTION__);
  }
  return NS_OK;
}

// Invoked under lock of mCodecMutex!
unsigned int
WebrtcVideoConduit::SelectSendFrameRate(unsigned int framerate) const
{
  mCodecMutex.AssertCurrentThreadOwns();
  unsigned int new_framerate = framerate;

  // Limit frame rate based on max-mbps
  if (mCurSendCodecConfig && mCurSendCodecConfig->mEncodingConstraints.maxMbps)
  {
    unsigned int cur_fs, mb_width, mb_height, max_fps;

    mb_width = (mSendingWidth + 15) >> 4;
    mb_height = (mSendingHeight + 15) >> 4;

    cur_fs = mb_width * mb_height;
    if (cur_fs > 0) { // in case no frames have been sent
      max_fps = mCurSendCodecConfig->mEncodingConstraints.maxMbps / cur_fs;
      if (max_fps < mSendingFramerate) {
        new_framerate = max_fps;
      }

      if (mCurSendCodecConfig->mEncodingConstraints.maxFps != 0 &&
          mCurSendCodecConfig->mEncodingConstraints.maxFps < mSendingFramerate) {
        new_framerate = mCurSendCodecConfig->mEncodingConstraints.maxFps;
      }
    }
  }
  return new_framerate;
}

MediaConduitErrorCode
WebrtcVideoConduit::SendVideoFrame(unsigned char* video_buffer,
                                   unsigned int video_length,
                                   unsigned short width,
                                   unsigned short height,
                                   VideoType video_type,
                                   uint64_t capture_time)
{

  // check for parameter sanity
  if (!video_buffer || video_length == 0 || width == 0 || height == 0) {
    CSFLogError(logTag, "%s Invalid Parameters ", __FUNCTION__);
    MOZ_ASSERT(false);
    return kMediaConduitMalformedArgument;
  }
  MOZ_ASSERT(video_type == VideoType::kVideoI420);

  // Transmission should be enabled before we insert any frames.
  if (!mEngineTransmitting) {
    CSFLogError(logTag, "%s Engine not transmitting ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  // insert the frame to video engine in I420 format only
  webrtc::VideoFrame video_frame;
  video_frame.CreateFrame(video_buffer, width, height, webrtc::kVideoRotation_0);
  video_frame.set_timestamp(capture_time);
  video_frame.set_render_time_ms(capture_time);

  return SendVideoFrame(video_frame);
}

MediaConduitErrorCode
WebrtcVideoConduit::SendVideoFrame(webrtc::VideoFrame& frame)
{
  CSFLogDebug(logTag, "%s", __FUNCTION__);
  // See if we need to recalculate what we're sending.
  // Don't compute mSendingWidth/Height, since those may not be the same as the input.
  {
    MutexAutoLock lock(mCodecMutex);
    if (mInReconfig) {
      // Waiting for it to finish
      return kMediaConduitNoError;
    }
    if (frame.width() != mLastWidth || frame.height() != mLastHeight) {
      CSFLogVerbose(logTag, "%s: call SelectSendResolution with %ux%u",
                    __FUNCTION__, frame.width(), frame.height());
      if (SelectSendResolution(frame.width(), frame.height(), &frame)) {
        // SelectSendResolution took ownership of the data in i420_frame.
        // Submit the frame after reconfig is done
        return kMediaConduitNoError;
      }
    }

    mSendStream->Input()->IncomingCapturedFrame(frame);
  }

  mSendStreamStats.SentFrame();
  CSFLogDebug(logTag, "%s Inserted a frame", __FUNCTION__);
  return kMediaConduitNoError;
}

// Transport Layer Callbacks

MediaConduitErrorCode
WebrtcVideoConduit::DeliverPacket(const void* data, int len)
{
  // Media Engine should be receiving already.
  if (/* XXX Remove this.... !mEngineReceiving ||*/ !mCall) {
    CSFLogError(logTag, "Error: %s when not receiving", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  // XXX we need to get passed the time the packet was received
  webrtc::PacketReceiver::DeliveryStatus status =
    mCall->Call()->Receiver()->DeliverPacket(webrtc::MediaType::VIDEO,
                                             static_cast<const uint8_t*>(data),
                                             len, webrtc::PacketTime());

  if (status != webrtc::PacketReceiver::DELIVERY_OK) {
    CSFLogError(logTag, "%s DeliverPacket Failed, %d", __FUNCTION__, status);
    return kMediaConduitRTPProcessingFailed;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::ReceivedRTPPacket(const void* data, int len)
{
  CSFLogDebug(logTag, "%s: seq# %u, Len %d ", __FUNCTION__,
              (uint16_t)ntohs(((uint16_t*) data)[1]), len);

  if (DeliverPacket(data, len) != kMediaConduitNoError) {
    CSFLogError(logTag, "%s RTP Processing Failed", __FUNCTION__);
    return kMediaConduitRTPProcessingFailed;
  }
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::ReceivedRTCPPacket(const void* data, int len)
{
  CSFLogDebug(logTag, " %s Len %d ", __FUNCTION__, len);

  if (DeliverPacket(data, len) != kMediaConduitNoError) {
    CSFLogError(logTag, "%s RTCP Processing Failed", __FUNCTION__);
    return kMediaConduitRTPProcessingFailed;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::StopTransmitting()
{
  if (mEngineTransmitting) {
    {
      MutexAutoLock lock(mCodecMutex);
      if (mSendStream) {
          CSFLogDebug(logTag, "%s Engine Already Sending. Attemping to Stop ", __FUNCTION__);
          mSendStream->Stop();
      }
    }

    mEngineTransmitting = false;
  }
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::StartTransmitting()
{
  if (mEngineTransmitting) {
    return kMediaConduitNoError;
  }

  CSFLogDebug(logTag, "%s Attemping to start... ", __FUNCTION__);
  {
    // Start Transmitting on the video engine
    MutexAutoLock lock(mCodecMutex);

    if (!mSendStream) {
      MediaConduitErrorCode rval = CreateSendStream();
      if (rval != kMediaConduitNoError) {
        CSFLogError(logTag, "%s Start Send Error %d ", __FUNCTION__, rval);
        return rval;
      }
    }

    mSendStream->Start();
    mEngineTransmitting = true;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::StopReceiving()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  // Are we receiving already? If so, stop receiving and playout
  // since we can't apply new recv codec when the engine is playing.
  if (mEngineReceiving && mRecvStream) {
    CSFLogDebug(logTag, "%s Engine Already Receiving . Attemping to Stop ", __FUNCTION__);
    mRecvStream->Stop();
  }

  mEngineReceiving = false;
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::StartReceiving()
{
  if (mEngineReceiving) {
    return kMediaConduitNoError;
  }

  CSFLogDebug(logTag, "%s Attemping to start... ", __FUNCTION__);
  {
    // Start Receive on the video engine
    MutexAutoLock lock(mCodecMutex);

    if (!mRecvStream) {
      MediaConduitErrorCode rval = CreateRecvStream();
      if (rval != kMediaConduitNoError) {
        CSFLogError(logTag, "%s Start Receive Error %d ", __FUNCTION__, rval);
        return rval;
      }
    }

    mRecvStream->Start();
    mEngineReceiving = true;
  }

  return kMediaConduitNoError;
}

// WebRTC::RTP Callback Implementation
// Called on MSG thread
bool
WebrtcVideoConduit::SendRtp(const uint8_t* packet, size_t length,
                            const webrtc::PacketOptions& options)
{
  // XXX(pkerr) - PacketOptions possibly containing RTP extensions are ignored.
  // The only field in it is the packet_id, which is used when the header
  // extension for TransportSequenceNumber is being used, which we don't.
  CSFLogDebug(logTag, "%s : len %lu", __FUNCTION__, (unsigned long)length);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if (!mTransmitterTransport ||
     NS_FAILED(mTransmitterTransport->SendRtpPacket(packet, length)))
  {
    CSFLogError(logTag, "%s RTP Packet Send Failed ", __FUNCTION__);
    return false;
  }

  CSFLogDebug(logTag, "%s Sent RTP Packet ", __FUNCTION__);
  return true;
}

// Called from multiple threads including webrtc Process thread
bool
WebrtcVideoConduit::SendRtcp(const uint8_t* packet, size_t length)
{
  CSFLogDebug(logTag, "%s : len %lu ", __FUNCTION__, (unsigned long)length);
  // We come here if we have only one pipeline/conduit setup,
  // such as for unidirectional streams.
  // We also end up here if we are receiving
  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if (mReceiverTransport &&
      NS_SUCCEEDED(mReceiverTransport->SendRtcpPacket(packet, length)))
  {
    // Might be a sender report, might be a receiver report, we don't know.
    CSFLogDebug(logTag, "%s Sent RTCP Packet ", __FUNCTION__);
    return true;
  } else if (mTransmitterTransport &&
             NS_SUCCEEDED(mTransmitterTransport->SendRtcpPacket(packet, length))) {
    CSFLogDebug(logTag, "%s Sent RTCP Packet (sender report) ", __FUNCTION__);
    return true;
  }

  CSFLogError(logTag, "%s RTCP Packet Send Failed ", __FUNCTION__);
  return false;
}

void
WebrtcVideoConduit::RenderFrame(const webrtc::VideoFrame& video_frame,
                                int time_to_render_ms)
{
  CSFLogDebug(logTag, "%s ", __FUNCTION__);
  ReentrantMonitorAutoEnter enter(mTransportMonitor);

  if (!mRenderer) {
    CSFLogError(logTag, "%s Renderer is NULL  ", __FUNCTION__);
    return;
  }

  if (mReceivingWidth != video_frame.width() ||
      mReceivingHeight != video_frame.height()) {
    mReceivingWidth = video_frame.width();
    mReceivingHeight = video_frame.height();
    mRenderer->FrameSizeChange(mReceivingWidth, mReceivingHeight, mNumReceivingStreams);
  }

  // Attempt to retrieve an timestamp encoded in the image pixels if enabled.
  if (mVideoLatencyTestEnable && mReceivingWidth && mReceivingHeight) {
    uint64_t now = PR_Now();
    uint64_t timestamp = 0;
    bool ok = YuvStamper::Decode(mReceivingWidth, mReceivingHeight, mReceivingWidth,
                                 const_cast<unsigned char*>(video_frame.buffer(webrtc::kYPlane)),
                                 reinterpret_cast<unsigned char*>(&timestamp),
                                 sizeof(timestamp), 0, 0);
    if (ok) {
      VideoLatencyUpdate(now - timestamp);
    }
  }

  const ImageHandle img_handle(nullptr);
  mRenderer->RenderVideoFrame(video_frame.buffer(webrtc::kYPlane),
                              video_frame.allocated_size(webrtc::kYPlane) +
                              video_frame.allocated_size(webrtc::kUPlane) +
                              video_frame.allocated_size(webrtc::kVPlane),
                              video_frame.stride(webrtc::kYPlane),
                              video_frame.stride(webrtc::kUPlane),
                              video_frame.timestamp(),
                              video_frame.render_time_ms(),
                              img_handle);
}

// Copy the codec passed into Conduit's database
bool
WebrtcVideoConduit::CopyCodecToDB(const VideoCodecConfig* codecInfo)
{
  mRecvCodecList.AppendElement(new VideoCodecConfig(*codecInfo));
  return true;
}

/**
 * Perform validation on the codecConfig to be applied
 * Verifies if the codec is already applied.
 */
MediaConduitErrorCode
WebrtcVideoConduit::ValidateCodecConfig(const VideoCodecConfig* codecInfo,
                                        bool send)
{
  if(!codecInfo) {
    CSFLogError(logTag, "%s Null CodecConfig ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  if((codecInfo->mName.empty()) ||
     (codecInfo->mName.length() >= CODEC_PLNAME_SIZE)) {
    CSFLogError(logTag, "%s Invalid Payload Name Length ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  return kMediaConduitNoError;
}

void
WebrtcVideoConduit::DumpCodecDB() const
{
  for (auto& entry : mRecvCodecList) {
    CSFLogDebug(logTag, "Payload Name: %s", entry->mName.c_str());
    CSFLogDebug(logTag, "Payload Type: %d", entry->mType);
    CSFLogDebug(logTag, "Payload Max Frame Size: %d", entry->mEncodingConstraints.maxFs);
    CSFLogDebug(logTag, "Payload Max Frame Rate: %d", entry->mEncodingConstraints.maxFps);
  }
}

void
WebrtcVideoConduit::VideoLatencyUpdate(uint64_t newSample)
{
  mVideoLatencyAvg = (sRoundingPadding * newSample + sAlphaNum * mVideoLatencyAvg) / sAlphaDen;
}

uint64_t
WebrtcVideoConduit::MozVideoLatencyAvg()
{
  return mVideoLatencyAvg / sRoundingPadding;
}

uint64_t
WebrtcVideoConduit::CodecPluginID()
{
  if (mSendCodecPlugin) {
    return mSendCodecPlugin->PluginID();
  } else if (mRecvCodecPlugin) {
    return mRecvCodecPlugin->PluginID();
  }

  return 0;
}

void
WebrtcVideoConduit::VideoEncoderConfigBuilder::SetEncoderSpecificSettings(
  void* aSettingsObj)
{
  mConfig.encoder_specific_settings = aSettingsObj;
};

void
WebrtcVideoConduit::VideoEncoderConfigBuilder::SetMinTransmitBitrateBps(
  int aXmitMinBps)
{
  mConfig.min_transmit_bitrate_bps = aXmitMinBps;
}

void
WebrtcVideoConduit::VideoEncoderConfigBuilder::SetContentType(
  webrtc::VideoEncoderConfig::ContentType aContentType)
{
  mConfig.content_type = aContentType;
}

void
WebrtcVideoConduit::VideoEncoderConfigBuilder::SetResolutionDivisor(
  unsigned char aDivisor)
{
  mConfig.resolution_divisor = aDivisor;
}

void
WebrtcVideoConduit::VideoEncoderConfigBuilder::AddStream(
  webrtc::VideoStream aStream)
{
  mConfig.streams.push_back(aStream);
  mSimulcastStreams.push_back(SimulcastStreamConfig());
}

void
WebrtcVideoConduit::VideoEncoderConfigBuilder::AddStream(
  webrtc::VideoStream aStream, const SimulcastStreamConfig& aSimulcastConfig)
{
  mConfig.streams.push_back(aStream);
  mSimulcastStreams.push_back(aSimulcastConfig);
}

size_t
WebrtcVideoConduit::VideoEncoderConfigBuilder::StreamCount()
{
  return mConfig.streams.size();
}

void
WebrtcVideoConduit::VideoEncoderConfigBuilder::ClearStreams()
{
  mConfig.streams.clear();
  mSimulcastStreams.clear();
}

void
WebrtcVideoConduit::VideoEncoderConfigBuilder::ForEachStream(
  const std::function<void(webrtc::VideoStream&, SimulcastStreamConfig&,
                           const size_t index)>&& f)
{
  size_t index = 0;
  for (auto simulcastStream : mSimulcastStreams) {
    f(mConfig.streams[index], simulcastStream, index);
    ++index;
  }
}

webrtc::VideoEncoderConfig
WebrtcVideoConduit::VideoEncoderConfigBuilder::GenerateConfig()
{
  return mConfig;
}

} // end namespace
