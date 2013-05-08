/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"
#include "nspr.h"

#include "AudioConduit.h"
#include "nsCOMPtr.h"
#include "mozilla/Services.h"
#include "nsServiceManagerUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsThreadUtils.h"

#include "voice_engine/include/voe_errors.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidJNIWrapper.h"
#endif

namespace mozilla {

static const char* logTag ="WebrtcAudioSessionConduit";

// 32 bytes is what WebRTC CodecInst expects
const unsigned int WebrtcAudioConduit::CODEC_PLNAME_SIZE = 32;

/**
 * Factory Method for AudioConduit
 */
mozilla::RefPtr<AudioSessionConduit> AudioSessionConduit::Create(AudioSessionConduit *aOther)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
#ifdef MOZILLA_INTERNAL_API
  // unit tests create their own "main thread"
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
#endif

  WebrtcAudioConduit* obj = new WebrtcAudioConduit();
  if(obj->Init(static_cast<WebrtcAudioConduit*>(aOther)) != kMediaConduitNoError)
  {
    CSFLogError(logTag,  "%s AudioConduit Init Failed ", __FUNCTION__);
    delete obj;
    return NULL;
  }
  CSFLogDebug(logTag,  "%s Successfully created AudioConduit ", __FUNCTION__);
  return obj;
}

/**
 * Destruction defines for our super-classes
 */
WebrtcAudioConduit::~WebrtcAudioConduit()
{
#ifdef MOZILLA_INTERNAL_API
  // unit tests create their own "main thread"
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
#endif

  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  for(std::vector<AudioCodecConfig*>::size_type i=0;i < mRecvCodecList.size();i++)
  {
    delete mRecvCodecList[i];
  }

  delete mCurSendCodecConfig;

  // The first one of a pair to be deleted shuts down media for both
  if(mPtrVoEXmedia)
  {
    if (!mShutDown) {
      mPtrVoEXmedia->SetExternalRecordingStatus(false);
      mPtrVoEXmedia->SetExternalPlayoutStatus(false);
    }
    mPtrVoEXmedia->Release();
  }

  if(mPtrVoEProcessing)
  {
    mPtrVoEProcessing->Release();
  }

  //Deal with the transport
  if(mPtrVoENetwork)
  {
    if (!mShutDown) {
      mPtrVoENetwork->DeRegisterExternalTransport(mChannel);
    }
    mPtrVoENetwork->Release();
  }

  if(mPtrVoECodec)
  {
    mPtrVoECodec->Release();
  }

  if(mPtrVoEBase)
  {
    if (!mShutDown) {
      mPtrVoEBase->StopPlayout(mChannel);
      mPtrVoEBase->StopSend(mChannel);
      mPtrVoEBase->StopReceive(mChannel);
      mPtrVoEBase->DeleteChannel(mChannel);
      mPtrVoEBase->Terminate();
    }
    mPtrVoEBase->Release();
  }

  if (mOtherDirection)
  {
    // mOtherDirection owns these now!
    mOtherDirection->mOtherDirection = NULL;
    // let other side we terminated the channel
    mOtherDirection->mShutDown = true;
    mVoiceEngine = nullptr;
  } else {
    // only one opener can call Delete.  Have it be the last to close.
    if(mVoiceEngine)
    {
      webrtc::VoiceEngine::Delete(mVoiceEngine);
    }
  }
}

/*
 * WebRTCAudioConduit Implementation
 */
