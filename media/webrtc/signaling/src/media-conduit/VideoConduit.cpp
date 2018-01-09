/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"
#include "nspr.h"
#include "plstr.h"

#include "AudioConduit.h"
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
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/api/video/i420_buffer.h"

#ifdef WEBRTC_MAC
#include <AvailabilityMacros.h>
#endif

#if defined(MAC_OS_X_VERSION_10_8) && \
  (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_8)
// XXX not available in Mac 10.7 SDK
#include "webrtc/common_video/include/corevideo_frame_buffer.h"
#endif

#include "mozilla/Unused.h"

#if defined(MOZ_WIDGET_ANDROID)
#include "VideoEngine.h"
#endif

#include "GmpVideoCodec.h"

#ifdef MOZ_WEBRTC_MEDIACODEC
#include "MediaCodecVideoCodec.h"
#endif
#include "WebrtcGmpVideoCodec.h"

#include "MediaDataDecoderCodec.h"

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

static const char* vcLogTag = "WebrtcVideoSessionConduit";
#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG vcLogTag

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
static const int kViEMinCodecBitrate_bps = KBPS(30);

template<typename T>
T MinIgnoreZero(const T& a, const T& b)
{
  return std::min(a? a:b, b? b:a);
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
}

void
WebrtcVideoConduit::SendStreamStatistics::DroppedFrames(
  uint32_t& aOutDroppedFrames) const
{
      aOutDroppedFrames = mDroppedFrames;
}

void
WebrtcVideoConduit::SendStreamStatistics::Update(
  const webrtc::VideoSendStream::Stats& aStats)
{
  StreamStatistics::Update(aStats.encode_frame_rate, aStats.media_bitrate_bps);
  if (!aStats.substreams.empty()) {
    const webrtc::FrameCounts& fc =
      aStats.substreams.begin()->second.frame_counts;
    mFramesEncoded = fc.key_frames + fc.delta_frames;
    CSFLogVerbose(LOGTAG,
                  "%s: framerate: %u, bitrate: %u, dropped frames delta: %u",
                  __FUNCTION__, aStats.encode_frame_rate,
                  aStats.media_bitrate_bps,
                  mFramesDeliveredToEncoder - mFramesEncoded - mDroppedFrames);
    mDroppedFrames = mFramesDeliveredToEncoder - mFramesEncoded;
  } else {
    CSFLogVerbose(LOGTAG, "%s stats.substreams is empty", __FUNCTION__);
  }
}

void
WebrtcVideoConduit::ReceiveStreamStatistics::DiscardedPackets(
  uint32_t& aOutDiscPackets) const
{
  aOutDiscPackets = mDiscardedPackets;
}

void
WebrtcVideoConduit::ReceiveStreamStatistics::FramesDecoded(
  uint32_t& aFramesDecoded) const
{
  aFramesDecoded = mFramesDecoded;
}

void
WebrtcVideoConduit::ReceiveStreamStatistics::Update(
  const webrtc::VideoReceiveStream::Stats& aStats)
{
  CSFLogVerbose(LOGTAG, "%s ", __FUNCTION__);
  StreamStatistics::Update(aStats.decode_frame_rate, aStats.total_bitrate_bps);
  mDiscardedPackets = aStats.discarded_packets;
  mFramesDecoded = aStats.frame_counts.key_frames
                   + aStats.frame_counts.delta_frames;
}

/**
 * Factory Method for VideoConduit
 */
RefPtr<VideoSessionConduit>
VideoSessionConduit::Create(RefPtr<WebRtcCallWrapper> aCall)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  NS_ASSERTION(aCall, "missing required parameter: aCall");
  CSFLogVerbose(LOGTAG, "%s", __FUNCTION__);

  if (!aCall) {
    return nullptr;
  }

  UniquePtr<cricket::VideoAdapter> videoAdapter(new cricket::VideoAdapter(1));
  nsAutoPtr<WebrtcVideoConduit> obj(new WebrtcVideoConduit(aCall,
                                    std::move(videoAdapter)));
  if(obj->Init() != kMediaConduitNoError) {
    CSFLogError(LOGTAG, "%s VideoConduit Init Failed ", __FUNCTION__);
    return nullptr;
  }
  CSFLogVerbose(LOGTAG, "%s Successfully created VideoConduit ", __FUNCTION__);
  return obj.forget();
}

WebrtcVideoConduit::WebrtcVideoConduit(RefPtr<WebRtcCallWrapper> aCall,
                                       UniquePtr<cricket::VideoAdapter>&& aVideoAdapter)
  : mTransportMonitor("WebrtcVideoConduit")
  , mRenderer(nullptr)
  , mVideoAdapter(std::move(aVideoAdapter))
  , mVideoBroadcaster()
  , mEngineTransmitting(false)
  , mEngineReceiving(false)
  , mCapId(-1)
  , mCodecMutex("VideoConduit codec db")
  , mInReconfig(false)
  , mRecvStream(nullptr)
  , mSendStream(nullptr)
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
  , mDenoising(false)
  , mLockScaling(false)
  , mSpatialLayers(1)
  , mTemporalLayers(1)
  , mCodecMode(webrtc::kRealtimeVideo)
  , mCall(aCall) // refcounted store of the call object
  , mSendStreamConfig(this) // 'this' is stored but not  dereferenced in the constructor.
  , mRecvStreamConfig(this) // 'this' is stored but not  dereferenced in the constructor.
  , mRecvSSRC(0)
  , mRecvSSRCSetInProgress(false)
  , mSendCodecPlugin(nullptr)
  , mRecvCodecPlugin(nullptr)
  , mVideoStatsTimer(NS_NewTimer())
{
  mRecvStreamConfig.renderer = this;

  // Video Stats Callback
  nsTimerCallbackFunc callback = [](nsITimer* aTimer, void* aClosure) {
    CSFLogDebug(LOGTAG, "StreamStats polling scheduled for VideoConduit: %p", aClosure);
    auto self = static_cast<WebrtcVideoConduit*>(aClosure);
    MutexAutoLock lock(self->mCodecMutex);
    if (self->mEngineTransmitting && self->mSendStream) {
      const auto& stats = self->mSendStream->GetStats();
      self->mSendStreamStats.Update(stats);
      if (!stats.substreams.empty()) {
          self->mSendPacketCounts =
            stats.substreams.begin()->second.rtcp_packet_type_counts;
      }
    }
    if (self->mEngineReceiving && self->mRecvStream) {
      const auto& stats = self->mRecvStream->GetStats();
      self->mRecvStreamStats.Update(stats);
      self->mRecvPacketCounts = stats.rtcp_packet_type_counts;
    }
  };
  mVideoStatsTimer->InitWithNamedFuncCallback(
    callback, this, 1000, nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP,
    "WebrtcVideoConduit::WebrtcVideoConduit");
}

WebrtcVideoConduit::~WebrtcVideoConduit()
{
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  if (mVideoStatsTimer) {
    CSFLogDebug(LOGTAG, "canceling StreamStats for VideoConduit: %p", this);
    MutexAutoLock lock(mCodecMutex);
    CSFLogDebug(LOGTAG, "StreamStats cancelled for VideoConduit: %p", this);
    mVideoStatsTimer->Cancel();
  }

  // Release AudioConduit first by dropping reference on MainThread, where it expects to be
  SyncTo(nullptr);
  Destroy();
}

void
WebrtcVideoConduit::SetLocalRTPExtensions(bool aIsSend,
                                          const RtpExtList & aExtensions)
{
  auto& extList = aIsSend ? mSendStreamConfig.rtp.extensions :
                  mRecvStreamConfig.rtp.extensions;
  extList = aExtensions;
}

