/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"
#include "nspr.h"

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#elif defined XP_WIN
#include <winsock2.h>
#endif

#include "AudioConduit.h"
#include "nsCOMPtr.h"
#include "mozilla/Services.h"
#include "nsServiceManagerUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsThreadUtils.h"
#include "Latency.h"
#include "mozilla/Telemetry.h"

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/voice_engine_impl.h"
#include "webrtc/system_wrappers/include/clock.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

namespace mozilla {

static const char* acLogTag ="WebrtcAudioSessionConduit";
#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG acLogTag

// 32 bytes is what WebRTC CodecInst expects
const unsigned int WebrtcAudioConduit::CODEC_PLNAME_SIZE = 32;

/**
 * Factory Method for AudioConduit
 */
RefPtr<AudioSessionConduit> AudioSessionConduit::Create()
{
  CSFLogDebug(LOGTAG,  "%s ", __FUNCTION__);
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  WebrtcAudioConduit* obj = new WebrtcAudioConduit();
  if(obj->Init() != kMediaConduitNoError)
  {
    CSFLogError(LOGTAG,  "%s AudioConduit Init Failed ", __FUNCTION__);
    delete obj;
    return nullptr;
  }
  CSFLogDebug(LOGTAG,  "%s Successfully created AudioConduit ", __FUNCTION__);
  return obj;
}

/**
 * Destruction defines for our super-classes
 */
WebrtcAudioConduit::~WebrtcAudioConduit()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  CSFLogDebug(LOGTAG,  "%s ", __FUNCTION__);
  for(auto & codec : mRecvCodecList)
  {
    delete codec;
  }

  //Deal with the transport
  if(mPtrVoENetwork)
  {
    mPtrVoENetwork->DeRegisterExternalTransport(mChannel);
  }

  if(mPtrVoEBase)
  {
    mPtrVoEBase->StopPlayout(mChannel);
    mPtrVoEBase->StopSend(mChannel);
    mPtrVoEBase->StopReceive(mChannel);
    mChannelProxy = nullptr;
    mPtrVoEBase->DeleteChannel(mChannel);
    // We don't Terminate() the VoEBase here, because the Call (owned by
    // PeerConnectionMedia) actually owns the (shared) VoEBase/VoiceEngine
    // here
  }

  // We shouldn't delete the VoiceEngine until all these are released!
  // And we can't use a Scoped ptr, since the order is arbitrary
  mPtrVoENetwork = nullptr;
  mPtrVoEBase = nullptr;
  mPtrVoECodec = nullptr;
  mPtrVoEXmedia = nullptr;
  mPtrVoEProcessing = nullptr;
  mPtrVoEVideoSync = nullptr;
  mPtrVoERTP_RTCP = nullptr;
  mPtrRTP = nullptr;

  if (mVoiceEngine)
  {
    webrtc::VoiceEngine::Delete(mVoiceEngine);
  }
}

bool WebrtcAudioConduit::SetLocalSSRCs(const std::vector<unsigned int> & aSSRCs)
{
  // This should hold true until the WebRTC.org VoE refactor
  MOZ_ASSERT(aSSRCs.size() == 1,"WebrtcAudioConduit::SetLocalSSRCs accepts exactly 1 ssrc.");

  std::vector<unsigned int> oldSsrcs = GetLocalSSRCs();
  if (oldSsrcs.empty()) {
    MOZ_ASSERT(false, "GetLocalSSRC failed");
    return false;
  }

  if (oldSsrcs == aSSRCs) {
    return true;
  }

  bool wasTransmitting = mEngineTransmitting;
  if (StopTransmitting() != kMediaConduitNoError) {
    return false;
  }

  if (mPtrRTP->SetLocalSSRC(mChannel, aSSRCs[0])) {
    return false;
  }

  if (wasTransmitting) {
    if (StartTransmitting() != kMediaConduitNoError) {
      return false;
    }
  }
  return true;
}

std::vector<unsigned int> WebrtcAudioConduit::GetLocalSSRCs() const {
  unsigned int ssrc;
  if (!mPtrRTP->GetLocalSSRC(mChannel, ssrc)) {
    return std::vector<unsigned int>(1,ssrc);
  }
  return std::vector<unsigned int>();
}

bool WebrtcAudioConduit::GetRemoteSSRC(unsigned int* ssrc) {
  return !mPtrRTP->GetRemoteSSRC(mChannel, *ssrc);
}

bool WebrtcAudioConduit::SetLocalCNAME(const char* cname)
{
  char temp[256];
  strncpy(temp, cname, sizeof(temp) - 1);
  temp[sizeof(temp) - 1] = 0;
  return !mPtrRTP->SetRTCP_CNAME(mChannel, temp);
}

bool WebrtcAudioConduit::SetLocalMID(const std::string& mid)
{
  if (mPtrRTP->SetLocalMID(mChannel, mid.c_str())) {
    return false;
  }
  return true;
}