MediaConduitErrorCode WebrtcAudioConduit::Init(WebrtcAudioConduit *other)
{
  CSFLogDebug(logTag,  "%s this=%p other=%p", __FUNCTION__, this, other);

  if (other) {
    MOZ_ASSERT(!other->mOtherDirection);
    other->mOtherDirection = this;
    mOtherDirection = other;

    // only one can call ::Create()/GetVoiceEngine()
    MOZ_ASSERT(other->mVoiceEngine);
    mVoiceEngine = other->mVoiceEngine;
  } else {
#ifdef MOZ_WIDGET_ANDROID
      jobject context = jsjni_GetGlobalContextRef();

      // get the JVM
      JavaVM *jvm = jsjni_GetVM();

      JNIEnv* env;
      if (jvm->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
          CSFLogError(logTag,  "%s: could not get Java environment", __FUNCTION__);
          return kMediaConduitSessionNotInited;
      }
      jvm->AttachCurrentThread(&env, NULL);

      if (webrtc::VoiceEngine::SetAndroidObjects(jvm, (void*)context) != 0) {
          CSFLogError(logTag, "%s Unable to set Android objects", __FUNCTION__);
          return kMediaConduitSessionNotInited;
      }

      env->DeleteGlobalRef(context);
#endif

    //Per WebRTC APIs below function calls return NULL on failure
    if(!(mVoiceEngine = webrtc::VoiceEngine::Create()))
    {
      CSFLogError(logTag, "%s Unable to create voice engine", __FUNCTION__);
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
      mVoiceEngine->SetTraceFilter(logs->level);
      mVoiceEngine->SetTraceFile(file);
    }
  }

  if(!(mPtrVoEBase = VoEBase::GetInterface(mVoiceEngine)))
  {
    CSFLogError(logTag, "%s Unable to initialize VoEBase", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if(!(mPtrVoENetwork = VoENetwork::GetInterface(mVoiceEngine)))
  {
    CSFLogError(logTag, "%s Unable to initialize VoENetwork", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if(!(mPtrVoECodec = VoECodec::GetInterface(mVoiceEngine)))
  {
    CSFLogError(logTag, "%s Unable to initialize VoEBCodec", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if(!(mPtrVoEProcessing = VoEAudioProcessing::GetInterface(mVoiceEngine)))
  {
    CSFLogError(logTag, "%s Unable to initialize VoEProcessing", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if(!(mPtrVoEXmedia = VoEExternalMedia::GetInterface(mVoiceEngine)))
  {
    CSFLogError(logTag, "%s Unable to initialize VoEExternalMedia", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if (other) {
    mChannel = other->mChannel;
  } else {
    // init the engine with our audio device layer
    if(mPtrVoEBase->Init() == -1)
    {
      CSFLogError(logTag, "%s VoiceEngine Base Not Initialized", __FUNCTION__);
      return kMediaConduitSessionNotInited;
    }

    if( (mChannel = mPtrVoEBase->CreateChannel()) == -1)
    {
      CSFLogError(logTag, "%s VoiceEngine Channel creation failed",__FUNCTION__);
      return kMediaConduitChannelError;
    }

    CSFLogDebug(logTag, "%s Channel Created %d ",__FUNCTION__, mChannel);

    if(mPtrVoENetwork->RegisterExternalTransport(mChannel, *this) == -1)
    {
      CSFLogError(logTag, "%s VoiceEngine, External Transport Failed",__FUNCTION__);
      return kMediaConduitTransportRegistrationFail;
    }

    if(mPtrVoEXmedia->SetExternalRecordingStatus(true) == -1)
    {
      CSFLogError(logTag, "%s SetExternalRecordingStatus Failed %d",__FUNCTION__,
                  mPtrVoEBase->LastError());
      return kMediaConduitExternalPlayoutError;
    }

    if(mPtrVoEXmedia->SetExternalPlayoutStatus(true) == -1)
    {
      CSFLogError(logTag, "%s SetExternalPlayoutStatus Failed %d ",__FUNCTION__,
                  mPtrVoEBase->LastError());
      return kMediaConduitExternalRecordingError;
    }
    CSFLogDebug(logTag ,  "%s AudioSessionConduit Initialization Done (%p)",__FUNCTION__, this);
  }
  return kMediaConduitNoError;
}

// AudioSessionConduit Implementation
MediaConduitErrorCode
WebrtcAudioConduit::AttachTransport(mozilla::RefPtr<TransportInterface> aTransport)
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

MediaConduitErrorCode
WebrtcAudioConduit::ConfigureSendMediaCodec(const AudioCodecConfig* codecConfig)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  MediaConduitErrorCode condError = kMediaConduitNoError;
  int error = 0;//webrtc engine errors
  webrtc::CodecInst cinst;

  //validate codec param
  if((condError = ValidateCodecConfig(codecConfig, true)) != kMediaConduitNoError)
  {
    return condError;
  }

  //are we transmitting already, stop and apply the send codec
  if(mEngineTransmitting)
  {
    CSFLogDebug(logTag, "%s Engine Already Sending. Attemping to Stop ", __FUNCTION__);
    if(mPtrVoEBase->StopSend(mChannel) == -1)
    {
      CSFLogError(logTag, "%s StopSend() Failed %d ", __FUNCTION__,
                                          mPtrVoEBase->LastError());
      return kMediaConduitUnknownError;
    }
  }

  mEngineTransmitting = false;

  if(!CodecConfigToWebRTCCodec(codecConfig,cinst))
  {
    CSFLogError(logTag,"%s CodecConfig to WebRTC Codec Failed ",__FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  if(mPtrVoECodec->SetSendCodec(mChannel, cinst) == -1)
  {
    error = mPtrVoEBase->LastError();
    CSFLogError(logTag, "%s SetSendCodec - Invalid Codec %d ",__FUNCTION__,
                                                                    error);

    if(error ==  VE_CANNOT_SET_SEND_CODEC || error == VE_CODEC_ERROR)
    {
      return kMediaConduitInvalidSendCodec;
    }

    return kMediaConduitUnknownError;
  }

  // TEMPORARY - see bug 694814 comment 2
  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);

    if (branch) {
      int32_t aec = 0; // 0 == unchanged
      bool aec_on = false;

      branch->GetBoolPref("media.peerconnection.aec_enabled", &aec_on);
      branch->GetIntPref("media.peerconnection.aec", &aec);

      CSFLogDebug(logTag,"Audio config: aec: %d", aec_on ? aec : -1);
      mEchoOn = aec_on;
      if (static_cast<webrtc::EcModes>(aec) != webrtc::kEcUnchanged)
        mEchoCancel = static_cast<webrtc::EcModes>(aec);

      branch->GetIntPref("media.peerconnection.capture_delay", &mCaptureDelay);
    }
  }

  if (0 != (error = mPtrVoEProcessing->SetEcStatus(mEchoOn, mEchoCancel))) {
    CSFLogError(logTag,"%s Error setting EVStatus: %d ",__FUNCTION__, error);
    return kMediaConduitUnknownError;
  }

  //Let's Send Transport State-machine on the Engine
  if(mPtrVoEBase->StartSend(mChannel) == -1)
  {
    error = mPtrVoEBase->LastError();
    CSFLogError(logTag, "%s StartSend failed %d", __FUNCTION__, error);
    return kMediaConduitUnknownError;
  }

  //Copy the applied config for future reference.
  delete mCurSendCodecConfig;

  mCurSendCodecConfig = new AudioCodecConfig(codecConfig->mType,
                                              codecConfig->mName,
                                              codecConfig->mFreq,
                                              codecConfig->mPacSize,
                                              codecConfig->mChannels,
                                              codecConfig->mRate);


  mEngineTransmitting = true;
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::ConfigureRecvMediaCodecs(
                    const std::vector<AudioCodecConfig*>& codecConfigList)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  MediaConduitErrorCode condError = kMediaConduitNoError;
  int error = 0; //webrtc engine errors
  bool success = false;

  // are we receiving already. If so, stop receiving and playout
  // since we can't apply new recv codec when the engine is playing
  if(mEngineReceiving)
  {
    CSFLogDebug(logTag, "%s Engine Already Receiving. Attemping to Stop ", __FUNCTION__);
    // AudioEngine doesn't fail fatal on stop reception. Ref:voe_errors.h.
    // hence we need-not be strict in failing here on error
    mPtrVoEBase->StopReceive(mChannel);
    CSFLogDebug(logTag, "%s Attemping to Stop playout ", __FUNCTION__);
    if(mPtrVoEBase->StopPlayout(mChannel) == -1)
    {
      if( mPtrVoEBase->LastError() == VE_CANNOT_STOP_PLAYOUT)
      {
        CSFLogDebug(logTag, "%s Stop-Playout Failed %d", __FUNCTION__, mPtrVoEBase->LastError());
        return kMediaConduitPlayoutError;
      }
    }
  }

  mEngineReceiving = false;

  if(!codecConfigList.size())
  {
    CSFLogError(logTag, "%s Zero number of codecs to configure", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  //Try Applying the codecs in the list
  for(std::vector<AudioCodecConfig*>::size_type i=0 ;i<codecConfigList.size();i++)
  {
    //if the codec param is invalid or diplicate, return error
    if((condError = ValidateCodecConfig(codecConfigList[i],false)) != kMediaConduitNoError)
    {
      return condError;
    }

    webrtc::CodecInst cinst;
    if(!CodecConfigToWebRTCCodec(codecConfigList[i],cinst))
    {
      CSFLogError(logTag,"%s CodecConfig to WebRTC Codec Failed ",__FUNCTION__);
      continue;
    }

    if(mPtrVoECodec->SetRecPayloadType(mChannel,cinst) == -1)
    {
      error = mPtrVoEBase->LastError();
      CSFLogError(logTag,  "%s SetRecvCodec Failed %d ",__FUNCTION__, error);
      continue;
    } else {
      CSFLogDebug(logTag, "%s Successfully Set RecvCodec %s", __FUNCTION__,
                                          codecConfigList[i]->mName.c_str());
      //copy this to local database
      if(CopyCodecToDB(codecConfigList[i]))
      {
        success = true;
      } else {
        CSFLogError(logTag,"%s Unable to updated Codec Database", __FUNCTION__);
        return kMediaConduitUnknownError;
      }

    }

  } //end for

  //Success == false indicates none of the codec was applied
  if(!success)
  {
    CSFLogError(logTag, "%s Setting Receive Codec Failed ", __FUNCTION__);
    return kMediaConduitInvalidReceiveCodec;
  }

  //If we are here, atleast one codec should have been set
  if(mPtrVoEBase->StartReceive(mChannel) == -1)
  {
    error = mPtrVoEBase->LastError();
    CSFLogError(logTag ,  "%s StartReceive Failed %d ",__FUNCTION__, error);
    if(error == VE_RECV_SOCKET_ERROR)
    {
      return kMediaConduitSocketError;
    }
    return kMediaConduitUnknownError;
  }


  if(mPtrVoEBase->StartPlayout(mChannel) == -1)
  {
    CSFLogError(logTag, "%s Starting playout Failed", __FUNCTION__);
    return kMediaConduitPlayoutError;
  }

  //we should be good here for setting this.
  mEngineReceiving = true;
  DumpCodecDB();
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::SendAudioFrame(const int16_t audio_data[],
                                    int32_t lengthSamples,
                                    int32_t samplingFreqHz,
                                    int32_t capture_delay)
{
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  // Following checks need to be performed
  // 1. Non null audio buffer pointer,
  // 2. invalid sampling frequency -  less than 0 or unsupported ones
  // 3. Appropriate Sample Length for 10 ms audio-frame. This represents
  //    block size the VoiceEngine feeds into encoder for passed in audio-frame
  //    Ex: for 16000 sampling rate , valid block-length is 160
  //    Similarly for 32000 sampling rate, valid block length is 320
  //    We do the check by the verify modular operator below to be zero

  if(!audio_data || (lengthSamples <= 0) ||
                    (IsSamplingFreqSupported(samplingFreqHz) == false) ||
                    ((lengthSamples % (samplingFreqHz / 100) != 0)) )
  {
    CSFLogError(logTag, "%s Invalid Params ", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  //validate capture time
  if(capture_delay < 0 )
  {
    CSFLogError(logTag,"%s Invalid Capture Delay ", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  // if transmission is not started .. conduit cannot insert frames
  if(!mEngineTransmitting)
  {
    CSFLogError(logTag, "%s Engine not transmitting ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  capture_delay = mCaptureDelay;
  //Insert the samples
  if(mPtrVoEXmedia->ExternalRecordingInsertData(audio_data,
                                                lengthSamples,
                                                samplingFreqHz,
                                                capture_delay) == -1)
  {
    int error = mPtrVoEBase->LastError();
    CSFLogError(logTag,  "%s Inserting audio data Failed %d", __FUNCTION__, error);
    if(error == VE_RUNTIME_REC_ERROR)
    {
      return kMediaConduitRecordingError;
    }
    return kMediaConduitUnknownError;
  }
  // we should be good here
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::GetAudioFrame(int16_t speechData[],
                                   int32_t samplingFreqHz,
                                   int32_t capture_delay,
                                   int& lengthSamples)
{

  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  unsigned int numSamples = 0;

  //validate params
  if(!speechData )
  {
    CSFLogError(logTag,"%s Null Audio Buffer Pointer", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  // Validate sample length
  if((numSamples = GetNum10msSamplesForFrequency(samplingFreqHz)) == 0  )
  {
    CSFLogError(logTag,"%s Invalid Sampling Frequency ", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  //validate capture time
  if(capture_delay < 0 )
  {
    CSFLogError(logTag,"%s Invalid Capture Delay ", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  //Conduit should have reception enabled before we ask for decoded
  // samples
  if(!mEngineReceiving)
  {
    CSFLogError(logTag, "%s Engine not Receiving ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }


  lengthSamples = 0;  //output paramter

  if(mPtrVoEXmedia->ExternalPlayoutGetData( speechData,
                                            samplingFreqHz,
                                            capture_delay,
                                            lengthSamples) == -1)
  {
    int error = mPtrVoEBase->LastError();
    CSFLogError(logTag,  "%s Getting audio data Failed %d", __FUNCTION__, error);
    if(error == VE_RUNTIME_PLAY_ERROR)
    {
      return kMediaConduitPlayoutError;
    }
    return kMediaConduitUnknownError;
  }

  CSFLogDebug(logTag,"%s GetAudioFrame:Got samples: length %d ",__FUNCTION__,
                                                               lengthSamples);
  return kMediaConduitNoError;
}

// Transport Layer Callbacks
MediaConduitErrorCode
WebrtcAudioConduit::ReceivedRTPPacket(const void *data, int len)
{
  CSFLogDebug(logTag,  "%s : channel %d", __FUNCTION__, mChannel);

  if(mEngineReceiving)
  {
    if(mPtrVoENetwork->ReceivedRTPPacket(mChannel,data,len) == -1)
    {
      int error = mPtrVoEBase->LastError();
      CSFLogError(logTag, "%s RTP Processing Error %d", __FUNCTION__, error);
      if(error == VE_RTP_RTCP_MODULE_ERROR)
      {
        return kMediaConduitRTPRTCPModuleError;
      }
      return kMediaConduitUnknownError;
    }
  } else {
    //engine not receiving
    CSFLogError(logTag, "%s ReceivedRTPPacket: Engine Error", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }
  //good here
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::ReceivedRTCPPacket(const void *data, int len)
{
  CSFLogDebug(logTag,  "%s : channel %d",__FUNCTION__, mChannel);

  if(mEngineTransmitting)
  {
    if(mPtrVoENetwork->ReceivedRTCPPacket(mChannel, data, len) == -1)
    {
      int error = mPtrVoEBase->LastError();
      CSFLogError(logTag, "%s RTCP Processing Error %d", __FUNCTION__, error);
      if(error == VE_RTP_RTCP_MODULE_ERROR)
      {
        return kMediaConduitRTPRTCPModuleError;
      }
      return kMediaConduitUnknownError;
    }
  } else {
    //engine not running
    CSFLogError(logTag, "%s ReceivedRTPPacket: Engine Error", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }
  //good here
  return kMediaConduitNoError;
}

//WebRTC::RTP Callback Implementation

int WebrtcAudioConduit::SendPacket(int channel, const void* data, int len)
{
  CSFLogDebug(logTag,  "%s : channel %d %s",__FUNCTION__,channel,
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

int WebrtcAudioConduit::SendRTCPPacket(int channel, const void* data, int len)
{
  CSFLogDebug(logTag,  "%s : channel %d", __FUNCTION__, channel);

  if (mEngineTransmitting)
  {
    if (mOtherDirection)
    {
      return mOtherDirection->SendRTCPPacket(channel, data, len);
    }
    CSFLogDebug(logTag,  "%s : Asked to send RTCP without an RTP receiver on channel %d",
                __FUNCTION__, channel);
    return -1;
  } else {
    if(mTransport && mTransport->SendRtcpPacket(data, len) == NS_OK)
    {
      CSFLogDebug(logTag, "%s Sent RTCP Packet ", __FUNCTION__);
      return len;
    } else {
      CSFLogError(logTag, "%s RTCP Packet Send Failed ", __FUNCTION__);
      return -1;
    }
  }
}

/**
 * Converts between CodecConfig to WebRTC Codec Structure.
 */

bool
WebrtcAudioConduit::CodecConfigToWebRTCCodec(const AudioCodecConfig* codecInfo,
                                              webrtc::CodecInst& cinst)
 {
  const unsigned int plNameLength = codecInfo->mName.length()+1;
  memset(&cinst, 0, sizeof(webrtc::CodecInst));
  if(sizeof(cinst.plname) < plNameLength)
  {
    CSFLogError(logTag, "%s Payload name buffer capacity mismatch ",
                                                      __FUNCTION__);
    return false;
  }
  memcpy(cinst.plname, codecInfo->mName.c_str(),codecInfo->mName.length());
  cinst.plname[plNameLength]='\0';
  cinst.pltype   =  codecInfo->mType;
  cinst.rate     =  codecInfo->mRate;
  cinst.pacsize  =  codecInfo->mPacSize;
  cinst.plfreq   =  codecInfo->mFreq;
  cinst.channels =  codecInfo->mChannels;
  return true;
 }

/**
  *  Supported Sampling Frequncies.
  */
bool
WebrtcAudioConduit::IsSamplingFreqSupported(int freq) const
{
  if(GetNum10msSamplesForFrequency(freq))
  {
    return true;
  } else {
    return false;
  }
}

/* Return block-length of 10 ms audio frame in number of samples */
unsigned int
WebrtcAudioConduit::GetNum10msSamplesForFrequency(int samplingFreqHz) const
{
  switch(samplingFreqHz)
  {
    case 16000: return 160; //160 samples
    case 32000: return 320; //320 samples
    case 44000: return 440; //440 samples
    case 48000: return 480; //480 samples
    default:    return 0; // invalid or unsupported
  }
}

//Copy the codec passed into Conduit's database
bool
WebrtcAudioConduit::CopyCodecToDB(const AudioCodecConfig* codecInfo)
{

  AudioCodecConfig* cdcConfig = new AudioCodecConfig(codecInfo->mType,
                                                     codecInfo->mName,
                                                     codecInfo->mFreq,
                                                     codecInfo->mPacSize,
                                                     codecInfo->mChannels,
                                                     codecInfo->mRate);
  mRecvCodecList.push_back(cdcConfig);
  return true;
}

/**
 * Checks if 2 codec structs are same
 */
bool
WebrtcAudioConduit::CheckCodecsForMatch(const AudioCodecConfig* curCodecConfig,
                                         const AudioCodecConfig* codecInfo) const
{
  if(!curCodecConfig)
  {
    return false;
  }

  if(curCodecConfig->mType   == codecInfo->mType &&
      (curCodecConfig->mName.compare(codecInfo->mName) == 0) &&
      curCodecConfig->mFreq   == codecInfo->mFreq &&
      curCodecConfig->mPacSize == codecInfo->mPacSize &&
      curCodecConfig->mChannels == codecInfo->mChannels &&
      curCodecConfig->mRate == codecInfo->mRate)
  {
    return true;
  }

  return false;
}

/**
 * Checks if the codec is already in Conduit's database
 */
bool
WebrtcAudioConduit::CheckCodecForMatch(const AudioCodecConfig* codecInfo) const
{
  //the db should have atleast one codec
  for(std::vector<AudioCodecConfig*>::size_type i=0;i < mRecvCodecList.size();i++)
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
 * Perform validation on the codecCofig to be applied
 * Verifies if the codec is already applied.
 */
MediaConduitErrorCode
WebrtcAudioConduit::ValidateCodecConfig(const AudioCodecConfig* codecInfo,
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

  //Only mono or stereo channels supported
  if( (codecInfo->mChannels != 1) && (codecInfo->mChannels != 2))
  {
    CSFLogError(logTag, "%s Channel Unsupported ", __FUNCTION__);
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
WebrtcAudioConduit::DumpCodecDB() const
 {
    for(std::vector<AudioCodecConfig*>::size_type i=0;i < mRecvCodecList.size();i++)
    {
      CSFLogDebug(logTag,"Payload Name: %s", mRecvCodecList[i]->mName.c_str());
      CSFLogDebug(logTag,"Payload Type: %d", mRecvCodecList[i]->mType);
      CSFLogDebug(logTag,"Payload Frequency: %d", mRecvCodecList[i]->mFreq);
      CSFLogDebug(logTag,"Payload PacketSize: %d", mRecvCodecList[i]->mPacSize);
      CSFLogDebug(logTag,"Payload Channels: %d", mRecvCodecList[i]->mChannels);
      CSFLogDebug(logTag,"Payload Sampling Rate: %d", mRecvCodecList[i]->mRate);
    }
 }
}// end namespace