RtpExtList
WebrtcVideoConduit::GetLocalRTPExtensions(bool aIsSend) const
{
  return aIsSend ? mSendStreamConfig.rtp.extensions :
                   mRecvStreamConfig.rtp.extensions;
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

  MutexAutoLock lock(mCodecMutex);
  // On the next StartTransmitting() or ConfigureSendMediaCodec, force
  // building a new SendStream to switch SSRCs.
  DeleteSendStream();
  if (wasTransmitting) {
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

bool WebrtcVideoConduit::SetLocalMID(const std::string& mid)
{
  mSendStreamConfig.rtp.mid = mid;
  return true;
}

MediaConduitErrorCode
WebrtcVideoConduit::ConfigureCodecMode(webrtc::VideoCodecMode mode)
{
  CSFLogVerbose(LOGTAG, "%s ", __FUNCTION__);
  if (mode == webrtc::VideoCodecMode::kRealtimeVideo ||
      mode == webrtc::VideoCodecMode::kScreensharing) {
    mCodecMode = mode;
    return kMediaConduitNoError;
  }

  return kMediaConduitMalformedArgument;
}

void
WebrtcVideoConduit::DeleteSendStream()
{
  mCodecMutex.AssertCurrentThreadOwns();
  if (mSendStream) {
    mCall->Call()->DestroyVideoSendStream(mSendStream);
    mSendStream = nullptr;
    mEncoder = nullptr;
  }
}

webrtc::VideoCodecType
SupportedCodecType(webrtc::VideoCodecType aType)
{
  switch (aType) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
    case webrtc::VideoCodecType::kVideoCodecVP9:
    case webrtc::VideoCodecType::kVideoCodecH264:
      return aType;
    default:
      return webrtc::VideoCodecType::kVideoCodecUnknown;
  }
  // NOTREACHED
}

MediaConduitErrorCode
WebrtcVideoConduit::CreateSendStream()
{
  mCodecMutex.AssertCurrentThreadOwns();

  webrtc::VideoCodecType encoder_type =
    SupportedCodecType(
      webrtc::PayloadNameToCodecType(mSendStreamConfig.encoder_settings.payload_name)
        .value_or(webrtc::VideoCodecType::kVideoCodecUnknown));
  if (encoder_type == webrtc::VideoCodecType::kVideoCodecUnknown) {
    return kMediaConduitInvalidSendCodec;
  }

  nsAutoPtr<webrtc::VideoEncoder> encoder(
    CreateEncoder(encoder_type, mEncoderConfig.StreamCount() > 0));
  if (!encoder) {
    return kMediaConduitInvalidSendCodec;
  }

  mSendStreamConfig.encoder_settings.encoder = encoder.get();

  MOZ_RELEASE_ASSERT(mEncoderConfig.NumberOfStreams() != 0,
                     "mEncoderConfig - There are no configured streams!");
  MOZ_ASSERT(mSendStreamConfig.rtp.ssrcs.size() == mEncoderConfig.NumberOfStreams(),
             "Each video substream must have a corresponding ssrc.");

  mSendStream = mCall->Call()->CreateVideoSendStream(mSendStreamConfig.Copy(), mEncoderConfig.CopyConfig());

  if (!mSendStream) {
    return kMediaConduitVideoSendStreamError;
  }
  mSendStream->SetSource(this, webrtc::VideoSendStream::DegradationPreference::kBalanced);

  mEncoder = encoder;

  return kMediaConduitNoError;
}

void
WebrtcVideoConduit::DeleteRecvStream()
{
  mCodecMutex.AssertCurrentThreadOwns();
  if (mRecvStream) {
    mCall->Call()->DestroyVideoReceiveStream(mRecvStream);
    mRecvStream = nullptr;
    mDecoders.clear();
  }
}

MediaConduitErrorCode
WebrtcVideoConduit::CreateRecvStream()
{
  mCodecMutex.AssertCurrentThreadOwns();

  webrtc::VideoReceiveStream::Decoder decoder_desc;
  std::unique_ptr<webrtc::VideoDecoder> decoder;
  webrtc::VideoCodecType decoder_type;

  mRecvStreamConfig.decoders.clear();
  for (auto& config : mRecvCodecList) {
    decoder_type = SupportedCodecType(webrtc::PayloadNameToCodecType(config->mName)
                                      .value_or(webrtc::VideoCodecType::kVideoCodecUnknown));
    if (decoder_type == webrtc::VideoCodecType::kVideoCodecUnknown) {
      CSFLogError(LOGTAG, "%s Unknown decoder type: %s", __FUNCTION__,
                  config->mName.c_str());
      continue;
    }

    decoder.reset(CreateDecoder(decoder_type));

    if (!decoder) {
      // This really should never happen unless something went wrong
      // in the negotiation code
      NS_ASSERTION(decoder, "Failed to create video decoder");
      CSFLogError(LOGTAG, "Failed to create decoder of type %s (%d)",
                  config->mName.c_str(), decoder_type);
      // don't stop
      continue;
    }

    decoder_desc.decoder = decoder.get();
    mDecoders.push_back(std::move(decoder));
    decoder_desc.payload_name = config->mName;
    decoder_desc.payload_type = config->mType;
    // XXX Ok, add:
    // Set decoder_desc.codec_params (fmtp)
    mRecvStreamConfig.decoders.push_back(decoder_desc);
  }

  mRecvStream = mCall->Call()->CreateVideoReceiveStream(mRecvStreamConfig.Copy());
  if (!mRecvStream) {
    mDecoders.clear();
    return kMediaConduitUnknownError;
  }
  CSFLogDebug(LOGTAG, "Created VideoReceiveStream %p for SSRC %u (0x%x)",
              mRecvStream, mRecvStreamConfig.rtp.remote_ssrc, mRecvStreamConfig.rtp.remote_ssrc);

  return kMediaConduitNoError;
}

static rtc::scoped_refptr<webrtc::VideoEncoderConfig::EncoderSpecificSettings>
ConfigureVideoEncoderSettings(const VideoCodecConfig* aConfig,
                              const WebrtcVideoConduit* aConduit)
{
  bool is_screencast = aConduit->CodecMode() == webrtc::VideoCodecMode::kScreensharing;
  // No automatic resizing when using simulcast or screencast.
  bool automatic_resize = !is_screencast && aConfig->mSimulcastEncodings.size() <= 1;
  bool frame_dropping = !is_screencast;
  bool denoising;
  bool codec_default_denoising = false;
  if (is_screencast) {
    denoising = false;
  } else {
    // Use codec default if video_noise_reduction is unset.
    denoising = aConduit->Denoising();
    codec_default_denoising = !denoising;
  }

  if (aConfig->mName == "H264") {
    webrtc::VideoCodecH264 h264_settings =
        webrtc::VideoEncoder::GetDefaultH264Settings();
    h264_settings.frameDroppingOn = frame_dropping;
    h264_settings.packetizationMode = aConfig->mPacketizationMode;
    return new rtc::RefCountedObject<
        webrtc::VideoEncoderConfig::H264EncoderSpecificSettings>(h264_settings);

  }
  if (aConfig->mName == "VP8") {
    webrtc::VideoCodecVP8 vp8_settings =
        webrtc::VideoEncoder::GetDefaultVp8Settings();
    vp8_settings.automaticResizeOn = automatic_resize;
    // VP8 denoising is enabled by default.
    vp8_settings.denoisingOn = codec_default_denoising ? true : denoising;
    vp8_settings.frameDroppingOn = frame_dropping;
    return new rtc::RefCountedObject<
        webrtc::VideoEncoderConfig::Vp8EncoderSpecificSettings>(vp8_settings);

  }
  if (aConfig->mName == "VP9") {
    webrtc::VideoCodecVP9 vp9_settings =
        webrtc::VideoEncoder::GetDefaultVp9Settings();
    if (is_screencast) {
      // TODO(asapersson): Set to 2 for now since there is a DCHECK in
      // VideoSendStream::ReconfigureVideoEncoder.
      vp9_settings.numberOfSpatialLayers = 2;
    } else {
      vp9_settings.numberOfSpatialLayers = aConduit->SpatialLayers();
    }
    // VP9 denoising is disabled by default.
    vp9_settings.denoisingOn = codec_default_denoising ? false : denoising;
    vp9_settings.frameDroppingOn = frame_dropping;
    return new rtc::RefCountedObject<
        webrtc::VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9_settings);
  }
  return nullptr;
}

