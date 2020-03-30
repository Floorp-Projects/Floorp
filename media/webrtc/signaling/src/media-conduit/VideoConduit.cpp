/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"
#include "nspr.h"
#include "plstr.h"

#include "AudioConduit.h"
#include "VideoConduit.h"
#include "VideoStreamFactory.h"
#include "YuvStamper.h"
#include "mozilla/TemplateLib.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/UniquePtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIPrefBranch.h"
#include "nsIGfxInfo.h"
#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"

#include "nsThreadUtils.h"

#include "pk11pub.h"

#include "api/video_codecs/sdp_video_format.h"
#include "media/engine/vp8_encoder_simulcast_proxy.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#include "webrtc/common_video/include/video_frame_buffer.h"

#include "mozilla/Unused.h"

#if defined(MOZ_WIDGET_ANDROID)
#  include "VideoEngine.h"
#endif

#include "GmpVideoCodec.h"

#ifdef MOZ_WEBRTC_MEDIACODEC
#  include "MediaCodecVideoCodec.h"
#endif
#include "WebrtcGmpVideoCodec.h"

#include "MediaDataCodec.h"

// for ntohs
#ifdef _MSC_VER
#  include "Winsock2.h"
#else
#  include <netinet/in.h>
#endif

#include <algorithm>
#include <math.h>
#include <cinttypes>

#define DEFAULT_VIDEO_MAX_FRAMERATE 30
#define INVALID_RTP_PAYLOAD 255  // valid payload types are 0 to 127

namespace mozilla {

static const char* vcLogTag = "WebrtcVideoSessionConduit";
#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG vcLogTag

using LocalDirection = MediaSessionConduitLocalDirection;

static const int kNullPayloadType = -1;
static const char* kUlpFecPayloadName = "ulpfec";
static const char* kRedPayloadName = "red";

// The number of frame buffers WebrtcVideoConduit may create before returning
// errors.
// Sometimes these are released synchronously but they can be forwarded all the
// way to the encoder for asynchronous encoding. With a pool size of 5,
// we allow 1 buffer for the current conversion, and 4 buffers to be queued at
// the encoder.
#define SCALER_BUFFER_POOL_SIZE 5

// The pixel alignment to use for the highest resolution layer when simulcast
// is active and one or more layers are being scaled.
#define SIMULCAST_RESOLUTION_ALIGNMENT 16

// 32 bytes is what WebRTC CodecInst expects
const unsigned int WebrtcVideoConduit::CODEC_PLNAME_SIZE = 32;

template <typename T>
T MinIgnoreZero(const T& a, const T& b) {
  return std::min(a ? a : b, b ? b : a);
}

template <class t>
static void ConstrainPreservingAspectRatioExact(uint32_t max_fs, t* width,
                                                t* height) {
  // We could try to pick a better starting divisor, but it won't make any real
  // performance difference.
  for (size_t d = 1; d < std::min(*width, *height); ++d) {
    if ((*width % d) || (*height % d)) {
      continue;  // Not divisible
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
static void ConstrainPreservingAspectRatio(uint16_t max_width,
                                           uint16_t max_height, t* width,
                                           t* height) {
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

/**
 * Function to select and change the encoding frame rate based on incoming frame
 * rate and max-mbps setting.
 * @param current framerate
 * @result new framerate
 */
static unsigned int SelectSendFrameRate(const VideoCodecConfig* codecConfig,
                                        unsigned int old_framerate,
                                        unsigned short sending_width,
                                        unsigned short sending_height) {
  unsigned int new_framerate = old_framerate;

  // Limit frame rate based on max-mbps
  if (codecConfig && codecConfig->mEncodingConstraints.maxMbps) {
    unsigned int cur_fs, mb_width, mb_height;

    mb_width = (sending_width + 15) >> 4;
    mb_height = (sending_height + 15) >> 4;

    cur_fs = mb_width * mb_height;
    if (cur_fs > 0) {  // in case no frames have been sent
      new_framerate = codecConfig->mEncodingConstraints.maxMbps / cur_fs;

      new_framerate = MinIgnoreZero(new_framerate,
                                    codecConfig->mEncodingConstraints.maxFps);
    }
  }
  return new_framerate;
}

/**
 * Perform validation on the codecConfig to be applied
 */
static MediaConduitErrorCode ValidateCodecConfig(
    const VideoCodecConfig* codecInfo) {
  if (!codecInfo) {
    CSFLogError(LOGTAG, "%s Null CodecConfig ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  if ((codecInfo->mName.empty()) ||
      (codecInfo->mName.length() >= WebrtcVideoConduit::CODEC_PLNAME_SIZE)) {
    CSFLogError(LOGTAG, "%s Invalid Payload Name Length ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  return kMediaConduitNoError;
}

void WebrtcVideoConduit::CallStatistics::Update(
    const webrtc::Call::Stats& aStats) {
  ASSERT_ON_THREAD(mStatsThread);

  const auto rtt = aStats.rtt_ms;
  if (rtt > static_cast<decltype(aStats.rtt_ms)>(INT32_MAX)) {
    // If we get a bogus RTT we will keep using the previous RTT
#ifdef DEBUG
    CSFLogError(LOGTAG,
                "%s for VideoConduit:%p RTT is larger than the"
                " maximum size of an RTCP RTT.",
                __FUNCTION__, this);
#endif
    mRttSec = Nothing();
  } else {
    if (mRttSec && rtt < 0) {
      CSFLogError(LOGTAG,
                  "%s for VideoConduit:%p RTT returned an error after "
                  " previously succeeding.",
                  __FUNCTION__, this);
      mRttSec = Nothing();
    }
    if (rtt >= 0) {
      mRttSec = Some(static_cast<DOMHighResTimeStamp>(rtt) / 1000.0);
    }
  }
}

Maybe<DOMHighResTimeStamp> WebrtcVideoConduit::CallStatistics::RttSec() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mRttSec;
}

void WebrtcVideoConduit::StreamStatistics::Update(
    const double aFrameRate, const double aBitrate,
    const webrtc::RtcpPacketTypeCounter& aPacketCounts) {
  ASSERT_ON_THREAD(mStatsThread);

  mFrameRate.Push(aFrameRate);
  mBitRate.Push(aBitrate);
  mPacketCounts = aPacketCounts;
}

bool WebrtcVideoConduit::StreamStatistics::GetVideoStreamStats(
    double& aOutFrMean, double& aOutFrStdDev, double& aOutBrMean,
    double& aOutBrStdDev) const {
  ASSERT_ON_THREAD(mStatsThread);

  if (mFrameRate.NumDataValues() && mBitRate.NumDataValues()) {
    aOutFrMean = mFrameRate.Mean();
    aOutFrStdDev = mFrameRate.StandardDeviation();
    aOutBrMean = mBitRate.Mean();
    aOutBrStdDev = mBitRate.StandardDeviation();
    return true;
  }
  return false;
}

void WebrtcVideoConduit::StreamStatistics::RecordTelemetry() const {
  ASSERT_ON_THREAD(mStatsThread);

  if (!mActive) {
    return;
  }
  using namespace Telemetry;
  Accumulate(IsSend() ? WEBRTC_VIDEO_ENCODER_BITRATE_AVG_PER_CALL_KBPS
                      : WEBRTC_VIDEO_DECODER_BITRATE_AVG_PER_CALL_KBPS,
             mBitRate.Mean() / 1000);
  Accumulate(IsSend() ? WEBRTC_VIDEO_ENCODER_BITRATE_STD_DEV_PER_CALL_KBPS
                      : WEBRTC_VIDEO_DECODER_BITRATE_STD_DEV_PER_CALL_KBPS,
             mBitRate.StandardDeviation() / 1000);
  Accumulate(IsSend() ? WEBRTC_VIDEO_ENCODER_FRAMERATE_AVG_PER_CALL
                      : WEBRTC_VIDEO_DECODER_FRAMERATE_AVG_PER_CALL,
             mFrameRate.Mean());
  Accumulate(IsSend() ? WEBRTC_VIDEO_ENCODER_FRAMERATE_10X_STD_DEV_PER_CALL
                      : WEBRTC_VIDEO_DECODER_FRAMERATE_10X_STD_DEV_PER_CALL,
             mFrameRate.StandardDeviation() * 10);
}

const webrtc::RtcpPacketTypeCounter&
WebrtcVideoConduit::StreamStatistics::PacketCounts() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mPacketCounts;
}

bool WebrtcVideoConduit::StreamStatistics::Active() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mActive;
}

void WebrtcVideoConduit::StreamStatistics::SetActive(bool aActive) {
  ASSERT_ON_THREAD(mStatsThread);

  mActive = aActive;
}

uint32_t WebrtcVideoConduit::SendStreamStatistics::DroppedFrames() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mDroppedFrames;
}

uint32_t WebrtcVideoConduit::SendStreamStatistics::FramesEncoded() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mFramesEncoded;
}

void WebrtcVideoConduit::SendStreamStatistics::FrameDeliveredToEncoder() {
  ASSERT_ON_THREAD(mStatsThread);

  ++mFramesDeliveredToEncoder;
}

bool WebrtcVideoConduit::SendStreamStatistics::SsrcFound() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mSsrcFound;
}

uint32_t WebrtcVideoConduit::SendStreamStatistics::JitterMs() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mJitterMs;
}

uint32_t WebrtcVideoConduit::SendStreamStatistics::PacketsLost() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mPacketsLost;
}

