/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"
#include "nspr.h"

// For rtcp-fb constants
#include "ccsdp.h"

#include "VideoConduit.h"
#include "AudioConduit.h"
#include "nsThreadUtils.h"
#include "LoadManager.h"
#include "YuvStamper.h"
#include "nsServiceManagerUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "webrtc/common_video/interface/native_handle.h"
#include "webrtc/video_engine/include/vie_errors.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidJNIWrapper.h"
#endif

#include <algorithm>
#include <math.h>

namespace mozilla {

static const char* logTag ="WebrtcVideoSessionConduit";

// 32 bytes is what WebRTC CodecInst expects
const unsigned int WebrtcVideoConduit::CODEC_PLNAME_SIZE = 32;

/**
 * Factory Method for VideoConduit
 */
mozilla::RefPtr<VideoSessionConduit> VideoSessionConduit::Create(VideoSessionConduit *aOther)
{
#ifdef MOZILLA_INTERNAL_API
  // unit tests create their own "main thread"
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
#endif
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  WebrtcVideoConduit* obj = new WebrtcVideoConduit();
  if(obj->Init(static_cast<WebrtcVideoConduit*>(aOther)) != kMediaConduitNoError)
  {
    CSFLogError(logTag,  "%s VideoConduit Init Failed ", __FUNCTION__);
    delete obj;
    return nullptr;
  }
  CSFLogDebug(logTag,  "%s Successfully created VideoConduit ", __FUNCTION__);
  return obj;
}

WebrtcVideoConduit::~WebrtcVideoConduit()
{
#ifdef MOZILLA_INTERNAL_API
  // unit tests create their own "main thread"
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
#endif
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  for(std::vector<VideoCodecConfig*>::size_type i=0;i < mRecvCodecList.size();i++)
  {
    delete mRecvCodecList[i];
  }

  delete mCurSendCodecConfig;

  // The first one of a pair to be deleted shuts down media for both
  //Deal with External Capturer
  if(mPtrViECapture)
  {
    if (!mShutDown) {
      mPtrViECapture->DisconnectCaptureDevice(mCapId);
      mPtrViECapture->ReleaseCaptureDevice(mCapId);
      mPtrExtCapture = nullptr;
      if (mOtherDirection)
        mOtherDirection->mPtrExtCapture = nullptr;
    }
  }

  //Deal with External Renderer
  if(mPtrViERender)
  {
    if (!mShutDown) {
      if(mRenderer) {
        mPtrViERender->StopRender(mChannel);
      }
      mPtrViERender->RemoveRenderer(mChannel);
    }
  }

  //Deal with the transport
  if(mPtrViENetwork)
  {
    if (!mShutDown) {
      mPtrViENetwork->DeregisterSendTransport(mChannel);
    }
  }

  if(mPtrViEBase)
  {
    if (!mShutDown) {
      mPtrViEBase->StopSend(mChannel);
      mPtrViEBase->StopReceive(mChannel);
      SyncTo(nullptr);
      mPtrViEBase->DeleteChannel(mChannel);
    }
  }

  if (mOtherDirection)
  {
    // mOtherDirection owns these now!
    mOtherDirection->mOtherDirection = nullptr;
    // let other side we terminated the channel
    mOtherDirection->mShutDown = true;
    mVideoEngine = nullptr;
  } else {
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
}

bool WebrtcVideoConduit::GetLocalSSRC(unsigned int* ssrc) {
  return !mPtrRTP->GetLocalSSRC(mChannel, *ssrc);
}

bool WebrtcVideoConduit::GetRemoteSSRC(unsigned int* ssrc) {
  return !mPtrRTP->GetRemoteSSRC(mChannel, *ssrc);
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
  int rttMs;
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

/**
 * Peforms intialization of the MANDATORY components of the Video Engine
 */
MediaConduitErrorCode WebrtcVideoConduit::Init(WebrtcVideoConduit *other)
{
  CSFLogDebug(logTag,  "%s this=%p other=%p", __FUNCTION__, this, other);

  if (other) {
    MOZ_ASSERT(!other->mOtherDirection);
    other->mOtherDirection = this;
    mOtherDirection = other;

    // only one can call ::Create()/GetVideoEngine()
    MOZ_ASSERT(other->mVideoEngine);
    mVideoEngine = other->mVideoEngine;
  } else {

#ifdef MOZ_WIDGET_ANDROID
    jobject context = jsjni_GetGlobalContextRef();

    // get the JVM
    JavaVM *jvm = jsjni_GetVM();

    if (webrtc::VideoEngine::SetAndroidObjects(jvm, (void*)context) != 0) {
      CSFLogError(logTag,  "%s: could not set Android objects", __FUNCTION__);
      return kMediaConduitSessionNotInited;
    }
#endif

    // Per WebRTC APIs below function calls return nullptr on failure
    if( !(mVideoEngine = webrtc::VideoEngine::Create()) )
    {
      CSFLogError(logTag, "%s Unable to create video engine ", __FUNCTION__);
      return kMediaConduitSessionNotInited;
    }

    PRLogModuleInfo *logs = GetWebRTCLogInfo();
    if (!gWebrtcTraceLoggingOn && logs && logs->level > 0) {
      // no need to a critical section or lock here
      gWebrtcTraceLoggingOn = 1;

      const char *file = PR_GetEnv("WEBRTC_TRACE_FILE");
      if (!file) {
        file = "WebRTC.log";
      }
      CSFLogDebug(logTag,  "%s Logging webrtc to %s level %d", __FUNCTION__,
                  file, logs->level);
      mVideoEngine->SetTraceFilter(logs->level);
      mVideoEngine->SetTraceFile(file);
    }
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

  if (other) {
    mChannel = other->mChannel;
    mPtrExtCapture = other->mPtrExtCapture;
    mCapId = other->mCapId;
  } else {
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

    if(mPtrViERender->AddRenderer(mChannel,
                                  webrtc::kVideoI420,
                                  (webrtc::ExternalRenderer*) this) == -1)
    {
      CSFLogError(logTag, "%s Failed to added external renderer ", __FUNCTION__);
      return kMediaConduitInvalidRenderer;
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
  }

  CSFLogError(logTag, "%s Initialization Done", __FUNCTION__);
  return kMediaConduitNoError;
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
  } else if ((mOtherDirection && mOtherDirection->mSyncedTo) || mSyncedTo) {
    mPtrViEBase->DisconnectAudioChannel(mChannel);
    mPtrViEBase->SetVoiceEngine(nullptr);
  }

  // Now manage the shared sync reference (ugly)
  if (mSyncedTo || !mOtherDirection ) {
    mSyncedTo = aConduit;
  } else {
    mOtherDirection->mSyncedTo = aConduit;
  }
}

MediaConduitErrorCode
WebrtcVideoConduit::AttachRenderer(mozilla::RefPtr<VideoRenderer> aVideoRenderer)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  //null renderer
  if(!aVideoRenderer)
  {
    CSFLogError(logTag, "%s NULL Renderer", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitInvalidRenderer;
  }

  //Start Rendering if we haven't already
  if(!mRenderer)
  {
    mRenderer = aVideoRenderer; // must be done before StartRender()

    if(mPtrViERender->StartRender(mChannel) == -1)
    {
      CSFLogError(logTag, "%s Starting the Renderer Failed %d ", __FUNCTION__,
                                                      mPtrViEBase->LastError());
      mRenderer = nullptr;
      return kMediaConduitRendererFail;
    }
  } else {
    //Assign the new renderer - overwrites if there is already one
    mRenderer = aVideoRenderer;
  }

  return kMediaConduitNoError;
}

void
WebrtcVideoConduit::DetachRenderer()
{
  if(mRenderer)
  {
    mPtrViERender->StopRender(mChannel);
    mRenderer = nullptr;
  }
}

MediaConduitErrorCode
WebrtcVideoConduit::AttachTransport(mozilla::RefPtr<TransportInterface> aTransport)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  if(!aTransport)
  {
    CSFLogError(logTag, "%s NULL Transport", __FUNCTION__);
    return kMediaConduitInvalidTransport;
  }
  // set the transport
  mTransport = aTransport;
  return kMediaConduitNoError;
}

/**
 * Note: Setting the send-codec on the Video Engine will restart the encoder,
 * sets up new SSRC and reset RTP_RTCP module with the new codec setting.
 */
MediaConduitErrorCode
WebrtcVideoConduit::ConfigureSendMediaCodec(const VideoCodecConfig* codecConfig)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  bool codecFound = false;
  MediaConduitErrorCode condError = kMediaConduitNoError;
  int error = 0; //webrtc engine errors
  webrtc::VideoCodec  video_codec;
  std::string payloadName;

  //validate basic params
  if((condError = ValidateCodecConfig(codecConfig,true)) != kMediaConduitNoError)
  {
    return condError;
  }

  //Check if we have same codec already applied
  if(CheckCodecsForMatch(mCurSendCodecConfig, codecConfig))
  {
    CSFLogDebug(logTag,  "%s Codec has been applied already ", __FUNCTION__);
    return kMediaConduitCodecInUse;
  }

  //transmitting already ?
  if(mEngineTransmitting)
  {
    CSFLogDebug(logTag, "%s Engine Already Sending. Attemping to Stop ", __FUNCTION__);
    if(mPtrViEBase->StopSend(mChannel) == -1)
    {
      CSFLogError(logTag, "%s StopSend() Failed %d ",__FUNCTION__,
                  mPtrViEBase->LastError());
      return kMediaConduitUnknownError;
    }
  }

  mEngineTransmitting = false;

  if (codecConfig->mLoadManager) {
    mPtrViEBase->RegisterCpuOveruseObserver(mChannel, codecConfig->mLoadManager);
    mPtrViEBase->SetLoadManager(codecConfig->mLoadManager);
  }

  // we should be good here to set the new codec.
  for(int idx=0; idx < mPtrViECodec->NumberOfCodecs(); idx++)
  {
    if(0 == mPtrViECodec->GetCodec(idx, video_codec))
    {
      payloadName = video_codec.plName;
      if(codecConfig->mName.compare(payloadName) == 0)
      {
        CodecConfigToWebRTCCodec(codecConfig, video_codec);
        codecFound = true;
        break;
      }
    }
  }//for

  if(codecFound == false)
  {
    CSFLogError(logTag, "%s Codec Mismatch ", __FUNCTION__);
    return kMediaConduitInvalidSendCodec;
  }

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
  mSendingWidth = 0;
  mSendingHeight = 0;

  if(codecConfig->RtcpFbIsSet(SDP_RTCP_FB_NACK_BASIC)) {
    CSFLogDebug(logTag, "Enabling NACK (send) for video stream\n");
    if (mPtrRTP->SetNACKStatus(mChannel, true) != 0)
    {
      CSFLogError(logTag,  "%s NACKStatus Failed %d ", __FUNCTION__,
                  mPtrViEBase->LastError());
      return kMediaConduitNACKStatusError;
    }
  }

  if(mPtrViEBase->StartSend(mChannel) == -1)
  {
    CSFLogError(logTag, "%s Start Send Error %d ", __FUNCTION__,
                mPtrViEBase->LastError());
    return kMediaConduitUnknownError;
  }

  //Copy the applied config for future reference.
  delete mCurSendCodecConfig;

  mCurSendCodecConfig = new VideoCodecConfig(*codecConfig);

  mPtrRTP->SetRembStatus(mChannel, true, false);

  // by now we should be successfully started the transmission
  mEngineTransmitting = true;
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::ConfigureRecvMediaCodecs(
    const std::vector<VideoCodecConfig* >& codecConfigList)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  MediaConduitErrorCode condError = kMediaConduitNoError;
  int error = 0; //webrtc engine errors
  bool success = false;
  std::string  payloadName;

  // are we receiving already? If so, stop receiving and playout
  // since we can't apply new recv codec when the engine is playing.
  if(mEngineReceiving)
  {
    CSFLogDebug(logTag, "%s Engine Already Receiving . Attemping to Stop ", __FUNCTION__);
    if(mPtrViEBase->StopReceive(mChannel) == -1)
    {
      error = mPtrViEBase->LastError();
      if(error == kViEBaseUnknownError)
      {
        CSFLogDebug(logTag, "%s StopReceive() Success ", __FUNCTION__);
        mEngineReceiving = false;
      } else {
        CSFLogError(logTag, "%s StopReceive() Failed %d ", __FUNCTION__,
                    mPtrViEBase->LastError());
        return kMediaConduitUnknownError;
      }
    }
  }

  mEngineReceiving = false;

  if(codecConfigList.empty())
  {
    CSFLogError(logTag, "%s Zero number of codecs to configure", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  webrtc::ViEKeyFrameRequestMethod kf_request = webrtc::kViEKeyFrameRequestNone;
  bool use_nack_basic = false;

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
    if (codecConfigList[i]->RtcpFbIsSet(SDP_RTCP_FB_NACK_PLI))
    {
      kf_request = webrtc::kViEKeyFrameRequestPliRtcp;
    } else if(kf_request == webrtc::kViEKeyFrameRequestNone &&
              codecConfigList[i]->RtcpFbIsSet(SDP_RTCP_FB_CCM_FIR))
    {
      kf_request = webrtc::kViEKeyFrameRequestFirRtcp;
    }

    // Check whether NACK is requested
    if(codecConfigList[i]->RtcpFbIsSet(SDP_RTCP_FB_NACK_BASIC))
    {
      use_nack_basic = true;
    }

    webrtc::VideoCodec  video_codec;

    mEngineReceiving = false;
    memset(&video_codec, 0, sizeof(webrtc::VideoCodec));
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
            if(CopyCodecToDB(codecConfigList[i]))
            {
              success = true;
            } else {
              CSFLogError(logTag,"%s Unable to updated Codec Database", __FUNCTION__);
              return kMediaConduitUnknownError;
            }
          }
          break; //we found a match
        }
      }
    }//end for codeclist

  }//end for

  if(!success)
  {
    CSFLogError(logTag, "%s Setting Receive Codec Failed ", __FUNCTION__);
    return kMediaConduitInvalidReceiveCodec;
  }

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
      MOZ_ASSERT(PR_FALSE);
      mFrameRequestMethod = FrameRequestUnknown;
  }

  if(use_nack_basic)
  {
    CSFLogDebug(logTag, "Enabling NACK (recv) for video stream\n");
    if (mPtrRTP->SetNACKStatus(mChannel, true) != 0)
    {
      CSFLogError(logTag,  "%s NACKStatus Failed %d ", __FUNCTION__,
                  mPtrViEBase->LastError());
      return kMediaConduitNACKStatusError;
    }
  }
  mUsingNackBasic = use_nack_basic;

  //Start Receive on the video engine
  if(mPtrViEBase->StartReceive(mChannel) == -1)
  {
    error = mPtrViEBase->LastError();
    CSFLogError(logTag, "%s Start Receive Error %d ", __FUNCTION__, error);


    return kMediaConduitUnknownError;
  }