std::vector<webrtc::VideoStream>
WebrtcVideoConduit::VideoStreamFactory::CreateEncoderStreams(int width, int height,
                                                             const webrtc::VideoEncoderConfig& config)
{
  auto streamCount = config.number_of_streams;
  std::vector<webrtc::VideoStream> streams;
  streams.reserve(streamCount);
  MOZ_ASSERT(mConduit);
  MutexAutoLock lock(mConduit->mCodecMutex); // for mCurSendCodecConfig

  // XXX webrtc.org code has a restriction on simulcast layers that each
  // layer must be 1/2 the dimension of the previous layer - not sure why.
  // This means we can't use scaleResolutionBy/scaleDownBy (yet), even if
  // the user specified it.  The one exception is that we can apply it on
  // the full-resolution stream (which also happens to handle the
  // non-simulcast usage case). NOTE: we make an assumption here, not in the
  // spec, that the first stream is the full-resolution stream.
  auto& simulcastEncoding = mConduit->mCurSendCodecConfig->mSimulcastEncodings[0];
#if 0
  // XXX What we'd like to do for each simulcast stream...
  if (simulcastEncoding.constraints.scaleDownBy > 1.0) {
    uint32_t new_width = width / simulcastEncoding.constraints.scaleDownBy;
    uint32_t new_height = height / simulcastEncoding.constraints.scaleDownBy;

    if (new_width != width || new_height != height) {
      if (streamCount == 1) {
        CSFLogVerbose(LOGTAG, "%s: ConstrainPreservingAspectRatio", __FUNCTION__);
        // Use less strict scaling in unicast. That way 320x240 / 3 = 106x79.
        ConstrainPreservingAspectRatio(new_width, new_height,
                                       &width, &height);
      } else {
        CSFLogVerbose(LOGTAG, "%s: ConstrainPreservingAspectRatioExact", __FUNCTION__);
        // webrtc.org supposedly won't tolerate simulcast unless every stream
        // is exactly the same aspect ratio. 320x240 / 3 = 80x60.
        ConstrainPreservingAspectRatioExact(new_width * new_height,
                                            &width, &height);
      }
    }
  }
#endif

  for (size_t idx = streamCount - 1; streamCount > 0; idx--, streamCount--) {
    webrtc::VideoStream video_stream;
    // Stream dimensions must be divisable by 2^(n-1), where n is the number of layers.
    // Each lower resolution layer is 1/2^(n-1) of the size of largest layer,
    // where n is the number of the layer

    // width/height will be overridden on the first frame; they must be 'sane' for
    // SetSendCodec()
    video_stream.width = width >> idx;
    video_stream.height = height >> idx;
    // We want to ensure this picks up the current framerate, so indirect
    video_stream.max_framerate = mConduit->mSendingFramerate;

    simulcastEncoding = mConduit->mCurSendCodecConfig->mSimulcastEncodings[idx];
    MOZ_ASSERT(simulcastEncoding.constraints.scaleDownBy >= 1.0);

    // leave vector temporal_layer_thresholds_bps empty for non-simulcast
    if (config.number_of_streams > 1) {
      // Oddly, though this is a 'bps' array, nothing really looks at the
      // values, just the size of the array to know the number of temporal
      // layers.
      video_stream.temporal_layer_thresholds_bps.resize(2);
      // XXX Note: in simulcast.cc in upstream code, the array value is
      // 3(-1) for all streams, though it's in an array, except for screencasts,
      // which use 1 (i.e 2 layers).

      // XXX Bug 1390215 investigate using more of
      // simulcast.cc:GetSimulcastConfig() or our own algorithm to replace it
    } else {
      video_stream.temporal_layer_thresholds_bps.clear();
    }

    // Calculate these first
    video_stream.max_bitrate_bps = MinIgnoreZero(simulcastEncoding.constraints.maxBr,
                                                 kDefaultMaxBitrate_bps);
    video_stream.max_bitrate_bps = MinIgnoreZero((int) mConduit->mPrefMaxBitrate*1000,
                                                 video_stream.max_bitrate_bps);
    video_stream.min_bitrate_bps = (mConduit->mMinBitrate ?
                                    mConduit->mMinBitrate : kDefaultMinBitrate_bps);
    if (video_stream.min_bitrate_bps > video_stream.max_bitrate_bps) {
      video_stream.min_bitrate_bps = video_stream.max_bitrate_bps;
    }
    video_stream.target_bitrate_bps = (mConduit->mStartBitrate ?
                                       mConduit->mStartBitrate : kDefaultStartBitrate_bps);
    if (video_stream.target_bitrate_bps > video_stream.max_bitrate_bps) {
      video_stream.target_bitrate_bps = video_stream.max_bitrate_bps;
    }
    if (video_stream.target_bitrate_bps < video_stream.min_bitrate_bps) {
      video_stream.target_bitrate_bps = video_stream.min_bitrate_bps;
    }
    // We should use SelectBitrates here for the case of already-sending and no reconfig needed;
    // overrides the calculations above
    if (mConduit->mSendingWidth) { // cleared if we need a reconfig
      mConduit->SelectBitrates(video_stream.width, video_stream.height, // use video_stream.foo!
                               simulcastEncoding.constraints.maxBr,
                               mConduit->mLastFramerateTenths, video_stream);
    }

    video_stream.max_qp = kQpMax;
    video_stream.SetRid(simulcastEncoding.rid);

    if (mConduit->mCurSendCodecConfig->mName == "H264") {
      if (mConduit->mCurSendCodecConfig->mEncodingConstraints.maxMbps > 0) {
        // Not supported yet!
        CSFLogError(LOGTAG, "%s H.264 max_mbps not supported yet", __FUNCTION__);
      }
    }
    streams.push_back(video_stream);
  }
  return streams;
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
  CSFLogDebug(LOGTAG, "%s for %s", __FUNCTION__,
    codecConfig ? codecConfig->mName.c_str() : "<null>");

  MediaConduitErrorCode condError = kMediaConduitNoError;

  // validate basic params
  if ((condError = ValidateCodecConfig(codecConfig)) != kMediaConduitNoError) {
    return condError;
  }

  size_t streamCount = std::min(codecConfig->mSimulcastEncodings.size(),
                                (size_t)webrtc::kMaxSimulcastStreams);
  CSFLogDebug(LOGTAG, "%s for VideoConduit:%p stream count:%d", __FUNCTION__,
              this, static_cast<int>(streamCount));

  mSendingFramerate = 0;
  mEncoderConfig.ClearStreams();
  mSendStreamConfig.rtp.rids.clear();

  int max_framerate;
  if (codecConfig->mEncodingConstraints.maxFps > 0) {
    max_framerate = codecConfig->mEncodingConstraints.maxFps;
  } else {
    max_framerate = DEFAULT_VIDEO_MAX_FRAMERATE;
  }
  // apply restrictions from maxMbps/etc
  mSendingFramerate = SelectSendFrameRate(codecConfig,
                                          max_framerate,
                                          mSendingWidth,
                                          mSendingHeight);

  // So we can comply with b=TIAS/b=AS/maxbr=X when input resolution changes
  mNegotiatedMaxBitrate = codecConfig->mTias;

  // width/height will be overridden on the first frame; they must be 'sane' for
  // SetSendCodec()

  if (mSendingWidth != 0) {
    // We're already in a call and are reconfiguring (perhaps due to
    // ReplaceTrack).
    bool resolutionChanged;
    {
      MutexAutoLock lock(mCodecMutex);
      resolutionChanged = !mCurSendCodecConfig->ResolutionEquals(*codecConfig);
    }

    if (resolutionChanged) {
      // We're already in a call and due to renegotiation an encoder parameter
      // that requires reconfiguration has changed. Resetting these members
      // triggers reconfig on the next frame.
      mLastWidth = 0;
      mLastHeight = 0;
      mSendingWidth = 0;
      mSendingHeight = 0;
    } else {
      // We're already in a call but changes don't require a reconfiguration.
      // We update the resolutions in the send codec to match the current
      // settings.  Framerate is already set.
    }
  } else if (mMinBitrateEstimate) {
    // Only do this at the start; use "have we send a frame" as a reasonable stand-in.
    // min <= start <= max (which can be -1, note!)
    webrtc::Call::Config::BitrateConfig config;
    config.min_bitrate_bps = mMinBitrateEstimate;
    if (config.start_bitrate_bps < mMinBitrateEstimate) {
      config.start_bitrate_bps = mMinBitrateEstimate;
    }
    if (config.max_bitrate_bps > 0 &&
        config.max_bitrate_bps < mMinBitrateEstimate) {
      config.max_bitrate_bps = mMinBitrateEstimate;
    }
    mCall->Call()->SetBitrateConfig(config);
  }

  // NOTE: the lifetime of this object MUST be less than the lifetime of the Conduit
  mEncoderConfig.SetVideoStreamFactory(
    new rtc::RefCountedObject<WebrtcVideoConduit::VideoStreamFactory>(
      codecConfig->mName, this));

  // Always call this to ensure it's reset
  mVideoAdapter->OnScaleResolutionBy(
    (streamCount >= 1 && codecConfig->mSimulcastEncodings[0].constraints.scaleDownBy > 1.0) ?
    rtc::Optional<float>(codecConfig->mSimulcastEncodings[0].constraints.scaleDownBy) :
    rtc::Optional<float>());

  // XXX parse the encoded SPS/PPS data and set spsData/spsLen/ppsData/ppsLen
  mEncoderConfig.SetEncoderSpecificSettings(ConfigureVideoEncoderSettings(codecConfig, this));
  mEncoderConfig.SetResolutionDivisor(1);

  mEncoderConfig.SetContentType(mCodecMode == webrtc::kRealtimeVideo ?
    webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo :
    webrtc::VideoEncoderConfig::ContentType::kScreen);
  // for the GMP H.264 encoder/decoder!!
  mEncoderConfig.SetMinTransmitBitrateBps(0);
  // Expected max number of encodings
  mEncoderConfig.SetMaxEncodings(codecConfig->mSimulcastEncodings.size());

  // If only encoder stream attibutes have been changed, there is no need to stop,
  // create a new webrtc::VideoSendStream, and restart.
  // Recreating on PayloadType change may be overkill, but is safe.
  if (mSendStream) {
    if (!RequiresNewSendStream(*codecConfig)) {
      {
        MutexAutoLock lock(mCodecMutex);
        mCurSendCodecConfig->mEncodingConstraints = codecConfig->mEncodingConstraints;
        mCurSendCodecConfig->mSimulcastEncodings = codecConfig->mSimulcastEncodings;
      }
      mSendStream->ReconfigureVideoEncoder(mEncoderConfig.CopyConfig());
      return kMediaConduitNoError;
    }

    condError = StopTransmitting();
    if (condError != kMediaConduitNoError) {
      return condError;
    }

    // This will cause a new encoder to be created by StartTransmitting()
    MutexAutoLock lock(mCodecMutex);
    DeleteSendStream();
  }

  mSendStreamConfig.encoder_settings.payload_name = codecConfig->mName;
  mSendStreamConfig.encoder_settings.payload_type = codecConfig->mType;
  mSendStreamConfig.rtp.rtcp_mode = webrtc::RtcpMode::kCompound;
  mSendStreamConfig.rtp.max_packet_size = kVideoMtu;

  // See Bug 1297058, enabling FEC when basic NACK is to be enabled in H.264 is problematic
  if (codecConfig->RtcpFbFECIsSet() &&
      !(codecConfig->mName == "H264" && codecConfig->RtcpFbNackIsSet(""))) {
    mSendStreamConfig.rtp.ulpfec.ulpfec_payload_type = codecConfig->mULPFECPayloadType;
    mSendStreamConfig.rtp.ulpfec.red_payload_type = codecConfig->mREDPayloadType;
    mSendStreamConfig.rtp.ulpfec.red_rtx_payload_type = codecConfig->mREDRTXPayloadType;
  } else {
    // Reset to defaults
    mSendStreamConfig.rtp.ulpfec.ulpfec_payload_type = -1;
    mSendStreamConfig.rtp.ulpfec.red_payload_type = -1;
    mSendStreamConfig.rtp.ulpfec.red_rtx_payload_type = -1;
  }

  mSendStreamConfig.rtp.nack.rtp_history_ms =
    codecConfig->RtcpFbNackIsSet("") ? 1000 : 0;

  {
    MutexAutoLock lock(mCodecMutex);
    // Copy the applied config for future reference.
    mCurSendCodecConfig = new VideoCodecConfig(*codecConfig);
  }

  mSendStreamConfig.rtp.rids.clear();
  bool has_rid = false;
  for (size_t idx = 0; idx < streamCount; idx++) {
    auto& simulcastEncoding = mCurSendCodecConfig->mSimulcastEncodings[idx];
    if (simulcastEncoding.rid[0]) {
      has_rid = true;
      break;
    }
  }
  if (has_rid) {
    for (size_t idx = streamCount; idx > 0; idx--) {
      auto& simulcastEncoding = mCurSendCodecConfig->mSimulcastEncodings[idx-1];
      mSendStreamConfig.rtp.rids.push_back(simulcastEncoding.rid);
    }
  }

  return condError;
}