uint64_t WebrtcVideoConduit::SendStreamStatistics::BytesReceived() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mBytesReceived;
}

uint32_t WebrtcVideoConduit::SendStreamStatistics::PacketsReceived() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mPacketsReceived;
}

Maybe<uint64_t> WebrtcVideoConduit::SendStreamStatistics::QpSum() const {
  ASSERT_ON_THREAD(mStatsThread);
  return mQpSum;
}

void WebrtcVideoConduit::SendStreamStatistics::Update(
    const webrtc::VideoSendStream::Stats& aStats, uint32_t aConfiguredSsrc) {
  ASSERT_ON_THREAD(mStatsThread);

  mSsrcFound = false;

  if (aStats.substreams.empty()) {
    CSFLogVerbose(LOGTAG, "%s stats.substreams is empty", __FUNCTION__);
    return;
  }

  auto ind = aStats.substreams.find(aConfiguredSsrc);
  if (ind == aStats.substreams.end()) {
    CSFLogError(LOGTAG,
                "%s for VideoConduit:%p ssrc not found in SendStream stats.",
                __FUNCTION__, this);
    return;
  }

  mSsrcFound = true;

  StreamStatistics::Update(aStats.encode_frame_rate, aStats.media_bitrate_bps,
                           ind->second.rtcp_packet_type_counts);
  if (aStats.qp_sum) {
    mQpSum = Some(aStats.qp_sum.value());
  } else {
    mQpSum = Nothing();
  }

  const webrtc::FrameCounts& fc = ind->second.frame_counts;
  mFramesEncoded = fc.key_frames + fc.delta_frames;
  CSFLogVerbose(
      LOGTAG, "%s: framerate: %u, bitrate: %u, dropped frames delta: %u",
      __FUNCTION__, aStats.encode_frame_rate, aStats.media_bitrate_bps,
      mFramesDeliveredToEncoder - mFramesEncoded - mDroppedFrames);
  mDroppedFrames = mFramesDeliveredToEncoder - mFramesEncoded;
  mJitterMs = ind->second.rtcp_stats.jitter /
              (webrtc::kVideoPayloadTypeFrequency / 1000);
  mPacketsLost = ind->second.rtcp_stats.packets_lost;
  mBytesReceived = ind->second.rtp_stats.MediaPayloadBytes();
  mPacketsReceived = ind->second.rtp_stats.transmitted.packets;
}

uint32_t WebrtcVideoConduit::ReceiveStreamStatistics::BytesSent() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mBytesSent;
}

uint32_t WebrtcVideoConduit::ReceiveStreamStatistics::DiscardedPackets() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mDiscardedPackets;
}

uint32_t WebrtcVideoConduit::ReceiveStreamStatistics::FramesDecoded() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mFramesDecoded;
}

uint32_t WebrtcVideoConduit::ReceiveStreamStatistics::JitterMs() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mJitterMs;
}

uint32_t WebrtcVideoConduit::ReceiveStreamStatistics::PacketsLost() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mPacketsLost;
}

uint32_t WebrtcVideoConduit::ReceiveStreamStatistics::PacketsSent() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mPacketsSent;
}

uint32_t WebrtcVideoConduit::ReceiveStreamStatistics::Ssrc() const {
  ASSERT_ON_THREAD(mStatsThread);

  return mSsrc;
}

void WebrtcVideoConduit::ReceiveStreamStatistics::Update(
    const webrtc::VideoReceiveStream::Stats& aStats) {
  ASSERT_ON_THREAD(mStatsThread);

  CSFLogVerbose(LOGTAG, "%s ", __FUNCTION__);
  StreamStatistics::Update(aStats.decode_frame_rate, aStats.total_bitrate_bps,
                           aStats.rtcp_packet_type_counts);
  mBytesSent = aStats.rtcp_sender_octets_sent;
  mDiscardedPackets = aStats.discarded_packets;
  mFramesDecoded =
      aStats.frame_counts.key_frames + aStats.frame_counts.delta_frames;
  mJitterMs =
      aStats.rtcp_stats.jitter / (webrtc::kVideoPayloadTypeFrequency / 1000);
  mPacketsLost = aStats.rtcp_stats.packets_lost;
  mPacketsSent = aStats.rtcp_sender_packets_sent;
  mSsrc = aStats.ssrc;
}

/**
 * Factory Method for VideoConduit
 */
RefPtr<VideoSessionConduit> VideoSessionConduit::Create(
    RefPtr<WebRtcCallWrapper> aCall,
    nsCOMPtr<nsISerialEventTarget> aStsThread) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCall, "missing required parameter: aCall");
  CSFLogVerbose(LOGTAG, "%s", __FUNCTION__);

  if (!aCall) {
    return nullptr;
  }

  auto obj = MakeRefPtr<WebrtcVideoConduit>(aCall, aStsThread);
  if (obj->Init() != kMediaConduitNoError) {
    CSFLogError(LOGTAG, "%s VideoConduit Init Failed ", __FUNCTION__);
    return nullptr;
  }
  CSFLogVerbose(LOGTAG, "%s Successfully created VideoConduit ", __FUNCTION__);
  return obj.forget();
}

WebrtcVideoConduit::WebrtcVideoConduit(
    RefPtr<WebRtcCallWrapper> aCall, nsCOMPtr<nsISerialEventTarget> aStsThread)
    : mTransportMonitor("WebrtcVideoConduit"),
      mStsThread(aStsThread),
      mMutex("WebrtcVideoConduit::mMutex"),
      mVideoAdapter(MakeUnique<cricket::VideoAdapter>()),
      mBufferPool(false, SCALER_BUFFER_POOL_SIZE),
      mEngineTransmitting(false),
      mEngineReceiving(false),
      mSendStreamStats(aStsThread),
      mRecvStreamStats(aStsThread),
      mCallStats(aStsThread),
      mSendingFramerate(DEFAULT_VIDEO_MAX_FRAMERATE),
      mActiveCodecMode(webrtc::kRealtimeVideo),
      mCodecMode(webrtc::kRealtimeVideo),
      mCall(aCall),
      mSendStreamConfig(
          this)  // 'this' is stored but not dereferenced in the constructor.
      ,
      mRecvStreamConfig(
          this)  // 'this' is stored but not dereferenced in the constructor.
      ,
      mRecvSSRC(0),
      mVideoStatsTimer(NS_NewTimer()) {
  mCall->RegisterConduit(this);
  mRecvStreamConfig.renderer = this;
  mRecvStreamConfig.rtcp_event_observer = this;
}

WebrtcVideoConduit::~WebrtcVideoConduit() {
  MOZ_ASSERT(NS_IsMainThread());

  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  mCall->UnregisterConduit(this);

  // Release AudioConduit first by dropping reference on MainThread, where it
  // expects to be
  MOZ_ASSERT(!mSendStream && !mRecvStream,
             "Call DeleteStreams prior to ~WebrtcVideoConduit.");
}

MediaConduitErrorCode WebrtcVideoConduit::SetLocalRTPExtensions(
    LocalDirection aDirection, const RtpExtList& aExtensions) {
  MOZ_ASSERT(NS_IsMainThread());

  auto& extList = aDirection == LocalDirection::kSend
                      ? mSendStreamConfig.rtp.extensions
                      : mRecvStreamConfig.rtp.extensions;
  extList = aExtensions;
  return kMediaConduitNoError;
}

bool WebrtcVideoConduit::SetLocalSSRCs(
    const std::vector<unsigned int>& aSSRCs) {
  MOZ_ASSERT(NS_IsMainThread());

  // Special case: the local SSRCs are the same - do nothing.
  if (mSendStreamConfig.rtp.ssrcs == aSSRCs) {
    return true;
  }

  {
    MutexAutoLock lock(mMutex);
    // Update the value of the ssrcs in the config structure.
    mSendStreamConfig.rtp.ssrcs = aSSRCs;

    bool wasTransmitting = mEngineTransmitting;
    if (StopTransmittingLocked() != kMediaConduitNoError) {
      return false;
    }

    // On the next StartTransmitting() or ConfigureSendMediaCodec, force
    // building a new SendStream to switch SSRCs.
    DeleteSendStream();

    if (wasTransmitting) {
      if (StartTransmittingLocked() != kMediaConduitNoError) {
        return false;
      }
    }
  }

  return true;
}

std::vector<unsigned int> WebrtcVideoConduit::GetLocalSSRCs() {
  MutexAutoLock lock(mMutex);

  return mSendStreamConfig.rtp.ssrcs;
}

bool WebrtcVideoConduit::SetLocalCNAME(const char* cname) {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  mSendStreamConfig.rtp.c_name = cname;
  return true;
}

bool WebrtcVideoConduit::SetLocalMID(const std::string& mid) {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  mSendStreamConfig.rtp.mid = mid;
  return true;
}

void WebrtcVideoConduit::SetSyncGroup(const std::string& group) {
  mRecvStreamConfig.sync_group = group;
}

MediaConduitErrorCode WebrtcVideoConduit::ConfigureCodecMode(
    webrtc::VideoCodecMode mode) {
  MOZ_ASSERT(NS_IsMainThread());

  CSFLogVerbose(LOGTAG, "%s ", __FUNCTION__);
  if (mode == webrtc::VideoCodecMode::kRealtimeVideo ||
      mode == webrtc::VideoCodecMode::kScreensharing) {
    mCodecMode = mode;
    if (mVideoStreamFactory) {
      mVideoStreamFactory->SetCodecMode(mCodecMode);
    }
    return kMediaConduitNoError;
  }

  return kMediaConduitMalformedArgument;
}