bool WebrtcAudioConduit::GetSendPacketTypeStats(
  webrtc::RtcpPacketTypeCounter* aPacketCounts)
{
  if (!mEngineTransmitting) {
    return false;
  }
  return !mPtrVoERTP_RTCP->GetRTCPPacketTypeCounters(mChannel, *aPacketCounts);
}

bool WebrtcAudioConduit::GetRecvPacketTypeStats(
  webrtc::RtcpPacketTypeCounter* aPacketCounts)
{
  if (!mEngineReceiving) {
    return false;
  }
  return !mPtrRTP->GetRTCPPacketTypeCounters(mChannel, *aPacketCounts);
}

bool WebrtcAudioConduit::GetAVStats(int32_t* jitterBufferDelayMs,
                                    int32_t* playoutBufferDelayMs,
                                    int32_t* avSyncOffsetMs) {
  return !mPtrVoEVideoSync->GetDelayEstimate(mChannel,
                                             jitterBufferDelayMs,
                                             playoutBufferDelayMs,
                                             avSyncOffsetMs);
}

bool WebrtcAudioConduit::GetRTPStats(unsigned int* jitterMs,
                                     unsigned int* cumulativeLost) {
  unsigned int maxJitterMs = 0;
  unsigned int discardedPackets;
  *jitterMs = 0;
  *cumulativeLost = 0;
  return !mPtrRTP->GetRTPStatistics(mChannel, *jitterMs, maxJitterMs,
                                    discardedPackets, *cumulativeLost);
}

DOMHighResTimeStamp
NTPtoDOMHighResTimeStamp(uint32_t ntpHigh, uint32_t ntpLow) {
  return (uint32_t(ntpHigh - webrtc::kNtpJan1970) +
          double(ntpLow) / webrtc::kMagicNtpFractionalUnit) * 1000;
}

bool WebrtcAudioConduit::GetRTCPReceiverReport(DOMHighResTimeStamp* timestamp,
                                               uint32_t* jitterMs,
                                               uint32_t* packetsReceived,
                                               uint64_t* bytesReceived,
                                               uint32_t* cumulativeLost,
                                               int32_t* rttMs) {
  double fractionLost;
  int64_t timestampTmp;
  int64_t rttMsTmp;
  bool res = mChannelProxy->GetRTCPReceiverStatistics(&timestampTmp,
                                                      jitterMs,
                                                      cumulativeLost,
                                                      packetsReceived,
                                                      bytesReceived,
                                                      &fractionLost,
                                                      &rttMsTmp);
  *timestamp = static_cast<double>(timestampTmp);
  *rttMs = static_cast<uint32_t>(rttMsTmp);
  return res;
}

bool WebrtcAudioConduit::GetRTCPSenderReport(DOMHighResTimeStamp* timestamp,
                                             unsigned int* packetsSent,
                                             uint64_t* bytesSent) {
  webrtc::RTCPSenderInfo senderInfo;
  webrtc::RtpRtcp * rtpRtcpModule;
  webrtc::RtpReceiver * rtp_receiver;
  bool result =
    !mPtrVoEVideoSync->GetRtpRtcp(mChannel,&rtpRtcpModule,&rtp_receiver) &&
    !rtpRtcpModule->RemoteRTCPStat(&senderInfo);
  if (result){
    *timestamp = NTPtoDOMHighResTimeStamp(senderInfo.NTPseconds,
                                          senderInfo.NTPfraction);
    *packetsSent = senderInfo.sendPacketCount;
    *bytesSent = senderInfo.sendOctetCount;
   }
   return result;
 }

bool WebrtcAudioConduit::SetDtmfPayloadType(unsigned char type, int freq) {
  CSFLogInfo(LOGTAG, "%s : setting dtmf payload %d", __FUNCTION__, (int)type);

  int result = mChannelProxy->SetSendTelephoneEventPayloadType(type, freq);
  if (result == -1) {
    CSFLogError(LOGTAG, "%s Failed call to SetSendTelephoneEventPayloadType(%u, %d)",
                __FUNCTION__, type, freq);
  }
  return result != -1;
}

bool WebrtcAudioConduit::InsertDTMFTone(int channel, int eventCode,
                                        bool outOfBand, int lengthMs,
                                        int attenuationDb) {
  NS_ASSERTION(!NS_IsMainThread(), "Do not call on main thread");

  if (!mVoiceEngine || !mDtmfEnabled) {
    return false;
  }

  int result = 0;
  if (outOfBand){
    result = mChannelProxy->SendTelephoneEventOutband(eventCode, lengthMs);
  }
  return result != -1;
}

void
WebrtcAudioConduit::OnRtpPacket(const webrtc::WebRtcRTPHeader* aHeader,
                                const int64_t aTimestamp,
                                const uint32_t aJitter) {
  mRtpSourceObserver.OnRtpPacket(aHeader, aTimestamp, aJitter);
}

