/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"
#include "nspr.h"
#include "plstr.h"

#include "VideoConduit.h"
#include "AudioConduit.h"
#include "nsThreadUtils.h"
#include "LoadManager.h"
#include "YuvStamper.h"
#include "nsServiceManagerUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/TemplateLib.h"

#include "webrtc/common_types.h"
#include "webrtc/common_video/interface/native_handle.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/video_engine/include/vie_errors.h"
#include "webrtc/video_engine/vie_defines.h"

#include "mozilla/Unused.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidJNIWrapper.h"
#endif

// for ntohs
#ifdef _MSC_VER
#include "Winsock2.h"
#else
#include <netinet/in.h>
#endif

#include <algorithm>
#include <math.h>

#define DEFAULT_VIDEO_MAX_FRAMERATE 30
#define INVALID_RTP_PAYLOAD 255  //valid payload types are 0 to 127

namespace mozilla {

static const char* logTag ="WebrtcVideoSessionConduit";

// 32 bytes is what WebRTC CodecInst expects
const unsigned int WebrtcVideoConduit::CODEC_PLNAME_SIZE = 32;

template<typename T>
T MinIgnoreZero(const T& a, const T& b)
{
  return std::min(a? a:b, b? b:a);
}

/**
 * Factory Method for VideoConduit
 */
RefPtr<VideoSessionConduit>
VideoSessionConduit::Create()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  WebrtcVideoConduit* obj = new WebrtcVideoConduit();
  if(obj->Init() != kMediaConduitNoError)
  {
    CSFLogError(logTag,  "%s VideoConduit Init Failed ", __FUNCTION__);
    delete obj;
    return nullptr;
  }
  CSFLogDebug(logTag,  "%s Successfully created VideoConduit ", __FUNCTION__);
  return obj;
}

WebrtcVideoConduit::WebrtcVideoConduit():
  mVideoEngine(nullptr),
  mTransportMonitor("WebrtcVideoConduit"),
  mTransmitterTransport(nullptr),
  mReceiverTransport(nullptr),
  mRenderer(nullptr),
  mPtrExtCapture(nullptr),
  mEngineTransmitting(false),
  mEngineReceiving(false),
  mChannel(-1),
  mCapId(-1),
  mCodecMutex("VideoConduit codec db"),
  mInReconfig(false),
  mLastWidth(0), // forces a check for reconfig at start
  mLastHeight(0),
  mSendingWidth(0),
  mSendingHeight(0),
  mReceivingWidth(0),
  mReceivingHeight(0),
  mSendingFramerate(DEFAULT_VIDEO_MAX_FRAMERATE),
  mLastFramerateTenths(DEFAULT_VIDEO_MAX_FRAMERATE*10),
  mNumReceivingStreams(1),
  mVideoLatencyTestEnable(false),
  mVideoLatencyAvg(0),
  mMinBitrate(0),
  mStartBitrate(0),
  mPrefMaxBitrate(0),
  mNegotiatedMaxBitrate(0),
  mMinBitrateEstimate(0),
  mRtpStreamIdEnabled(false),
  mRtpStreamIdExtId(0),
  mCodecMode(webrtc::kRealtimeVideo)
{}

WebrtcVideoConduit::~WebrtcVideoConduit()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  // Release AudioConduit first by dropping reference on MainThread, where it expects to be
  SyncTo(nullptr);
  Destroy();
}

bool WebrtcVideoConduit::SetLocalSSRC(unsigned int ssrc)
{
  unsigned int oldSsrc;
  if (!GetLocalSSRC(&oldSsrc)) {
    MOZ_ASSERT(false, "GetLocalSSRC failed");
    return false;
  }

  if (oldSsrc == ssrc) {
    return true;
  }

  bool wasTransmitting = mEngineTransmitting;
  if (StopTransmitting() != kMediaConduitNoError) {
    return false;
  }

  if (mPtrRTP->SetLocalSSRC(mChannel, ssrc)) {
    return false;
  }

  if (wasTransmitting) {
    if (StartTransmitting() != kMediaConduitNoError) {
      return false;
    }
  }
  return true;
}

bool WebrtcVideoConduit::GetLocalSSRC(unsigned int* ssrc)
{
  return !mPtrRTP->GetLocalSSRC(mChannel, *ssrc);
}

bool WebrtcVideoConduit::GetRemoteSSRC(unsigned int* ssrc)
{
  return !mPtrRTP->GetRemoteSSRC(mChannel, *ssrc);
}

bool WebrtcVideoConduit::SetLocalCNAME(const char* cname)
{
  char temp[256];
  strncpy(temp, cname, sizeof(temp) - 1);
  temp[sizeof(temp) - 1] = 0;
  return !mPtrRTP->SetRTCPCName(mChannel, temp);
}

bool WebrtcVideoConduit::GetVideoEncoderStats(double* framerateMean,
                                              double* framerateStdDev,
                                              double* bitrateMean,
                                              double* bitrateStdDev,
                                              uint32_t* droppedFrames)
{
  if (!mEngineTransmitting) {
    return false;
  }
  MOZ_ASSERT(mVideoCodecStat);
  mVideoCodecStat->GetEncoderStats(framerateMean, framerateStdDev,
                                   bitrateMean, bitrateStdDev,
                                   droppedFrames);

  // See if we need to adjust bandwidth.
  // Avoid changing bandwidth constantly; use hysteresis.

  // Note: mLastFramerate is a relaxed Atomic because we're setting it here, and
  // reading it on whatever thread calls DeliverFrame/SendVideoFrame.  Alternately
  // we could use a lock.  Note that we don't change it often, and read it once per frame.
  // We scale by *10 because mozilla::Atomic<> doesn't do 'double' or 'float'.
  double framerate = mLastFramerateTenths/10.0; // fetch once
  if (std::abs(*framerateMean - framerate)/framerate > 0.1 &&
      *framerateMean >= 0.5) {
    // unchanged resolution, but adjust bandwidth limits to match camera fps
    CSFLogDebug(logTag, "Encoder frame rate changed from %f to %f",
                (mLastFramerateTenths/10.0), *framerateMean);
    MutexAutoLock lock(mCodecMutex);
    mLastFramerateTenths = *framerateMean * 10;
    SelectSendResolution(mSendingWidth, mSendingHeight, nullptr);
  }
  return true;
}

bool WebrtcVideoConduit::GetVideoDecoderStats(double* framerateMean,
                                              double* framerateStdDev,
                                              double* bitrateMean,
                                              double* bitrateStdDev,
                                              uint32_t* discardedPackets)
{
  if (!mEngineReceiving) {
    return false;
  }
  MOZ_ASSERT(mVideoCodecStat);
  mVideoCodecStat->GetDecoderStats(framerateMean, framerateStdDev,
                                   bitrateMean, bitrateStdDev,
                                   discardedPackets);
  return true;
}

bool WebrtcVideoConduit::GetAVStats(int32_t* jitterBufferDelayMs,
                                    int32_t* playoutBufferDelayMs,
                                    int32_t* avSyncOffsetMs) {
  return false;
}

bool WebrtcVideoConduit::GetRTPStats(unsigned int* jitterMs,
                                     unsigned int* cumulativeLost) {
  unsigned short fractionLost;
  unsigned extendedMax;
  int64_t rttMs;
  // GetReceivedRTCPStatistics is a poorly named GetRTPStatistics variant
  return !mPtrRTP->GetReceivedRTCPStatistics(mChannel, fractionLost,
                                             *cumulativeLost,
                                             extendedMax,
                                             *jitterMs,
                                             rttMs);
}

bool WebrtcVideoConduit::GetRTCPReceiverReport(DOMHighResTimeStamp* timestamp,
                                               uint32_t* jitterMs,
                                               uint32_t* packetsReceived,
                                               uint64_t* bytesReceived,
                                               uint32_t* cumulativeLost,
                                               int32_t* rttMs) {
  uint32_t ntpHigh, ntpLow;
  uint16_t fractionLost;
  bool result = !mPtrRTP->GetRemoteRTCPReceiverInfo(mChannel, ntpHigh, ntpLow,
                                                    *packetsReceived,
                                                    *bytesReceived,
                                                    jitterMs,
                                                    &fractionLost,
                                                    cumulativeLost,
                                                    rttMs);
  if (result) {
    *timestamp = NTPtoDOMHighResTimeStamp(ntpHigh, ntpLow);
  }
  return result;
}

bool WebrtcVideoConduit::GetRTCPSenderReport(DOMHighResTimeStamp* timestamp,
                                             unsigned int* packetsSent,
                                             uint64_t* bytesSent) {
  struct webrtc::SenderInfo senderInfo;
  bool result = !mPtrRTP->GetRemoteRTCPSenderInfo(mChannel, &senderInfo);
  if (result) {
    *timestamp = NTPtoDOMHighResTimeStamp(senderInfo.NTP_timestamp_high,
                                          senderInfo.NTP_timestamp_low);
    *packetsSent = senderInfo.sender_packet_count;
    *bytesSent = senderInfo.sender_octet_count;
  }
  return result;
}