void WebrtcVideoConduit::DeleteSendStream() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mSendStream) {
    mCall->Call()->DestroyVideoSendStream(mSendStream);
    mSendStream = nullptr;
    mEncoder = nullptr;
  }
}

webrtc::VideoCodecType SupportedCodecType(webrtc::VideoCodecType aType) {
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

MediaConduitErrorCode WebrtcVideoConduit::CreateSendStream() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  nsAutoString codecName;
  codecName.AssignASCII(
      mSendStreamConfig.encoder_settings.payload_name.c_str());
  Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_VIDEO_SEND_CODEC_USED,
                       codecName, 1);

  webrtc::VideoCodecType encoder_type =
      SupportedCodecType(webrtc::PayloadStringToCodecType(
          mSendStreamConfig.encoder_settings.payload_name));
  if (encoder_type == webrtc::VideoCodecType::kVideoCodecUnknown) {
    return kMediaConduitInvalidSendCodec;
  }

  std::unique_ptr<webrtc::VideoEncoder> encoder(CreateEncoder(encoder_type));
  if (!encoder) {
    return kMediaConduitInvalidSendCodec;
  }

  mSendStreamConfig.encoder_settings.encoder = encoder.get();

  MOZ_ASSERT(
      mSendStreamConfig.rtp.ssrcs.size() == mEncoderConfig.number_of_streams,
      "Each video substream must have a corresponding ssrc.");

  mSendStream = mCall->Call()->CreateVideoSendStream(mSendStreamConfig.Copy(),
                                                     mEncoderConfig.Copy());

  if (!mSendStream) {
    return kMediaConduitVideoSendStreamError;
  }
  mSendStream->SetSource(
      this, webrtc::VideoSendStream::DegradationPreference::kBalanced);

  mEncoder = std::move(encoder);

  mActiveCodecMode = mCodecMode;

  return kMediaConduitNoError;
}

void WebrtcVideoConduit::DeleteRecvStream() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mRecvStream) {
    mCall->Call()->DestroyVideoReceiveStream(mRecvStream);
    mRecvStream = nullptr;
    mDecoders.clear();
  }
}

MediaConduitErrorCode WebrtcVideoConduit::CreateRecvStream() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  webrtc::VideoReceiveStream::Decoder decoder_desc;
  std::unique_ptr<webrtc::VideoDecoder> decoder;
  webrtc::VideoCodecType decoder_type;

  mRecvStreamConfig.decoders.clear();
  for (auto& config : mRecvCodecList) {
    nsAutoString codecName;
    codecName.AssignASCII(config->mName.c_str());
    Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_VIDEO_RECV_CODEC_USED,
                         codecName, 1);

    decoder_type =
        SupportedCodecType(webrtc::PayloadStringToCodecType(config->mName));
    if (decoder_type == webrtc::VideoCodecType::kVideoCodecUnknown) {
      CSFLogError(LOGTAG, "%s Unknown decoder type: %s", __FUNCTION__,
                  config->mName.c_str());
      continue;
    }

    decoder = CreateDecoder(decoder_type);

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

  mRecvStream =
      mCall->Call()->CreateVideoReceiveStream(mRecvStreamConfig.Copy());
  if (!mRecvStream) {
    mDecoders.clear();
    return kMediaConduitUnknownError;
  }
  CSFLogDebug(LOGTAG, "Created VideoReceiveStream %p for SSRC %u (0x%x)",
              mRecvStream, mRecvStreamConfig.rtp.remote_ssrc,
              mRecvStreamConfig.rtp.remote_ssrc);

  return kMediaConduitNoError;
}