#ifdef MOZILLA_INTERNAL_API
  if (NS_IsMainThread()) {
    nsresult rv;
    nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);

      if (branch) {
	branch->GetBoolPref("media.video.test_latency", &mVideoLatencyTestEnable);
      }
    }
  }
#endif

  // by now we should be successfully started the reception
  mPtrRTP->SetRembStatus(mChannel, false, true);
  mEngineReceiving = true;
  DumpCodecDB();
  return kMediaConduitNoError;
}

// XXX we need to figure out how to feed back changes in preferred capture
// resolution to the getUserMedia source
bool
WebrtcVideoConduit::SelectSendResolution(unsigned short width,
                                         unsigned short height)
{
  // XXX This will do bandwidth-resolution adaptation as well - bug 877954

  // Limit resolution to max-fs while keeping same aspect ratio as the
  // incoming image.
  if (mCurSendCodecConfig && mCurSendCodecConfig->mMaxFrameSize)
  {
    unsigned int cur_fs, max_width, max_height, mb_width, mb_height, mb_max;

    mb_width = (width + 15) >> 4;
    mb_height = (height + 15) >> 4;

    cur_fs = mb_width * mb_height;

    // Limit resolution to max_fs, but don't scale up.
    if (cur_fs > mCurSendCodecConfig->mMaxFrameSize)
    {
      double scale_ratio;

      scale_ratio = sqrt((double) mCurSendCodecConfig->mMaxFrameSize /
                         (double) cur_fs);

      mb_width = mb_width * scale_ratio;
      mb_height = mb_height * scale_ratio;

      // Adjust mb_width and mb_height if they were truncated to zero.
      if (mb_width == 0) {
        mb_width = 1;
        mb_height = std::min(mb_height, mCurSendCodecConfig->mMaxFrameSize);
      }
      if (mb_height == 0) {
        mb_height = 1;
        mb_width = std::min(mb_width, mCurSendCodecConfig->mMaxFrameSize);
      }
    }

    // Limit width/height seperately to limit effect of extreme aspect ratios.
    mb_max = (unsigned) sqrt(8 * (double) mCurSendCodecConfig->mMaxFrameSize);

    max_width = 16 * std::min(mb_width, mb_max);
    max_height = 16 * std::min(mb_height, mb_max);

    if (width * max_height > max_width * height)
    {
      if (width > max_width)
      {
        // Due to the value is truncated to integer here and forced to even
        // value later, adding 1 to improve accuracy.
        height = max_width * height / width + 1;
        width = max_width;
      }
    }
    else
    {
      if (height > max_height)
      {
        // Due to the value is truncated to integer here and forced to even
        // value later, adding 1 to improve accuracy.
        width = max_height * width / height + 1;
        height = max_height;
      }
    }

    // Favor even multiples of pixels for width and height.
    width = std::max(width & ~1, 2);
    height = std::max(height & ~1, 2);
  }

  // Adapt to getUserMedia resolution changes
  // check if we need to reconfigure the sending resolution
  if (mSendingWidth != width || mSendingHeight != height)
  {
    // This will avoid us continually retrying this operation if it fails.
    // If the resolution changes, we'll try again.  In the meantime, we'll
    // keep using the old size in the encoder.
    mSendingWidth = width;
    mSendingHeight = height;

    // Get current vie codec.
    webrtc::VideoCodec vie_codec;
    int32_t err;

    if ((err = mPtrViECodec->GetSendCodec(mChannel, vie_codec)) != 0)
    {
      CSFLogError(logTag, "%s: GetSendCodec failed, err %d", __FUNCTION__, err);
      return false;
    }
    if (vie_codec.width != width || vie_codec.height != height)
    {
      vie_codec.width = width;
      vie_codec.height = height;

      if ((err = mPtrViECodec->SetSendCodec(mChannel, vie_codec)) != 0)
      {
        CSFLogError(logTag, "%s: SetSendCodec(%ux%u) failed, err %d",
                    __FUNCTION__, width, height, err);
        return false;
      }
      CSFLogDebug(logTag, "%s: Encoder resolution changed to %ux%u",
                  __FUNCTION__, width, height);
    } // else no change; mSendingWidth likely was 0
  }
  return true;
}