MediaConduitErrorCode
WebrtcVideoConduit::InitMain()
{
#if defined(MOZILLA_INTERNAL_API)
  // already know we must be on MainThread barring unit test weirdness
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
  if (!NS_WARN_IF(NS_FAILED(rv)))
  {
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);

    if (branch)
    {
      int32_t temp;
      Unused << NS_WARN_IF(NS_FAILED(branch->GetBoolPref("media.video.test_latency", &mVideoLatencyTestEnable)));
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref("media.peerconnection.video.min_bitrate", &temp))))
      {
         if (temp >= 0) {
            mMinBitrate = temp;
         }
      }
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref("media.peerconnection.video.start_bitrate", &temp))))
      {
         if (temp >= 0) {
         mStartBitrate = temp;
         }
      }
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref("media.peerconnection.video.max_bitrate", &temp))))
      {
        if (temp >= 0) {
          mPrefMaxBitrate = temp;
          mNegotiatedMaxBitrate = temp; // simplifies logic in SelectBitrate (don't have to do two limit tests)
        }
      }
      if (mMinBitrate != 0 && mMinBitrate < webrtc::kViEMinCodecBitrate) {
        mMinBitrate = webrtc::kViEMinCodecBitrate;
      }
      if (mStartBitrate < mMinBitrate) {
        mStartBitrate = mMinBitrate;
      }
      if (mPrefMaxBitrate && mStartBitrate > mPrefMaxBitrate) {
        mStartBitrate = mPrefMaxBitrate;
      }
      if (!NS_WARN_IF(NS_FAILED(branch->GetIntPref("media.peerconnection.video.min_bitrate_estimate", &temp))))
      {
        if (temp >= 0) {
          mMinBitrateEstimate = temp;
        }
      }
      bool use_loadmanager = false;
      if (!NS_WARN_IF(NS_FAILED(branch->GetBoolPref("media.navigator.load_adapt", &use_loadmanager))))
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

  if (webrtc::VideoEngine::SetAndroidObjects(jvm) != 0) {
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
  CSFLogDebug(logTag,  "%s this=%p", __FUNCTION__, this);
  MediaConduitErrorCode result;
  // Run code that must run on MainThread first
  MOZ_ASSERT(NS_IsMainThread());
  result = InitMain();
  if (result != kMediaConduitNoError) {
    return result;
  }

  // Per WebRTC APIs below function calls return nullptr on failure
  mVideoEngine = webrtc::VideoEngine::Create();
  if(!mVideoEngine)
  {
    CSFLogError(logTag, "%s Unable to create video engine ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if( !(mPtrViEBase = ViEBase::GetInterface(mVideoEngine)))
  {
    CSFLogError(logTag, "%s Unable to get video base interface ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if( !(mPtrViECapture = ViECapture::GetInterface(mVideoEngine)))
  {
    CSFLogError(logTag, "%s Unable to get video capture interface", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if( !(mPtrViECodec = ViECodec::GetInterface(mVideoEngine)))
  {
    CSFLogError(logTag, "%s Unable to get video codec interface ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if( !(mPtrViENetwork = ViENetwork::GetInterface(mVideoEngine)))
  {
    CSFLogError(logTag, "%s Unable to get video network interface ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if( !(mPtrViERender = ViERender::GetInterface(mVideoEngine)))
  {
    CSFLogError(logTag, "%s Unable to get video render interface ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  mPtrExtCodec = webrtc::ViEExternalCodec::GetInterface(mVideoEngine);
  if (!mPtrExtCodec) {
    CSFLogError(logTag, "%s Unable to get external codec interface: %d ",
                __FUNCTION__,mPtrViEBase->LastError());
    return kMediaConduitSessionNotInited;
  }

  if( !(mPtrRTP = webrtc::ViERTP_RTCP::GetInterface(mVideoEngine)))
  {
    CSFLogError(logTag, "%s Unable to get video RTCP interface ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if ( !(mPtrExtCodec = webrtc::ViEExternalCodec::GetInterface(mVideoEngine)))
  {
    CSFLogError(logTag, "%s Unable to get external codec interface %d ",
                __FUNCTION__, mPtrViEBase->LastError());
    return kMediaConduitSessionNotInited;
  }

  CSFLogDebug(logTag, "%s Engine Created: Init'ng the interfaces ",__FUNCTION__);

  if(mPtrViEBase->Init() == -1)
  {
    CSFLogError(logTag, " %s Video Engine Init Failed %d ",__FUNCTION__,
                mPtrViEBase->LastError());
    return kMediaConduitSessionNotInited;
  }

  if(mPtrViEBase->CreateChannel(mChannel) == -1)
  {
    CSFLogError(logTag, " %s Channel creation Failed %d ",__FUNCTION__,
                mPtrViEBase->LastError());
    return kMediaConduitChannelError;
  }

  if(mPtrViENetwork->RegisterSendTransport(mChannel, *this) == -1)
  {
    CSFLogError(logTag,  "%s ViENetwork Failed %d ", __FUNCTION__,
                mPtrViEBase->LastError());
    return kMediaConduitTransportRegistrationFail;
  }

  if(mPtrViECapture->AllocateExternalCaptureDevice(mCapId,
                                                   mPtrExtCapture) == -1)
  {
    CSFLogError(logTag, "%s Unable to Allocate capture module: %d ",
                __FUNCTION__, mPtrViEBase->LastError());
    return kMediaConduitCaptureError;
  }

  if(mPtrViECapture->ConnectCaptureDevice(mCapId,mChannel) == -1)
  {
    CSFLogError(logTag, "%s Unable to Connect capture module: %d ",
                __FUNCTION__,mPtrViEBase->LastError());
    return kMediaConduitCaptureError;
  }
  // Set up some parameters, per juberti. Set MTU.
  if(mPtrViENetwork->SetMTU(mChannel, 1200) != 0)
  {
    CSFLogError(logTag,  "%s MTU Failed %d ", __FUNCTION__,
                mPtrViEBase->LastError());
    return kMediaConduitMTUError;
  }
  // Turn on RTCP and loss feedback reporting.
  if(mPtrRTP->SetRTCPStatus(mChannel, webrtc::kRtcpCompound_RFC4585) != 0)
  {
    CSFLogError(logTag,  "%s RTCPStatus Failed %d ", __FUNCTION__,
                mPtrViEBase->LastError());
    return kMediaConduitRTCPStatusError;
  }

  if (mPtrViERender->AddRenderer(mChannel,
                                webrtc::kVideoI420,
                                (webrtc::ExternalRenderer*) this) == -1) {
      CSFLogError(logTag, "%s Failed to added external renderer ", __FUNCTION__);
      return kMediaConduitInvalidRenderer;
  }

  if (mLoadManager) {
    mPtrViEBase->RegisterCpuOveruseObserver(mChannel, mLoadManager);
    mPtrViEBase->SetLoadManager(mLoadManager);
  }

  CSFLogError(logTag, "%s Initialization Done", __FUNCTION__);
  return kMediaConduitNoError;
}

void
WebrtcVideoConduit::Destroy()
{
  // The first one of a pair to be deleted shuts down media for both
  //Deal with External Capturer
  if(mPtrViECapture)
  {
    mPtrViECapture->DisconnectCaptureDevice(mCapId);
    mPtrViECapture->ReleaseCaptureDevice(mCapId);
    mPtrExtCapture = nullptr;
  }

   if (mPtrExtCodec) {
     mPtrExtCodec->Release();
     mPtrExtCodec = NULL;
   }

  //Deal with External Renderer
  if(mPtrViERender)
  {
    if(mRenderer) {
      mPtrViERender->StopRender(mChannel);
    }
    mPtrViERender->RemoveRenderer(mChannel);
  }

  //Deal with the transport
  if(mPtrViENetwork)
  {
    mPtrViENetwork->DeregisterSendTransport(mChannel);
  }

  if(mPtrViEBase)
  {
    mPtrViEBase->StopSend(mChannel);
    mPtrViEBase->StopReceive(mChannel);
    mPtrViEBase->DeleteChannel(mChannel);
  }

  // mVideoCodecStat has a back-ptr to mPtrViECodec that must be released first
  if (mVideoCodecStat) {
    mVideoCodecStat->EndOfCallStats();
  }
  mVideoCodecStat = nullptr;
  // We can't delete the VideoEngine until all these are released!
  // And we can't use a Scoped ptr, since the order is arbitrary
  mPtrViEBase = nullptr;
  mPtrViECapture = nullptr;
  mPtrViECodec = nullptr;
  mPtrViENetwork = nullptr;
  mPtrViERender = nullptr;
  mPtrRTP = nullptr;
  mPtrExtCodec = nullptr;

  // only one opener can call Delete.  Have it be the last to close.
  if(mVideoEngine)
  {
    webrtc::VideoEngine::Delete(mVideoEngine);
  }
}

void
WebrtcVideoConduit::SyncTo(WebrtcAudioConduit *aConduit)
{
  CSFLogDebug(logTag, "%s Synced to %p", __FUNCTION__, aConduit);

  // SyncTo(value) syncs to the AudioConduit, and if already synced replaces
  // the current sync target.  SyncTo(nullptr) cancels any existing sync and
  // releases the strong ref to AudioConduit.
  if (aConduit) {
    mPtrViEBase->SetVoiceEngine(aConduit->GetVoiceEngine());
    mPtrViEBase->ConnectAudioChannel(mChannel, aConduit->GetChannel());
    // NOTE: this means the VideoConduit will keep the AudioConduit alive!
  } else {
    mPtrViEBase->DisconnectAudioChannel(mChannel);
    mPtrViEBase->SetVoiceEngine(nullptr);
  }

  mSyncedTo = aConduit;
}

MediaConduitErrorCode
WebrtcVideoConduit::AttachRenderer(RefPtr<VideoRenderer> aVideoRenderer)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  //null renderer
  if(!aVideoRenderer)
  {
    CSFLogError(logTag, "%s NULL Renderer", __FUNCTION__);
    MOZ_ASSERT(false);
    return kMediaConduitInvalidRenderer;
  }

  // This function is called only from main, so we only need to protect against
  // modifying mRenderer while any webrtc.org code is trying to use it.
  bool wasRendering;
  {
    ReentrantMonitorAutoEnter enter(mTransportMonitor);
    wasRendering = !!mRenderer;
    mRenderer = aVideoRenderer;
    // Make sure the renderer knows the resolution
    mRenderer->FrameSizeChange(mReceivingWidth,
                               mReceivingHeight,
                               mNumReceivingStreams);
  }

  if (!wasRendering) {
    if(mPtrViERender->StartRender(mChannel) == -1)
    {
      CSFLogError(logTag, "%s Starting the Renderer Failed %d ", __FUNCTION__,
                                                      mPtrViEBase->LastError());
      ReentrantMonitorAutoEnter enter(mTransportMonitor);
      mRenderer = nullptr;
      return kMediaConduitRendererFail;
    }
  }

  return kMediaConduitNoError;
}

void
WebrtcVideoConduit::DetachRenderer()
{
  {
    ReentrantMonitorAutoEnter enter(mTransportMonitor);
    if(mRenderer)
    {
      mRenderer = nullptr;
    }
  }

  mPtrViERender->StopRender(mChannel);
}

MediaConduitErrorCode
WebrtcVideoConduit::SetTransmitterTransport(RefPtr<TransportInterface> aTransport)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mTransmitterTransport = aTransport;
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::SetReceiverTransport(RefPtr<TransportInterface> aTransport)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mReceiverTransport = aTransport;
  return kMediaConduitNoError;
}
MediaConduitErrorCode
WebrtcVideoConduit::ConfigureCodecMode(webrtc::VideoCodecMode mode)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  mCodecMode = mode;
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
  CSFLogDebug(logTag,  "%s for %s", __FUNCTION__, codecConfig ? codecConfig->mName.c_str() : "<null>");
  bool codecFound = false;
  MediaConduitErrorCode condError = kMediaConduitNoError;
  int error = 0; //webrtc engine errors
  webrtc::VideoCodec  video_codec;
  std::string payloadName;

  memset(&video_codec, 0, sizeof(video_codec));

  {
    //validate basic params
    if((condError = ValidateCodecConfig(codecConfig,true)) != kMediaConduitNoError)
    {
      return condError;
    }
  }

  condError = StopTransmitting();
  if (condError != kMediaConduitNoError) {
    return condError;
  }

  if (mRtpStreamIdEnabled) {
    video_codec.ridId = mRtpStreamIdExtId;
  }
  if (mExternalSendCodec &&
      codecConfig->mType == mExternalSendCodec->mType) {
    CSFLogError(logTag, "%s Configuring External H264 Send Codec", __FUNCTION__);

    // width/height will be overridden on the first frame
    video_codec.width = 320;
    video_codec.height = 240;
#ifdef MOZ_WEBRTC_OMX
    if (codecConfig->mType == webrtc::kVideoCodecH264) {
      video_codec.resolution_divisor = 16;
    } else {
      video_codec.resolution_divisor = 1; // We could try using it to handle odd resolutions
    }
#else
    video_codec.resolution_divisor = 1; // We could try using it to handle odd resolutions
#endif
    video_codec.qpMax = 56;
    video_codec.numberOfSimulcastStreams = 1;
    video_codec.simulcastStream[0].jsScaleDownBy =
        codecConfig->mEncodingConstraints.scaleDownBy;
    video_codec.mode = mCodecMode;

    codecFound = true;
  } else {
    // we should be good here to set the new codec.
    for(int idx=0; idx < mPtrViECodec->NumberOfCodecs(); idx++)
    {
      if(0 == mPtrViECodec->GetCodec(idx, video_codec))
      {
        payloadName = video_codec.plName;
        if(codecConfig->mName.compare(payloadName) == 0)
        {
          // Note: side-effect of this is that video_codec is filled in
          // by GetCodec()
          codecFound = true;
          break;
        }
      }
    }//for
  }

  if(codecFound == false)
  {
    CSFLogError(logTag, "%s Codec Mismatch ", __FUNCTION__);
    return kMediaConduitInvalidSendCodec;
  }
  // Note: only for overriding parameters from GetCodec()!
  CodecConfigToWebRTCCodec(codecConfig, video_codec);
  if (mSendingWidth != 0) {
    // We're already in a call and are reconfiguring (perhaps due to
    // ReplaceTrack).  Set to match the last frame we sent.

    // We could also set mLastWidth to 0, to force immediate reconfig -
    // more expensive, but perhaps less risk of missing something.  Really
    // on ReplaceTrack we should just call ConfigureCodecMode(), and if the
    // mode changed, we re-configure.
    // Do this after CodecConfigToWebRTCCodec() to avoid messing up simulcast
    video_codec.width = mSendingWidth;
    video_codec.height = mSendingHeight;
    video_codec.maxFramerate = mSendingFramerate;
  } else {
    mSendingWidth = 0;
    mSendingHeight = 0;
    mSendingFramerate = video_codec.maxFramerate;
  }
  // So we can comply with b=TIAS/b=AS/maxbr=X when input resolution changes
  mNegotiatedMaxBitrate = MinIgnoreZero(mPrefMaxBitrate, video_codec.maxBitrate);

  video_codec.mode = mCodecMode;

  if(mPtrViECodec->SetSendCodec(mChannel, video_codec) == -1)
  {
    error = mPtrViEBase->LastError();
    if(error == kViECodecInvalidCodec)
    {
      CSFLogError(logTag, "%s Invalid Send Codec", __FUNCTION__);
      return kMediaConduitInvalidSendCodec;
    }
    CSFLogError(logTag, "%s SetSendCodec Failed %d ", __FUNCTION__,
                mPtrViEBase->LastError());
    return kMediaConduitUnknownError;
  }

  if (mMinBitrateEstimate != 0) {
    mPtrViENetwork->SetBitrateConfig(mChannel,
                                     mMinBitrateEstimate,
                                     std::max(video_codec.startBitrate,
                                              mMinBitrateEstimate),
                                     std::max(video_codec.maxBitrate,
                                              mMinBitrateEstimate));
  }

  if (!mVideoCodecStat) {
    mVideoCodecStat = new VideoCodecStatistics(mChannel, mPtrViECodec);
  }
  mVideoCodecStat->Register(true);

  // See Bug 1297058, enabling FEC when NACK is set on H.264 is problematic
  bool use_fec = codecConfig->RtcpFbFECIsSet();
  if ((mExternalSendCodec && codecConfig->mType == mExternalSendCodec->mType)
      || codecConfig->mType == webrtc::kVideoCodecH264) {
    if(codecConfig->RtcpFbNackIsSet("")) {
      use_fec = false;
    }
  }

  if (use_fec)
  {
    uint8_t payload_type_red = INVALID_RTP_PAYLOAD;
    uint8_t payload_type_ulpfec = INVALID_RTP_PAYLOAD;
    if (!DetermineREDAndULPFECPayloadTypes(payload_type_red, payload_type_ulpfec)) {
      CSFLogError(logTag, "%s Unable to set FEC status: could not determine"
                  "payload type: red %u ulpfec %u",
                  __FUNCTION__, payload_type_red, payload_type_ulpfec);
        return kMediaConduitFECStatusError;
    }

    if(codecConfig->RtcpFbNackIsSet("")) {
      CSFLogDebug(logTag, "Enabling NACK/FEC (send) for video stream\n");
      if (mPtrRTP->SetHybridNACKFECStatus(mChannel, true,
                                          payload_type_red,
                                          payload_type_ulpfec) != 0) {
        CSFLogError(logTag,  "%s SetHybridNACKFECStatus Failed %d ",
                    __FUNCTION__, mPtrViEBase->LastError());
        return kMediaConduitHybridNACKFECStatusError;
      }
    } else {
      CSFLogDebug(logTag, "Enabling FEC (send) for video stream\n");
      if (mPtrRTP->SetFECStatus(mChannel, true,
                                payload_type_red, payload_type_ulpfec) != 0)
      {
        CSFLogError(logTag,  "%s SetFECStatus Failed %d ", __FUNCTION__,
                    mPtrViEBase->LastError());
        return kMediaConduitFECStatusError;
      }
    }
  } else if(codecConfig->RtcpFbNackIsSet("")) {
    CSFLogDebug(logTag, "Enabling NACK (send) for video stream\n");
    if (mPtrRTP->SetNACKStatus(mChannel, true) != 0)
    {
      CSFLogError(logTag,  "%s NACKStatus Failed %d ", __FUNCTION__,
                  mPtrViEBase->LastError());
      return kMediaConduitNACKStatusError;
    }
  }

  {
    MutexAutoLock lock(mCodecMutex);

    //Copy the applied config for future reference.
    mCurSendCodecConfig = new VideoCodecConfig(*codecConfig);
  }

  bool remb_requested = codecConfig->RtcpFbRembIsSet();
  mPtrRTP->SetRembStatus(mChannel, true, remb_requested);

  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::ConfigureRecvMediaCodecs(
    const std::vector<VideoCodecConfig* >& codecConfigList)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  MediaConduitErrorCode condError = kMediaConduitNoError;
  bool success = false;
  std::string  payloadName;

  condError = StopReceiving();
  if (condError != kMediaConduitNoError) {
    return condError;
  }

  if(codecConfigList.empty())
  {
    CSFLogError(logTag, "%s Zero number of codecs to configure", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  webrtc::ViEKeyFrameRequestMethod kf_request = webrtc::kViEKeyFrameRequestNone;
  bool use_nack_basic = false;
  bool use_tmmbr = false;
  bool use_remb = false;
  bool use_fec = false;

  //Try Applying the codecs in the list
  // we treat as success if atleast one codec was applied and reception was
  // started successfully.
  for(std::vector<VideoCodecConfig*>::size_type i=0;i < codecConfigList.size();i++)
  {
    //if the codec param is invalid or diplicate, return error
    if((condError = ValidateCodecConfig(codecConfigList[i],false)) != kMediaConduitNoError)
    {
      return condError;
    }

    // Check for the keyframe request type: PLI is preferred
    // over FIR, and FIR is preferred over none.
    if (codecConfigList[i]->RtcpFbNackIsSet("pli"))
    {
      kf_request = webrtc::kViEKeyFrameRequestPliRtcp;
    } else if(kf_request == webrtc::kViEKeyFrameRequestNone &&
              codecConfigList[i]->RtcpFbCcmIsSet("fir"))
    {
      kf_request = webrtc::kViEKeyFrameRequestFirRtcp;
    }

    // Check whether NACK is requested
    if(codecConfigList[i]->RtcpFbNackIsSet(""))
    {
      use_nack_basic = true;
    }

    // Check whether TMMBR is requested
    if (codecConfigList[i]->RtcpFbCcmIsSet("tmmbr")) {
      use_tmmbr = true;
    }

    // Check whether REMB is requested
    if (codecConfigList[i]->RtcpFbRembIsSet()) {
      use_remb = true;
    }

    // Check whether FEC is requested
    if (codecConfigList[i]->RtcpFbFECIsSet()) {
      use_fec = true;
    }

    webrtc::VideoCodec  video_codec;

    memset(&video_codec, 0, sizeof(webrtc::VideoCodec));

    if (mExternalRecvCodec &&
        codecConfigList[i]->mType == mExternalRecvCodec->mType) {
      CSFLogError(logTag, "%s Configuring External H264 Receive Codec", __FUNCTION__);

      // XXX Do we need a separate setting for receive maxbitrate?  Is it
      // different for hardware codecs?  For now assume symmetry.
      CodecConfigToWebRTCCodec(codecConfigList[i], video_codec);

      // values SetReceiveCodec() cares about are name, type, maxbitrate
      if(mPtrViECodec->SetReceiveCodec(mChannel,video_codec) == -1)
      {
        CSFLogError(logTag, "%s Invalid Receive Codec %d ", __FUNCTION__,
                    mPtrViEBase->LastError());
      } else {
        CSFLogError(logTag, "%s Successfully Set the codec %s", __FUNCTION__,
                    codecConfigList[i]->mName.c_str());
        success = true;
      }
    } else {
      //Retrieve pre-populated codec structure for our codec.
      for(int idx=0; idx < mPtrViECodec->NumberOfCodecs(); idx++)
      {
        if(mPtrViECodec->GetCodec(idx, video_codec) == 0)
        {
          payloadName = video_codec.plName;
          if(codecConfigList[i]->mName.compare(payloadName) == 0)
          {
            CodecConfigToWebRTCCodec(codecConfigList[i], video_codec);
            if(mPtrViECodec->SetReceiveCodec(mChannel,video_codec) == -1)
            {
              CSFLogError(logTag, "%s Invalid Receive Codec %d ", __FUNCTION__,
                          mPtrViEBase->LastError());
            } else {
              CSFLogError(logTag, "%s Successfully Set the codec %s", __FUNCTION__,
                          codecConfigList[i]->mName.c_str());
              success = true;
            }
            break; //we found a match
          }
        }
      }//end for codeclist
    }
  }//end for

  if(!success)
  {
    CSFLogError(logTag, "%s Setting Receive Codec Failed ", __FUNCTION__);
    return kMediaConduitInvalidReceiveCodec;
  }

  if (!mVideoCodecStat) {
    mVideoCodecStat = new VideoCodecStatistics(mChannel, mPtrViECodec);
  }
  mVideoCodecStat->Register(false);

  // XXX Currently, we gather up all of the feedback types that the remote
  // party indicated it supports for all video codecs and configure the entire
  // conduit based on those capabilities. This is technically out of spec,
  // as these values should be configured on a per-codec basis. However,
  // the video engine only provides this API on a per-conduit basis, so that's
  // how we have to do it. The approach of considering the remote capablities
  // for the entire conduit to be a union of all remote codec capabilities
  // (rather than the more conservative approach of using an intersection)
  // is made to provide as many feedback mechanisms as are likely to be
  // processed by the remote party (and should be relatively safe, since the
  // remote party is required to ignore feedback types that it does not
  // understand).
  //
  // Note that our configuration uses this union of remote capabilites as
  // input to the configuration. It is not isomorphic to the configuration.
  // For example, it only makes sense to have one frame request mechanism
  // active at a time; so, if the remote party indicates more than one
  // supported mechanism, we're only configuring the one we most prefer.
  //
  // See http://code.google.com/p/webrtc/issues/detail?id=2331

  if (kf_request != webrtc::kViEKeyFrameRequestNone)
  {
    CSFLogDebug(logTag, "Enabling %s frame requests for video stream\n",
                (kf_request == webrtc::kViEKeyFrameRequestPliRtcp ?
                 "PLI" : "FIR"));
    if(mPtrRTP->SetKeyFrameRequestMethod(mChannel, kf_request) != 0)
    {
      CSFLogError(logTag,  "%s KeyFrameRequest Failed %d ", __FUNCTION__,
                  mPtrViEBase->LastError());
      return kMediaConduitKeyFrameRequestError;
    }
  }

  switch (kf_request) {
    case webrtc::kViEKeyFrameRequestNone:
      mFrameRequestMethod = FrameRequestNone;
      break;
    case webrtc::kViEKeyFrameRequestPliRtcp:
      mFrameRequestMethod = FrameRequestPli;
      break;
    case webrtc::kViEKeyFrameRequestFirRtcp:
      mFrameRequestMethod = FrameRequestFir;
      break;
    default:
      MOZ_ASSERT(false);
      mFrameRequestMethod = FrameRequestUnknown;
  }

  if (use_fec)
  {
    uint8_t payload_type_red = INVALID_RTP_PAYLOAD;
    uint8_t payload_type_ulpfec = INVALID_RTP_PAYLOAD;
    if (!DetermineREDAndULPFECPayloadTypes(payload_type_red, payload_type_ulpfec)) {
      CSFLogError(logTag, "%s Unable to set FEC status: could not determine"
                  "payload type: red %u ulpfec %u",
                  __FUNCTION__, payload_type_red, payload_type_ulpfec);
        return kMediaConduitFECStatusError;
    }

    // We also need to call SetReceiveCodec for RED and ULPFEC codecs
    for(int idx=0; idx < mPtrViECodec->NumberOfCodecs(); idx++) {
      webrtc::VideoCodec video_codec;
      if(mPtrViECodec->GetCodec(idx, video_codec) == 0) {
        payloadName = video_codec.plName;
        if(video_codec.codecType == webrtc::VideoCodecType::kVideoCodecRED ||
           video_codec.codecType == webrtc::VideoCodecType::kVideoCodecULPFEC) {
          if(mPtrViECodec->SetReceiveCodec(mChannel,video_codec) == -1) {
            CSFLogError(logTag, "%s Invalid Receive Codec %d ", __FUNCTION__,
                        mPtrViEBase->LastError());
          } else {
            CSFLogDebug(logTag, "%s Successfully Set the codec %s", __FUNCTION__,
                        video_codec.plName);
          }
        }
      }
    }

    if (use_nack_basic) {
      CSFLogDebug(logTag, "Enabling NACK/FEC (recv) for video stream\n");
      if (mPtrRTP->SetHybridNACKFECStatus(mChannel, true,
                                          payload_type_red,
                                          payload_type_ulpfec) != 0) {
        CSFLogError(logTag,  "%s SetHybridNACKFECStatus Failed %d ",
                    __FUNCTION__, mPtrViEBase->LastError());
        return kMediaConduitNACKStatusError;
      }
    } else {
      CSFLogDebug(logTag, "Enabling FEC (recv) for video stream\n");
      if (mPtrRTP->SetFECStatus(mChannel, true,
                                payload_type_red, payload_type_ulpfec) != 0)
      {
        CSFLogError(logTag,  "%s SetFECStatus Failed %d ", __FUNCTION__,
                    mPtrViEBase->LastError());
        return kMediaConduitNACKStatusError;
      }
    }
  } else if(use_nack_basic) {
    CSFLogDebug(logTag, "Enabling NACK (recv) for video stream\n");
    if (mPtrRTP->SetNACKStatus(mChannel, true) != 0)
    {
      CSFLogError(logTag,  "%s NACKStatus Failed %d ", __FUNCTION__,
                  mPtrViEBase->LastError());
      return kMediaConduitNACKStatusError;
    }
  }
  mUsingNackBasic = use_nack_basic;
  mUsingFEC = use_fec;

  if (use_tmmbr) {
    CSFLogDebug(logTag, "Enabling TMMBR for video stream");
    if (mPtrRTP->SetTMMBRStatus(mChannel, true) != 0) {
      CSFLogError(logTag, "%s SetTMMBRStatus Failed %d ", __FUNCTION__,
        mPtrViEBase->LastError());
      return kMediaConduitTMMBRStatusError;
    }
  }
  mUsingTmmbr = use_tmmbr;

  condError = StartReceiving();
  if (condError != kMediaConduitNoError) {
    return condError;
  }

  // by now we should be successfully started the reception
  CSFLogDebug(logTag, "REMB enabled for video stream %s",
              (use_remb ? "yes" : "no"));
  mPtrRTP->SetRembStatus(mChannel, use_remb, true);
  return kMediaConduitNoError;
}

struct ResolutionAndBitrateLimits {
  uint32_t resolution_in_mb;
  uint16_t min_bitrate;
  uint16_t start_bitrate;
  uint16_t max_bitrate;
};

#define MB_OF(w,h) ((unsigned int)((((w+15)>>4))*((unsigned int)((h+15)>>4))))

// For now, try to set the max rates well above the knee in the curve.
// Chosen somewhat arbitrarily; it's hard to find good data oriented for
// realtime interactive/talking-head recording.  These rates assume
// 30fps.

// XXX Populate this based on a pref (which we should consider sorting because
// people won't assume they need to).
static ResolutionAndBitrateLimits kResolutionAndBitrateLimits[] = {
  {MB_OF(1920, 1200), 1500, 2000, 10000}, // >HD (3K, 4K, etc)
  {MB_OF(1280, 720), 1200, 1500, 5000}, // HD ~1080-1200
  {MB_OF(800, 480), 600, 800, 2500}, // HD ~720
  {tl::Max<MB_OF(400, 240), MB_OF(352, 288)>::value, 200, 300, 1300}, // VGA, WVGA
  {MB_OF(176, 144), 100, 150, 500}, // WQVGA, CIF
  {0 , 40, 80, 250} // QCIF and below
};

void
WebrtcVideoConduit::SelectBitrates(unsigned short width,
                                   unsigned short height,
                                   unsigned int cap,
                                   mozilla::Atomic<int32_t, mozilla::Relaxed>& aLastFramerateTenths,
                                   unsigned int& out_min,
                                   unsigned int& out_start,
                                   unsigned int& out_max)
{
  // max bandwidth should be proportional (not linearly!) to resolution, and
  // proportional (perhaps linearly, or close) to current frame rate.
  unsigned int fs = MB_OF(width, height);

  for (ResolutionAndBitrateLimits resAndLimits : kResolutionAndBitrateLimits) {
    if (fs > resAndLimits.resolution_in_mb &&
        // pick the highest range where at least start rate is within cap
        // (or if we're at the end of the array).
        (!cap || resAndLimits.start_bitrate <= cap ||
         resAndLimits.resolution_in_mb == 0)) {
      out_min = MinIgnoreZero((unsigned int)resAndLimits.min_bitrate, cap);
      out_start = MinIgnoreZero((unsigned int)resAndLimits.start_bitrate, cap);
      out_max = MinIgnoreZero((unsigned int)resAndLimits.max_bitrate, cap);
      break;
    }
  }

  // mLastFramerateTenths is an atomic, and scaled by *10
  double framerate = std::min((aLastFramerateTenths/10.),60.0);
  MOZ_ASSERT(framerate > 0);
  // Now linear reduction/increase based on fps (max 60fps i.e. doubling)
  if (framerate >= 10) {
    out_min = out_min * (framerate/30);
    out_start = out_start * (framerate/30);
    out_max = std::max((unsigned int)(out_max * (framerate/30)), cap);
  } else {
    // At low framerates, don't reduce bandwidth as much - cut slope to 1/2.
    // Mostly this would be ultra-low-light situations/mobile or screensharing.
    out_min = out_min * ((10-(framerate/2))/30);
    out_start = out_start * ((10-(framerate/2))/30);
    out_max = std::max((unsigned int)(out_max * ((10-(framerate/2))/30)), cap);
  }

  if (mMinBitrate && mMinBitrate > out_min) {
    out_min = mMinBitrate;
  }
  // If we try to set a minimum bitrate that is too low, ViE will reject it.
  out_min = std::max((unsigned int) webrtc::kViEMinCodecBitrate,
                                  out_min);
  if (mStartBitrate && mStartBitrate > out_start) {
    out_start = mStartBitrate;
  }
  out_start = std::max(out_start, out_min);

  // Note: mNegotiatedMaxBitrate is the max transport bitrate - it applies to
  // a single codec encoding, but should also apply to the sum of all
  // simulcast layers in this encoding!  So sum(layers.maxBitrate) <=
  // mNegotiatedMaxBitrate
  // Note that out_max already has had mPrefMaxBitrate applied to it
  out_max = MinIgnoreZero(mNegotiatedMaxBitrate, out_max);

  MOZ_ASSERT(mPrefMaxBitrate == 0 || out_max <= mPrefMaxBitrate);
}

static void ConstrainPreservingAspectRatioExact(uint32_t max_fs,
                                                unsigned short* width,
                                                unsigned short* height)
{
  // We could try to pick a better starting divisor, but it won't make any real
  // performance difference.
  for (size_t d = 1; d < std::min(*width, *height); ++d) {
    if ((*width % d) || (*height % d)) {
      continue; // Not divisible
    }

    if (((*width) * (*height))/(d*d) <= max_fs) {
      *width /= d;
      *height /= d;
      return;
    }
  }

  *width = 0;
  *height = 0;
}

static void ConstrainPreservingAspectRatio(uint16_t max_width,
                                           uint16_t max_height,
                                           unsigned short* width,
                                           unsigned short* height)
{
  if (((*width) <= max_width) && ((*height) <= max_height)) {
    return;
  }

  if ((*width) * max_height > max_width * (*height))
  {
    (*height) = max_width * (*height) / (*width);
    (*width) = max_width;
  }
  else
  {
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
                                         webrtc::I420VideoFrame *frame) // may be null
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
    if (mCurSendCodecConfig->mEncodingConstraints.maxFs)
    {
      uint32_t max_fs = mCurSendCodecConfig->mEncodingConstraints.maxFs;
      unsigned int cur_fs, mb_width, mb_height, mb_max;

      // Could we make this simpler by picking the larger of width and height,
      // calculating a max for just that value based on the scale parameter,
      // and then let ConstrainPreservingAspectRatio do the rest?
      mb_width = (width + 15) >> 4;
      mb_height = (height + 15) >> 4;

      cur_fs = mb_width * mb_height;

      // Limit resolution to max_fs, but don't scale up.
      if (cur_fs > max_fs)
      {
        double scale_ratio;

        scale_ratio = sqrt((double) max_fs / (double) cur_fs);

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
      mb_max = (unsigned) sqrt(8 * (double) max_fs);

      max_width = 16 * std::min(mb_width, mb_max);
      max_height = 16 * std::min(mb_height, mb_max);
      ConstrainPreservingAspectRatio(max_width, max_height, &width, &height);
    }
  }


  // Adapt to getUserMedia resolution changes
  // check if we need to reconfigure the sending resolution.
  bool changed = false;
  if (mSendingWidth != width || mSendingHeight != height)
  {
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
      webrtc::I420VideoFrame *new_frame = nullptr;
      if (frame) {
        new_frame = new webrtc::I420VideoFrame();
        // the internal buffer pointer is refcounted, so we don't have 2 copies here
        new_frame->ShallowCopy(*frame);
      }
      RefPtr<WebrtcVideoConduit> self(this);
      RefPtr<Runnable> webrtc_runnable =
        media::NewRunnableFrom([self, width, height, new_frame]() -> nsresult {
            UniquePtr<webrtc::I420VideoFrame> local_frame(new_frame); // Simplify cleanup

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
                                         webrtc::I420VideoFrame *frame)
{
  mCodecMutex.AssertCurrentThreadOwns();

  // Get current vie codec.
  webrtc::VideoCodec vie_codec;
  int32_t err;

  mInReconfig = false;
  if ((err = mPtrViECodec->GetSendCodec(mChannel, vie_codec)) != 0)
  {
    CSFLogError(logTag, "%s: GetSendCodec failed, err %d", __FUNCTION__, err);
    return NS_ERROR_FAILURE;
  }

  CSFLogDebug(logTag,
              "%s: Requesting resolution change to %ux%u (from %ux%u)",
              __FUNCTION__, width, height, vie_codec.width, vie_codec.height);

  if (mRtpStreamIdEnabled) {
    vie_codec.ridId = mRtpStreamIdExtId;
  }

  vie_codec.width = width;
  vie_codec.height = height;
  vie_codec.maxFramerate = mSendingFramerate;
  SelectBitrates(vie_codec.width, vie_codec.height, 0,
                 mLastFramerateTenths,
                 vie_codec.minBitrate,
                 vie_codec.startBitrate,
                 vie_codec.maxBitrate);

  // These are based on lowest-fidelity, because if there is insufficient
  // bandwidth for all streams, only the lowest fidelity one will be sent.
  uint32_t minMinBitrate = 0;
  uint32_t minStartBitrate = 0;
  // Total for all simulcast streams.
  uint32_t totalMaxBitrate = 0;

  for (size_t i = vie_codec.numberOfSimulcastStreams; i > 0; --i) {
    webrtc::SimulcastStream& stream(vie_codec.simulcastStream[i - 1]);
    stream.width = width;
    stream.height = height;
    MOZ_ASSERT(stream.jsScaleDownBy >= 1.0);
    uint32_t new_width = uint32_t(width / stream.jsScaleDownBy);
    uint32_t new_height = uint32_t(height / stream.jsScaleDownBy);
    // TODO: If two layers are similar, only alloc bits to one (Bug 1249859)
    if (new_width != width || new_height != height) {
      if (vie_codec.numberOfSimulcastStreams == 1) {
        // Use less strict scaling in unicast. That way 320x240 / 3 = 106x79.
        ConstrainPreservingAspectRatio(new_width, new_height,
                                       &stream.width, &stream.height);
      } else {
        // webrtc.org supposedly won't tolerate simulcast unless every stream
        // is exactly the same aspect ratio. 320x240 / 3 = 80x60.
        ConstrainPreservingAspectRatioExact(new_width*new_height,
                                            &stream.width, &stream.height);
      }
    }
    // Give each layer default appropriate bandwidth limits based on the
    // resolution/framerate of that layer
    SelectBitrates(stream.width, stream.height,
                   MinIgnoreZero(stream.jsMaxBitrate, vie_codec.maxBitrate),
                   mLastFramerateTenths,
                   stream.minBitrate,
                   stream.targetBitrate,
                   stream.maxBitrate);

    // webrtc.org expects the last, highest fidelity, simulcast stream to
    // always have the same resolution as vie_codec
    // Also set the least user-constrained of the stream bitrates on vie_codec.
    if (i == vie_codec.numberOfSimulcastStreams) {
      vie_codec.width = stream.width;
      vie_codec.height = stream.height;
    }
    minMinBitrate = MinIgnoreZero(stream.minBitrate, minMinBitrate);
    minStartBitrate = MinIgnoreZero(stream.targetBitrate, minStartBitrate);
    totalMaxBitrate += stream.maxBitrate;
  }
  if (vie_codec.numberOfSimulcastStreams != 0) {
    vie_codec.minBitrate = std::max(minMinBitrate, vie_codec.minBitrate);
    vie_codec.maxBitrate = std::min(totalMaxBitrate, vie_codec.maxBitrate);
    vie_codec.startBitrate = std::max(vie_codec.minBitrate,
                                      std::min(minStartBitrate,
                                               vie_codec.maxBitrate));
  }
  vie_codec.mode = mCodecMode;
  if ((err = mPtrViECodec->SetSendCodec(mChannel, vie_codec)) != 0)
  {
    CSFLogError(logTag, "%s: SetSendCodec(%ux%u) failed, err %d",
                __FUNCTION__, width, height, err);
    return NS_ERROR_FAILURE;
  }
  if (mMinBitrateEstimate != 0) {
    mPtrViENetwork->SetBitrateConfig(mChannel,
                                     mMinBitrateEstimate,
                                     std::max(vie_codec.startBitrate,
                                              mMinBitrateEstimate),
                                     std::max(vie_codec.maxBitrate,
                                              mMinBitrateEstimate));
  }

  CSFLogDebug(logTag, "%s: Encoder resolution changed to %ux%u @ %ufps, bitrate %u:%u",
              __FUNCTION__, width, height, mSendingFramerate,
              vie_codec.minBitrate, vie_codec.maxBitrate);
  if (frame) {
    // XXX I really don't like doing this from MainThread...
    mPtrExtCapture->IncomingFrame(*frame);
    mVideoCodecStat->SentFrame();
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
      max_fps = mCurSendCodecConfig->mEncodingConstraints.maxMbps/cur_fs;
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
WebrtcVideoConduit::SetExternalSendCodec(VideoCodecConfig* config,
                                         VideoEncoder* encoder) {
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  if (!mPtrExtCodec->RegisterExternalSendCodec(mChannel,
                                              config->mType,
                                              static_cast<WebrtcVideoEncoder*>(encoder),
                                              false)) {
    mExternalSendCodecHandle = encoder;
    mExternalSendCodec = new VideoCodecConfig(*config);
    return kMediaConduitNoError;
  }
  return kMediaConduitInvalidSendCodec;
}

MediaConduitErrorCode
WebrtcVideoConduit::SetExternalRecvCodec(VideoCodecConfig* config,
                                         VideoDecoder* decoder) {
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  if (!mPtrExtCodec->RegisterExternalReceiveCodec(mChannel,
                                                  config->mType,
                                                  static_cast<WebrtcVideoDecoder*>(decoder))) {
    mExternalRecvCodecHandle = decoder;
    mExternalRecvCodec = new VideoCodecConfig(*config);
    return kMediaConduitNoError;
  }
  return kMediaConduitInvalidReceiveCodec;
}

MediaConduitErrorCode
WebrtcVideoConduit::EnableRTPStreamIdExtension(bool enabled, uint8_t id) {
  mRtpStreamIdEnabled = enabled;
  mRtpStreamIdExtId = id;
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::SendVideoFrame(unsigned char* video_frame,
                                   unsigned int video_frame_length,
                                   unsigned short width,
                                   unsigned short height,
                                   VideoType video_type,
                                   uint64_t capture_time)
{

  //check for  the parameters sanity
  if(!video_frame || video_frame_length == 0 ||
     width == 0 || height == 0)
  {
    CSFLogError(logTag,  "%s Invalid Parameters ",__FUNCTION__);
    MOZ_ASSERT(false);
    return kMediaConduitMalformedArgument;
  }
  MOZ_ASSERT(video_type == VideoType::kVideoI420);
  MOZ_ASSERT(mPtrExtCapture);

  // Transmission should be enabled before we insert any frames.
  if(!mEngineTransmitting)
  {
    CSFLogError(logTag, "%s Engine not transmitting ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  // insert the frame to video engine in I420 format only
  webrtc::I420VideoFrame i420_frame;
  i420_frame.CreateFrame(video_frame, width, height, webrtc::kVideoRotation_0);
  i420_frame.set_timestamp(capture_time);
  i420_frame.set_render_time_ms(capture_time);

  return SendVideoFrame(i420_frame);
}

MediaConduitErrorCode
WebrtcVideoConduit::SendVideoFrame(webrtc::I420VideoFrame& frame)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  // See if we need to recalculate what we're sending.
  // Don't compare mSendingWidth/Height, since those may not be the same as the input.
  {
    MutexAutoLock lock(mCodecMutex);
    if (mInReconfig) {
      // Waiting for it to finish
      return kMediaConduitNoError;
    }
    if (frame.width() != mLastWidth || frame.height() != mLastHeight) {
      CSFLogDebug(logTag, "%s: call SelectSendResolution with %ux%u",
                  __FUNCTION__, frame.width(), frame.height());
      if (SelectSendResolution(frame.width(), frame.height(), &frame)) {
        // SelectSendResolution took ownership of the data in i420_frame.
        // Submit the frame after reconfig is done
        return kMediaConduitNoError;
      }
    }
  }
  mPtrExtCapture->IncomingFrame(frame);

  mVideoCodecStat->SentFrame();
  CSFLogDebug(logTag, "%s Inserted a frame", __FUNCTION__);
  return kMediaConduitNoError;
}

// Transport Layer Callbacks
MediaConduitErrorCode
WebrtcVideoConduit::ReceivedRTPPacket(const void *data, int len)
{
  CSFLogDebug(logTag, "%s: seq# %u, Channel %d, Len %d ", __FUNCTION__,
              (uint16_t) ntohs(((uint16_t*) data)[1]), mChannel, len);

  // Media Engine should be receiving already.
  if(mEngineReceiving)
  {
    // let the engine know of a RTP packet to decode
    // XXX we need to get passed the time the packet was received
    if(mPtrViENetwork->ReceivedRTPPacket(mChannel, data, len, webrtc::PacketTime()) == -1)
    {
      int error = mPtrViEBase->LastError();
      CSFLogError(logTag, "%s RTP Processing Failed %d ", __FUNCTION__, error);
      if(error >= kViERtpRtcpInvalidChannelId && error <= kViERtpRtcpRtcpDisabled)
      {
        return kMediaConduitRTPProcessingFailed;
      }
      return kMediaConduitRTPRTCPModuleError;
    }
  } else {
    CSFLogError(logTag, "Error: %s when not receiving", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::ReceivedRTCPPacket(const void *data, int len)
{
  CSFLogDebug(logTag, " %s Channel %d, Len %d ", __FUNCTION__, mChannel, len);

  //Media Engine should be receiving already
  if(mPtrViENetwork->ReceivedRTCPPacket(mChannel,data,len) == -1)
  {
    int error = mPtrViEBase->LastError();
    CSFLogError(logTag, "%s RTCP Processing Failed %d", __FUNCTION__, error);
    if(error >= kViERtpRtcpInvalidChannelId && error <= kViERtpRtcpRtcpDisabled)
    {
      return kMediaConduitRTPProcessingFailed;
    }
    return kMediaConduitRTPRTCPModuleError;
  }
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::StopTransmitting()
{
  if(mEngineTransmitting)
  {
    CSFLogDebug(logTag, "%s Engine Already Sending. Attemping to Stop ", __FUNCTION__);
    if(mPtrViEBase->StopSend(mChannel) == -1)
    {
      CSFLogError(logTag, "%s StopSend() Failed %d ",__FUNCTION__,
                  mPtrViEBase->LastError());
      return kMediaConduitUnknownError;
    }

    mEngineTransmitting = false;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::StartTransmitting()
{
  if (!mEngineTransmitting) {
    if(mPtrViEBase->StartSend(mChannel) == -1)
    {
      CSFLogError(logTag, "%s Start Send Error %d ", __FUNCTION__,
                  mPtrViEBase->LastError());
      return kMediaConduitUnknownError;
    }

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
  if(mEngineReceiving)
  {
    CSFLogDebug(logTag, "%s Engine Already Receiving . Attemping to Stop ", __FUNCTION__);
    if(mPtrViEBase->StopReceive(mChannel) == -1)
    {
      int error = mPtrViEBase->LastError();
      if(error == kViEBaseUnknownError)
      {
        CSFLogDebug(logTag, "%s StopReceive() Success ", __FUNCTION__);
      } else {
        CSFLogError(logTag, "%s StopReceive() Failed %d ", __FUNCTION__,
                    mPtrViEBase->LastError());
        return kMediaConduitUnknownError;
      }
    }
    mEngineReceiving = false;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::StartReceiving()
{
  if (!mEngineReceiving) {
    CSFLogDebug(logTag, "%s Attemping to start... ", __FUNCTION__);
    //Start Receive on the video engine
    if(mPtrViEBase->StartReceive(mChannel) == -1)
    {
      int error = mPtrViEBase->LastError();
      CSFLogError(logTag, "%s Start Receive Error %d ", __FUNCTION__, error);

      return kMediaConduitUnknownError;
    }

    mEngineReceiving = true;
  }

  return kMediaConduitNoError;
}

//WebRTC::RTP Callback Implementation
// Called on MSG thread
int WebrtcVideoConduit::SendPacket(int channel, const void* data, size_t len)
{
  CSFLogDebug(logTag,  "%s : channel %d len %lu", __FUNCTION__, channel, (unsigned long) len);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if(mTransmitterTransport &&
     (mTransmitterTransport->SendRtpPacket(data, len) == NS_OK))
  {
    CSFLogDebug(logTag, "%s Sent RTP Packet ", __FUNCTION__);
    return len;
  } else {
    CSFLogError(logTag, "%s RTP Packet Send Failed ", __FUNCTION__);
    return -1;
  }
}

// Called from multiple threads including webrtc Process thread
int WebrtcVideoConduit::SendRTCPPacket(int channel, const void* data, size_t len)
{
  CSFLogDebug(logTag,  "%s : channel %d , len %lu ", __FUNCTION__, channel, (unsigned long) len);

  // We come here if we have only one pipeline/conduit setup,
  // such as for unidirectional streams.
  // We also end up here if we are receiving
  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if(mReceiverTransport &&
     mReceiverTransport->SendRtcpPacket(data, len) == NS_OK)
  {
    // Might be a sender report, might be a receiver report, we don't know.
    CSFLogDebug(logTag, "%s Sent RTCP Packet ", __FUNCTION__);
    return len;
  } else if(mTransmitterTransport &&
            (mTransmitterTransport->SendRtcpPacket(data, len) == NS_OK)) {
      CSFLogDebug(logTag, "%s Sent RTCP Packet (sender report) ", __FUNCTION__);
      return len;
  } else {
    CSFLogError(logTag, "%s RTCP Packet Send Failed ", __FUNCTION__);
    return -1;
  }
}

// WebRTC::ExternalMedia Implementation
int
WebrtcVideoConduit::FrameSizeChange(unsigned int width,
                                    unsigned int height,
                                    unsigned int numStreams)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);


  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  mReceivingWidth = width;
  mReceivingHeight = height;
  mNumReceivingStreams = numStreams;

  if(mRenderer)
  {
    mRenderer->FrameSizeChange(width, height, numStreams);
    return 0;
  }

  CSFLogError(logTag,  "%s Renderer is NULL ", __FUNCTION__);
  return -1;
}

int
WebrtcVideoConduit::DeliverFrame(unsigned char* buffer,
                                 size_t buffer_size,
                                 uint32_t time_stamp,
                                 int64_t ntp_time_ms,
                                 int64_t render_time,
                                 void *handle)
{
  return DeliverFrame(buffer, buffer_size, mReceivingWidth, (mReceivingWidth+1)>>1,
                      time_stamp, ntp_time_ms, render_time, handle);
}

int
WebrtcVideoConduit::DeliverFrame(unsigned char* buffer,
                                 size_t buffer_size,
                                 uint32_t y_stride,
                                 uint32_t cbcr_stride,
                                 uint32_t time_stamp,
                                 int64_t ntp_time_ms,
                                 int64_t render_time,
                                 void *handle)
{
  CSFLogDebug(logTag,  "%s Buffer Size %lu", __FUNCTION__, (unsigned long) buffer_size);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if(mRenderer)
  {
    layers::Image* img = nullptr;
    // |handle| should be a webrtc::NativeHandle if available.
    if (handle) {
      webrtc::NativeHandle* native_h = static_cast<webrtc::NativeHandle*>(handle);
      // In the handle, there should be a layers::Image.
      img = static_cast<layers::Image*>(native_h->GetHandle());
    }

    if (mVideoLatencyTestEnable && mReceivingWidth && mReceivingHeight) {
      uint64_t now = PR_Now();
      uint64_t timestamp = 0;
      bool ok = YuvStamper::Decode(mReceivingWidth, mReceivingHeight, mReceivingWidth,
                                   buffer,
                                   reinterpret_cast<unsigned char*>(&timestamp),
                                   sizeof(timestamp), 0, 0);
      if (ok) {
        VideoLatencyUpdate(now - timestamp);
      }
    }

    const ImageHandle img_h(img);
    mRenderer->RenderVideoFrame(buffer, buffer_size, y_stride, cbcr_stride,
                                time_stamp, render_time, img_h);
    return 0;
  }

  CSFLogError(logTag,  "%s Renderer is NULL  ", __FUNCTION__);
  return -1;
}

int
WebrtcVideoConduit::DeliverI420Frame(const webrtc::I420VideoFrame& webrtc_frame)
{
  if (!webrtc_frame.native_handle()) {
    uint32_t y_stride = webrtc_frame.stride(static_cast<webrtc::PlaneType>(0));
    return DeliverFrame(const_cast<uint8_t*>(webrtc_frame.buffer(webrtc::kYPlane)),
                        CalcBufferSize(webrtc::kI420, y_stride, webrtc_frame.height()),
                        y_stride,
                        webrtc_frame.stride(static_cast<webrtc::PlaneType>(1)),
                        webrtc_frame.timestamp(),
                        webrtc_frame.ntp_time_ms(),
                        webrtc_frame.render_time_ms(), nullptr);
  }
  size_t buffer_size = CalcBufferSize(webrtc::kI420, webrtc_frame.width(), webrtc_frame.height());
  CSFLogDebug(logTag,  "%s Buffer Size %lu", __FUNCTION__, (unsigned long) buffer_size);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if(mRenderer)
  {
    layers::Image* img = nullptr;
    // |handle| should be a webrtc::NativeHandle if available.
    webrtc::NativeHandle* native_h = static_cast<webrtc::NativeHandle*>(webrtc_frame.native_handle());
    if (native_h) {
      // In the handle, there should be a layers::Image.
      img = static_cast<layers::Image*>(native_h->GetHandle());
    }

#if 0
    //#ifndef MOZ_WEBRTC_OMX
    // XXX - this may not be possible on GONK with textures!
    if (mVideoLatencyTestEnable && mReceivingWidth && mReceivingHeight) {
      uint64_t now = PR_Now();
      uint64_t timestamp = 0;
      bool ok = YuvStamper::Decode(mReceivingWidth, mReceivingHeight, mReceivingWidth,
                                   buffer,
                                   reinterpret_cast<unsigned char*>(&timestamp),
                                   sizeof(timestamp), 0, 0);
      if (ok) {
        VideoLatencyUpdate(now - timestamp);
      }
    }
#endif

    const ImageHandle img_h(img);
    mRenderer->RenderVideoFrame(nullptr, buffer_size, webrtc_frame.timestamp(),
                                webrtc_frame.render_time_ms(), img_h);
    return 0;
  }

  CSFLogError(logTag,  "%s Renderer is NULL  ", __FUNCTION__);
  return -1;
}

/**
 * Copy the codec passed into Conduit's database
 */

void
WebrtcVideoConduit::CodecConfigToWebRTCCodec(const VideoCodecConfig* codecInfo,
                                              webrtc::VideoCodec& cinst)
{
  // Note: this assumes cinst is initialized to a base state either by
  // hand or from a config fetched with GetConfig(); this modifies the config
  // to match parameters from VideoCodecConfig
  cinst.plType  = codecInfo->mType;
  if (codecInfo->mName == "H264") {
    cinst.codecType = webrtc::kVideoCodecH264;
    PL_strncpyz(cinst.plName, "H264", sizeof(cinst.plName));
  } else if (codecInfo->mName == "VP8") {
    cinst.codecType = webrtc::kVideoCodecVP8;
    PL_strncpyz(cinst.plName, "VP8", sizeof(cinst.plName));
  } else if (codecInfo->mName == "VP9") {
    cinst.codecType = webrtc::kVideoCodecVP9;
    PL_strncpyz(cinst.plName, "VP9", sizeof(cinst.plName));
  } else if (codecInfo->mName == "I420") {
    cinst.codecType = webrtc::kVideoCodecI420;
    PL_strncpyz(cinst.plName, "I420", sizeof(cinst.plName));
  } else {
    cinst.codecType = webrtc::kVideoCodecUnknown;
    PL_strncpyz(cinst.plName, "Unknown", sizeof(cinst.plName));
  }

  // width/height will be overridden on the first frame; they must be 'sane' for
  // SetSendCodec()
  if (codecInfo->mEncodingConstraints.maxFps > 0) {
    cinst.maxFramerate = codecInfo->mEncodingConstraints.maxFps;
  } else {
    cinst.maxFramerate = DEFAULT_VIDEO_MAX_FRAMERATE;
  }

  // Defaults if rates aren't forced by pref.  Typically defaults are
  // overridden on the first video frame.
  cinst.minBitrate = mMinBitrate ? mMinBitrate : 200;
  cinst.startBitrate = mStartBitrate ? mStartBitrate : 300;
  cinst.targetBitrate = cinst.startBitrate;
  cinst.maxBitrate = MinIgnoreZero(2000U, codecInfo->mEncodingConstraints.maxBr/1000);
  // not mNegotiatedMaxBitrate! cinst.maxBitrate is the max for the codec, which will be overridden
  cinst.maxBitrate = MinIgnoreZero(cinst.maxBitrate, mPrefMaxBitrate);

  if (cinst.codecType == webrtc::kVideoCodecH264)
  {
#ifdef MOZ_WEBRTC_OMX
    cinst.resolution_divisor = 16;
#endif
    // cinst.codecSpecific.H264.profile = ?
    cinst.codecSpecific.H264.profile_byte = codecInfo->mProfile;
    cinst.codecSpecific.H264.constraints = codecInfo->mConstraints;
    cinst.codecSpecific.H264.level = codecInfo->mLevel;
    cinst.codecSpecific.H264.packetizationMode = codecInfo->mPacketizationMode;
    if (codecInfo->mEncodingConstraints.maxMbps > 0) {
      // Not supported yet!
      CSFLogError(logTag,  "%s H.264 max_mbps not supported yet  ", __FUNCTION__);
    }
    // XXX parse the encoded SPS/PPS data
    // paranoia
    cinst.codecSpecific.H264.spsData = nullptr;
    cinst.codecSpecific.H264.spsLen = 0;
    cinst.codecSpecific.H264.ppsData = nullptr;
    cinst.codecSpecific.H264.ppsLen = 0;
  }
  // Init mSimulcastEncodings always since they hold info from setParameters.
  // TODO(bug 1210175): H264 doesn't support simulcast yet.
  size_t numberOfSimulcastEncodings = std::min(codecInfo->mSimulcastEncodings.size(), (size_t)webrtc::kMaxSimulcastStreams);
  for (size_t i = 0; i < numberOfSimulcastEncodings; ++i) {
    const VideoCodecConfig::SimulcastEncoding& encoding =
      codecInfo->mSimulcastEncodings[i];
    // Make sure the constraints on the whole stream are reflected.
    webrtc::SimulcastStream stream;
    memset(&stream, 0, sizeof(stream));
    stream.width = cinst.width;
    stream.height = cinst.height;
    stream.numberOfTemporalLayers = 1;
    stream.maxBitrate = cinst.maxBitrate;
    stream.targetBitrate = cinst.targetBitrate;
    stream.minBitrate = cinst.minBitrate;
    stream.qpMax = cinst.qpMax;
    strncpy(stream.rid, encoding.rid.c_str(), sizeof(stream.rid)-1);
    stream.rid[sizeof(stream.rid) - 1] = 0;

    // Apply encoding-specific constraints.
    stream.width = MinIgnoreZero(
        stream.width,
        (unsigned short)encoding.constraints.maxWidth);
    stream.height = MinIgnoreZero(
        stream.height,
        (unsigned short)encoding.constraints.maxHeight);

    // webrtc.org uses kbps, we use bps
    stream.jsMaxBitrate = encoding.constraints.maxBr/1000;
    stream.jsScaleDownBy = encoding.constraints.scaleDownBy;

    MOZ_ASSERT(stream.jsScaleDownBy >= 1.0);
    uint32_t width = stream.width? stream.width : 640;
    uint32_t height = stream.height? stream.height : 480;
    uint32_t new_width = uint32_t(width / stream.jsScaleDownBy);
    uint32_t new_height = uint32_t(height / stream.jsScaleDownBy);

    if (new_width != width || new_height != height) {
      // Estimate. Overridden on first frame.
      SelectBitrates(new_width, new_height, stream.jsMaxBitrate,
                     mLastFramerateTenths,
                     stream.minBitrate,
                     stream.targetBitrate,
                     stream.maxBitrate);
    }
    // webrtc.org expects simulcast streams to be ordered by increasing
    // fidelity, our jsep code does the opposite.
    cinst.simulcastStream[numberOfSimulcastEncodings-i-1] = stream;
  }

  cinst.numberOfSimulcastStreams = numberOfSimulcastEncodings;
}

/**
 * Perform validation on the codecConfig to be applied
 * Verifies if the codec is already applied.
 */
MediaConduitErrorCode
WebrtcVideoConduit::ValidateCodecConfig(const VideoCodecConfig* codecInfo,
                                        bool send)
{
  if(!codecInfo)
  {
    CSFLogError(logTag, "%s Null CodecConfig ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  if((codecInfo->mName.empty()) ||
     (codecInfo->mName.length() >= CODEC_PLNAME_SIZE))
  {
    CSFLogError(logTag, "%s Invalid Payload Name Length ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  return kMediaConduitNoError;
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
  if (mExternalSendCodecHandle) {
    return mExternalSendCodecHandle->PluginID();
  } else if (mExternalRecvCodecHandle) {
    return mExternalRecvCodecHandle->PluginID();
  }
  return 0;
}

bool
WebrtcVideoConduit::DetermineREDAndULPFECPayloadTypes(uint8_t &payload_type_red, uint8_t &payload_type_ulpfec)
{
    webrtc::VideoCodec video_codec;
    payload_type_red = INVALID_RTP_PAYLOAD;
    payload_type_ulpfec = INVALID_RTP_PAYLOAD;

    for(int idx=0; idx < mPtrViECodec->NumberOfCodecs(); idx++)
    {
      if(mPtrViECodec->GetCodec(idx, video_codec) == 0)
      {
        switch(video_codec.codecType) {
          case webrtc::VideoCodecType::kVideoCodecRED:
            payload_type_red = video_codec.plType;
            break;
          case webrtc::VideoCodecType::kVideoCodecULPFEC:
            payload_type_ulpfec = video_codec.plType;
            break;
          default:
            break;
        }
      }
    }

    return payload_type_red != INVALID_RTP_PAYLOAD
           && payload_type_ulpfec != INVALID_RTP_PAYLOAD;
}

}// end namespace