static rtc::scoped_refptr<webrtc::VideoEncoderConfig::EncoderSpecificSettings>
ConfigureVideoEncoderSettings(const VideoCodecConfig* aConfig,
                              const WebrtcVideoConduit* aConduit) {
  MOZ_ASSERT(NS_IsMainThread());

  bool is_screencast =
      aConduit->CodecMode() == webrtc::VideoCodecMode::kScreensharing;
  // No automatic resizing when using simulcast or screencast.
  bool automatic_resize = !is_screencast && aConfig->mEncodings.size() <= 1;
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

// Compare lists of codecs
static bool CodecsDifferent(const nsTArray<UniquePtr<VideoCodecConfig>>& a,
                            const nsTArray<UniquePtr<VideoCodecConfig>>& b) {
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
 * Note: Setting the send-codec on the Video Engine will restart the encoder,
 * sets up new SSRC and reset RTP_RTCP module with the new codec setting.
 *
 * Note: this is called from MainThread, and the codec settings are read on
 * videoframe delivery threads (i.e in SendVideoFrame().  With
 * renegotiation/reconfiguration, this now needs a lock!  Alternatively
 * changes could be queued until the next frame is delivered using an
 * Atomic pointer and swaps.
 */
MediaConduitErrorCode WebrtcVideoConduit::ConfigureSendMediaCodec(
    const VideoCodecConfig* codecConfig) {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);
  mUpdateResolution = true;

  CSFLogDebug(LOGTAG, "%s for %s", __FUNCTION__,
              codecConfig ? codecConfig->mName.c_str() : "<null>");

  MediaConduitErrorCode condError = kMediaConduitNoError;

  // validate basic params
  if ((condError = ValidateCodecConfig(codecConfig)) != kMediaConduitNoError) {
    return condError;
  }

  size_t streamCount = std::min(codecConfig->mEncodings.size(),
                                (size_t)webrtc::kMaxSimulcastStreams);

  MOZ_RELEASE_ASSERT(streamCount >= 1, "streamCount should be at least one");

  CSFLogDebug(LOGTAG, "%s for VideoConduit:%p stream count:%zu", __FUNCTION__,
              this, streamCount);

  mSendingFramerate = 0;
  mSendStreamConfig.rtp.rids.clear();

  int max_framerate;
  if (codecConfig->mEncodingConstraints.maxFps > 0) {
    max_framerate = codecConfig->mEncodingConstraints.maxFps;
  } else {
    max_framerate = DEFAULT_VIDEO_MAX_FRAMERATE;
  }
  // apply restrictions from maxMbps/etc
  mSendingFramerate =
      SelectSendFrameRate(codecConfig, max_framerate, mLastWidth, mLastHeight);

  // So we can comply with b=TIAS/b=AS/maxbr=X when input resolution changes
  mNegotiatedMaxBitrate = codecConfig->mTias;

  if (mLastWidth == 0 && mMinBitrateEstimate != 0) {
    // Only do this at the start; use "have we send a frame" as a reasonable
    // stand-in. min <= start <= max (which can be -1, note!)
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

  mVideoStreamFactory = new rtc::RefCountedObject<VideoStreamFactory>(
      *codecConfig, mCodecMode, mMinBitrate, mStartBitrate, mPrefMaxBitrate,
      mNegotiatedMaxBitrate, mSendingFramerate);
  mEncoderConfig.video_stream_factory = mVideoStreamFactory.get();

  // Reset the VideoAdapter. SelectResolution will ensure limits are set.
  mVideoAdapter = MakeUnique<cricket::VideoAdapter>(
      streamCount > 1 ? SIMULCAST_RESOLUTION_ALIGNMENT : 1);
  mVideoAdapter->OnScaleResolutionBy(
      codecConfig->mEncodings[0].constraints.scaleDownBy > 1.0
          ? rtc::Optional<float>(
                codecConfig->mEncodings[0].constraints.scaleDownBy)
          : rtc::Optional<float>());

  // XXX parse the encoded SPS/PPS data and set spsData/spsLen/ppsData/ppsLen
  mEncoderConfig.encoder_specific_settings =
      ConfigureVideoEncoderSettings(codecConfig, this);

  mEncoderConfig.content_type =
      mCodecMode == webrtc::kRealtimeVideo
          ? webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo
          : webrtc::VideoEncoderConfig::ContentType::kScreen;
  // for the GMP H.264 encoder/decoder!!
  mEncoderConfig.min_transmit_bitrate_bps = 0;
  // Expected max number of encodings
  mEncoderConfig.number_of_streams = streamCount;

  // If only encoder stream attibutes have been changed, there is no need to
  // stop, create a new webrtc::VideoSendStream, and restart. Recreating on
  // PayloadType change may be overkill, but is safe.
  if (mSendStream) {
    if (!RequiresNewSendStream(*codecConfig) &&
        mActiveCodecMode == mCodecMode) {
      mCurSendCodecConfig->mEncodingConstraints =
          codecConfig->mEncodingConstraints;
      mCurSendCodecConfig->mEncodings = codecConfig->mEncodings;
      mSendStream->ReconfigureVideoEncoder(mEncoderConfig.Copy());
      return kMediaConduitNoError;
    }

    condError = StopTransmittingLocked();
    if (condError != kMediaConduitNoError) {
      return condError;
    }

    // This will cause a new encoder to be created by StartTransmitting()
    DeleteSendStream();
  }

  mSendStreamConfig.encoder_settings.payload_name = codecConfig->mName;
  mSendStreamConfig.encoder_settings.payload_type = codecConfig->mType;
  mSendStreamConfig.rtp.rtcp_mode = webrtc::RtcpMode::kCompound;
  mSendStreamConfig.rtp.max_packet_size = kVideoMtu;

  // See Bug 1297058, enabling FEC when basic NACK is to be enabled in H.264 is
  // problematic
  if (codecConfig->RtcpFbFECIsSet() &&
      !(codecConfig->mName == "H264" && codecConfig->RtcpFbNackIsSet(""))) {
    mSendStreamConfig.rtp.ulpfec.ulpfec_payload_type =
        codecConfig->mULPFECPayloadType;
    mSendStreamConfig.rtp.ulpfec.red_payload_type =
        codecConfig->mREDPayloadType;
    mSendStreamConfig.rtp.ulpfec.red_rtx_payload_type =
        codecConfig->mREDRTXPayloadType;
  } else {
    // Reset to defaults
    mSendStreamConfig.rtp.ulpfec.ulpfec_payload_type = -1;
    mSendStreamConfig.rtp.ulpfec.red_payload_type = -1;
    mSendStreamConfig.rtp.ulpfec.red_rtx_payload_type = -1;
  }

  mSendStreamConfig.rtp.nack.rtp_history_ms =
      codecConfig->RtcpFbNackIsSet("") ? 1000 : 0;

  // Copy the applied config for future reference.
  mCurSendCodecConfig = MakeUnique<VideoCodecConfig>(*codecConfig);

  mSendStreamConfig.rtp.rids.clear();
  bool has_rid = false;
  for (size_t idx = 0; idx < streamCount; idx++) {
    auto& encoding = mCurSendCodecConfig->mEncodings[idx];
    if (encoding.rid[0]) {
      has_rid = true;
      break;
    }
  }
  if (has_rid) {
    for (size_t idx = streamCount; idx > 0; idx--) {
      auto& encoding = mCurSendCodecConfig->mEncodings[idx - 1];
      mSendStreamConfig.rtp.rids.push_back(encoding.rid);
    }
  }

  return condError;
}

static uint32_t GenerateRandomSSRC() {
  uint32_t ssrc;
  do {
    SECStatus rv = PK11_GenerateRandom(reinterpret_cast<unsigned char*>(&ssrc),
                                       sizeof(ssrc));
    if (rv != SECSuccess) {
      CSFLogError(LOGTAG, "%s: PK11_GenerateRandom failed with error %d",
                  __FUNCTION__, rv);
      return 0;
    }
  } while (ssrc == 0);  // webrtc.org code has fits if you select an SSRC of 0

  return ssrc;
}

bool WebrtcVideoConduit::SetRemoteSSRC(unsigned int ssrc) {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  return SetRemoteSSRCLocked(ssrc);
}

bool WebrtcVideoConduit::SetRemoteSSRCLocked(unsigned int ssrc) {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  unsigned int current_ssrc;
  if (!GetRemoteSSRCLocked(&current_ssrc)) {
    return false;
  }

  if (current_ssrc == ssrc) {
    return true;
  }

  bool wasReceiving = mEngineReceiving;
  if (StopReceivingLocked() != kMediaConduitNoError) {
    return false;
  }

  {
    CSFLogDebug(LOGTAG, "%s: SSRC %u (0x%x)", __FUNCTION__, ssrc, ssrc);
    MutexAutoUnlock unlock(mMutex);
    if (!mCall->UnsetRemoteSSRC(ssrc)) {
      CSFLogError(LOGTAG,
                  "%s: Failed to unset SSRC %u (0x%x) on other conduits,"
                  " bailing",
                  __FUNCTION__, ssrc, ssrc);
      return false;
    }
  }

  mRecvStreamConfig.rtp.remote_ssrc = ssrc;
  mStsThread->Dispatch(NS_NewRunnableFunction(
      "WebrtcVideoConduit::WaitingForInitialSsrcNoMore",
      [this, self = RefPtr<WebrtcVideoConduit>(this)]() mutable {
        mWaitingForInitialSsrc = false;
        NS_ReleaseOnMainThreadSystemGroup(
            "WebrtcVideoConduit::WaitingForInitialSsrcNoMore", self.forget());
      }));
  // On the next StartReceiving() or ConfigureRecvMediaCodec, force
  // building a new RecvStream to switch SSRCs.
  DeleteRecvStream();

  if (wasReceiving) {
    if (StartReceivingLocked() != kMediaConduitNoError) {
      return false;
    }
  }

  return true;
}

bool WebrtcVideoConduit::UnsetRemoteSSRC(uint32_t ssrc) {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  unsigned int our_ssrc;
  if (!GetRemoteSSRCLocked(&our_ssrc)) {
    // This only fails when we aren't sending, which isn't really an error here
    return true;
  }

  if (our_ssrc != ssrc) {
    return true;
  }

  while (our_ssrc == ssrc) {
    our_ssrc = GenerateRandomSSRC();
    if (our_ssrc == 0) {
      return false;
    }
  }

  // There is a (tiny) chance that this new random ssrc will collide with some
  // other conduit's remote ssrc, in which case that conduit will choose a new
  // one.
  SetRemoteSSRCLocked(our_ssrc);
  return true;
}

bool WebrtcVideoConduit::GetRemoteSSRC(unsigned int* ssrc) {
  MutexAutoLock lock(mMutex);

  return GetRemoteSSRCLocked(ssrc);
}

bool WebrtcVideoConduit::GetRemoteSSRCLocked(unsigned int* ssrc) {
  mMutex.AssertCurrentThreadOwns();

  if (NS_IsMainThread()) {
    if (!mRecvStream) {
      return false;
    }
    *ssrc = mRecvStream->GetStats().ssrc;
  } else {
    ASSERT_ON_THREAD(mStsThread);
    *ssrc = mRecvStreamStats.Ssrc();
  }
  return true;
}

bool WebrtcVideoConduit::GetSendPacketTypeStats(
    webrtc::RtcpPacketTypeCounter* aPacketCounts) {
  ASSERT_ON_THREAD(mStsThread);

  MutexAutoLock lock(mMutex);
  if (!mSendStreamStats.Active()) {
    return false;
  }
  *aPacketCounts = mSendStreamStats.PacketCounts();
  return true;
}

bool WebrtcVideoConduit::GetRecvPacketTypeStats(
    webrtc::RtcpPacketTypeCounter* aPacketCounts) {
  ASSERT_ON_THREAD(mStsThread);

  if (!mRecvStreamStats.Active()) {
    return false;
  }
  *aPacketCounts = mRecvStreamStats.PacketCounts();
  return true;
}

void WebrtcVideoConduit::PollStats() {
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<RefPtr<Runnable>> runnables(2);
  if (mEngineTransmitting) {
    MOZ_RELEASE_ASSERT(mSendStream);
    if (!mSendStreamConfig.rtp.ssrcs.empty()) {
      uint32_t ssrc = mSendStreamConfig.rtp.ssrcs.front();
      webrtc::VideoSendStream::Stats stats = mSendStream->GetStats();
      runnables.AppendElement(NS_NewRunnableFunction(
          "WebrtcVideoConduit::SendStreamStatistics::Update",
          [this, self = RefPtr<WebrtcVideoConduit>(this),
           stats = std::move(stats),
           ssrc]() { mSendStreamStats.Update(stats, ssrc); }));
    }
  }
  if (mEngineReceiving) {
    MOZ_RELEASE_ASSERT(mRecvStream);
    webrtc::VideoReceiveStream::Stats stats = mRecvStream->GetStats();
    runnables.AppendElement(NS_NewRunnableFunction(
        "WebrtcVideoConduit::RecvStreamStatistics::Update",
        [this, self = RefPtr<WebrtcVideoConduit>(this),
         stats = std::move(stats)]() { mRecvStreamStats.Update(stats); }));
  }
  webrtc::Call::Stats stats = mCall->Call()->GetStats();
  mStsThread->Dispatch(NS_NewRunnableFunction(
      "WebrtcVideoConduit::UpdateStreamStatistics",
      [this, self = RefPtr<WebrtcVideoConduit>(this), stats = std::move(stats),
       runnables = std::move(runnables)]() mutable {
        mCallStats.Update(stats);
        for (const auto& runnable : runnables) {
          runnable->Run();
        }
        NS_ReleaseOnMainThreadSystemGroup(
            "WebrtcVideoConduit::UpdateStreamStatistics", self.forget());
      }));
}

void WebrtcVideoConduit::UpdateVideoStatsTimer() {
  MOZ_ASSERT(NS_IsMainThread());

  bool transmitting = mEngineTransmitting;
  bool receiving = mEngineReceiving;
  mStsThread->Dispatch(NS_NewRunnableFunction(
      "WebrtcVideoConduit::SetSendStreamStatsActive",
      [this, self = RefPtr<WebrtcVideoConduit>(this), transmitting,
       receiving]() mutable {
        mSendStreamStats.SetActive(transmitting);
        mRecvStreamStats.SetActive(receiving);
        NS_ReleaseOnMainThreadSystemGroup(
            "WebrtcVideoConduit::SetSendStreamStatsActive", self.forget());
      }));

  bool shouldBeActive = transmitting || receiving;
  if (mVideoStatsTimerActive == shouldBeActive) {
    return;
  }
  mVideoStatsTimerActive = shouldBeActive;
  if (shouldBeActive) {
    nsTimerCallbackFunc callback = [](nsITimer*, void* aClosure) {
      CSFLogDebug(LOGTAG, "StreamStats polling scheduled for VideoConduit: %p",
                  aClosure);
      static_cast<WebrtcVideoConduit*>(aClosure)->PollStats();
    };
    mVideoStatsTimer->InitWithNamedFuncCallback(
        callback, this, 1000, nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP,
        "WebrtcVideoConduit::SendStreamStatsUpdater");
  } else {
    mVideoStatsTimer->Cancel();
  }
}

bool WebrtcVideoConduit::GetVideoEncoderStats(
    double* framerateMean, double* framerateStdDev, double* bitrateMean,
    double* bitrateStdDev, uint32_t* droppedFrames, uint32_t* framesEncoded,
    Maybe<uint64_t>* qpSum) {
  ASSERT_ON_THREAD(mStsThread);

  MutexAutoLock lock(mMutex);
  if (!mEngineTransmitting || !mSendStream) {
    return false;
  }
  mSendStreamStats.GetVideoStreamStats(*framerateMean, *framerateStdDev,
                                       *bitrateMean, *bitrateStdDev);
  *droppedFrames = mSendStreamStats.DroppedFrames();
  *framesEncoded = mSendStreamStats.FramesEncoded();
  *qpSum = mSendStreamStats.QpSum();
  return true;
}

bool WebrtcVideoConduit::GetVideoDecoderStats(double* framerateMean,
                                              double* framerateStdDev,
                                              double* bitrateMean,
                                              double* bitrateStdDev,
                                              uint32_t* discardedPackets,
                                              uint32_t* framesDecoded) {
  ASSERT_ON_THREAD(mStsThread);

  MutexAutoLock lock(mMutex);
  if (!mEngineReceiving || !mRecvStream) {
    return false;
  }
  mRecvStreamStats.GetVideoStreamStats(*framerateMean, *framerateStdDev,
                                       *bitrateMean, *bitrateStdDev);
  *discardedPackets = mRecvStreamStats.DiscardedPackets();
  *framesDecoded = mRecvStreamStats.FramesDecoded();
  return true;
}

bool WebrtcVideoConduit::GetRTPReceiverStats(uint32_t* jitterMs,
                                             uint32_t* packetsLost) {
  ASSERT_ON_THREAD(mStsThread);

  CSFLogVerbose(LOGTAG, "%s for VideoConduit:%p", __FUNCTION__, this);
  MutexAutoLock lock(mMutex);
  if (!mRecvStream) {
    return false;
  }

  *jitterMs = mRecvStreamStats.JitterMs();
  *packetsLost = mRecvStreamStats.PacketsLost();
  return true;
}

bool WebrtcVideoConduit::GetRTCPReceiverReport(uint32_t* jitterMs,
                                               uint32_t* packetsReceived,
                                               uint64_t* bytesReceived,
                                               uint32_t* cumulativeLost,
                                               Maybe<double>* aOutRttSec) {
  ASSERT_ON_THREAD(mStsThread);

  CSFLogVerbose(LOGTAG, "%s for VideoConduit:%p", __FUNCTION__, this);
  aOutRttSec->reset();
  if (!mSendStreamStats.Active()) {
    return false;
  }
  if (!mSendStreamStats.SsrcFound()) {
    return false;
  }
  *jitterMs = mSendStreamStats.JitterMs();
  *packetsReceived = mSendStreamStats.PacketsReceived();
  *bytesReceived = mSendStreamStats.BytesReceived();
  *cumulativeLost = mSendStreamStats.PacketsLost();
  *aOutRttSec = mCallStats.RttSec();
  return true;
}

bool WebrtcVideoConduit::GetRTCPSenderReport(unsigned int* packetsSent,
                                             uint64_t* bytesSent) {
  ASSERT_ON_THREAD(mStsThread);

  CSFLogVerbose(LOGTAG, "%s for VideoConduit:%p", __FUNCTION__, this);

  if (!mRecvStreamStats.Active()) {
    return false;
  }

  *packetsSent = mRecvStreamStats.PacketsSent();
  *bytesSent = mRecvStreamStats.BytesSent();
  return true;
}

MediaConduitErrorCode WebrtcVideoConduit::InitMain() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs =
      do_GetService("@mozilla.org/preferences-service;1", &rv);
  if (!NS_WARN_IF(NS_FAILED(rv))) {
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);

    if (branch) {
      int32_t temp;
      Unused << NS_WARN_IF(NS_FAILED(branch->GetBoolPref(
          "media.video.test_latency", &mVideoLatencyTestEnable)));
      Unused << NS_WARN_IF(NS_FAILED(branch->GetBoolPref(
          "media.video.test_latency", &mVideoLatencyTestEnable)));
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref(
              "media.peerconnection.video.min_bitrate", &temp)))) {
        if (temp >= 0) {
          mMinBitrate = KBPS(temp);
        }
      }
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref(
              "media.peerconnection.video.start_bitrate", &temp)))) {
        if (temp >= 0) {
          mStartBitrate = KBPS(temp);
        }
      }
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref(
              "media.peerconnection.video.max_bitrate", &temp)))) {
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
              "media.peerconnection.video.min_bitrate_estimate", &temp)))) {
        if (temp >= 0) {
          mMinBitrateEstimate = temp;  // bps!
        }
      }
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref(
              "media.peerconnection.video.svc.spatial", &temp)))) {
        if (temp >= 0) {
          mSpatialLayers = temp;
        }
      }
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref(
              "media.peerconnection.video.svc.temporal", &temp)))) {
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
  if (mozilla::camera::VideoEngine::SetAndroidObjects() != 0) {
    CSFLogError(LOGTAG, "%s: could not set Android objects", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }
#endif  // MOZ_WIDGET_ANDROID
  return kMediaConduitNoError;
}

/**
 * Performs initialization of the MANDATORY components of the Video Engine
 */
MediaConduitErrorCode WebrtcVideoConduit::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  CSFLogDebug(LOGTAG, "%s this=%p", __FUNCTION__, this);
  MediaConduitErrorCode result;
  result = InitMain();
  if (result != kMediaConduitNoError) {
    return result;
  }

  CSFLogDebug(LOGTAG, "%s Initialization Done", __FUNCTION__);
  return kMediaConduitNoError;
}