void
WebrtcAudioConduit::GetRtpSources(const int64_t aTimeNow,
                                  nsTArray<dom::RTCRtpSourceEntry>& outSources)
{
  return mRtpSourceObserver.GetRtpSources(aTimeNow, outSources);
}

// test-only: inserts a CSRC entry in a RtpSourceObserver's history for
// getContributingSources mochitests
void InsertAudioLevelForContributingSource(RtpSourceObserver& observer,
                                           uint32_t aCsrcSource,
                                           int64_t aTimestamp,
                                           bool aHasAudioLevel,
                                           uint8_t aAudioLevel)
{
  using EntryType = dom::RTCRtpSourceEntryType;
  auto key = RtpSourceObserver::GetKey(aCsrcSource, EntryType::Contributing);
  auto& hist = observer.mRtpSources[key];
  hist.Insert(aTimestamp, aTimestamp, aHasAudioLevel, aAudioLevel);
}


void
WebrtcAudioConduit::InsertAudioLevelForContributingSource(uint32_t aCsrcSource,
                                                          int64_t aTimestamp,
                                                          bool aHasAudioLevel,
                                                          uint8_t aAudioLevel)
{
  mozilla::InsertAudioLevelForContributingSource(mRtpSourceObserver,
                                                 aCsrcSource,
                                                 aTimestamp,
                                                 aHasAudioLevel,
                                                 aAudioLevel);
}

/*
 * WebRTCAudioConduit Implementation
 */