bool
WebrtcVideoConduit::SetRemoteSSRC(unsigned int ssrc)
{
  CSFLogDebug(LOGTAG, "%s: SSRC %u (0x%x)", __FUNCTION__, ssrc, ssrc);
  mRecvStreamConfig.rtp.remote_ssrc = ssrc;

  unsigned int current_ssrc;
  if (!GetRemoteSSRC(&current_ssrc)) {
    return false;
  }

  if (current_ssrc == ssrc) {
    return true;
  }

  bool wasReceiving = mEngineReceiving;
  if (StopReceiving() != kMediaConduitNoError) {
    return false;
  }

  // This will destroy mRecvStream and create a new one (argh, why can't we change
  // it without a full destroy?)
  // We're going to modify mRecvStream, we must lock.  Only modified on MainThread.
  // All non-MainThread users must lock before reading/using
  {
    MutexAutoLock lock(mCodecMutex);
    // On the next StartReceiving() or ConfigureRecvMediaCodec, force
    // building a new RecvStream to switch SSRCs.
    DeleteRecvStream();
    if (!wasReceiving) {
      return true;
    }
    MediaConduitErrorCode rval = CreateRecvStream();
    if (rval != kMediaConduitNoError) {
      CSFLogError(LOGTAG, "%s Start Receive Error %d ", __FUNCTION__, rval);
      return false;
    }
  }
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
WebrtcVideoConduit::GetSendPacketTypeStats(
    webrtc::RtcpPacketTypeCounter* aPacketCounts)
{
  MutexAutoLock lock(mCodecMutex);
  if (!mEngineTransmitting || !mSendStream) { // Not transmitting
    return false;
  }
  *aPacketCounts = mSendPacketCounts;
  return true;
}

bool
WebrtcVideoConduit::GetRecvPacketTypeStats(
    webrtc::RtcpPacketTypeCounter* aPacketCounts)
{
  MutexAutoLock lock(mCodecMutex);
  if (!mEngineReceiving || !mRecvStream) { // Not receiving
    return false;
  }
  *aPacketCounts = mRecvPacketCounts;
  return true;
}

bool
WebrtcVideoConduit::GetVideoEncoderStats(double* framerateMean,
                                         double* framerateStdDev,
                                         double* bitrateMean,
                                         double* bitrateStdDev,
                                         uint32_t* droppedFrames,
                                         uint32_t* framesEncoded)
{
  {
    MutexAutoLock lock(mCodecMutex);
    if (!mEngineTransmitting || !mSendStream) {
      return false;
    }
    mSendStreamStats.GetVideoStreamStats(*framerateMean, *framerateStdDev,
      *bitrateMean, *bitrateStdDev);
    mSendStreamStats.DroppedFrames(*droppedFrames);
    *framesEncoded = mSendStreamStats.FramesEncoded();
    return true;
  }
}

bool
WebrtcVideoConduit::GetVideoDecoderStats(double* framerateMean,
                                         double* framerateStdDev,
                                         double* bitrateMean,
                                         double* bitrateStdDev,
                                         uint32_t* discardedPackets,
                                         uint32_t* framesDecoded)
{
  {
    MutexAutoLock lock(mCodecMutex);
    if (!mEngineReceiving || !mRecvStream) {
      return false;
    }
    mRecvStreamStats.GetVideoStreamStats(*framerateMean, *framerateStdDev,
      *bitrateMean, *bitrateStdDev);
    mRecvStreamStats.DiscardedPackets(*discardedPackets);
    mRecvStreamStats.FramesDecoded(*framesDecoded);
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
  CSFLogVerbose(LOGTAG, "%s for VideoConduit:%p", __FUNCTION__, this);
  {
    MutexAutoLock lock(mCodecMutex);
    if (!mRecvStream) {
      return false;
    }

    const webrtc::VideoReceiveStream::Stats& stats = mRecvStream->GetStats();
    *jitterMs =
        stats.rtcp_stats.jitter / (webrtc::kVideoPayloadTypeFrequency / 1000);
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
    CSFLogVerbose(LOGTAG, "%s for VideoConduit:%p", __FUNCTION__, this);
    MutexAutoLock lock(mCodecMutex);
    if (!mSendStream) {
      return false;
    }
    const webrtc::VideoSendStream::Stats& sendStats = mSendStream->GetStats();
    if (sendStats.substreams.empty()
        || mSendStreamConfig.rtp.ssrcs.empty()) {
      return false;
    }
    uint32_t ssrc = mSendStreamConfig.rtp.ssrcs.front();
    auto ind = sendStats.substreams.find(ssrc);
    if (ind == sendStats.substreams.end()) {
      CSFLogError(LOGTAG,
        "%s for VideoConduit:%p ssrc not found in SendStream stats.",
        __FUNCTION__, this);
      return false;
    }
    *jitterMs = ind->second.rtcp_stats.jitter
        / (webrtc::kVideoPayloadTypeFrequency / 1000);
    *cumulativeLost = ind->second.rtcp_stats.cumulative_lost;
    *bytesReceived = ind->second.rtp_stats.MediaPayloadBytes();
    *packetsReceived = ind->second.rtp_stats.transmitted.packets;
    auto stats = mCall->Call()->GetStats();
    int64_t rtt = stats.rtt_ms;
#ifdef DEBUG
    if (rtt > INT32_MAX) {
      CSFLogError(LOGTAG,
        "%s for VideoConduit:%p RTT is larger than the"
        " maximum size of an RTCP RTT.", __FUNCTION__, this);
    }
#endif
    if (rtt > 0) {
      *rttMs = rtt;
    } else {
      *rttMs = 0;
    }
    // Note: timestamp is not correct per the spec... should be time the rtcp
    // was received (remote) or sent (local)
    *timestamp = webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();
  }
  return true;
}

bool
WebrtcVideoConduit::GetRTCPSenderReport(DOMHighResTimeStamp* timestamp,
                                        unsigned int* packetsSent,
                                        uint64_t* bytesSent)
{
  CSFLogVerbose(LOGTAG, "%s for VideoConduit:%p", __FUNCTION__, this);
  webrtc::RTCPSenderInfo senderInfo;
  {
    MutexAutoLock lock(mCodecMutex);
    if (!mRecvStream || !mRecvStream->GetRemoteRTCPSenderInfo(&senderInfo)) {
      return false;
    }
  }
  *timestamp = webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();
  *packetsSent = senderInfo.sendPacketCount;
  *bytesSent = senderInfo.sendOctetCount;
  return true;
}

MediaConduitErrorCode
WebrtcVideoConduit::InitMain()
{
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
           mMinBitrate = KBPS(temp);
         }
      }
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref(
            "media.peerconnection.video.start_bitrate", &temp))))
      {
         if (temp >= 0) {
           mStartBitrate = KBPS(temp);
         }
      }
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref(
            "media.peerconnection.video.max_bitrate", &temp))))
      {
        if (temp >= 0) {
          mPrefMaxBitrate = KBPS(temp);
        }
      }
      if (mMinBitrate != 0 && mMinBitrate < kViEMinCodecBitrate_bps) {
        mMinBitrate = kViEMinCodecBitrate_bps;
      }
      if (mStartBitrate < mMinBitrate) {
        mStartBitrate = mMinBitrate;
      }
      if (mPrefMaxBitrate && mStartBitrate > mPrefMaxBitrate) {
        mStartBitrate = mPrefMaxBitrate;
      }
      // XXX We'd love if this was a live param for testing adaptation/etc
      // in automation
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref(
            "media.peerconnection.video.min_bitrate_estimate", &temp))))
      {
        if (temp >= 0) {
          mMinBitrateEstimate = temp; // bps!
        }
      }
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref(
            "media.peerconnection.video.svc.spatial", &temp))))
      {
         if (temp >= 0) {
            mSpatialLayers = temp;
         }
      }
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref(
            "media.peerconnection.video.svc.temporal", &temp))))
      {
         if (temp >= 0) {
            mTemporalLayers = temp;
         }
      }
      Unused << NS_WARN_IF(NS_FAILED(branch->GetBoolPref(
        "media.peerconnection.video.denoising", &mDenoising)));
      Unused << NS_WARN_IF(NS_FAILED(branch->GetBoolPref(
        "media.peerconnection.video.lock_scaling", &mLockScaling)));
    }
  }
#ifdef MOZ_WIDGET_ANDROID
  JavaVM* jvm = mozilla::jni::GetVM();

  if (mozilla::camera::VideoEngine::SetAndroidObjects(jvm) != 0) {
    CSFLogError(LOGTAG,  "%s: could not set Android objects", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }
#endif  //MOZ_WIDGET_ANDROID
  return kMediaConduitNoError;
}

/**
 * Performs initialization of the MANDATORY components of the Video Engine
 */
MediaConduitErrorCode
WebrtcVideoConduit::Init()
{
  CSFLogDebug(LOGTAG, "%s this=%p", __FUNCTION__, this);
  MediaConduitErrorCode result;
  // Run code that must run on MainThread first
  MOZ_ASSERT(NS_IsMainThread());
  result = InitMain();
  if (result != kMediaConduitNoError) {
    return result;
  }

  CSFLogError(LOGTAG, "%s Initialization Done", __FUNCTION__);
  return kMediaConduitNoError;
}

void
WebrtcVideoConduit::Destroy()
{
  // We can't delete the VideoEngine until all these are released!
  // And we can't use a Scoped ptr, since the order is arbitrary

  MutexAutoLock lock(mCodecMutex);
  DeleteSendStream();
  DeleteRecvStream();
}