void WebrtcVideoConduit::DeleteStreams() {
  MOZ_ASSERT(NS_IsMainThread());

  // We can't delete the VideoEngine until all these are released!
  // And we can't use a Scoped ptr, since the order is arbitrary

  MutexAutoLock lock(mMutex);
  DeleteSendStream();
  DeleteRecvStream();
}

MediaConduitErrorCode WebrtcVideoConduit::AttachRenderer(
    RefPtr<mozilla::VideoRenderer> aVideoRenderer) {
  MOZ_ASSERT(NS_IsMainThread());

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
    mRenderer->FrameSizeChange(mReceivingWidth, mReceivingHeight);
  }

  return kMediaConduitNoError;
}

void WebrtcVideoConduit::DetachRenderer() {
  MOZ_ASSERT(NS_IsMainThread());

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if (mRenderer) {
    mRenderer = nullptr;
  }
}

MediaConduitErrorCode WebrtcVideoConduit::SetTransmitterTransport(
    RefPtr<TransportInterface> aTransport) {
  MOZ_ASSERT(NS_IsMainThread());

  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mTransmitterTransport = aTransport;
  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcVideoConduit::SetReceiverTransport(
    RefPtr<TransportInterface> aTransport) {
  MOZ_ASSERT(NS_IsMainThread());

  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mReceiverTransport = aTransport;
  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcVideoConduit::ConfigureRecvMediaCodecs(
    const std::vector<UniquePtr<VideoCodecConfig>>& codecConfigList) {
  MOZ_ASSERT(NS_IsMainThread());

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
  bool use_transport_cc = false;
  int ulpfec_payload_type = kNullPayloadType;
  int red_payload_type = kNullPayloadType;
  bool configuredH264 = false;
  nsTArray<UniquePtr<VideoCodecConfig>> recv_codecs;

  // Try Applying the codecs in the list
  // we treat as success if at least one codec was applied and reception was
  // started successfully.
  std::set<unsigned int> codec_types_seen;
  for (const auto& codec_config : codecConfigList) {
    if ((condError = ValidateCodecConfig(codec_config.get())) !=
        kMediaConduitNoError) {
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
    // XXX (See upstream issue
    // https://bugs.chromium.org/p/webrtc/issues/detail?id=7002): There is no
    // 'none' option in webrtc.org
    if (codec_config->RtcpFbNackIsSet("pli")) {
      kf_request_enabled = true;
      kf_request_method = webrtc::kKeyFrameReqPliRtcp;
    } else if (!kf_request_enabled && codec_config->RtcpFbCcmIsSet("fir")) {
      kf_request_enabled = true;
      kf_request_method = webrtc::kKeyFrameReqFirRtcp;
    }

    // What if codec A has Nack and REMB, and codec B has TMMBR, and codec C has
    // none? In practice, that's not a useful configuration, and
    // VideoReceiveStream::Config can't represent that, so simply union the
    // (boolean) settings
    use_nack_basic |= codec_config->RtcpFbNackIsSet("");
    use_tmmbr |= codec_config->RtcpFbCcmIsSet("tmmbr");
    use_remb |= codec_config->RtcpFbRembIsSet();
    use_fec |= codec_config->RtcpFbFECIsSet();
    use_transport_cc |= codec_config->RtcpFbTransportCCIsSet();

    recv_codecs.AppendElement(new VideoCodecConfig(*codec_config));
  }

  if (!recv_codecs.Length()) {
    CSFLogError(LOGTAG, "%s Found no valid receive codecs", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  // Now decide if we need to recreate the receive stream, or can keep it
  if (!mRecvStream || CodecsDifferent(recv_codecs, mRecvCodecList) ||
      mRecvStreamConfig.rtp.nack.rtp_history_ms !=
          (use_nack_basic ? 1000 : 0) ||
      mRecvStreamConfig.rtp.remb != use_remb ||
      mRecvStreamConfig.rtp.transport_cc != use_transport_cc ||
      mRecvStreamConfig.rtp.tmmbr != use_tmmbr ||
      mRecvStreamConfig.rtp.keyframe_method != kf_request_method ||
      (use_fec &&
       (mRecvStreamConfig.rtp.ulpfec_payload_type != ulpfec_payload_type ||
        mRecvStreamConfig.rtp.red_payload_type != red_payload_type))) {
    MutexAutoLock lock(mMutex);

    condError = StopReceivingLocked();
    if (condError != kMediaConduitNoError) {
      return condError;
    }

    // If we fail after here things get ugly
    mRecvStreamConfig.rtp.rtcp_mode = webrtc::RtcpMode::kCompound;
    mRecvStreamConfig.rtp.nack.rtp_history_ms = use_nack_basic ? 1000 : 0;
    mRecvStreamConfig.rtp.remb = use_remb;
    mRecvStreamConfig.rtp.transport_cc = use_transport_cc;
    mRecvStreamConfig.rtp.tmmbr = use_tmmbr;
    mRecvStreamConfig.rtp.keyframe_method = kf_request_method;

    if (use_fec) {
      mRecvStreamConfig.rtp.ulpfec_payload_type = ulpfec_payload_type;
      mRecvStreamConfig.rtp.red_payload_type = red_payload_type;
    } else {
      // Reset to defaults
      mRecvStreamConfig.rtp.ulpfec_payload_type = -1;
      mRecvStreamConfig.rtp.red_payload_type = -1;
    }

    // SetRemoteSSRC should have populated this already
    mRecvSSRC = mRecvStreamConfig.rtp.remote_ssrc;

    // XXX ugh! same SSRC==0 problem that webrtc.org has
    if (mRecvSSRC == 0) {
      // Handle un-signalled SSRCs by creating a random one and then when it
      // actually gets set, we'll destroy and recreate.  Simpler than trying to
      // unwind all the logic that assumes the receive stream is created and
      // started when we ConfigureRecvMediaCodecs()
      uint32_t ssrc = GenerateRandomSSRC();
      if (ssrc == 0) {
        // webrtc.org code has fits if you select an SSRC of 0, so that's how
        // we signal an error.
        return kMediaConduitUnknownError;
      }

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

    while (ssrc == mRecvStreamConfig.rtp.remote_ssrc) {
      ssrc = GenerateRandomSSRC();
      if (ssrc == 0) {
        return kMediaConduitUnknownError;
      }
    }

    mRecvStreamConfig.rtp.local_ssrc = ssrc;
    CSFLogDebug(LOGTAG,
                "%s (%p): Local SSRC 0x%08x (of %u), remote SSRC 0x%08x",
                __FUNCTION__, (void*)this, ssrc,
                (uint32_t)mSendStreamConfig.rtp.ssrcs.size(),
                mRecvStreamConfig.rtp.remote_ssrc);

    // XXX Copy over those that are the same and don't rebuild them
    mRecvCodecList.SwapElements(recv_codecs);
    recv_codecs.Clear();
    mRecvStreamConfig.rtp.rtx_associated_payload_types.clear();

    DeleteRecvStream();
    return StartReceivingLocked();
  }
  return kMediaConduitNoError;
}

std::unique_ptr<webrtc::VideoDecoder> WebrtcVideoConduit::CreateDecoder(
    webrtc::VideoCodecType aType) {
  MOZ_ASSERT(NS_IsMainThread());

  std::unique_ptr<webrtc::VideoDecoder> decoder = nullptr;
  mRecvCodecPluginID = 0;

#ifdef MOZ_WEBRTC_MEDIACODEC
  bool enabled = false;
#endif

  // Attempt to create a decoder using MediaDataDecoder.
  decoder.reset(MediaDataCodec::CreateDecoder(aType));
  if (decoder) {
    return decoder;
  }

  switch (aType) {
    case webrtc::VideoCodecType::kVideoCodecH264:
      // get an external decoder
      decoder.reset(GmpVideoCodec::CreateDecoder());
      if (decoder) {
        mRecvCodecPluginID =
            static_cast<WebrtcVideoDecoder*>(decoder.get())->PluginID();
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
              NS_WARNING(
                  "VP8 decoder hardware is not whitelisted: disabling.\n");
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

std::unique_ptr<webrtc::VideoEncoder> WebrtcVideoConduit::CreateEncoder(
    webrtc::VideoCodecType aType) {
  MOZ_ASSERT(NS_IsMainThread());

  std::unique_ptr<webrtc::VideoEncoder> encoder = nullptr;
  mSendCodecPluginID = 0;

#ifdef MOZ_WEBRTC_MEDIACODEC
  bool enabled = false;
#endif

  if (StaticPrefs::media_webrtc_platformencoder()) {
    encoder.reset(MediaDataCodec::CreateEncoder(aType));
    if (encoder) {
      return encoder;
    }
  }

  switch (aType) {
    case webrtc::VideoCodecType::kVideoCodecH264:
      // get an external encoder
      encoder.reset(GmpVideoCodec::CreateEncoder());
      if (encoder) {
        mSendCodecPluginID =
            static_cast<WebrtcVideoEncoder*>(encoder.get())->PluginID();
      }
      break;

    case webrtc::VideoCodecType::kVideoCodecVP8:
      encoder.reset(new webrtc::VP8EncoderSimulcastProxy(this));
      break;

    case webrtc::VideoCodecType::kVideoCodecVP9:
      encoder = webrtc::VP9Encoder::Create();
      break;

    default:
      break;
  }
  return encoder;
}

std::vector<webrtc::SdpVideoFormat> WebrtcVideoConduit::GetSupportedFormats()
    const {
  MOZ_ASSERT_UNREACHABLE("Unexpected call");
  CSFLogError(LOGTAG, "Unexpected call to GetSupportedFormats()");
  return {webrtc::SdpVideoFormat("VP8")};
}

WebrtcVideoConduit::CodecInfo WebrtcVideoConduit::QueryVideoEncoder(
    const webrtc::SdpVideoFormat& format) const {
  MOZ_ASSERT_UNREACHABLE("Unexpected call");
  CSFLogError(LOGTAG, "Unexpected call to QueryVideoEncoder()");
  CodecInfo info;
  info.is_hardware_accelerated = false;
  info.has_internal_source = false;
  return info;
}

std::unique_ptr<webrtc::VideoEncoder> WebrtcVideoConduit::CreateVideoEncoder(
    const webrtc::SdpVideoFormat& format) {
  MOZ_ASSERT(format.name == "VP8");
  std::unique_ptr<webrtc::VideoEncoder> encoder = nullptr;
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
  encoder = webrtc::VP8Encoder::Create();
  return encoder;
}

// XXX we need to figure out how to feed back changes in preferred capture
// resolution to the getUserMedia source.
void WebrtcVideoConduit::SelectSendResolution(unsigned short width,
                                              unsigned short height) {
  mMutex.AssertCurrentThreadOwns();
  // XXX This will do bandwidth-resolution adaptation as well - bug 877954

  // Enforce constraints
  if (mCurSendCodecConfig) {
    uint16_t max_width = mCurSendCodecConfig->mEncodingConstraints.maxWidth;
    uint16_t max_height = mCurSendCodecConfig->mEncodingConstraints.maxHeight;
    if (max_width || max_height) {
      max_width = max_width ? max_width : UINT16_MAX;
      max_height = max_height ? max_height : UINT16_MAX;
      ConstrainPreservingAspectRatio(max_width, max_height, &width, &height);
    }

    int max_fs = mSinkWantsPixelCount;
    // Limit resolution to max-fs
    if (mCurSendCodecConfig->mEncodingConstraints.maxFs) {
      // max-fs is in macroblocks, convert to pixels
      max_fs = std::min(
          max_fs,
          static_cast<int>(mCurSendCodecConfig->mEncodingConstraints.maxFs *
                           (16 * 16)));
    }
    mVideoAdapter->OnResolutionFramerateRequest(
        rtc::Optional<int>(), max_fs, std::numeric_limits<int>::max());
  }

  unsigned int framerate = SelectSendFrameRate(
      mCurSendCodecConfig.get(), mSendingFramerate, width, height);
  if (mSendingFramerate != framerate) {
    CSFLogDebug(LOGTAG, "%s: framerate changing to %u (from %u)", __FUNCTION__,
                framerate, mSendingFramerate);
    mSendingFramerate = framerate;
    mVideoStreamFactory->SetSendingFramerate(mSendingFramerate);
  }
}

void WebrtcVideoConduit::AddOrUpdateSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  if (!NS_IsMainThread()) {
    // This may be called off main thread, but only to update an already added
    // sink. If we add it after the dispatch we're at risk of a UAF.
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("WebrtcVideoConduit::UpdateSink",
                               [this, self = RefPtr<WebrtcVideoConduit>(this),
                                sink, wants = std::move(wants)]() {
                                 AddOrUpdateSinkNotLocked(sink, wants);
                               }));
    return;
  }

  mMutex.AssertCurrentThreadOwns();
  if (!mRegisteredSinks.Contains(sink)) {
    mRegisteredSinks.AppendElement(sink);
  }
  mVideoBroadcaster.AddOrUpdateSink(sink, wants);
  OnSinkWantsChanged(mVideoBroadcaster.wants());
}

void WebrtcVideoConduit::AddOrUpdateSinkNotLocked(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  MutexAutoLock lock(mMutex);
  AddOrUpdateSink(sink, wants);
}

void WebrtcVideoConduit::RemoveSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  mRegisteredSinks.RemoveElement(sink);
  mVideoBroadcaster.RemoveSink(sink);
  OnSinkWantsChanged(mVideoBroadcaster.wants());
}

void WebrtcVideoConduit::RemoveSinkNotLocked(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  MutexAutoLock lock(mMutex);
  RemoveSink(sink);
}

void WebrtcVideoConduit::OnSinkWantsChanged(const rtc::VideoSinkWants& wants) {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mLockScaling) {
    return;
  }

  CSFLogDebug(LOGTAG, "%s (send SSRC %u (0x%x)) - wants pixels = %d",
              __FUNCTION__, mSendStreamConfig.rtp.ssrcs.front(),
              mSendStreamConfig.rtp.ssrcs.front(), wants.max_pixel_count);

  if (!mCurSendCodecConfig) {
    return;
  }

  mSinkWantsPixelCount = wants.max_pixel_count;
  mUpdateResolution = true;
}

MediaConduitErrorCode WebrtcVideoConduit::SendVideoFrame(
    const webrtc::VideoFrame& frame) {
  // XXX Google uses a "timestamp_aligner" to translate timestamps from the
  // camera via TranslateTimestamp(); we should look at doing the same.  This
  // avoids sampling error when capturing frames, but google had to deal with
  // some broken cameras, include Logitech c920's IIRC.

  int cropWidth;
  int cropHeight;
  int adaptedWidth;
  int adaptedHeight;
  {
    MutexAutoLock lock(mMutex);
    CSFLogVerbose(LOGTAG, "WebrtcVideoConduit %p %s (send SSRC %u (0x%x))",
                  this, __FUNCTION__, mSendStreamConfig.rtp.ssrcs.front(),
                  mSendStreamConfig.rtp.ssrcs.front());

    if (mUpdateResolution || frame.width() != mLastWidth ||
        frame.height() != mLastHeight) {
      // See if we need to recalculate what we're sending.
      CSFLogVerbose(LOGTAG, "%s: call SelectSendResolution with %ux%u",
                    __FUNCTION__, frame.width(), frame.height());
      MOZ_ASSERT(frame.width() != 0 && frame.height() != 0);
      // Note coverity will flag this since it thinks they can be 0
      MOZ_ASSERT(mCurSendCodecConfig);

      mLastWidth = frame.width();
      mLastHeight = frame.height();
      mUpdateResolution = false;
      SelectSendResolution(frame.width(), frame.height());
    }

    // adapt input video to wants of sink
    if (!mVideoBroadcaster.frame_wanted()) {
      return kMediaConduitNoError;
    }

    if (!mVideoAdapter->AdaptFrameResolution(
            frame.width(), frame.height(),
            frame.timestamp_us() * rtc::kNumNanosecsPerMicrosec, &cropWidth,
            &cropHeight, &adaptedWidth, &adaptedHeight)) {
      // VideoAdapter dropped the frame.
      return kMediaConduitNoError;
    }
  }

  // If we have zero width or height, drop the frame here. Attempting to send
  // it will cause all sorts of problems in the webrtc.org code.
  if (cropWidth == 0 || cropHeight == 0) {
    return kMediaConduitNoError;
  }

  int cropX = (frame.width() - cropWidth) / 2;
  int cropY = (frame.height() - cropHeight) / 2;

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer;
  if (adaptedWidth == frame.width() && adaptedHeight == frame.height()) {
    // No adaption - optimized path.
    buffer = frame.video_frame_buffer();
  } else {
    // Adapted I420 frame.
    rtc::scoped_refptr<webrtc::I420Buffer> i420Buffer =
        mBufferPool.CreateBuffer(adaptedWidth, adaptedHeight);
    if (!i420Buffer) {
      CSFLogWarn(LOGTAG, "Creating a buffer for scaling failed, pool is empty");
      return kMediaConduitNoError;
    }
    i420Buffer->CropAndScaleFrom(*frame.video_frame_buffer()->GetI420().get(),
                                 cropX, cropY, cropWidth, cropHeight);
    buffer = i420Buffer;
  }

  mVideoBroadcaster.OnFrame(webrtc::VideoFrame(
      buffer, frame.timestamp(), frame.render_time_ms(), frame.rotation()));

  mStsThread->Dispatch(NS_NewRunnableFunction(
      "SendStreamStatistics::FrameDeliveredToEncoder",
      [self = RefPtr<WebrtcVideoConduit>(this), this]() mutable {
        mSendStreamStats.FrameDeliveredToEncoder();
        NS_ReleaseOnMainThreadSystemGroup(
            "SendStreamStatistics::FrameDeliveredToEncoder", self.forget());
      }));
  return kMediaConduitNoError;
}

// Transport Layer Callbacks

MediaConduitErrorCode WebrtcVideoConduit::DeliverPacket(const void* data,
                                                        int len) {
  ASSERT_ON_THREAD(mStsThread);

  // Bug 1499796 - we need to get passed the time the packet was received
  webrtc::PacketReceiver::DeliveryStatus status =
      mCall->Call()->Receiver()->DeliverPacket(
          webrtc::MediaType::VIDEO, static_cast<const uint8_t*>(data), len,
          webrtc::PacketTime());

  if (status != webrtc::PacketReceiver::DELIVERY_OK) {
    CSFLogError(LOGTAG, "%s DeliverPacket Failed, %d", __FUNCTION__, status);
    return kMediaConduitRTPProcessingFailed;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcVideoConduit::ReceivedRTPPacket(const void* data,
                                                            int len,
                                                            uint32_t ssrc) {
  ASSERT_ON_THREAD(mStsThread);

  if (mAllowSsrcChange || mWaitingForInitialSsrc) {
    // Handle the unknown ssrc (and ssrc-not-signaled case).
    // We can't just do this here; it has to happen on MainThread :-(
    // We also don't want to drop the packet, nor stall this thread, so we hold
    // the packet (and any following) for inserting once the SSRC is set.
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
      NS_DispatchToMainThread(NS_NewRunnableFunction(
          "WebrtcVideoConduit::WebrtcGmpPCHandleSetter",
          [this, self = RefPtr<WebrtcVideoConduit>(this), ssrc]() mutable {
            // Normally this is done in CreateOrUpdateMediaPipeline() for
            // initial creation and renegotiation, but here we're rebuilding the
            // Receive channel at a lower level.  This is needed whenever we're
            // creating a GMPVideoCodec (in particular, H264) so it can
            // communicate errors to the PC.
            WebrtcGmpPCHandleSetter setter(mPCHandle);
            SetRemoteSSRC(
                ssrc);  // this will likely re-create the VideoReceiveStream
            // We want to unblock the queued packets on the original thread
            mStsThread->Dispatch(NS_NewRunnableFunction(
                "WebrtcVideoConduit::QueuedPacketsHandler",
                [this, self = RefPtr<WebrtcVideoConduit>(this),
                 ssrc]() mutable {
                  if (ssrc != mRecvSSRC) {
                    // this is an intermediate switch; another is in-flight
                    return;
                  }
                  mRtpPacketQueue.DequeueAll(this);
                  NS_ReleaseOnMainThreadSystemGroup(
                      "WebrtcVideoConduit::QueuedPacketsHandler",
                      self.forget());
                }));
          }));
      return kMediaConduitNoError;
    }
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

MediaConduitErrorCode WebrtcVideoConduit::ReceivedRTCPPacket(const void* data,
                                                             int len) {
  ASSERT_ON_THREAD(mStsThread);

  CSFLogVerbose(LOGTAG, " %s Len %d ", __FUNCTION__, len);

  if (DeliverPacket(data, len) != kMediaConduitNoError) {
    CSFLogError(LOGTAG, "%s RTCP Processing Failed", __FUNCTION__);
    return kMediaConduitRTPProcessingFailed;
  }

  // TODO(bug 1496533): We will need to keep separate timestamps for each SSRC,
  // and for each SSRC we will need to keep a timestamp for SR and RR.
  mLastRtcpReceived = Some(GetNow());
  return kMediaConduitNoError;
}

// TODO(bug 1496533): We will need to add a type (ie; SR or RR) param here, or
// perhaps break this function into two functions, one for each type.
Maybe<DOMHighResTimeStamp> WebrtcVideoConduit::LastRtcpReceived() const {
  ASSERT_ON_THREAD(mStsThread);
  return mLastRtcpReceived;
}

MediaConduitErrorCode WebrtcVideoConduit::StopTransmitting() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  return StopTransmittingLocked();
}

MediaConduitErrorCode WebrtcVideoConduit::StartTransmitting() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  return StartTransmittingLocked();
}

MediaConduitErrorCode WebrtcVideoConduit::StopReceiving() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  return StopReceivingLocked();
}

MediaConduitErrorCode WebrtcVideoConduit::StartReceiving() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  return StartReceivingLocked();
}

MediaConduitErrorCode WebrtcVideoConduit::StopTransmittingLocked() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mEngineTransmitting) {
    if (mSendStream) {
      CSFLogDebug(LOGTAG, "%s Engine Already Sending. Attemping to Stop ",
                  __FUNCTION__);
      mSendStream->Stop();
    }

    mEngineTransmitting = false;
    UpdateVideoStatsTimer();
  }
  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcVideoConduit::StartTransmittingLocked() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mEngineTransmitting) {
    return kMediaConduitNoError;
  }

  CSFLogDebug(LOGTAG, "%s Attemping to start... ", __FUNCTION__);
  // Start Transmitting on the video engine
  if (!mSendStream) {
    MediaConduitErrorCode rval = CreateSendStream();
    if (rval != kMediaConduitNoError) {
      CSFLogError(LOGTAG, "%s Start Send Error %d ", __FUNCTION__, rval);
      return rval;
    }
  }

  mSendStream->Start();
  // XXX File a bug to consider hooking this up to the state of mtransport
  mCall->Call()->SignalChannelNetworkState(webrtc::MediaType::VIDEO,
                                           webrtc::kNetworkUp);
  mEngineTransmitting = true;
  UpdateVideoStatsTimer();

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcVideoConduit::StopReceivingLocked() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  // Are we receiving already? If so, stop receiving and playout
  // since we can't apply new recv codec when the engine is playing.
  if (mEngineReceiving && mRecvStream) {
    CSFLogDebug(LOGTAG, "%s Engine Already Receiving . Attemping to Stop ",
                __FUNCTION__);
    mRecvStream->Stop();
  }

  mEngineReceiving = false;
  UpdateVideoStatsTimer();
  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcVideoConduit::StartReceivingLocked() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mEngineReceiving) {
    return kMediaConduitNoError;
  }

  CSFLogDebug(LOGTAG, "%s Attemping to start... (SSRC %u (0x%x))", __FUNCTION__,
              static_cast<uint32_t>(mRecvSSRC),
              static_cast<uint32_t>(mRecvSSRC));
  // Start Receiving on the video engine
  if (!mRecvStream) {
    MediaConduitErrorCode rval = CreateRecvStream();
    if (rval != kMediaConduitNoError) {
      CSFLogError(LOGTAG, "%s Start Receive Error %d ", __FUNCTION__, rval);
      return rval;
    }
  }

  mRecvStream->Start();
  // XXX File a bug to consider hooking this up to the state of mtransport
  mCall->Call()->SignalChannelNetworkState(webrtc::MediaType::VIDEO,
                                           webrtc::kNetworkUp);
  mEngineReceiving = true;
  UpdateVideoStatsTimer();

  return kMediaConduitNoError;
}