MediaConduitErrorCode
WebrtcVideoConduit::SetExternalSendCodec(int pltype,
                                         VideoEncoder* encoder) {
  int ret = mPtrExtCodec->RegisterExternalSendCodec(mChannel,
                                                    pltype,
                                                    static_cast<WebrtcVideoEncoder*>(encoder),
                                                    false);
  return ret ? kMediaConduitInvalidSendCodec : kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::SetExternalRecvCodec(int pltype,
                                         VideoDecoder* decoder) {
  int ret = mPtrExtCodec->RegisterExternalReceiveCodec(mChannel,
                                                       pltype,
                                                       static_cast<WebrtcVideoDecoder*>(decoder));
  return ret ? kMediaConduitInvalidReceiveCodec : kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcVideoConduit::SendVideoFrame(unsigned char* video_frame,
                                   unsigned int video_frame_length,
                                   unsigned short width,
                                   unsigned short height,
                                   VideoType video_type,
                                   uint64_t capture_time)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  //check for  the parameters sanity
  if(!video_frame || video_frame_length == 0 ||
     width == 0 || height == 0)
  {
    CSFLogError(logTag,  "%s Invalid Parameters ",__FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  webrtc::RawVideoType type;
  switch (video_type) {
    case kVideoI420:
      type = webrtc::kVideoI420;
      break;
    case kVideoNV21:
      type = webrtc::kVideoNV21;
      break;
    default:
      CSFLogError(logTag,  "%s VideoType Invalid. Only 1420 and NV21 Supported",__FUNCTION__);
      MOZ_ASSERT(PR_FALSE);
      return kMediaConduitMalformedArgument;
  }
  //Transmission should be enabled before we insert any frames.
  if(!mEngineTransmitting)
  {
    CSFLogError(logTag, "%s Engine not transmitting ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  // enforce even width/height (paranoia)
  MOZ_ASSERT(!(width & 1));
  MOZ_ASSERT(!(height & 1));

  if (!SelectSendResolution(width, height))
  {
    return kMediaConduitCaptureError;
  }

  //insert the frame to video engine in I420 format only
  MOZ_ASSERT(mPtrExtCapture);
  if(mPtrExtCapture->IncomingFrame(video_frame,
                                   video_frame_length,
                                   width, height,
                                   type,
                                   (unsigned long long)capture_time) == -1)
  {
    CSFLogError(logTag,  "%s IncomingFrame Failed %d ", __FUNCTION__,
                                            mPtrViEBase->LastError());
    return kMediaConduitCaptureError;
  }

  CSFLogDebug(logTag, "%s Inserted a frame", __FUNCTION__);
  return kMediaConduitNoError;
}

// Transport Layer Callbacks
MediaConduitErrorCode
WebrtcVideoConduit::ReceivedRTPPacket(const void *data, int len)
{
  CSFLogDebug(logTag, "%s: Channel %d, Len %d ", __FUNCTION__, mChannel, len);

  // Media Engine should be receiving already.
  if(mEngineReceiving)
  {
    // let the engine know of a RTP packet to decode
    if(mPtrViENetwork->ReceivedRTPPacket(mChannel,data,len) == -1)
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
  if(mEngineTransmitting)
  {
    if(mPtrViENetwork->ReceivedRTCPPacket(mChannel,data,len) == -1)
    {
      int error = mPtrViEBase->LastError();
      CSFLogError(logTag, "%s RTP Processing Failed %d", __FUNCTION__, error);
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

//WebRTC::RTP Callback Implementation
int WebrtcVideoConduit::SendPacket(int channel, const void* data, int len)
{
  CSFLogDebug(logTag,  "%s : channel %d len %d %s", __FUNCTION__, channel, len,
              (mEngineReceiving && mOtherDirection) ? "(using mOtherDirection)" : "");

  if (mEngineReceiving)
  {
    if (mOtherDirection)
    {
      return mOtherDirection->SendPacket(channel, data, len);
    }
    CSFLogDebug(logTag,  "%s : Asked to send RTP without an RTP sender on channel %d",
                __FUNCTION__, channel);
    return -1;
  } else {
    if(mTransport && (mTransport->SendRtpPacket(data, len) == NS_OK))
    {
      CSFLogDebug(logTag, "%s Sent RTP Packet ", __FUNCTION__);
      return len;
    } else {
      CSFLogError(logTag, "%s RTP Packet Send Failed ", __FUNCTION__);
      return -1;
    }
  }
}

int WebrtcVideoConduit::SendRTCPPacket(int channel, const void* data, int len)
{
  CSFLogDebug(logTag,  "%s : channel %d , len %d ", __FUNCTION__, channel,len);

  if (mEngineTransmitting)
  {
    if (mOtherDirection)
    {
      return mOtherDirection->SendRTCPPacket(channel, data, len);
    }
  }

  // We come here if we have only one pipeline/conduit setup,
  // such as for unidirectional streams.
  // We also end up here if we are receiving
  if(mTransport && mTransport->SendRtcpPacket(data, len) == NS_OK)
  {
    CSFLogDebug(logTag, "%s Sent RTCP Packet ", __FUNCTION__);
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


  mReceivingWidth = width;
  mReceivingHeight = height;

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
                                 int buffer_size,
                                 uint32_t time_stamp,
                                 int64_t render_time,
                                 void *handle)
{
  CSFLogDebug(logTag,  "%s Buffer Size %d", __FUNCTION__, buffer_size);

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
    mRenderer->RenderVideoFrame(buffer, buffer_size, time_stamp, render_time,
                                img_h);
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
  cinst.plType  = codecInfo->mType;
  // leave width/height alone; they'll be overridden on the first frame
  if (codecInfo->mMaxFrameRate > 0)
  {
    cinst.maxFramerate = codecInfo->mMaxFrameRate;
  }
  cinst.minBitrate = 200;
  cinst.startBitrate = 300;
  cinst.maxBitrate = 2000;
}

//Copy the codec passed into Conduit's database
bool
WebrtcVideoConduit::CopyCodecToDB(const VideoCodecConfig* codecInfo)
{
  VideoCodecConfig* cdcConfig = new VideoCodecConfig(*codecInfo);
  mRecvCodecList.push_back(cdcConfig);
  return true;
}

bool
WebrtcVideoConduit::CheckCodecsForMatch(const VideoCodecConfig* curCodecConfig,
                                        const VideoCodecConfig* codecInfo) const
{
  if(!curCodecConfig)
  {
    return false;
  }

  if(curCodecConfig->mType  == codecInfo->mType &&
     curCodecConfig->mName.compare(codecInfo->mName) == 0 &&
     curCodecConfig->mMaxFrameSize == codecInfo->mMaxFrameSize &&
     curCodecConfig->mMaxFrameRate == codecInfo->mMaxFrameRate)
  {
    return true;
  }

  return false;
}

/**
 * Checks if the codec is already in Conduit's database
 */
bool
WebrtcVideoConduit::CheckCodecForMatch(const VideoCodecConfig* codecInfo) const
{
  //the db should have atleast one codec
  for(std::vector<VideoCodecConfig*>::size_type i=0;i < mRecvCodecList.size();i++)
  {
    if(CheckCodecsForMatch(mRecvCodecList[i],codecInfo))
    {
      //match
      return true;
    }
  }
  //no match or empty local db
  return false;
}

/**
 * Perform validation on the codecConfig to be applied
 * Verifies if the codec is already applied.
 */
MediaConduitErrorCode
WebrtcVideoConduit::ValidateCodecConfig(const VideoCodecConfig* codecInfo,
                                        bool send) const
{
  bool codecAppliedAlready = false;

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

  //check if we have the same codec already applied
  if(send)
  {
    codecAppliedAlready = CheckCodecsForMatch(mCurSendCodecConfig,codecInfo);
  } else {
    codecAppliedAlready = CheckCodecForMatch(codecInfo);
  }

  if(codecAppliedAlready)
  {
    CSFLogDebug(logTag, "%s Codec %s Already Applied  ", __FUNCTION__, codecInfo->mName.c_str());
    return kMediaConduitCodecInUse;
  }
  return kMediaConduitNoError;
}

void
WebrtcVideoConduit::DumpCodecDB() const
{
  for(std::vector<VideoCodecConfig*>::size_type i=0;i<mRecvCodecList.size();i++)
  {
    CSFLogDebug(logTag,"Payload Name: %s", mRecvCodecList[i]->mName.c_str());
    CSFLogDebug(logTag,"Payload Type: %d", mRecvCodecList[i]->mType);
    CSFLogDebug(logTag,"Payload Max Frame Size: %d", mRecvCodecList[i]->mMaxFrameSize);
    CSFLogDebug(logTag,"Payload Max Frame Rate: %d", mRecvCodecList[i]->mMaxFrameRate);
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

}// end namespace