void
WebrtcVideoConduit::SyncTo(WebrtcAudioConduit* aConduit)
{
  CSFLogDebug(LOGTAG, "%s Synced to %p", __FUNCTION__, aConduit);
  {
    MutexAutoLock lock(mCodecMutex);

    if (!mRecvStream) {
      CSFLogError(LOGTAG, "SyncTo called with no receive stream");
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
  CSFLogDebug(LOGTAG, "%s", __FUNCTION__);

  // null renderer
  if (!aVideoRenderer) {
    CSFLogError(LOGTAG, "%s NULL Renderer", __FUNCTION__);
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
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mTransmitterTransport = aTransport;
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::SetReceiverTransport(RefPtr<TransportInterface> aTransport)
{
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mReceiverTransport = aTransport;
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::ConfigureRecvMediaCodecs(
  const std::vector<VideoCodecConfig* >& codecConfigList)
{
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  MediaConduitErrorCode condError = kMediaConduitNoError;
  std::string payloadName;

  if (codecConfigList.empty()) {
    CSFLogError(LOGTAG, "%s Zero number of codecs to configure", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  webrtc::KeyFrameRequestMethod kf_request_method = webrtc::kKeyFrameReqPliRtcp;
  bool kf_request_enabled = false;
  bool use_nack_basic = false;
  bool use_tmmbr = false;
  bool use_remb = false;
  bool use_fec = false;
  int ulpfec_payload_type = kNullPayloadType;
  int red_payload_type = kNullPayloadType;
  bool configuredH264 = false;
  nsTArray<UniquePtr<VideoCodecConfig>> recv_codecs;

  // Try Applying the codecs in the list
  // we treat as success if at least one codec was applied and reception was
  // started successfully.
  std::set<unsigned int> codec_types_seen;
  for (const auto& codec_config : codecConfigList) {
    if ((condError = ValidateCodecConfig(codec_config))
        != kMediaConduitNoError) {
      CSFLogError(LOGTAG, "%s Invalid config for %s decoder: %i", __FUNCTION__,
                  codec_config ? codec_config->mName.c_str() : "<null>",
                  condError);
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
    // XXX (See upstream issue https://bugs.chromium.org/p/webrtc/issues/detail?id=7002):
    // There is no 'none' option in webrtc.org
    if (codec_config->RtcpFbNackIsSet("pli")) {
      kf_request_enabled = true;
      kf_request_method = webrtc::kKeyFrameReqPliRtcp;
    } else if (!kf_request_enabled && codec_config->RtcpFbCcmIsSet("fir")) {
      kf_request_enabled = true;
      kf_request_method = webrtc::kKeyFrameReqFirRtcp;
    }

    // What if codec A has Nack and REMB, and codec B has TMMBR, and codec C has none?
    // In practice, that's not a useful configuration, and VideoReceiveStream::Config can't
    // represent that, so simply union the (boolean) settings
    use_nack_basic |= codec_config->RtcpFbNackIsSet("");
    use_tmmbr |= codec_config->RtcpFbCcmIsSet("tmmbr");
    use_remb |= codec_config->RtcpFbRembIsSet();
    use_fec |= codec_config->RtcpFbFECIsSet();

    recv_codecs.AppendElement(new VideoCodecConfig(*codec_config));
  }

  if (!recv_codecs.Length()) {
    CSFLogError(LOGTAG, "%s Found no valid receive codecs", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  // Now decide if we need to recreate the receive stream, or can keep it
  if (!mRecvStream ||
      CodecsDifferent(recv_codecs, mRecvCodecList) ||
      mRecvStreamConfig.rtp.nack.rtp_history_ms != (use_nack_basic ? 1000 : 0) ||
      mRecvStreamConfig.rtp.remb != use_remb ||
      mRecvStreamConfig.rtp.tmmbr != use_tmmbr ||
      mRecvStreamConfig.rtp.keyframe_method != kf_request_method ||
      (use_fec &&
       (mRecvStreamConfig.rtp.ulpfec.ulpfec_payload_type != ulpfec_payload_type ||
        mRecvStreamConfig.rtp.ulpfec.red_payload_type != red_payload_type))) {

    condError = StopReceiving();
    if (condError != kMediaConduitNoError) {
      return condError;
    }

    // If we fail after here things get ugly
    mRecvStreamConfig.rtp.rtcp_mode = webrtc::RtcpMode::kCompound;
    mRecvStreamConfig.rtp.nack.rtp_history_ms = use_nack_basic ? 1000 : 0;
    mRecvStreamConfig.rtp.remb = use_remb;
    mRecvStreamConfig.rtp.tmmbr = use_tmmbr;
    mRecvStreamConfig.rtp.keyframe_method = kf_request_method;

    if (use_fec) {
      mRecvStreamConfig.rtp.ulpfec.ulpfec_payload_type = ulpfec_payload_type;
      mRecvStreamConfig.rtp.ulpfec.red_payload_type = red_payload_type;
      mRecvStreamConfig.rtp.ulpfec.red_rtx_payload_type = -1;
    } else {
      // Reset to defaults
      mRecvStreamConfig.rtp.ulpfec.ulpfec_payload_type = -1;
      mRecvStreamConfig.rtp.ulpfec.red_payload_type = -1;
      mRecvStreamConfig.rtp.ulpfec.red_rtx_payload_type = -1;
    }

    // SetRemoteSSRC should have populated this already
    mRecvSSRC = mRecvStreamConfig.rtp.remote_ssrc;

    // XXX ugh! same SSRC==0 problem that webrtc.org has
    if (mRecvSSRC == 0) {
      // Handle un-signalled SSRCs by creating a random one and then when it actually gets set,
      // we'll destroy and recreate.  Simpler than trying to unwind all the logic that assumes
      // the receive stream is created and started when we ConfigureRecvMediaCodecs()
      unsigned int ssrc;
      do {
        SECStatus rv = PK11_GenerateRandom(reinterpret_cast<unsigned char*>(&ssrc), sizeof(ssrc));
        if (rv != SECSuccess) {
          return kMediaConduitUnknownError;
        }
      } while (ssrc == 0); // webrtc.org code has fits if you select an SSRC of 0

      mRecvStreamConfig.rtp.remote_ssrc = ssrc;
      mRecvSSRC = ssrc;
    }

    // 0 isn't allowed.  Would be best to ask for a random SSRC from the
    // RTP code.  Would need to call rtp_sender.cc -- GenerateNewSSRC(),
    // which isn't exposed.  It's called on collision, or when we decide to
    // send.  it should be called on receiver creation.  Here, we're
    // generating the SSRC value - but this causes ssrc_forced in set in
    // rtp_sender, which locks us into the SSRC - even a collision won't
    // change it!!!
    MOZ_ASSERT(!mSendStreamConfig.rtp.ssrcs.empty());
    auto ssrc = mSendStreamConfig.rtp.ssrcs.front();
    Unused << NS_WARN_IF(ssrc == mRecvStreamConfig.rtp.remote_ssrc);

    while (ssrc == mRecvStreamConfig.rtp.remote_ssrc || ssrc == 0) {
      SECStatus rv = PK11_GenerateRandom(reinterpret_cast<unsigned char*>(&ssrc), sizeof(ssrc));
      if (rv != SECSuccess) {
        return kMediaConduitUnknownError;
      }
    }
    // webrtc.org code has fits if you select an SSRC of 0

    mRecvStreamConfig.rtp.local_ssrc = ssrc;
    CSFLogDebug(LOGTAG, "%s (%p): Local SSRC 0x%08x (of %u), remote SSRC 0x%08x",
                __FUNCTION__, (void*) this, ssrc,
                (uint32_t) mSendStreamConfig.rtp.ssrcs.size(),
                mRecvStreamConfig.rtp.remote_ssrc);

    // XXX Copy over those that are the same and don't rebuild them
    mRecvCodecList.SwapElements(recv_codecs);
    recv_codecs.Clear();
    mRecvStreamConfig.rtp.rtx.clear();

    {
      MutexAutoLock lock(mCodecMutex);
      DeleteRecvStream();
      // Rebuilds mRecvStream from mRecvStreamConfig
      MediaConduitErrorCode rval = CreateRecvStream();
      if (rval != kMediaConduitNoError) {
        CSFLogError(LOGTAG, "%s Start Receive Error %d ", __FUNCTION__, rval);
        return rval;
      }
    }
    return StartReceiving();
  }
  return kMediaConduitNoError;
}

webrtc::VideoDecoder*
WebrtcVideoConduit::CreateDecoder(webrtc::VideoCodecType aType)
{
  webrtc::VideoDecoder* decoder = nullptr;
#ifdef MOZ_WEBRTC_MEDIACODEC
  bool enabled = false;
#endif

  // Attempt to create a decoder using MediaDataDecoder.
  decoder = MediaDataDecoderCodec::CreateDecoder(aType);
  if (decoder) {
    return decoder;
  }

  switch (aType) {
    case webrtc::VideoCodecType::kVideoCodecH264:
      // get an external decoder
      decoder = GmpVideoCodec::CreateDecoder();
      if (decoder) {
        mRecvCodecPlugin = static_cast<WebrtcVideoDecoder*>(decoder);
      }
      break;

    case webrtc::VideoCodecType::kVideoCodecVP8:
#ifdef MOZ_WEBRTC_MEDIACODEC
      // attempt to get a decoder
      enabled = mozilla::Preferences::GetBool(
        "media.navigator.hardware.vp8_decode.acceleration_enabled", false);
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
#endif
      // Use a software VP8 decoder as a fallback.
      if (!decoder) {
        decoder = webrtc::VP8Decoder::Create();
      }
      break;

    case webrtc::VideoCodecType::kVideoCodecVP9:
      MOZ_ASSERT(webrtc::VP9Decoder::IsSupported());
      decoder = webrtc::VP9Decoder::Create();
      break;

    default:
      break;
  }

  return decoder;
}

webrtc::VideoEncoder*
WebrtcVideoConduit::CreateEncoder(webrtc::VideoCodecType aType,
                                  bool enable_simulcast)
{
  webrtc::VideoEncoder* encoder = nullptr;
#ifdef MOZ_WEBRTC_MEDIACODEC
  bool enabled = false;
#endif

  switch (aType) {
    case webrtc::VideoCodecType::kVideoCodecH264:
      // get an external encoder
      encoder = GmpVideoCodec::CreateEncoder();
      if (encoder) {
        mSendCodecPlugin = static_cast<WebrtcVideoEncoder*>(encoder);
      }
      break;

    case webrtc::VideoCodecType::kVideoCodecVP8:
#ifdef MOZ_WEBRTC_MEDIACODEC
      // attempt to get a encoder
      enabled = mozilla::Preferences::GetBool(
        "media.navigator.hardware.vp8_encode.acceleration_enabled", false);
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
#endif
      // Use a software VP8 encoder as a fallback.
      if (!encoder) {
        encoder = webrtc::VP8Encoder::Create();
      }
      break;

    case webrtc::VideoCodecType::kVideoCodecVP9:
      encoder = webrtc::VP9Encoder::Create();
      break;

    default:
      break;
  }
  return encoder;
}

struct ResolutionAndBitrateLimits
{
  int resolution_in_mb;
  int min_bitrate_bps;
  int start_bitrate_bps;
  int max_bitrate_bps;
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
  unsigned short width, unsigned short height, int cap,
  int32_t aLastFramerateTenths,
  webrtc::VideoStream& aVideoStream)
{
  int& out_min = aVideoStream.min_bitrate_bps;
  int& out_start = aVideoStream.target_bitrate_bps;
  int& out_max = aVideoStream.max_bitrate_bps;
  // max bandwidth should be proportional (not linearly!) to resolution, and
  // proportional (perhaps linearly, or close) to current frame rate.
  int fs = MB_OF(width, height);

  for (ResolutionAndBitrateLimits resAndLimits : kResolutionAndBitrateLimits) {
    if (fs > resAndLimits.resolution_in_mb &&
        // pick the highest range where at least start rate is within cap
        // (or if we're at the end of the array).
        (!cap || resAndLimits.start_bitrate_bps <= cap ||
         resAndLimits.resolution_in_mb == 0)) {
      out_min = MinIgnoreZero(resAndLimits.min_bitrate_bps, cap);
      out_start = MinIgnoreZero(resAndLimits.start_bitrate_bps, cap);
      out_max = MinIgnoreZero(resAndLimits.max_bitrate_bps, cap);
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
    out_max = std::max(static_cast<int>(out_max * (framerate / 30)), cap);
  } else {
    // At low framerates, don't reduce bandwidth as much - cut slope to 1/2.
    // Mostly this would be ultra-low-light situations/mobile or screensharing.
    out_min = out_min * ((10 - (framerate / 2)) / 30);
    out_start = out_start * ((10 - (framerate / 2)) / 30);
    out_max = std::max(static_cast<int>(out_max * ((10 - (framerate / 2)) / 30)), cap);
  }

  // Note: mNegotiatedMaxBitrate is the max transport bitrate - it applies to
  // a single codec encoding, but should also apply to the sum of all
  // simulcast layers in this encoding!  So sum(layers.maxBitrate) <=
  // mNegotiatedMaxBitrate
  // Note that out_max already has had mPrefMaxBitrate applied to it
  out_max = MinIgnoreZero((int)mNegotiatedMaxBitrate, out_max);
  out_min = std::min(out_min, out_max);
  out_start = std::min(out_start, out_max);

  if (mMinBitrate && mMinBitrate > out_min) {
    out_min = mMinBitrate;
  }
  // If we try to set a minimum bitrate that is too low, ViE will reject it.
  out_min = std::max(kViEMinCodecBitrate_bps, out_min);
  out_max = std::max(kViEMinCodecBitrate_bps, out_max);
  if (mStartBitrate && mStartBitrate > out_start) {
    out_start = mStartBitrate;
  }

  // Ensure that min <= start <= max
  if (out_min > out_max) {
    out_min = out_max;
  }
  out_start = std::min(out_max, std::max(out_start, out_min));

  MOZ_ASSERT(mPrefMaxBitrate == 0 || out_max <= mPrefMaxBitrate);
}

// XXX we need to figure out how to feed back changes in preferred capture
// resolution to the getUserMedia source.
// Returns boolean if we've submitted an async change (and took ownership
// of *frame's data)
bool
WebrtcVideoConduit::SelectSendResolution(unsigned short width,
                                         unsigned short height,
                                         const webrtc::VideoFrame* frame) // may be null
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

    // Limit resolution to max-fs
    if (mCurSendCodecConfig->mEncodingConstraints.maxFs) {
      // max-fs is in macroblocks, convert to pixels
      int max_fs(mCurSendCodecConfig->mEncodingConstraints.maxFs*(16*16));
      if (max_fs > mLastSinkWanted.max_pixel_count.value_or(max_fs)) {
        max_fs = mLastSinkWanted.max_pixel_count.value_or(max_fs);
      }
      mVideoAdapter->OnResolutionRequest(rtc::Optional<int>(max_fs),
                                         rtc::Optional<int>());
    }
  }

  // Adapt to getUserMedia resolution changes
  // check if we need to reconfigure the sending resolution.
  // NOTE: mSendingWidth != mLastWidth, because of maxwidth/height/etc above
  bool changed = false;
  if (mSendingWidth != width || mSendingHeight != height) {
    CSFLogDebug(LOGTAG, "%s: resolution changing to %ux%u (from %ux%u)",
                __FUNCTION__, width, height, mSendingWidth, mSendingHeight);
    // This will avoid us continually retrying this operation if it fails.
    // If the resolution changes, we'll try again.  In the meantime, we'll
    // keep using the old size in the encoder.
    mSendingWidth = width;
    mSendingHeight = height;
    changed = true;
  }

  unsigned int framerate = SelectSendFrameRate(mCurSendCodecConfig,
                                               mSendingFramerate,
                                               mSendingWidth,
                                               mSendingHeight);
  if (mSendingFramerate != framerate) {
    CSFLogDebug(LOGTAG, "%s: framerate changing to %u (from %u)",
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
        // the internal buffer pointer is refcounted, so we don't have 2 copies here
        new_frame = new webrtc::VideoFrame(*frame);
      }
      RefPtr<WebrtcVideoConduit> self(this);
      RefPtr<Runnable> webrtc_runnable =
        media::NewRunnableFrom([self, width, height, new_frame]() -> nsresult {
            UniquePtr<webrtc::VideoFrame> local_frame(new_frame); // Simplify cleanup

            MutexAutoLock lock(self->mCodecMutex);
            return self->ReconfigureSendCodec(width, height, new_frame);
          });
      // new_frame now owned by lambda
      CSFLogDebug(LOGTAG, "%s: proxying lambda to WebRTC thread for reconfig (width %u/%u, height %u/%u",
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
                                         const webrtc::VideoFrame* frame)
{
  mCodecMutex.AssertCurrentThreadOwns();

  // Test in case the stream hasn't started yet!  We could get a frame in
  // before we get around to StartTransmitting(), and that would dispatch a
  // runnable to call this.
  mInReconfig = false;
  if (mSendStream) {
    mSendStream->ReconfigureVideoEncoder(mEncoderConfig.CopyConfig());
    if (frame) {
      mVideoBroadcaster.OnFrame(*frame);
      CSFLogDebug(LOGTAG, "%s Inserted a frame from reconfig lambda", __FUNCTION__);
    }
  }
  return NS_OK;
}

unsigned int
WebrtcVideoConduit::SelectSendFrameRate(const VideoCodecConfig* codecConfig,
                                        unsigned int old_framerate,
                                        unsigned short sending_width,
                                        unsigned short sending_height) const
{
  unsigned int new_framerate = old_framerate;

  // Limit frame rate based on max-mbps
  if (codecConfig && codecConfig->mEncodingConstraints.maxMbps)
  {
    unsigned int cur_fs, mb_width, mb_height;

    mb_width = (sending_width + 15) >> 4;
    mb_height = (sending_height + 15) >> 4;

    cur_fs = mb_width * mb_height;
    if (cur_fs > 0) { // in case no frames have been sent
      new_framerate = codecConfig->mEncodingConstraints.maxMbps / cur_fs;

      new_framerate = MinIgnoreZero(new_framerate, codecConfig->mEncodingConstraints.maxFps);
    }
  }
  return new_framerate;
}

MediaConduitErrorCode
WebrtcVideoConduit::SendVideoFrame(const unsigned char* video_buffer,
                                   unsigned int video_length,
                                   unsigned short width,
                                   unsigned short height,
                                   VideoType video_type,
                                   uint64_t capture_time)
{
  // check for parameter sanity
  if (!video_buffer || video_length == 0 || width == 0 || height == 0) {
    CSFLogError(LOGTAG, "%s Invalid Parameters ", __FUNCTION__);
    MOZ_ASSERT(false);
    return kMediaConduitMalformedArgument;
  }
  MOZ_ASSERT(video_type == VideoType::kVideoI420);

  // Transmission should be enabled before we insert any frames.
  if (!mEngineTransmitting) {
    CSFLogError(LOGTAG, "%s Engine not transmitting ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  // insert the frame to video engine in I420 format only
  const int stride_y = width;
  const int stride_uv = (width + 1) / 2;

  const uint8_t* buffer_y = video_buffer;
  const uint8_t* buffer_u = buffer_y + stride_y * height;
  const uint8_t* buffer_v = buffer_u + stride_uv * ((height + 1) / 2);
  rtc::Callback0<void> callback_unused;
  rtc::scoped_refptr<webrtc::WrappedI420Buffer> video_frame_buffer(
    new rtc::RefCountedObject<webrtc::WrappedI420Buffer>(
      width, height,
      buffer_y, stride_y,
      buffer_u, stride_uv,
      buffer_v, stride_uv,
      callback_unused));

  webrtc::VideoFrame video_frame(video_frame_buffer, capture_time,
                                 capture_time, webrtc::kVideoRotation_0); // XXX

  return SendVideoFrame(video_frame);
}

void
WebrtcVideoConduit::AddOrUpdateSink(
  rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
  const rtc::VideoSinkWants& wants)
{
  CSFLogDebug(LOGTAG, "%s (send SSRC %u (0x%x)) - wants pixels = %d/%d", __FUNCTION__,
              mSendStreamConfig.rtp.ssrcs.front(), mSendStreamConfig.rtp.ssrcs.front(),
              wants.max_pixel_count ? *wants.max_pixel_count : -1,
              wants.max_pixel_count_step_up ? *wants.max_pixel_count_step_up : -1);

  // MUST run on the same thread as first call (MainThread)
  if (!NS_IsMainThread()) {
    // This can be asynchronous
    RefPtr<WebrtcVideoConduit> self(this);
    NS_DispatchToMainThread(media::NewRunnableFrom([self, sink, wants]() {
          self->mVideoBroadcaster.AddOrUpdateSink(sink, wants);
          self->OnSinkWantsChanged(self->mVideoBroadcaster.wants());
          return NS_OK;
        }));
  } else {
    mVideoBroadcaster.AddOrUpdateSink(sink, wants);
    OnSinkWantsChanged(mVideoBroadcaster.wants());
  }
}

void
WebrtcVideoConduit::RemoveSink(
  rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
  mVideoBroadcaster.RemoveSink(sink);
  OnSinkWantsChanged(mVideoBroadcaster.wants());
}

void
WebrtcVideoConduit::OnSinkWantsChanged(
  const rtc::VideoSinkWants& wants) {
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  if (!mLockScaling) {
    mLastSinkWanted = wants;

    // limit sink wants based upon max-fs constraint
    int max_fs = mCurSendCodecConfig->mEncodingConstraints.maxFs*(16*16);
    rtc::Optional<int> max_pixel_count = wants.max_pixel_count;
    rtc::Optional<int> max_pixel_count_step_up = wants.max_pixel_count_step_up;

    if (max_fs > 0) {
      // max_fs was explicitly set by signaling and needs to be accounted for

      if (max_pixel_count.value_or(max_fs) > max_fs) {
        max_pixel_count = rtc::Optional<int>(max_fs);
      }

      if (max_pixel_count_step_up.value_or(max_fs) > max_fs) {
        max_pixel_count_step_up = rtc::Optional<int>(max_fs);
      }
    }

    mVideoAdapter->OnResolutionRequest(max_pixel_count,
                                       max_pixel_count_step_up);
  }
}

MediaConduitErrorCode
WebrtcVideoConduit::SendVideoFrame(const webrtc::VideoFrame& frame)
{
  // XXX Google uses a "timestamp_aligner" to translate timestamps from the
  // camera via TranslateTimestamp(); we should look at doing the same.  This
  // avoids sampling error when capturing frames, but google had to deal with some
  // broken cameras, include Logitech c920's IIRC.

  CSFLogVerbose(LOGTAG, "%s (send SSRC %u (0x%x))", __FUNCTION__,
              mSendStreamConfig.rtp.ssrcs.front(), mSendStreamConfig.rtp.ssrcs.front());
  // See if we need to recalculate what we're sending.
  // Don't compute mSendingWidth/Height, since those may not be the same as the input.
  {
    MutexAutoLock lock(mCodecMutex);
    if (mInReconfig) {
      // Waiting for it to finish
      return kMediaConduitNoError;
    }
    // mLastWidth/Height starts at 0, so we'll never call SelectSendResolution with a 0 size.
    // We in some cases set them back to 0 to force SelectSendResolution to be called again.
    if (frame.width() != mLastWidth || frame.height() != mLastHeight) {
      CSFLogVerbose(LOGTAG, "%s: call SelectSendResolution with %ux%u",
                    __FUNCTION__, frame.width(), frame.height());
      MOZ_ASSERT(frame.width() != 0 && frame.height() != 0);
      // Note coverity will flag this since it thinks they can be 0
      if (SelectSendResolution(frame.width(), frame.height(), &frame)) {
        // SelectSendResolution took ownership of the data in i420_frame.
        // Submit the frame after reconfig is done
        return kMediaConduitNoError;
      }
    }
    // adapt input video to wants of sink
    if (!mVideoBroadcaster.frame_wanted()) {
      return kMediaConduitNoError;
    }

    int adapted_width;
    int adapted_height;
    int crop_width;
    int crop_height;
    int crop_x;
    int crop_y;
    if (!mVideoAdapter->AdaptFrameResolution(
          frame.width(), frame.height(),
          frame.timestamp_us() * rtc::kNumNanosecsPerMicrosec,
          &crop_width, &crop_height, &adapted_width, &adapted_height)) {
      // VideoAdapter dropped the frame.
      return kMediaConduitNoError;
    }
    crop_x = (frame.width() - crop_width) / 2;
    crop_y = (frame.height() - crop_height) / 2;

    rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer;
    if (adapted_width == frame.width() && adapted_height == frame.height()) {
      // No adaption - optimized path.
      buffer = frame.video_frame_buffer();
      // XXX Bug 1367651 - Use nativehandles where possible instead of software scaling
#ifdef WEBRTC_MAC
#if defined(MAC_OS_X_VERSION_10_8) && \
  (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_8)
      // XXX not available in Mac 10.7 SDK
      // code adapted from objvideotracksource.mm
    } else if (frame.video_frame_buffer()->native_handle()) {
      // Adapted CVPixelBuffer frame.
      buffer = new rtc::RefCountedObject<webrtc::CoreVideoFrameBuffer>(
        static_cast<CVPixelBufferRef>(frame.video_frame_buffer()->native_handle()), adapted_width, adapted_height,
        crop_width, crop_height, crop_x, crop_y);
#endif
#elif WEBRTC_WIN
      // XX FIX
#elif WEBRTC_LINUX
      // XX FIX
#elif WEBRTC_ANDROID
      // XX FIX
#endif
    } else {
      // Adapted I420 frame.
      // TODO(magjed): Optimize this I420 path.
      rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer =
        webrtc::I420Buffer::Create(adapted_width, adapted_height);
      i420_buffer->CropAndScaleFrom(*frame.video_frame_buffer(), crop_x, crop_y, crop_width, crop_height);
      buffer = i420_buffer;
    }

#if 0
    // Applying rotation is only supported for legacy reasons and performance is
    // not critical here.
    // XXX We're rotating at capture time; if we want to change that we'll need to
    // rotate at input to any sink that can't handle rotated frames internally. We
    // probably wouldn't need to rotate here unless the CVO extension wasn't agreed to.
    // That state (CVO) would feed apply_rotation()
    webrtc::VideoRotation rotation = static_cast<webrtc::VideoRotation>(frame.rotation);
    if (apply_rotation() && rotation != kVideoRotation_0) {
      buffer = I420Buffer::Rotate(*buffer->NativeToI420Buffer(), rotation);
      rotation = kVideoRotation_0;
    }
#endif

    mVideoBroadcaster.OnFrame(webrtc::VideoFrame(buffer, webrtc::kVideoRotation_0,
                                                 /*rotation, translated_*/ frame.timestamp_us()));
  }

  mSendStreamStats.FrameDeliveredToEncoder();
  return kMediaConduitNoError;
}

// Transport Layer Callbacks

MediaConduitErrorCode
WebrtcVideoConduit::DeliverPacket(const void* data, int len)
{
  // Media Engine should be receiving already.
  if (!mCall) {
    CSFLogError(LOGTAG, "Error: %s when not receiving", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  // XXX we need to get passed the time the packet was received
  webrtc::PacketReceiver::DeliveryStatus status =
    mCall->Call()->Receiver()->DeliverPacket(webrtc::MediaType::VIDEO,
                                             static_cast<const uint8_t*>(data),
                                             len, webrtc::PacketTime());

  if (status != webrtc::PacketReceiver::DELIVERY_OK) {
    CSFLogError(LOGTAG, "%s DeliverPacket Failed, %d", __FUNCTION__, status);
    return kMediaConduitRTPProcessingFailed;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::ReceivedRTPPacket(const void* data, int len, uint32_t ssrc)
{
  // Handle the unknown ssrc (and ssrc-not-signaled case).
  // We can't just do this here; it has to happen on MainThread :-(
  // We also don't want to drop the packet, nor stall this thread, so we hold
  // the packet (and any following) for inserting once the SSRC is set.
  bool queue = mRecvSSRCSetInProgress;
  if (queue || mRecvSSRC != ssrc) {
    // capture packet for insertion after ssrc is set -- do this before
    // sending the runnable, since it may pull from this.  Since it
    // dispatches back to us, it's less critial to do this here, but doesn't
    // hurt.
    UniquePtr<QueuedPacket> packet((QueuedPacket*) malloc(sizeof(QueuedPacket) + len-1));
    packet->mLen = len;
    memcpy(packet->mData, data, len);
    CSFLogDebug(LOGTAG, "queuing packet: seq# %u, Len %d ",
                (uint16_t)ntohs(((uint16_t*) packet->mData)[1]), packet->mLen);
    if (queue) {
      mQueuedPackets.AppendElement(Move(packet));
      return kMediaConduitNoError;
    }
    // a new switch needs to be done
    // any queued packets are from a previous switch that hasn't completed
    // yet; drop them and only process the latest SSRC
    mQueuedPackets.Clear();
    mQueuedPackets.AppendElement(Move(packet));

    CSFLogDebug(LOGTAG, "%s: switching from SSRC %u to %u", __FUNCTION__,
                mRecvSSRC, ssrc);
    // we "switch" here immediately, but buffer until the queue is released
    mRecvSSRC = ssrc;
    mRecvSSRCSetInProgress = true;
    queue = true;

    // Ensure lamba captures refs
    RefPtr<WebrtcVideoConduit> self = this;
    nsCOMPtr<nsIThread> thread;
    if (NS_WARN_IF(NS_FAILED(NS_GetCurrentThread(getter_AddRefs(thread))))) {
      return kMediaConduitRTPProcessingFailed;
    }
    NS_DispatchToMainThread(media::NewRunnableFrom([self, thread, ssrc]() mutable {
          // Normally this is done in CreateOrUpdateMediaPipeline() for
          // initial creation and renegotiation, but here we're rebuilding the
          // Receive channel at a lower level.  This is needed whenever we're
          // creating a GMPVideoCodec (in particular, H264) so it can communicate
          // errors to the PC.
          WebrtcGmpPCHandleSetter setter(self->mPCHandle);
          self->SetRemoteSSRC(ssrc); // this will likely re-create the VideoReceiveStream
          // We want to unblock the queued packets on the original thread
          thread->Dispatch(media::NewRunnableFrom([self, ssrc]() mutable {
                if (ssrc == self->mRecvSSRC) {
                  // SSRC is set; insert queued packets
                  for (auto& packet : self->mQueuedPackets) {
                    CSFLogDebug(LOGTAG, "Inserting queued packets: seq# %u, Len %d ",
                                (uint16_t)ntohs(((uint16_t*) packet->mData)[1]), packet->mLen);

                    if (self->DeliverPacket(packet->mData, packet->mLen) != kMediaConduitNoError) {
                      CSFLogError(LOGTAG, "%s RTP Processing Failed", __FUNCTION__);
                      // Keep delivering and then clear the queue
                    }
                  }
                  self->mQueuedPackets.Clear();
                  // we don't leave inprogress until there are no changes in-flight
                  self->mRecvSSRCSetInProgress = false;
                }
                // else this is an intermediate switch; another is in-flight

                return NS_OK;
              }), NS_DISPATCH_NORMAL);
          return NS_OK;
        }));
    return kMediaConduitNoError;
  }

  CSFLogVerbose(LOGTAG, "%s: seq# %u, Len %d, SSRC %u (0x%x) ", __FUNCTION__,
                (uint16_t)ntohs(((uint16_t*) data)[1]), len,
                (uint32_t) ntohl(((uint32_t*) data)[2]),
                (uint32_t) ntohl(((uint32_t*) data)[2]));

  if (DeliverPacket(data, len) != kMediaConduitNoError) {
    CSFLogError(LOGTAG, "%s RTP Processing Failed", __FUNCTION__);
    return kMediaConduitRTPProcessingFailed;
  }
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::ReceivedRTCPPacket(const void* data, int len)
{
  CSFLogVerbose(LOGTAG, " %s Len %d ", __FUNCTION__, len);

  if (DeliverPacket(data, len) != kMediaConduitNoError) {
    CSFLogError(LOGTAG, "%s RTCP Processing Failed", __FUNCTION__);
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
          CSFLogDebug(LOGTAG, "%s Engine Already Sending. Attemping to Stop ", __FUNCTION__);
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

  CSFLogDebug(LOGTAG, "%s Attemping to start... ", __FUNCTION__);
  {
    // Start Transmitting on the video engine
    MutexAutoLock lock(mCodecMutex);

    if (!mSendStream) {
      MediaConduitErrorCode rval = CreateSendStream();
      if (rval != kMediaConduitNoError) {
        CSFLogError(LOGTAG, "%s Start Send Error %d ", __FUNCTION__, rval);
        return rval;
      }
    }

    mSendStream->Start();
    // XXX File a bug to consider hooking this up to the state of mtransport
    mCall->Call()->SignalChannelNetworkState(webrtc::MediaType::VIDEO, webrtc::kNetworkUp);
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
    CSFLogDebug(LOGTAG, "%s Engine Already Receiving . Attemping to Stop ", __FUNCTION__);
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

  CSFLogDebug(LOGTAG, "%s Attemping to start... (SSRC %u (0x%x))", __FUNCTION__, mRecvSSRC, mRecvSSRC);
  {
    // Start Receive on the video engine
    MutexAutoLock lock(mCodecMutex);
    MOZ_ASSERT(mRecvStream);

    mRecvStream->Start();
    // XXX File a bug to consider hooking this up to the state of mtransport
    mCall->Call()->SignalChannelNetworkState(webrtc::MediaType::VIDEO, webrtc::kNetworkUp);
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
  CSFLogVerbose(LOGTAG, "%s Sent RTP Packet seq %d, len %lu, SSRC %u (0x%x)",
                __FUNCTION__,
                (uint16_t) ntohs(*((uint16_t*) &packet[2])),
                (unsigned long)length,
                (uint32_t) ntohl(*((uint32_t*) &packet[8])),
                (uint32_t) ntohl(*((uint32_t*) &packet[8])));

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if (!mTransmitterTransport ||
     NS_FAILED(mTransmitterTransport->SendRtpPacket(packet, length)))
  {
    CSFLogError(LOGTAG, "%s RTP Packet Send Failed ", __FUNCTION__);
    return false;
  }
  return true;
}

// Called from multiple threads including webrtc Process thread
bool
WebrtcVideoConduit::SendRtcp(const uint8_t* packet, size_t length)
{
  CSFLogVerbose(LOGTAG, "%s : len %lu ", __FUNCTION__, (unsigned long)length);
  // We come here if we have only one pipeline/conduit setup,
  // such as for unidirectional streams.
  // We also end up here if we are receiving
  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if (mReceiverTransport &&
      NS_SUCCEEDED(mReceiverTransport->SendRtcpPacket(packet, length)))
  {
    // Might be a sender report, might be a receiver report, we don't know.
    CSFLogDebug(LOGTAG, "%s Sent RTCP Packet ", __FUNCTION__);
    return true;
  }
  if (mTransmitterTransport &&
             NS_SUCCEEDED(mTransmitterTransport->SendRtcpPacket(packet, length))) {
    return true;
  }

  CSFLogError(LOGTAG, "%s RTCP Packet Send Failed ", __FUNCTION__);
  return false;
}

void
WebrtcVideoConduit::OnFrame(const webrtc::VideoFrame& video_frame)
{
  CSFLogVerbose(LOGTAG, "%s: recv SSRC %u (0x%x), size %ux%u", __FUNCTION__,
                mRecvSSRC, mRecvSSRC, video_frame.width(), video_frame.height());
  ReentrantMonitorAutoEnter enter(mTransportMonitor);

  if (!mRenderer) {
    CSFLogError(LOGTAG, "%s Renderer is NULL  ", __FUNCTION__);
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
                                 const_cast<unsigned char*>(video_frame.video_frame_buffer()->DataY()),
                                 reinterpret_cast<unsigned char*>(&timestamp),
                                 sizeof(timestamp), 0, 0);
    if (ok) {
      VideoLatencyUpdate(now - timestamp);
    }
  }

  mRenderer->RenderVideoFrame(*video_frame.video_frame_buffer(),
                              video_frame.timestamp(),
                              video_frame.render_time_ms());
}

// Compare lists of codecs
bool
WebrtcVideoConduit::CodecsDifferent(const nsTArray<UniquePtr<VideoCodecConfig>>& a,
                                    const nsTArray<UniquePtr<VideoCodecConfig>>& b)
{
  // return a != b;
  // would work if UniquePtr<> operator== compared contents!
  auto len = a.Length();
  if (len != b.Length()) {
    return true;
  }

  // XXX std::equal would work, if we could use it on this - fails for the
  // same reason as above.  c++14 would let us pass a comparator function.
  for (uint32_t i = 0; i < len; ++i) {
    if (!(*a[i] == *b[i])) {
      return true;
    }
  }

  return false;
}

/**
 * Perform validation on the codecConfig to be applied
 * Verifies if the codec is already applied.
 */
MediaConduitErrorCode
WebrtcVideoConduit::ValidateCodecConfig(const VideoCodecConfig* codecInfo)
{
  if(!codecInfo) {
    CSFLogError(LOGTAG, "%s Null CodecConfig ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  if((codecInfo->mName.empty()) ||
     (codecInfo->mName.length() >= CODEC_PLNAME_SIZE)) {
    CSFLogError(LOGTAG, "%s Invalid Payload Name Length ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  return kMediaConduitNoError;
}

void
WebrtcVideoConduit::DumpCodecDB() const
{
  for (auto& entry : mRecvCodecList) {
    CSFLogDebug(LOGTAG, "Payload Name: %s", entry->mName.c_str());
    CSFLogDebug(LOGTAG, "Payload Type: %d", entry->mType);
    CSFLogDebug(LOGTAG, "Payload Max Frame Size: %d", entry->mEncodingConstraints.maxFs);
    CSFLogDebug(LOGTAG, "Payload Max Frame Rate: %d", entry->mEncodingConstraints.maxFps);
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
  }
  if (mRecvCodecPlugin) {
    return mRecvCodecPlugin->PluginID();
  }

  return 0;
}

bool
WebrtcVideoConduit::RequiresNewSendStream(const VideoCodecConfig& newConfig) const
{
  return !mCurSendCodecConfig
    || mCurSendCodecConfig->mName != newConfig.mName
    || mCurSendCodecConfig->mType != newConfig.mType
    || mCurSendCodecConfig->RtcpFbNackIsSet("") != newConfig.RtcpFbNackIsSet("")
    || mCurSendCodecConfig->RtcpFbFECIsSet() != newConfig.RtcpFbFECIsSet()
#if 0
    // XXX Do we still want/need to do this?
    || (newConfig.mName == "H264" &&
        !CompatibleH264Config(mEncoderSpecificH264, newConfig))
#endif
    ;
}

void
WebrtcVideoConduit::VideoEncoderConfigBuilder::SetEncoderSpecificSettings(
  rtc::scoped_refptr<webrtc::VideoEncoderConfig::EncoderSpecificSettings> aSettings)
{
  mConfig.encoder_specific_settings = aSettings;
}

void
WebrtcVideoConduit::VideoEncoderConfigBuilder::SetVideoStreamFactory(rtc::scoped_refptr<WebrtcVideoConduit::VideoStreamFactory> aFactory)
{
  mConfig.video_stream_factory = aFactory;
}

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
WebrtcVideoConduit::VideoEncoderConfigBuilder::SetMaxEncodings(
  size_t aMaxStreams)
{
  mConfig.number_of_streams = aMaxStreams;
}

void
WebrtcVideoConduit::VideoEncoderConfigBuilder::AddStream(
  webrtc::VideoStream aStream)
{
  mSimulcastStreams.push_back(SimulcastStreamConfig());
  MOZ_ASSERT(mSimulcastStreams.size() <= mConfig.number_of_streams);
}

void
WebrtcVideoConduit::VideoEncoderConfigBuilder::AddStream(
  webrtc::VideoStream aStream, const SimulcastStreamConfig& aSimulcastConfig)
{
  mSimulcastStreams.push_back(aSimulcastConfig);
  MOZ_ASSERT(mSimulcastStreams.size() <= mConfig.number_of_streams);
}

size_t
WebrtcVideoConduit::VideoEncoderConfigBuilder::StreamCount() const
{
  return mSimulcastStreams.size();
}

void
WebrtcVideoConduit::VideoEncoderConfigBuilder::ClearStreams()
{
  mSimulcastStreams.clear();
}

} // end namespace