// WebRTC::RTP Callback Implementation
// Called on MTG thread
bool WebrtcVideoConduit::SendRtp(const uint8_t* packet, size_t length,
                                 const webrtc::PacketOptions& options) {
  // XXX(pkerr) - PacketOptions possibly containing RTP extensions are ignored.
  // The only field in it is the packet_id, which is used when the header
  // extension for TransportSequenceNumber is being used, which we don't.
  CSFLogVerbose(LOGTAG, "%s Sent RTP Packet seq %d, len %lu, SSRC %u (0x%x)",
                __FUNCTION__, (uint16_t)ntohs(*((uint16_t*)&packet[2])),
                (unsigned long)length,
                (uint32_t)ntohl(*((uint32_t*)&packet[8])),
                (uint32_t)ntohl(*((uint32_t*)&packet[8])));

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if (!mTransmitterTransport ||
      NS_FAILED(mTransmitterTransport->SendRtpPacket(packet, length))) {
    CSFLogError(LOGTAG, "%s RTP Packet Send Failed ", __FUNCTION__);
    return false;
  }
  return true;
}

// Called from multiple threads including webrtc Process thread
bool WebrtcVideoConduit::SendRtcp(const uint8_t* packet, size_t length) {
  CSFLogVerbose(LOGTAG, "%s : len %lu ", __FUNCTION__, (unsigned long)length);
  // We come here if we have only one pipeline/conduit setup,
  // such as for unidirectional streams.
  // We also end up here if we are receiving
  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if (mReceiverTransport &&
      NS_SUCCEEDED(mReceiverTransport->SendRtcpPacket(packet, length))) {
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

void WebrtcVideoConduit::OnFrame(const webrtc::VideoFrame& video_frame) {
  CSFLogVerbose(LOGTAG, "%s: recv SSRC %u (0x%x), size %ux%u", __FUNCTION__,
                static_cast<uint32_t>(mRecvSSRC),
                static_cast<uint32_t>(mRecvSSRC), video_frame.width(),
                video_frame.height());
  ReentrantMonitorAutoEnter enter(mTransportMonitor);

  if (!mRenderer) {
    CSFLogError(LOGTAG, "%s Renderer is NULL  ", __FUNCTION__);
    return;
  }

  if (mReceivingWidth != video_frame.width() ||
      mReceivingHeight != video_frame.height()) {
    mReceivingWidth = video_frame.width();
    mReceivingHeight = video_frame.height();
    mRenderer->FrameSizeChange(mReceivingWidth, mReceivingHeight);
  }

  // Attempt to retrieve an timestamp encoded in the image pixels if enabled.
  if (mVideoLatencyTestEnable && mReceivingWidth && mReceivingHeight) {
    uint64_t now = PR_Now();
    uint64_t timestamp = 0;
    uint8_t* data = const_cast<uint8_t*>(
        video_frame.video_frame_buffer()->GetI420()->DataY());
    bool ok = YuvStamper::Decode(
        mReceivingWidth, mReceivingHeight, mReceivingWidth, data,
        reinterpret_cast<unsigned char*>(&timestamp), sizeof(timestamp), 0, 0);
    if (ok) {
      VideoLatencyUpdate(now - timestamp);
    }
  }

  mRenderer->RenderVideoFrame(*video_frame.video_frame_buffer(),
                              video_frame.timestamp(),
                              video_frame.render_time_ms());
}

void WebrtcVideoConduit::DumpCodecDB() const {
  MOZ_ASSERT(NS_IsMainThread());

  for (auto& entry : mRecvCodecList) {
    CSFLogDebug(LOGTAG, "Payload Name: %s", entry->mName.c_str());
    CSFLogDebug(LOGTAG, "Payload Type: %d", entry->mType);
    CSFLogDebug(LOGTAG, "Payload Max Frame Size: %d",
                entry->mEncodingConstraints.maxFs);
    CSFLogDebug(LOGTAG, "Payload Max Frame Rate: %d",
                entry->mEncodingConstraints.maxFps);
  }
}

void WebrtcVideoConduit::VideoLatencyUpdate(uint64_t newSample) {
  mTransportMonitor.AssertCurrentThreadIn();

  mVideoLatencyAvg =
      (sRoundingPadding * newSample + sAlphaNum * mVideoLatencyAvg) / sAlphaDen;
}

uint64_t WebrtcVideoConduit::MozVideoLatencyAvg() {
  mTransportMonitor.AssertCurrentThreadIn();

  return mVideoLatencyAvg / sRoundingPadding;
}

void WebrtcVideoConduit::OnRtcpBye() {
  RefPtr<WebrtcVideoConduit> self = this;
  NS_DispatchToMainThread(media::NewRunnableFrom([self]() mutable {
    MOZ_ASSERT(NS_IsMainThread());
    if (self->mRtcpEventObserver) {
      self->mRtcpEventObserver->OnRtcpBye();
    }
    return NS_OK;
  }));
}

void WebrtcVideoConduit::OnRtcpTimeout() {
  RefPtr<WebrtcVideoConduit> self = this;
  NS_DispatchToMainThread(media::NewRunnableFrom([self]() mutable {
    MOZ_ASSERT(NS_IsMainThread());
    if (self->mRtcpEventObserver) {
      self->mRtcpEventObserver->OnRtcpTimeout();
    }
    return NS_OK;
  }));
}

void WebrtcVideoConduit::SetRtcpEventObserver(
    mozilla::RtcpEventObserver* observer) {
  MOZ_ASSERT(NS_IsMainThread());
  mRtcpEventObserver = observer;
}

uint64_t WebrtcVideoConduit::CodecPluginID() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mSendCodecPluginID) {
    return mSendCodecPluginID;
  }
  if (mRecvCodecPluginID) {
    return mRecvCodecPluginID;
  }

  return 0;
}

bool WebrtcVideoConduit::RequiresNewSendStream(
    const VideoCodecConfig& newConfig) const {
  MOZ_ASSERT(NS_IsMainThread());

  return !mCurSendCodecConfig ||
         mCurSendCodecConfig->mName != newConfig.mName ||
         mCurSendCodecConfig->mType != newConfig.mType ||
         mCurSendCodecConfig->RtcpFbNackIsSet("") !=
             newConfig.RtcpFbNackIsSet("") ||
         mCurSendCodecConfig->RtcpFbFECIsSet() != newConfig.RtcpFbFECIsSet()
#if 0
    // XXX Do we still want/need to do this?
    || (newConfig.mName == "H264" &&
        !CompatibleH264Config(mEncoderSpecificH264, newConfig))
#endif
      ;
}

bool WebrtcVideoConduit::HasH264Hardware() {
  nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
  if (!gfxInfo) {
    return false;
  }
  int32_t status;
  nsCString discardFailureId;
  return NS_SUCCEEDED(gfxInfo->GetFeatureStatus(
             nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_H264, discardFailureId,
             &status)) &&
         status == nsIGfxInfo::FEATURE_STATUS_OK;
}

}  // namespace mozilla