MediaConduitErrorCode WebrtcAudioConduit::Init()
{
  CSFLogDebug(LOGTAG,  "%s this=%p", __FUNCTION__, this);

#ifdef MOZ_WIDGET_ANDROID
  JavaVM* jvm = mozilla::jni::GetVM();
  jobject context = mozilla::AndroidBridge::Bridge()->GetGlobalContextRef();

  if (webrtc::VoiceEngine::SetAndroidObjects(jvm, (void*)context) != 0) {
    CSFLogError(LOGTAG, "%s Unable to set Android objects", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }
#endif

  // Per WebRTC APIs below function calls return nullptr on failure
  if(!(mVoiceEngine = webrtc::VoiceEngine::Create()))
  {
    CSFLogError(LOGTAG, "%s Unable to create voice engine", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if(!(mPtrVoEBase = VoEBase::GetInterface(mVoiceEngine)))
  {
    CSFLogError(LOGTAG, "%s Unable to initialize VoEBase", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  // Init the engine with a fake audio device (we're using cubeb for audio input
  // and output anyways).
  if(mPtrVoEBase->Init(mFakeAudioDevice.get()) == -1)
  {
    CSFLogError(LOGTAG, "%s VoiceEngine Base Not Initialized", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if(!(mPtrVoENetwork = VoENetwork::GetInterface(mVoiceEngine)))
  {
    CSFLogError(LOGTAG, "%s Unable to initialize VoENetwork", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if(!(mPtrVoECodec = VoECodec::GetInterface(mVoiceEngine)))
  {
    CSFLogError(LOGTAG, "%s Unable to initialize VoEBCodec", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if(!(mPtrVoEProcessing = VoEAudioProcessing::GetInterface(mVoiceEngine)))
  {
    CSFLogError(LOGTAG, "%s Unable to initialize VoEProcessing", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }
  if(!(mPtrVoEXmedia = VoEExternalMedia::GetInterface(mVoiceEngine)))
  {
    CSFLogError(LOGTAG, "%s Unable to initialize VoEExternalMedia", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }
  if(!(mPtrVoERTP_RTCP = VoERTP_RTCP::GetInterface(mVoiceEngine)))
  {
    CSFLogError(LOGTAG, "%s Unable to initialize VoERTP_RTCP", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if(!(mPtrVoEVideoSync = VoEVideoSync::GetInterface(mVoiceEngine)))
  {
    CSFLogError(LOGTAG, "%s Unable to initialize VoEVideoSync", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }
  if (!(mPtrRTP = webrtc::VoERTP_RTCP::GetInterface(mVoiceEngine)))
  {
    CSFLogError(LOGTAG, "%s Unable to get audio RTP/RTCP interface ",
                __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if( (mChannel = mPtrVoEBase->CreateChannel()) == -1)
  {
    CSFLogError(LOGTAG, "%s VoiceEngine Channel creation failed",__FUNCTION__);
    return kMediaConduitChannelError;
  }
  // Needed to access TelephoneEvent APIs in 57 if we're not using Call/audio_send_stream/etc
  webrtc::VoiceEngineImpl* s = static_cast<webrtc::VoiceEngineImpl*>(mVoiceEngine);
  mChannelProxy = s->GetChannelProxy(mChannel);
  MOZ_ASSERT(mChannelProxy);
  mChannelProxy->SetRtpPacketObserver(this);

  CSFLogDebug(LOGTAG, "%s Channel Created %d ",__FUNCTION__, mChannel);

  if(mPtrVoENetwork->RegisterExternalTransport(mChannel, *this) == -1)
  {
    CSFLogError(LOGTAG, "%s VoiceEngine, External Transport Failed",__FUNCTION__);
    return kMediaConduitTransportRegistrationFail;
  }

  CSFLogDebug(LOGTAG, "%s AudioSessionConduit Initialization Done (%p)",__FUNCTION__, this);
  return kMediaConduitNoError;
}

// AudioSessionConduit Implementation
MediaConduitErrorCode
WebrtcAudioConduit::SetTransmitterTransport(RefPtr<TransportInterface> aTransport)
{
  CSFLogDebug(LOGTAG,  "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mTransmitterTransport = aTransport;
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::SetReceiverTransport(RefPtr<TransportInterface> aTransport)
{
  CSFLogDebug(LOGTAG,  "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mReceiverTransport = aTransport;
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::ConfigureSendMediaCodec(const AudioCodecConfig* codecConfig)
{
  CSFLogDebug(LOGTAG,  "%s ", __FUNCTION__);
  MediaConduitErrorCode condError = kMediaConduitNoError;
  int error = 0;//webrtc engine errors
  webrtc::CodecInst cinst;

  {
    //validate codec param
    if((condError = ValidateCodecConfig(codecConfig, true)) != kMediaConduitNoError)
    {
      return condError;
    }
  }

  condError = StopTransmitting();
  if (condError != kMediaConduitNoError) {
    return condError;
  }

  if(!CodecConfigToWebRTCCodec(codecConfig,cinst))
  {
    CSFLogError(LOGTAG,"%s CodecConfig to WebRTC Codec Failed ",__FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  if(mPtrVoECodec->SetSendCodec(mChannel, cinst) == -1)
  {
    error = mPtrVoEBase->LastError();
    CSFLogError(LOGTAG, "%s SetSendCodec - Invalid Codec %d ",__FUNCTION__,
                                                                    error);

    if(error ==  VE_CANNOT_SET_SEND_CODEC || error == VE_CODEC_ERROR)
    {
      CSFLogError(LOGTAG, "%s Invalid Send Codec", __FUNCTION__);
      return kMediaConduitInvalidSendCodec;
    }
    CSFLogError(LOGTAG, "%s SetSendCodec Failed %d ", __FUNCTION__,
                                         mPtrVoEBase->LastError());
    return kMediaConduitUnknownError;
  }

  // This must be called after SetSendCodec
  if (mPtrVoECodec->SetFECStatus(mChannel, codecConfig->mFECEnabled) == -1) {
    CSFLogError(LOGTAG, "%s SetFECStatus Failed %d ", __FUNCTION__,
                mPtrVoEBase->LastError());
    return kMediaConduitFECStatusError;
  }

  mDtmfEnabled = codecConfig->mDtmfEnabled;

  if (codecConfig->mName == "opus" && codecConfig->mMaxPlaybackRate) {
    if (mPtrVoECodec->SetOpusMaxPlaybackRate(
          mChannel,
          codecConfig->mMaxPlaybackRate) == -1) {
      CSFLogError(LOGTAG, "%s SetOpusMaxPlaybackRate Failed %d ", __FUNCTION__,
                  mPtrVoEBase->LastError());
      return kMediaConduitUnknownError;
    }
  }

  // TEMPORARY - see bug 694814 comment 2
  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);

    if (branch) {
      branch->GetIntPref("media.peerconnection.capture_delay", &mCaptureDelay);
    }
  }

  condError = StartTransmitting();
  if (condError != kMediaConduitNoError) {
    return condError;
  }

  {
    MutexAutoLock lock(mCodecMutex);

    //Copy the applied config for future reference.
    mCurSendCodecConfig = new AudioCodecConfig(codecConfig->mType,
                                               codecConfig->mName,
                                               codecConfig->mFreq,
                                               codecConfig->mPacSize,
                                               codecConfig->mChannels,
                                               codecConfig->mRate,
                                               codecConfig->mFECEnabled);
  }
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::ConfigureRecvMediaCodecs(
                    const std::vector<AudioCodecConfig*>& codecConfigList)
{
  CSFLogDebug(LOGTAG,  "%s ", __FUNCTION__);
  MediaConduitErrorCode condError = kMediaConduitNoError;
  int error = 0; //webrtc engine errors
  bool success = false;

  // Are we receiving already? If so, stop receiving and playout
  // since we can't apply new recv codec when the engine is playing.
  condError = StopReceiving();
  if (condError != kMediaConduitNoError) {
    return condError;
  }

  if(codecConfigList.empty())
  {
    CSFLogError(LOGTAG, "%s Zero number of codecs to configure", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  // Try Applying the codecs in the list.
  // We succeed if at least one codec was applied and reception was
  // started successfully.
  for(auto codec : codecConfigList)
  {
    //if the codec param is invalid or diplicate, return error
    if((condError = ValidateCodecConfig(codec,false)) != kMediaConduitNoError)
    {
      return condError;
    }

    webrtc::CodecInst cinst;
    if(!CodecConfigToWebRTCCodec(codec,cinst))
    {
      CSFLogError(LOGTAG,"%s CodecConfig to WebRTC Codec Failed ",__FUNCTION__);
      continue;
    }

    if(mPtrVoECodec->SetRecPayloadType(mChannel,cinst) == -1)
    {
      error = mPtrVoEBase->LastError();
      CSFLogError(LOGTAG,  "%s SetRecvCodec Failed %d ",__FUNCTION__, error);
      continue;
    }
    CSFLogDebug(LOGTAG, "%s Successfully Set RecvCodec %s", __FUNCTION__,
                                        codec->mName.c_str());

    //copy this to local database
    if(!CopyCodecToDB(codec)) {
        CSFLogError(LOGTAG,"%s Unable to updated Codec Database", __FUNCTION__);
        return kMediaConduitUnknownError;
    }
    success = true;

  } //end for

  if(!success)
  {
    CSFLogError(LOGTAG, "%s Setting Receive Codec Failed ", __FUNCTION__);
    return kMediaConduitInvalidReceiveCodec;
  }

  //If we are here, atleast one codec should have been set
  condError = StartReceiving();
  if (condError != kMediaConduitNoError) {
    return condError;
  }

  DumpCodecDB();
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::EnableAudioLevelExtension(bool aEnabled,
                                              uint8_t aId,
                                              bool aDirectionIsSend)
{
  CSFLogDebug(LOGTAG,  "%s %d %d %d", __FUNCTION__, aEnabled, aId,
              aDirectionIsSend);

  bool ret;
  if (aDirectionIsSend) {
    ret = mPtrVoERTP_RTCP->SetSendAudioLevelIndicationStatus(mChannel,
                                                             aEnabled,
                                                             aId) == -1;
  } else {
    ret = mPtrRTP->SetReceiveAudioLevelIndicationStatus(mChannel,
                                                        aEnabled,
                                                        aId) == -1;
  }
  if (ret) {
    CSFLogError(LOGTAG, "%s SetSendAudioLevelIndicationStatus Failed", __FUNCTION__);
    return kMediaConduitUnknownError;
  }
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::EnableMIDExtension(bool enabled, uint8_t id)
{
  CSFLogDebug(LOGTAG,  "%s %d %d ", __FUNCTION__, enabled, id);

  if (mPtrVoERTP_RTCP->SetSendMIDStatus(mChannel, enabled, id) == -1)
  {
    CSFLogError(LOGTAG, "%s SetSendMIDStatus Failed", __FUNCTION__);
    return kMediaConduitUnknownError;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::SendAudioFrame(const int16_t audio_data[],
                                   int32_t lengthSamples, // per channel
                                   int32_t samplingFreqHz,
                                   uint32_t channels,
                                   int32_t capture_delay)
{
  CSFLogDebug(LOGTAG,  "%s ", __FUNCTION__);
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
    CSFLogError(LOGTAG, "%s Invalid Parameters ",__FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  //validate capture time
  if(capture_delay < 0 )
  {
    CSFLogError(LOGTAG,"%s Invalid Capture Delay ", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  // if transmission is not started .. conduit cannot insert frames
  if(!mEngineTransmitting)
  {
    CSFLogError(LOGTAG, "%s Engine not transmitting ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  if (MOZ_LOG_TEST(GetLatencyLog(), LogLevel::Debug)) {
    struct Processing insert = { TimeStamp::Now(), 0 };
    mProcessing.AppendElement(insert);
  }

  capture_delay = mCaptureDelay;
  // Insert the samples
  mPtrVoEBase->audio_transport()->PushCaptureData(mChannel, audio_data,
                                                  sizeof(audio_data[0])*8, // bits
                                                  samplingFreqHz,
                                                  channels,
                                                  lengthSamples);
  // we should be good here
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::GetAudioFrame(int16_t speechData[],
                                  int32_t samplingFreqHz,
                                  int32_t capture_delay,
                                  int& lengthSamples)
{

  CSFLogDebug(LOGTAG,  "%s ", __FUNCTION__);

  //validate params
  if(!speechData )
  {
    CSFLogError(LOGTAG,"%s Null Audio Buffer Pointer", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  // Validate sample length
  if(GetNum10msSamplesForFrequency(samplingFreqHz) == 0)
  {
    CSFLogError(LOGTAG,"%s Invalid Sampling Frequency ", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  //validate capture time
  if(capture_delay < 0 )
  {
    CSFLogError(LOGTAG,"%s Invalid Capture Delay ", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  //Conduit should have reception enabled before we ask for decoded
  // samples
  if(!mEngineReceiving)
  {
    CSFLogError(LOGTAG, "%s Engine not Receiving ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  int lengthSamplesAllowed = lengthSamples;
  lengthSamples = 0;  //output paramter

  if (mPtrVoEXmedia->GetAudioFrame(mChannel,
                                   samplingFreqHz,
                                   &mAudioFrame) != 0) {
    int error = mPtrVoEBase->LastError();
    CSFLogError(LOGTAG,  "%s Getting audio data Failed %d", __FUNCTION__, error);
    if(error == VE_RUNTIME_PLAY_ERROR)
    {
      return kMediaConduitPlayoutError;
    }
    return kMediaConduitUnknownError;
  }

  // XXX Annoying, have to copy to our buffers -- refactor?
  lengthSamples = mAudioFrame.samples_per_channel_ * mAudioFrame.num_channels_;
  MOZ_RELEASE_ASSERT(lengthSamples <= lengthSamplesAllowed);
  PodCopy(speechData, mAudioFrame.data_, lengthSamples);

  // Not #ifdef DEBUG or on a log module so we can use it for about:webrtc/etc
  mSamples += lengthSamples;
  if (mSamples >= mLastSyncLog + samplingFreqHz) {
    int jitter_buffer_delay_ms;
    int playout_buffer_delay_ms;
    int avsync_offset_ms;
    if (GetAVStats(&jitter_buffer_delay_ms,
                   &playout_buffer_delay_ms,
                   &avsync_offset_ms)) {
      if (avsync_offset_ms < 0) {
        Telemetry::Accumulate(Telemetry::WEBRTC_AVSYNC_WHEN_VIDEO_LAGS_AUDIO_MS,
                              -avsync_offset_ms);
      } else {
        Telemetry::Accumulate(Telemetry::WEBRTC_AVSYNC_WHEN_AUDIO_LAGS_VIDEO_MS,
                              avsync_offset_ms);
      }
      CSFLogError(LOGTAG,
                  "A/V sync: sync delta: %dms, audio jitter delay %dms, playout delay %dms",
                  avsync_offset_ms, jitter_buffer_delay_ms, playout_buffer_delay_ms);
    } else {
      CSFLogError(LOGTAG, "A/V sync: GetAVStats failed");
    }
    mLastSyncLog = mSamples;
  }

  if (MOZ_LOG_TEST(GetLatencyLog(), LogLevel::Debug)) {
    if (mProcessing.Length() > 0) {
      unsigned int now;
      mPtrVoEVideoSync->GetPlayoutTimestamp(mChannel, now);
      if (static_cast<uint32_t>(now) != mLastTimestamp) {
        mLastTimestamp = static_cast<uint32_t>(now);
        // Find the block that includes this timestamp in the network input
        while (mProcessing.Length() > 0) {
          // FIX! assumes 20ms @ 48000Hz
          // FIX handle wrap-around
          if (mProcessing[0].mRTPTimeStamp + 20*(48000/1000) >= now) {
            TimeDuration t = TimeStamp::Now() - mProcessing[0].mTimeStamp;
            // Wrap-around?
            int64_t delta = t.ToMilliseconds() + (now - mProcessing[0].mRTPTimeStamp)/(48000/1000);
            LogTime(AsyncLatencyLogger::AudioRecvRTP, ((uint64_t) this), delta);
            break;
          }
          mProcessing.RemoveElementAt(0);
        }
      }
    }
  }
  CSFLogDebug(LOGTAG,"%s GetAudioFrame:Got samples: length %d ",__FUNCTION__,
                                                               lengthSamples);
  return kMediaConduitNoError;
}

// Transport Layer Callbacks
MediaConduitErrorCode
WebrtcAudioConduit::ReceivedRTPPacket(const void *data, int len, uint32_t ssrc)
{
  CSFLogDebug(LOGTAG,  "%s : channel %d", __FUNCTION__, mChannel);

  if(mEngineReceiving)
  {
    if (MOZ_LOG_TEST(GetLatencyLog(), LogLevel::Debug)) {
      // timestamp is at 32 bits in ([1])
      struct Processing insert = { TimeStamp::Now(),
                                   ntohl(static_cast<const uint32_t *>(data)[1]) };
      mProcessing.AppendElement(insert);
    }

    // XXX we need to get passed the time the packet was received
    if(mPtrVoENetwork->ReceivedRTPPacket(mChannel, data, len) == -1)
    {
      int error = mPtrVoEBase->LastError();
      CSFLogError(LOGTAG, "%s RTP Processing Error %d", __FUNCTION__, error);
      if(error == VE_RTP_RTCP_MODULE_ERROR)
      {
        return kMediaConduitRTPRTCPModuleError;
      }
      return kMediaConduitUnknownError;
    }
  } else {
    CSFLogError(LOGTAG, "Error: %s when not receiving", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::ReceivedRTCPPacket(const void *data, int len)
{
  CSFLogDebug(LOGTAG,  "%s : channel %d",__FUNCTION__, mChannel);

  if(mPtrVoENetwork->ReceivedRTCPPacket(mChannel, data, len) == -1)
  {
    int error = mPtrVoEBase->LastError();
    CSFLogError(LOGTAG, "%s RTCP Processing Error %d", __FUNCTION__, error);
    if(error == VE_RTP_RTCP_MODULE_ERROR)
    {
      return kMediaConduitRTPRTCPModuleError;
    }
    return kMediaConduitUnknownError;
  }
  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::StopTransmitting()
{
  if(mEngineTransmitting)
  {
    CSFLogDebug(LOGTAG, "%s Engine Already Sending. Attemping to Stop ", __FUNCTION__);
    if(mPtrVoEBase->StopSend(mChannel) == -1)
    {
      CSFLogError(LOGTAG, "%s StopSend() Failed %d ", __FUNCTION__,
                  mPtrVoEBase->LastError());
      return kMediaConduitUnknownError;
    }
    mEngineTransmitting = false;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::StartTransmitting()
{
  if (!mEngineTransmitting) {
    //Let's Send Transport State-machine on the Engine
    if(mPtrVoEBase->StartSend(mChannel) == -1)
    {
      int error = mPtrVoEBase->LastError();
      CSFLogError(LOGTAG, "%s StartSend failed %d", __FUNCTION__, error);
      return kMediaConduitUnknownError;
    }
    mEngineTransmitting = true;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::StopReceiving()
{
  if(mEngineReceiving)
  {
    CSFLogDebug(LOGTAG, "%s Engine Already Receiving. Attemping to Stop ", __FUNCTION__);
    // AudioEngine doesn't fail fatally on stopping reception. Ref:voe_errors.h.
    // hence we need not be strict in failing here on errors
    mPtrVoEBase->StopReceive(mChannel);
    CSFLogDebug(LOGTAG, "%s Attemping to Stop playout ", __FUNCTION__);
    if(mPtrVoEBase->StopPlayout(mChannel) == -1)
    {
      if( mPtrVoEBase->LastError() == VE_CANNOT_STOP_PLAYOUT)
      {
        CSFLogDebug(LOGTAG, "%s Stop-Playout Failed %d", __FUNCTION__, mPtrVoEBase->LastError());
        return kMediaConduitPlayoutError;
      }
    }
    mEngineReceiving = false;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode
WebrtcAudioConduit::StartReceiving()
{
  if (!mEngineReceiving) {
    if(mPtrVoEBase->StartReceive(mChannel) == -1)
    {
      int error = mPtrVoEBase->LastError();
      CSFLogError(LOGTAG ,  "%s StartReceive Failed %d ",__FUNCTION__, error);
      if(error == VE_RECV_SOCKET_ERROR)
      {
        return kMediaConduitSocketError;
      }
      return kMediaConduitUnknownError;
    }

    // we can't call GetAudioFrame() if we don't enable "external" mixing
    if(mPtrVoEXmedia->SetExternalMixing(mChannel, true) == -1)
    {
      CSFLogError(LOGTAG, "%s SetExternalMixing Failed", __FUNCTION__);
      return kMediaConduitPlayoutError;
    }

    if(mPtrVoEBase->StartPlayout(mChannel) == -1)
    {
      CSFLogError(LOGTAG, "%s Starting playout Failed", __FUNCTION__);
      return kMediaConduitPlayoutError;
    }
    mEngineReceiving = true;
  }

  return kMediaConduitNoError;
}

//WebRTC::RTP Callback Implementation
// Called on AudioGUM or MSG thread
bool
WebrtcAudioConduit::SendRtp(const uint8_t* data,
                            size_t len,
                            const webrtc::PacketOptions& options)
{
  CSFLogDebug(LOGTAG,  "%s: len %lu", __FUNCTION__, (unsigned long)len);

  if (MOZ_LOG_TEST(GetLatencyLog(), LogLevel::Debug)) {
    if (mProcessing.Length() > 0) {
      TimeStamp started = mProcessing[0].mTimeStamp;
      mProcessing.RemoveElementAt(0);
      mProcessing.RemoveElementAt(0); // 20ms packetization!  Could automate this by watching sizes
      TimeDuration t = TimeStamp::Now() - started;
      int64_t delta = t.ToMilliseconds();
      LogTime(AsyncLatencyLogger::AudioSendRTP, ((uint64_t) this), delta);
    }
  }
  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // XXX(pkerr) - the PacketOptions are being ignored. This parameter was added along
  // with the Call API update in the webrtc.org codebase.
  // The only field in it is the packet_id, which is used when the header
  // extension for TransportSequenceNumber is being used, which we don't.
  (void)options;
  if(mTransmitterTransport &&
     (mTransmitterTransport->SendRtpPacket(data, len) == NS_OK))
  {
    CSFLogDebug(LOGTAG, "%s Sent RTP Packet ", __FUNCTION__);
    return true;
  }
  CSFLogError(LOGTAG, "%s RTP Packet Send Failed ", __FUNCTION__);
  return false;
}

// Called on WebRTC Process thread and perhaps others
bool
WebrtcAudioConduit::SendRtcp(const uint8_t* data, size_t len)
{
  CSFLogDebug(LOGTAG, "%s : len %lu, first rtcp = %u ",
              __FUNCTION__,
              (unsigned long) len,
              static_cast<unsigned>(data[1]));

  // We come here if we have only one pipeline/conduit setup,
  // such as for unidirectional streams.
  // We also end up here if we are receiving
  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if(mReceiverTransport &&
     mReceiverTransport->SendRtcpPacket(data, len) == NS_OK)
  {
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

bool
WebrtcAudioConduit::CodecConfigToWebRTCCodec(const AudioCodecConfig* codecInfo,
                                              webrtc::CodecInst& cinst)
{
  const unsigned int plNameLength = codecInfo->mName.length();
  memset(&cinst, 0, sizeof(webrtc::CodecInst));
  if(sizeof(cinst.plname) < plNameLength+1)
  {
    CSFLogError(LOGTAG, "%s Payload name buffer capacity mismatch ",
                                                      __FUNCTION__);
    return false;
  }
  memcpy(cinst.plname, codecInfo->mName.c_str(), plNameLength);
  cinst.plname[plNameLength]='\0';
  cinst.pltype   =  codecInfo->mType;
  cinst.rate     =  codecInfo->mRate;
  cinst.pacsize  =  codecInfo->mPacSize;
  cinst.plfreq   =  codecInfo->mFreq;
  if (codecInfo->mName == "G722") {
    // Compensate for G.722 spec error in RFC 1890
    cinst.plfreq = 16000;
  }
  cinst.channels =  codecInfo->mChannels;
  return true;
}

/**
  *  Supported Sampling Frequencies.
  */
bool
WebrtcAudioConduit::IsSamplingFreqSupported(int freq) const
{
  return GetNum10msSamplesForFrequency(freq) != 0;
}

/* Return block-length of 10 ms audio frame in number of samples */
unsigned int
WebrtcAudioConduit::GetNum10msSamplesForFrequency(int samplingFreqHz) const
{
  switch (samplingFreqHz)
  {
    case 16000: return 160; //160 samples
    case 32000: return 320; //320 samples
    case 44100: return 441; //441 samples
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
                                                     codecInfo->mRate,
                                                     codecInfo->mFECEnabled);
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
  for(auto codec : mRecvCodecList)
  {
    if(CheckCodecsForMatch(codec,codecInfo))
    {
      //match
      return true;
    }
  }
  //no match or empty local db
  return false;
}


/**
 * Perform validation on the codecConfig to be applied.
 * Verifies if the codec is already applied.
 */
MediaConduitErrorCode
WebrtcAudioConduit::ValidateCodecConfig(const AudioCodecConfig* codecInfo,
                                        bool send)
{
  bool codecAppliedAlready = false;

  if(!codecInfo)
  {
    CSFLogError(LOGTAG, "%s Null CodecConfig ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  if((codecInfo->mName.empty()) ||
     (codecInfo->mName.length() >= CODEC_PLNAME_SIZE))
  {
    CSFLogError(LOGTAG, "%s Invalid Payload Name Length ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  //Only mono or stereo channels supported
  if( (codecInfo->mChannels != 1) && (codecInfo->mChannels != 2))
  {
    CSFLogError(LOGTAG, "%s Channel Unsupported ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  //check if we have the same codec already applied
  if(send)
  {
    MutexAutoLock lock(mCodecMutex);

    codecAppliedAlready = CheckCodecsForMatch(mCurSendCodecConfig,codecInfo);
  } else {
    codecAppliedAlready = CheckCodecForMatch(codecInfo);
  }

  if(codecAppliedAlready)
  {
    CSFLogDebug(LOGTAG, "%s Codec %s Already Applied  ", __FUNCTION__, codecInfo->mName.c_str());
  }
  return kMediaConduitNoError;
}

void
WebrtcAudioConduit::DumpCodecDB() const
 {
    for(auto& codec : mRecvCodecList)
    {
      CSFLogDebug(LOGTAG,"Payload Name: %s", codec->mName.c_str());
      CSFLogDebug(LOGTAG,"Payload Type: %d", codec->mType);
      CSFLogDebug(LOGTAG,"Payload Frequency: %d", codec->mFreq);
      CSFLogDebug(LOGTAG,"Payload PacketSize: %d", codec->mPacSize);
      CSFLogDebug(LOGTAG,"Payload Channels: %d", codec->mChannels);
      CSFLogDebug(LOGTAG,"Payload Sampling Rate: %d", codec->mRate);
    }
 }
}// end namespace
