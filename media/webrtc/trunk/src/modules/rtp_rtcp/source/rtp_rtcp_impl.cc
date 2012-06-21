/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_types.h"
#include "rtp_rtcp_impl.h"
#include "trace.h"

#ifdef MATLAB
#include "../test/BWEStandAlone/MatlabPlot.h"
extern MatlabEngine eng; // global variable defined elsewhere
#endif

#include <string.h> //memcpy
#include <cassert> //assert

// local for this file
namespace {

const float FracMS = 4.294967296E6f;

}  // namepace

#ifdef _WIN32
// disable warning C4355: 'this' : used in base member initializer list
#pragma warning(disable : 4355)
#endif

namespace webrtc {

const WebRtc_UWord16 kDefaultRtt = 200;

RtpRtcp* RtpRtcp::CreateRtpRtcp(const WebRtc_Word32 id,
                                bool audio) {
  if(audio) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id, "CreateRtpRtcp(audio)");
  } else {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id, "CreateRtpRtcp(video)");
  }
  // ModuleRTPUtility::GetSystemClock() creates a new instance of a system
  // clock implementation. The OwnsClock() function informs the module that
  // it is responsible for deleting the instance.
  ModuleRtpRtcpImpl* rtp_rtcp_instance = new ModuleRtpRtcpImpl(id,
      audio, ModuleRTPUtility::GetSystemClock());
  rtp_rtcp_instance->OwnsClock();
  return rtp_rtcp_instance;
}

RtpRtcp* RtpRtcp::CreateRtpRtcp(const WebRtc_Word32 id,
                                const bool audio,
                                RtpRtcpClock* clock) {
  if (audio) {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id,
                 "CreateRtpRtcp(audio)");
  } else {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id,
                 "CreateRtpRtcp(video)");
  }
  return new ModuleRtpRtcpImpl(id, audio, clock);
}

void RtpRtcp::DestroyRtpRtcp(RtpRtcp* module) {
  if (module) {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 static_cast<ModuleRtpRtcpImpl*>(module)->Id(),
                 "DestroyRtpRtcp()");
    delete static_cast<ModuleRtpRtcpImpl*>(module);
  }
}

ModuleRtpRtcpImpl::ModuleRtpRtcpImpl(const WebRtc_Word32 id,
                                     const bool audio,
                                     RtpRtcpClock* clock):
  _rtpSender(id, audio, clock),
  _rtpReceiver(id, audio, clock, this),
  _rtcpSender(id, audio, clock, this),
  _rtcpReceiver(id, clock, this),
  _owns_clock(false),
  _clock(*clock),
  _id(id),
  _audio(audio),
  _collisionDetected(false),
  _lastProcessTime(clock->GetTimeInMS()),
  _lastBitrateProcessTime(clock->GetTimeInMS()),
  _lastPacketTimeoutProcessTime(clock->GetTimeInMS()),
  _packetOverHead(28), // IPV4 UDP
  _criticalSectionModulePtrs(CriticalSectionWrapper::CreateCriticalSection()),
  _criticalSectionModulePtrsFeedback(
    CriticalSectionWrapper::CreateCriticalSection()),
  _defaultModule(NULL),
  _audioModule(NULL),
  _videoModule(NULL),
  _deadOrAliveActive(false),
  _deadOrAliveTimeoutMS(0),
  _deadOrAliveLastTimer(0),
  _bandwidthManagement(id),
  _receivedNTPsecsAudio(0),
  _receivedNTPfracAudio(0),
  _RTCPArrivalTimeSecsAudio(0),
  _RTCPArrivalTimeFracAudio(0),
  _nackMethod(kNackOff),
  _nackLastTimeSent(0),
  _nackLastSeqNumberSent(0),
  _simulcast(false),
  _keyFrameReqMethod(kKeyFrameReqFirRtp)
#ifdef MATLAB
  , _plot1(NULL)
#endif
{
  _sendVideoCodec.codecType = kVideoCodecUnknown;
  // make sure that RTCP objects are aware of our SSRC
  WebRtc_UWord32 SSRC = _rtpSender.SSRC();
  _rtcpSender.SetSSRC(SSRC);

  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id, "%s created", __FUNCTION__);
}

ModuleRtpRtcpImpl::~ModuleRtpRtcpImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, _id, "%s deleted", __FUNCTION__);

  // make sure to unregister this module from other modules

  const bool defaultInstance(_childModules.empty() ? false : true);

  if (defaultInstance) {
    // deregister for the default module
    // will go in to the child modules and remove it self
    std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
    while (it != _childModules.end()) {
      RtpRtcp* module = *it;
      _childModules.erase(it);
      if (module) {
        module->DeRegisterDefaultModule();
      }
      it = _childModules.begin();
    }
  } else {
    // deregister for the child modules
    // will go in to the default and remove it self
    DeRegisterDefaultModule();
  }

  if (_audio) {
    DeRegisterVideoModule();
  } else {
    DeRegisterSyncModule();
  }

#ifdef MATLAB
  if (_plot1) {
    eng.DeletePlot(_plot1);
    _plot1 = NULL;
  }
#endif

  delete _criticalSectionModulePtrs;
  delete _criticalSectionModulePtrsFeedback;
  if (_owns_clock) {
    delete &_clock;
  }
}

WebRtc_Word32 ModuleRtpRtcpImpl::ChangeUniqueId(const WebRtc_Word32 id) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "ChangeUniqueId(new id:%d)", id);

  _id = id;

  _rtpReceiver.ChangeUniqueId(id);
  _rtcpReceiver.ChangeUniqueId(id);
  _rtpSender.ChangeUniqueId(id);
  _rtcpSender.ChangeUniqueId(id);
  return 0;
}

// default encoder that we need to multiplex out
WebRtc_Word32 ModuleRtpRtcpImpl::RegisterDefaultModule(RtpRtcp* module) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RegisterDefaultModule(module:0x%x)", module);

  if (module == NULL) {
    return -1;
  }
  if (module == this) {
    WEBRTC_TRACE(kTraceError,
                 kTraceRtpRtcp,
                 _id,
                 "RegisterDefaultModule can't register self as default");
    return -1;
  }
  CriticalSectionScoped lock(_criticalSectionModulePtrs);

  if (_defaultModule) {
    _defaultModule->DeRegisterChildModule(this);
  }
  _defaultModule = (ModuleRtpRtcpImpl*)module;
  _defaultModule->RegisterChildModule(this);
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::DeRegisterDefaultModule() {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "DeRegisterDefaultModule()");

  CriticalSectionScoped lock(_criticalSectionModulePtrs);
  if (_defaultModule) {
    _defaultModule->DeRegisterChildModule(this);
    _defaultModule = NULL;
  }
  return 0;
}

bool ModuleRtpRtcpImpl::DefaultModuleRegistered() {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "DefaultModuleRegistered()");

  CriticalSectionScoped lock(_criticalSectionModulePtrs);
  if (_defaultModule) {
    return true;
  }
  return false;
}

WebRtc_UWord32 ModuleRtpRtcpImpl::NumberChildModules() {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "NumberChildModules");

  CriticalSectionScoped lock(_criticalSectionModulePtrs);
  CriticalSectionScoped doubleLock(_criticalSectionModulePtrsFeedback);
  // we use two locks for protecting _childModules one
  // (_criticalSectionModulePtrsFeedback) for incoming  messages
  // (BitrateSent and UpdateTMMBR) and _criticalSectionModulePtrs for
  //  all outgoing messages sending packets etc

  return _childModules.size();
}

void ModuleRtpRtcpImpl::RegisterChildModule(RtpRtcp* module) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RegisterChildModule(module:0x%x)",
               module);

  CriticalSectionScoped lock(_criticalSectionModulePtrs);

  CriticalSectionScoped doubleLock(_criticalSectionModulePtrsFeedback);
  // we use two locks for protecting _childModules one
  // (_criticalSectionModulePtrsFeedback) for incoming
  // messages (BitrateSent) and _criticalSectionModulePtrs
  //  for all outgoing messages sending packets etc
  _childModules.push_back((ModuleRtpRtcpImpl*)module);
}

void ModuleRtpRtcpImpl::DeRegisterChildModule(RtpRtcp* removeModule) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "DeRegisterChildModule(module:0x%x)", removeModule);

  CriticalSectionScoped lock(_criticalSectionModulePtrs);

  CriticalSectionScoped doubleLock(_criticalSectionModulePtrsFeedback);

  std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
  while (it != _childModules.end()) {
    RtpRtcp* module = *it;
    if (module == removeModule) {
      _childModules.erase(it);
      return;
    }
    it++;
  }
}

// Lip-sync between voice-video engine,
WebRtc_Word32 ModuleRtpRtcpImpl::RegisterSyncModule(RtpRtcp* audioModule) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RegisterSyncModule(module:0x%x)",
               audioModule);

  if (audioModule == NULL) {
    return -1;
  }
  if (_audio) {
    return -1;
  }
  CriticalSectionScoped lock(_criticalSectionModulePtrs);
  _audioModule = (ModuleRtpRtcpImpl*)audioModule;
  return _audioModule->RegisterVideoModule(this);
}

WebRtc_Word32 ModuleRtpRtcpImpl::DeRegisterSyncModule() {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "DeRegisterSyncModule()");

  CriticalSectionScoped lock(_criticalSectionModulePtrs);
  if (_audioModule) {
    ModuleRtpRtcpImpl* audioModule = _audioModule;
    _audioModule = NULL;
    _receivedNTPsecsAudio = 0;
    _receivedNTPfracAudio = 0;
    _RTCPArrivalTimeSecsAudio = 0;
    _RTCPArrivalTimeFracAudio = 0;
    audioModule->DeRegisterVideoModule();
  }
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterVideoModule(RtpRtcp* videoModule) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RegisterVideoModule(module:0x%x)",
               videoModule);

  if (videoModule == NULL) {
    return -1;
  }
  if (!_audio) {
    return -1;
  }
  CriticalSectionScoped lock(_criticalSectionModulePtrs);
  _videoModule = (ModuleRtpRtcpImpl*)videoModule;
  return 0;
}

void ModuleRtpRtcpImpl::DeRegisterVideoModule() {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "DeRegisterVideoModule()");

  CriticalSectionScoped lock(_criticalSectionModulePtrs);
  if (_videoModule) {
    ModuleRtpRtcpImpl* videoModule = _videoModule;
    _videoModule = NULL;
    videoModule->DeRegisterSyncModule();
  }
}

// returns the number of milliseconds until the module want a worker thread
// to call Process
WebRtc_Word32 ModuleRtpRtcpImpl::TimeUntilNextProcess() {
  const WebRtc_UWord32 now = _clock.GetTimeInMS();
  return kRtpRtcpMaxIdleTimeProcess - (now - _lastProcessTime);
}

// Process any pending tasks such as timeouts
// non time critical events
WebRtc_Word32 ModuleRtpRtcpImpl::Process() {
  const WebRtc_UWord32 now = _clock.GetTimeInMS();
  _lastProcessTime = now;

  _rtpSender.ProcessSendToNetwork();

  if (now >= _lastPacketTimeoutProcessTime +
      kRtpRtcpPacketTimeoutProcessTimeMs) {
    _rtpReceiver.PacketTimeout();
    _rtcpReceiver.PacketTimeout();
    _lastPacketTimeoutProcessTime = now;
  }

  if (now >= _lastBitrateProcessTime + kRtpRtcpBitrateProcessTimeMs) {
    _rtpSender.ProcessBitrate();
    _rtpReceiver.ProcessBitrate();
    _lastBitrateProcessTime = now;
  }

  ProcessDeadOrAliveTimer();

  const bool defaultInstance(_childModules.empty() ? false : true);
  if (!defaultInstance && _rtcpSender.TimeToSendRTCPReport()) {
    WebRtc_UWord16 max_rtt = 0;
    if (_rtcpSender.Sending()) {
      std::vector<RTCPReportBlock> receive_blocks;
      _rtcpReceiver.StatisticsReceived(&receive_blocks);
      for (std::vector<RTCPReportBlock>::iterator it = receive_blocks.begin();
           it != receive_blocks.end(); ++it) {
        WebRtc_UWord16 rtt = 0;
        _rtcpReceiver.RTT(it->remoteSSRC, &max_rtt, NULL, NULL, NULL);
        max_rtt = (rtt > max_rtt) ? rtt : max_rtt;
      }
    } else {
      // We're only receiving, i.e. this module doesn't have its own RTT
      // estimate. Use the RTT set by a sending channel using the same default
      // module.
      max_rtt = _rtcpReceiver.RTT();
    }
    if (max_rtt == 0) {
      // No valid estimate available, i.e. no sending channel using the same
      // default module or no RTCP received yet.
      max_rtt = kDefaultRtt;
    }
    if (_rtcpSender.ValidBitrateEstimate()) {
      if (REMB()) {
        uint32_t target_bitrate =
            _rtcpSender.CalculateNewTargetBitrate(max_rtt);
        _rtcpSender.UpdateRemoteBitrateEstimate(target_bitrate);
      } else if (TMMBR()) {
        _rtcpSender.CalculateNewTargetBitrate(max_rtt);
      }
    }
    _rtcpSender.SendRTCP(kRtcpReport);
  }

  if (UpdateRTCPReceiveInformationTimers()) {
    // a receiver has timed out
    _rtcpReceiver.UpdateTMMBR();
  }
  return 0;
}

/**
*   Receiver
*/

WebRtc_Word32 ModuleRtpRtcpImpl::InitReceiver() {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "InitReceiver()");

  _packetOverHead = 28; // default is IPV4 UDP
  _receivedNTPsecsAudio = 0;
  _receivedNTPfracAudio = 0;
  _RTCPArrivalTimeSecsAudio = 0;
  _RTCPArrivalTimeFracAudio = 0;

  WebRtc_Word32 ret = _rtpReceiver.Init();
  if (ret < 0) {
    return ret;
  }
  _rtpReceiver.SetPacketOverHead(_packetOverHead);
  return ret;
}

void ModuleRtpRtcpImpl::ProcessDeadOrAliveTimer() {
  if (_deadOrAliveActive) {
    const WebRtc_UWord32 now = _clock.GetTimeInMS();
    if (now > _deadOrAliveTimeoutMS + _deadOrAliveLastTimer) {
      // RTCP is alive if we have received a report the last 12 seconds
      _deadOrAliveLastTimer += _deadOrAliveTimeoutMS;

      bool RTCPalive = false;
      if (_rtcpReceiver.LastReceived() + 12000 > now) {
        RTCPalive = true;
      }
      _rtpReceiver.ProcessDeadOrAlive(RTCPalive, now);
    }
  }
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetPeriodicDeadOrAliveStatus(
  const bool enable,
  const WebRtc_UWord8 sampleTimeSeconds) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 _id,
                 "SetPeriodicDeadOrAliveStatus(enable, %d)",
                 sampleTimeSeconds);
  } else {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 _id,
                 "SetPeriodicDeadOrAliveStatus(disable)");
  }
  if (sampleTimeSeconds == 0) {
    return -1;
  }
  _deadOrAliveActive = enable;
  _deadOrAliveTimeoutMS = sampleTimeSeconds * 1000;
  // trigger the first after one period
  _deadOrAliveLastTimer = _clock.GetTimeInMS();
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::PeriodicDeadOrAliveStatus(
    bool& enable,
    WebRtc_UWord8& sampleTimeSeconds) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "PeriodicDeadOrAliveStatus()");

  enable = _deadOrAliveActive;
  sampleTimeSeconds = (WebRtc_UWord8)(_deadOrAliveTimeoutMS / 1000);
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetPacketTimeout(
    const WebRtc_UWord32 RTPtimeoutMS,
    const WebRtc_UWord32 RTCPtimeoutMS) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SetPacketTimeout(%u,%u)",
               RTPtimeoutMS,
               RTCPtimeoutMS);

  if (_rtpReceiver.SetPacketTimeout(RTPtimeoutMS) == 0) {
    return _rtcpReceiver.SetPacketTimeout(RTCPtimeoutMS);
  }
  return -1;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterReceivePayload(
  const CodecInst& voiceCodec) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RegisterReceivePayload(voiceCodec)");

  return _rtpReceiver.RegisterReceivePayload(
           voiceCodec.plname,
           voiceCodec.pltype,
           voiceCodec.plfreq,
           voiceCodec.channels,
           (voiceCodec.rate < 0) ? 0 : voiceCodec.rate);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterReceivePayload(
  const VideoCodec& videoCodec) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RegisterReceivePayload(videoCodec)");

  return _rtpReceiver.RegisterReceivePayload(videoCodec.plName,
                                             videoCodec.plType,
                                             90000,
                                             0,
                                             videoCodec.maxBitrate);
}

WebRtc_Word32 ModuleRtpRtcpImpl::ReceivePayloadType(
  const CodecInst& voiceCodec,
  WebRtc_Word8* plType) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "ReceivePayloadType(voiceCodec)");

  return _rtpReceiver.ReceivePayloadType(
           voiceCodec.plname,
           voiceCodec.plfreq,
           voiceCodec.channels,
           (voiceCodec.rate < 0) ? 0 : voiceCodec.rate,
           plType);
}

WebRtc_Word32 ModuleRtpRtcpImpl::ReceivePayloadType(
  const VideoCodec& videoCodec,
  WebRtc_Word8* plType) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "ReceivePayloadType(videoCodec)");

  return _rtpReceiver.ReceivePayloadType(videoCodec.plName,
                                         90000,
                                         0,
                                         videoCodec.maxBitrate,
                                         plType);
}

WebRtc_Word32 ModuleRtpRtcpImpl::DeRegisterReceivePayload(
    const WebRtc_Word8 payloadType) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "DeRegisterReceivePayload(%d)",
               payloadType);

  return _rtpReceiver.DeRegisterReceivePayload(payloadType);
}

// get the currently configured SSRC filter
WebRtc_Word32 ModuleRtpRtcpImpl::SSRCFilter(WebRtc_UWord32& allowedSSRC) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SSRCFilter()");

  return _rtpReceiver.SSRCFilter(allowedSSRC);
}

// set a SSRC to be used as a filter for incoming RTP streams
WebRtc_Word32 ModuleRtpRtcpImpl::SetSSRCFilter(
  const bool enable,
  const WebRtc_UWord32 allowedSSRC) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 _id,
                 "SetSSRCFilter(enable, 0x%x)",
                 allowedSSRC);
  } else {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 _id,
                 "SetSSRCFilter(disable)");
  }

  return _rtpReceiver.SetSSRCFilter(enable, allowedSSRC);
}

// Get last received remote timestamp
WebRtc_UWord32 ModuleRtpRtcpImpl::RemoteTimestamp() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteTimestamp()");

  return _rtpReceiver.TimeStamp();
}

// Get the current estimated remote timestamp
WebRtc_Word32 ModuleRtpRtcpImpl::EstimatedRemoteTimeStamp(
    WebRtc_UWord32& timestamp) const {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "EstimatedRemoteTimeStamp()");

  return _rtpReceiver.EstimatedRemoteTimeStamp(timestamp);
}

// Get incoming SSRC
WebRtc_UWord32 ModuleRtpRtcpImpl::RemoteSSRC() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteSSRC()");

  return _rtpReceiver.SSRC();
}

// Get remote CSRC
WebRtc_Word32 ModuleRtpRtcpImpl::RemoteCSRCs(
    WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteCSRCs()");

  return _rtpReceiver.CSRCs(arrOfCSRC);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetRTXSendStatus(
  const bool enable,
  const bool setSSRC,
  const WebRtc_UWord32 SSRC) {
  _rtpSender.SetRTXStatus(enable, setSSRC, SSRC);
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RTXSendStatus(bool* enable,
                                               WebRtc_UWord32* SSRC) const {
  _rtpSender.RTXStatus(enable, SSRC);
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetRTXReceiveStatus(
  const bool enable,
  const WebRtc_UWord32 SSRC) {
  _rtpReceiver.SetRTXStatus(enable, SSRC);
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RTXReceiveStatus(bool* enable,
                                                  WebRtc_UWord32* SSRC) const {
  _rtpReceiver.RTXStatus(enable, SSRC);
  return 0;
}

// called by the network module when we receive a packet
WebRtc_Word32 ModuleRtpRtcpImpl::IncomingPacket(
    const WebRtc_UWord8* incomingPacket,
    const WebRtc_UWord16 incomingPacketLength) {
  WEBRTC_TRACE(kTraceStream,
               kTraceRtpRtcp,
               _id,
               "IncomingPacket(packetLength:%u)",
               incomingPacketLength);

  // minimum RTP is 12 bytes
  // minimum RTCP is 8 bytes (RTCP BYE)
  if (incomingPacketLength < 8 || incomingPacket == NULL) {
    WEBRTC_TRACE(kTraceDebug,
                 kTraceRtpRtcp,
                 _id,
                 "IncomingPacket invalid buffer or length");
    return -1;
  }
  // check RTP version
  const WebRtc_UWord8  version  = incomingPacket[0] >> 6 ;
  if (version != 2) {
    WEBRTC_TRACE(kTraceDebug,
                 kTraceRtpRtcp,
                 _id,
                 "IncomingPacket invalid RTP version");
    return -1;
  }

  ModuleRTPUtility::RTPHeaderParser rtpParser(incomingPacket,
                                              incomingPacketLength);

  if (rtpParser.RTCP()) {
    // Allow receive of non-compound RTCP packets.
    RTCPUtility::RTCPParserV2 rtcpParser(incomingPacket,
                                         incomingPacketLength,
                                         true);

    const bool validRTCPHeader = rtcpParser.IsValid();
    if (!validRTCPHeader) {
      WEBRTC_TRACE(kTraceDebug,
                   kTraceRtpRtcp,
                   _id,
                   "IncomingPacket invalid RTCP packet");
      return -1;
    }
    RTCPHelp::RTCPPacketInformation rtcpPacketInformation;
    WebRtc_Word32 retVal = _rtcpReceiver.IncomingRTCPPacket(
                             rtcpPacketInformation,
                             &rtcpParser);
    if (retVal == 0) {
      _rtcpReceiver.TriggerCallbacksFromRTCPPacket(rtcpPacketInformation);
    }
    return retVal;

  } else {
    WebRtcRTPHeader rtpHeader;
    memset(&rtpHeader, 0, sizeof(rtpHeader));

    RtpHeaderExtensionMap map;
    _rtpReceiver.GetHeaderExtensionMapCopy(&map);

    const bool validRTPHeader = rtpParser.Parse(rtpHeader, &map);
    if (!validRTPHeader) {
      WEBRTC_TRACE(kTraceDebug,
                   kTraceRtpRtcp,
                   _id,
                   "IncomingPacket invalid RTP header");
      return -1;
    }
    return _rtpReceiver.IncomingRTPPacket(&rtpHeader,
                                          incomingPacket,
                                          incomingPacketLength);
  }
}

WebRtc_Word32 ModuleRtpRtcpImpl::IncomingAudioNTP(
  const WebRtc_UWord32 audioReceivedNTPsecs,
  const WebRtc_UWord32 audioReceivedNTPfrac,
  const WebRtc_UWord32 audioRTCPArrivalTimeSecs,
  const WebRtc_UWord32 audioRTCPArrivalTimeFrac) {
  _receivedNTPsecsAudio = audioReceivedNTPsecs;
  _receivedNTPfracAudio = audioReceivedNTPfrac;
  _RTCPArrivalTimeSecsAudio = audioRTCPArrivalTimeSecs;
  _RTCPArrivalTimeFracAudio = audioRTCPArrivalTimeFrac;
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterIncomingDataCallback(
  RtpData* incomingDataCallback) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RegisterIncomingDataCallback(incomingDataCallback:0x%x)",
               incomingDataCallback);

  return _rtpReceiver.RegisterIncomingDataCallback(incomingDataCallback);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterIncomingRTPCallback(
  RtpFeedback* incomingMessagesCallback) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RegisterIncomingRTPCallback(incomingMessagesCallback:0x%x)",
               incomingMessagesCallback);

  return _rtpReceiver.RegisterIncomingRTPCallback(incomingMessagesCallback);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterIncomingRTCPCallback(
  RtcpFeedback* incomingMessagesCallback) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RegisterIncomingRTCPCallback(incomingMessagesCallback:0x%x)",
               incomingMessagesCallback);

  return _rtcpReceiver.RegisterIncomingRTCPCallback(incomingMessagesCallback);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterIncomingVideoCallback(
  RtpVideoFeedback* incomingMessagesCallback) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RegisterIncomingVideoCallback(incomingMessagesCallback:0x%x)",
               incomingMessagesCallback);

  if (_rtcpReceiver.RegisterIncomingVideoCallback(incomingMessagesCallback)
      == 0) {
    return _rtpReceiver.RegisterIncomingVideoCallback(
             incomingMessagesCallback);
  }
  return -1;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterAudioCallback(
  RtpAudioFeedback* messagesCallback) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RegisterAudioCallback(messagesCallback:0x%x)",
               messagesCallback);

  if (_rtpSender.RegisterAudioCallback(messagesCallback) == 0) {
    return _rtpReceiver.RegisterIncomingAudioCallback(messagesCallback);
  }
  return -1;
}

/**
*   Sender
*/

WebRtc_Word32 ModuleRtpRtcpImpl::InitSender() {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "InitSender()");

  _collisionDetected = false;

  // if we are already receiving inform our sender to avoid collision
  if (_rtpSender.Init(_rtpReceiver.SSRC()) != 0) {
    return -1;
  }
  WebRtc_Word32 retVal = _rtcpSender.Init();

  // make sure that RTCP objects are aware of our SSRC
  // (it could have changed due to collision)
  WebRtc_UWord32 SSRC = _rtpSender.SSRC();
  _rtcpReceiver.SetSSRC(SSRC);
  _rtcpSender.SetSSRC(SSRC);
  return retVal;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterSendPayload(
  const CodecInst& voiceCodec) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RegisterSendPayload(plName:%s plType:%d frequency:%u)",
               voiceCodec.plname,
               voiceCodec.pltype,
               voiceCodec.plfreq);

  return _rtpSender.RegisterPayload(
           voiceCodec.plname,
           voiceCodec.pltype,
           voiceCodec.plfreq,
           voiceCodec.channels,
           (voiceCodec.rate < 0) ? 0 : voiceCodec.rate);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterSendPayload(
  const VideoCodec& videoCodec) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RegisterSendPayload(plName:%s plType:%d)",
               videoCodec.plName,
               videoCodec.plType);

  _sendVideoCodec = videoCodec;
  _simulcast = (videoCodec.numberOfSimulcastStreams > 1) ? true : false;
  return _rtpSender.RegisterPayload(videoCodec.plName,
                                    videoCodec.plType,
                                    90000,
                                    0,
                                    videoCodec.maxBitrate);
}

WebRtc_Word32 ModuleRtpRtcpImpl::DeRegisterSendPayload(
    const WebRtc_Word8 payloadType) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "DeRegisterSendPayload(%d)", payloadType);

  return _rtpSender.DeRegisterSendPayload(payloadType);
}

WebRtc_Word8 ModuleRtpRtcpImpl::SendPayloadType() const {
  return _rtpSender.SendPayloadType();
}

WebRtc_UWord32 ModuleRtpRtcpImpl::StartTimestamp() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "StartTimestamp()");

  return _rtpSender.StartTimestamp();
}

// configure start timestamp, default is a random number
WebRtc_Word32 ModuleRtpRtcpImpl::SetStartTimestamp(
    const WebRtc_UWord32 timestamp) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SetStartTimestamp(%d)",
               timestamp);

  return _rtpSender.SetStartTimestamp(timestamp, true);
}

WebRtc_UWord16 ModuleRtpRtcpImpl::SequenceNumber() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SequenceNumber()");

  return _rtpSender.SequenceNumber();
}

// Set SequenceNumber, default is a random number
WebRtc_Word32 ModuleRtpRtcpImpl::SetSequenceNumber(
    const WebRtc_UWord16 seqNum) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SetSequenceNumber(%d)",
               seqNum);

  return _rtpSender.SetSequenceNumber(seqNum);
}

WebRtc_UWord32 ModuleRtpRtcpImpl::SSRC() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SSRC()");

  return _rtpSender.SSRC();
}

// configure SSRC, default is a random number
WebRtc_Word32 ModuleRtpRtcpImpl::SetSSRC(const WebRtc_UWord32 ssrc) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetSSRC(%d)", ssrc);

  if (_rtpSender.SetSSRC(ssrc) == 0) {
    _rtcpReceiver.SetSSRC(ssrc);
    _rtcpSender.SetSSRC(ssrc);
    return 0;
  }
  return -1;
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetCSRCStatus(const bool include) {
  _rtcpSender.SetCSRCStatus(include);
  return _rtpSender.SetCSRCStatus(include);
}

WebRtc_Word32 ModuleRtpRtcpImpl::CSRCs(
    WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "CSRCs()");

  return _rtpSender.CSRCs(arrOfCSRC);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetCSRCs(
    const WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize],
    const WebRtc_UWord8 arrLength) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SetCSRCs(arrLength:%d)",
               arrLength);

  const bool defaultInstance(_childModules.empty() ? false : true);

  if (defaultInstance) {
    // for default we need to update all child modules too
    CriticalSectionScoped lock(_criticalSectionModulePtrs);

    std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
    while (it != _childModules.end()) {
      RtpRtcp* module = *it;
      if (module) {
        module->SetCSRCs(arrOfCSRC, arrLength);
      }
      it++;
    }
    return 0;

  } else {
    for (int i = 0; i < arrLength; i++) {
      WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "\tidx:%d CSRC:%u", i,
                   arrOfCSRC[i]);
    }
    _rtcpSender.SetCSRCs(arrOfCSRC, arrLength);
    return _rtpSender.SetCSRCs(arrOfCSRC, arrLength);
  }
}

WebRtc_UWord32 ModuleRtpRtcpImpl::PacketCountSent() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "PacketCountSent()");

  return _rtpSender.Packets();
}

WebRtc_UWord32 ModuleRtpRtcpImpl::ByteCountSent() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "ByteCountSent()");

  return _rtpSender.Bytes();
}

int ModuleRtpRtcpImpl::CurrentSendFrequencyHz() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
               "CurrentSendFrequencyHz()");

  return _rtpSender.SendPayloadFrequency();
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetSendingStatus(const bool sending) {
  if (sending) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
                 "SetSendingStatus(sending)");
  } else {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
                 "SetSendingStatus(stopped)");
  }
  if (_rtcpSender.Sending() != sending) {
    // sends RTCP BYE when going from true to false
    if (_rtcpSender.SetSendingStatus(sending) != 0) {
      WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                   "Failed to send RTCP BYE");
    }

    _collisionDetected = false;

    // generate a new timeStamp if true and not configured via API
    // generate a new SSRC for the next "call" if false
    _rtpSender.SetSendingStatus(sending);

    // make sure that RTCP objects are aware of our SSRC (it could have changed
    // due to collision)
    WebRtc_UWord32 SSRC = _rtpSender.SSRC();
    _rtcpReceiver.SetSSRC(SSRC);
    _rtcpSender.SetSSRC(SSRC);
    return 0;
  }
  return 0;
}

bool ModuleRtpRtcpImpl::Sending() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "Sending()");

  return _rtcpSender.Sending();
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetSendingMediaStatus(const bool sending) {
  if (sending) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
                 "SetSendingMediaStatus(sending)");
  } else {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
                 "SetSendingMediaStatus(stopped)");
  }
  _rtpSender.SetSendingMediaStatus(sending);
  return 0;
}

bool ModuleRtpRtcpImpl::SendingMedia() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "Sending()");

  const bool haveChildModules(_childModules.empty() ? false : true);
  if (!haveChildModules) {
    return _rtpSender.SendingMedia();
  }

  CriticalSectionScoped lock(_criticalSectionModulePtrs);
  std::list<ModuleRtpRtcpImpl*>::const_iterator it = _childModules.begin();
  while (it != _childModules.end()) {
    RTPSender& rtpSender = (*it)->_rtpSender;
    if (rtpSender.SendingMedia()) {
      return true;
    }
    it++;
  }
  return false;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterSendTransport(
  Transport* outgoingTransport) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RegisterSendTransport(0x%x)", outgoingTransport);

  if (_rtpSender.RegisterSendTransport(outgoingTransport) == 0) {
    return _rtcpSender.RegisterSendTransport(outgoingTransport);
  }
  return -1;
}

WebRtc_Word32 ModuleRtpRtcpImpl::SendOutgoingData(
    FrameType frameType,
    WebRtc_Word8 payloadType,
    WebRtc_UWord32 timeStamp,
    const WebRtc_UWord8* payloadData,
    WebRtc_UWord32 payloadSize,
    const RTPFragmentationHeader* fragmentation,
    const RTPVideoHeader* rtpVideoHdr) {
  WEBRTC_TRACE(
    kTraceStream,
    kTraceRtpRtcp,
    _id,
    "SendOutgoingData(frameType:%d payloadType:%d timeStamp:%u size:%u)",
    frameType, payloadType, timeStamp, payloadSize);

  const bool haveChildModules(_childModules.empty() ? false : true);
  if (!haveChildModules) {
    // Don't sent RTCP from default module
    if (_rtcpSender.TimeToSendRTCPReport(kVideoFrameKey == frameType)) {
      _rtcpSender.SendRTCP(kRtcpReport);
    }
    return _rtpSender.SendOutgoingData(frameType,
                                       payloadType,
                                       timeStamp,
                                       payloadData,
                                       payloadSize,
                                       fragmentation,
                                       NULL,
                                       &(rtpVideoHdr->codecHeader));
  }
  WebRtc_Word32 retVal = -1;
  if (_simulcast) {
    if (rtpVideoHdr == NULL) {
      return -1;
    }
    int idx = 0;
    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
    for (; idx < rtpVideoHdr->simulcastIdx; idx++) {
      it++;
      if (it == _childModules.end()) {
        return -1;
      }
    }
    RTPSender& rtpSender = (*it)->_rtpSender;
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 _id,
                 "SendOutgoingData(SimulcastIdx:%u size:%u, ssrc:0x%x)",
                 idx, payloadSize, rtpSender.SSRC());
    return rtpSender.SendOutgoingData(frameType,
                                      payloadType,
                                      timeStamp,
                                      payloadData,
                                      payloadSize,
                                      fragmentation,
                                      NULL,
                                      &(rtpVideoHdr->codecHeader));
  } else {
    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    // TODO(pwestin) remove codecInfo from SendOutgoingData
    VideoCodecInformation* codecInfo = NULL;

    std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
    if (it != _childModules.end()) {
      RTPSender& rtpSender = (*it)->_rtpSender;
      retVal = rtpSender.SendOutgoingData(frameType,
                                          payloadType,
                                          timeStamp,
                                          payloadData,
                                          payloadSize,
                                          fragmentation,
                                          NULL,
                                          &(rtpVideoHdr->codecHeader));

      it++;
    }

    // send to all remaining "child" modules
    while (it != _childModules.end()) {
      RTPSender& rtpSender = (*it)->_rtpSender;
      retVal = rtpSender.SendOutgoingData(frameType,
                                          payloadType,
                                          timeStamp,
                                          payloadData,
                                          payloadSize,
                                          fragmentation,
                                          codecInfo,
                                          &(rtpVideoHdr->codecHeader));

      it++;
    }
  }
  return retVal;
}

WebRtc_UWord16 ModuleRtpRtcpImpl::MaxPayloadLength() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "MaxPayloadLength()");

  return _rtpSender.MaxPayloadLength();
}

WebRtc_UWord16 ModuleRtpRtcpImpl::MaxDataPayloadLength() const {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "MaxDataPayloadLength()");

  WebRtc_UWord16 minDataPayloadLength = IP_PACKET_SIZE - 28; // Assuming IP/UDP

  const bool defaultInstance(_childModules.empty() ? false : true);
  if (defaultInstance) {
    // for default we need to update all child modules too
    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    std::list<ModuleRtpRtcpImpl*>::const_iterator it =
      _childModules.begin();
    while (it != _childModules.end()) {
      RtpRtcp* module = *it;
      if (module) {
        WebRtc_UWord16 dataPayloadLength =
          module->MaxDataPayloadLength();
        if (dataPayloadLength < minDataPayloadLength) {
          minDataPayloadLength = dataPayloadLength;
        }
      }
      it++;
    }
  }

  WebRtc_UWord16 dataPayloadLength = _rtpSender.MaxDataPayloadLength();
  if (dataPayloadLength < minDataPayloadLength) {
    minDataPayloadLength = dataPayloadLength;
  }
  return minDataPayloadLength;
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetTransportOverhead(
  const bool TCP,
  const bool IPV6,
  const WebRtc_UWord8 authenticationOverhead) {
  WEBRTC_TRACE(
    kTraceModuleCall,
    kTraceRtpRtcp,
    _id,
    "SetTransportOverhead(TCP:%d, IPV6:%d authenticationOverhead:%u)",
    TCP, IPV6, authenticationOverhead);

  WebRtc_UWord16 packetOverHead = 0;
  if (IPV6) {
    packetOverHead = 40;
  } else {
    packetOverHead = 20;
  }
  if (TCP) {
    // TCP
    packetOverHead += 20;
  } else {
    // UDP
    packetOverHead += 8;
  }
  packetOverHead += authenticationOverhead;

  if (packetOverHead == _packetOverHead) {
    // ok same as before
    return 0;
  }
  // calc diff
  WebRtc_Word16 packetOverHeadDiff = packetOverHead - _packetOverHead;

  // store new
  _packetOverHead = packetOverHead;

  _rtpReceiver.SetPacketOverHead(_packetOverHead);
  WebRtc_UWord16 length = _rtpSender.MaxPayloadLength() - packetOverHeadDiff;
  return _rtpSender.SetMaxPayloadLength(length, _packetOverHead);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetMaxTransferUnit(const WebRtc_UWord16 MTU) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetMaxTransferUnit(%u)",
               MTU);

  if (MTU > IP_PACKET_SIZE) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                 "Invalid in argument to SetMaxTransferUnit(%u)", MTU);
    return -1;
  }
  return _rtpSender.SetMaxPayloadLength(MTU - _packetOverHead,
                                        _packetOverHead);
}

/*
*   RTCP
*/
RTCPMethod ModuleRtpRtcpImpl::RTCP() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RTCP()");

  if (_rtcpSender.Status() != kRtcpOff) {
    return _rtcpReceiver.Status();
  }
  return kRtcpOff;
}

// configure RTCP status i.e on/off
WebRtc_Word32 ModuleRtpRtcpImpl::SetRTCPStatus(const RTCPMethod method) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetRTCPStatus(%d)",
               method);

  if (_rtcpSender.SetRTCPStatus(method) == 0) {
    return _rtcpReceiver.SetRTCPStatus(method);
  }
  return -1;
}

// only for internal test
WebRtc_UWord32 ModuleRtpRtcpImpl::LastSendReport(WebRtc_UWord32& lastRTCPTime) {
  return _rtcpSender.LastSendReport(lastRTCPTime);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetCNAME(const char cName[RTCP_CNAME_SIZE]) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetCNAME(%s)", cName);
  return _rtcpSender.SetCNAME(cName);
}

WebRtc_Word32 ModuleRtpRtcpImpl::CNAME(char cName[RTCP_CNAME_SIZE]) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "CNAME()");
  return _rtcpSender.CNAME(cName);
}

WebRtc_Word32 ModuleRtpRtcpImpl::AddMixedCNAME(
  const WebRtc_UWord32 SSRC,
  const char cName[RTCP_CNAME_SIZE]) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
               "AddMixedCNAME(SSRC:%u)", SSRC);

  return _rtcpSender.AddMixedCNAME(SSRC, cName);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RemoveMixedCNAME(const WebRtc_UWord32 SSRC) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
               "RemoveMixedCNAME(SSRC:%u)", SSRC);
  return _rtcpSender.RemoveMixedCNAME(SSRC);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RemoteCNAME(
  const WebRtc_UWord32 remoteSSRC,
  char cName[RTCP_CNAME_SIZE]) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
               "RemoteCNAME(SSRC:%u)", remoteSSRC);

  return _rtcpReceiver.CNAME(remoteSSRC, cName);
}

WebRtc_UWord16 ModuleRtpRtcpImpl::RemoteSequenceNumber() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteSequenceNumber()");

  return _rtpReceiver.SequenceNumber();
}

WebRtc_Word32 ModuleRtpRtcpImpl::RemoteNTP(
    WebRtc_UWord32* receivedNTPsecs,
    WebRtc_UWord32* receivedNTPfrac,
    WebRtc_UWord32* RTCPArrivalTimeSecs,
    WebRtc_UWord32* RTCPArrivalTimeFrac) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteNTP()");

  return _rtcpReceiver.NTP(receivedNTPsecs,
                           receivedNTPfrac,
                           RTCPArrivalTimeSecs,
                           RTCPArrivalTimeFrac);
}

// Get RoundTripTime
WebRtc_Word32 ModuleRtpRtcpImpl::RTT(const WebRtc_UWord32 remoteSSRC,
                                     WebRtc_UWord16* RTT,
                                     WebRtc_UWord16* avgRTT,
                                     WebRtc_UWord16* minRTT,
                                     WebRtc_UWord16* maxRTT) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RTT()");

  return _rtcpReceiver.RTT(remoteSSRC, RTT, avgRTT, minRTT, maxRTT);
}

// Reset RoundTripTime statistics
WebRtc_Word32
ModuleRtpRtcpImpl::ResetRTT(const WebRtc_UWord32 remoteSSRC) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "ResetRTT(SSRC:%u)",
               remoteSSRC);

  return _rtcpReceiver.ResetRTT(remoteSSRC);
}

// Reset RTP statistics
WebRtc_Word32
ModuleRtpRtcpImpl::ResetStatisticsRTP() {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "ResetStatisticsRTP()");

  return _rtpReceiver.ResetStatistics();
}

// Reset RTP data counters for the receiving side
WebRtc_Word32 ModuleRtpRtcpImpl::ResetReceiveDataCountersRTP() {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
               "ResetReceiveDataCountersRTP()");

  return _rtpReceiver.ResetDataCounters();
}

// Reset RTP data counters for the sending side
WebRtc_Word32 ModuleRtpRtcpImpl::ResetSendDataCountersRTP() {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
               "ResetSendDataCountersRTP()");

  return _rtpSender.ResetDataCounters();
}

// Force a send of an RTCP packet
// normal SR and RR are triggered via the process function
WebRtc_Word32 ModuleRtpRtcpImpl::SendRTCP(WebRtc_UWord32 rtcpPacketType) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SendRTCP(0x%x)",
               rtcpPacketType);

  return  _rtcpSender.SendRTCP(rtcpPacketType);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetRTCPApplicationSpecificData(
    const WebRtc_UWord8 subType,
    const WebRtc_UWord32 name,
    const WebRtc_UWord8* data,
    const WebRtc_UWord16 length) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
               "SetRTCPApplicationSpecificData(subType:%d name:0x%x)", subType,
               name);

  return  _rtcpSender.SetApplicationSpecificData(subType, name, data, length);
}

/*
*   (XR) VOIP metric
*/
WebRtc_Word32 ModuleRtpRtcpImpl::SetRTCPVoIPMetrics(
    const RTCPVoIPMetric* VoIPMetric) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetRTCPVoIPMetrics()");

  return  _rtcpSender.SetRTCPVoIPMetrics(VoIPMetric);
}

// our localy created statistics of the received RTP stream
WebRtc_Word32 ModuleRtpRtcpImpl::StatisticsRTP(
    WebRtc_UWord8*  fraction_lost,
    WebRtc_UWord32* cum_lost,
    WebRtc_UWord32* ext_max,
    WebRtc_UWord32* jitter,
    WebRtc_UWord32* max_jitter) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "StatisticsRTP()");

  WebRtc_UWord32 jitter_transmission_time_offset = 0;

  WebRtc_Word32 retVal = _rtpReceiver.Statistics(
      fraction_lost,
      cum_lost,
      ext_max,
      jitter,
      max_jitter,
      &jitter_transmission_time_offset,
      (_rtcpSender.Status() == kRtcpOff));
  if (retVal == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                 "StatisticsRTP() no statisitics availble");
  }
  return retVal;
}

WebRtc_Word32 ModuleRtpRtcpImpl::DataCountersRTP(
    WebRtc_UWord32* bytesSent,
    WebRtc_UWord32* packetsSent,
    WebRtc_UWord32* bytesReceived,
    WebRtc_UWord32* packetsReceived) const {
  WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id, "DataCountersRTP()");

  if (bytesSent) {
    *bytesSent = _rtpSender.Bytes();
  }
  if (packetsSent) {
    *packetsSent = _rtpSender.Packets();
  }
  return _rtpReceiver.DataCounters(bytesReceived, packetsReceived);
}

WebRtc_Word32 ModuleRtpRtcpImpl::ReportBlockStatistics(
    WebRtc_UWord8* fraction_lost,
    WebRtc_UWord32* cum_lost,
    WebRtc_UWord32* ext_max,
    WebRtc_UWord32* jitter,
    WebRtc_UWord32* jitter_transmission_time_offset) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "ReportBlockStatistics()");
  WebRtc_Word32 missing = 0;
  WebRtc_Word32 ret = _rtpReceiver.Statistics(fraction_lost,
                                              cum_lost,
                                              ext_max,
                                              jitter,
                                              NULL,
                                              jitter_transmission_time_offset,
                                              &missing,
                                              true);

#ifdef MATLAB
  if (_plot1 == NULL) {
    _plot1 = eng.NewPlot(new MatlabPlot());
    _plot1->AddTimeLine(30, "b", "lost", _clock.GetTimeInMS());
  }
  _plot1->Append("lost", missing);
  _plot1->Plot();
#endif

  return ret;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RemoteRTCPStat(RTCPSenderInfo* senderInfo) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteRTCPStat()");

  return _rtcpReceiver.SenderInfoReceived(senderInfo);
}

// received RTCP report
WebRtc_Word32 ModuleRtpRtcpImpl::RemoteRTCPStat(
  std::vector<RTCPReportBlock>* receiveBlocks) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteRTCPStat()");

  return _rtcpReceiver.StatisticsReceived(receiveBlocks);
}

WebRtc_Word32 ModuleRtpRtcpImpl::AddRTCPReportBlock(
    const WebRtc_UWord32 SSRC,
    const RTCPReportBlock* reportBlock) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "AddRTCPReportBlock()");

  return _rtcpSender.AddReportBlock(SSRC, reportBlock);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RemoveRTCPReportBlock(
    const WebRtc_UWord32 SSRC) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoveRTCPReportBlock()");

  return _rtcpSender.RemoveReportBlock(SSRC);
}

/*
 *  (REMB) Receiver Estimated Max Bitrate
 */
bool ModuleRtpRtcpImpl::REMB() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "REMB()");

  return _rtcpSender.REMB();
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetREMBStatus(const bool enable) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 _id,
                 "SetREMBStatus(enable)");
  } else {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 _id,
                 "SetREMBStatus(disable)");
  }
  return _rtcpSender.SetREMBStatus(enable);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetREMBData(const WebRtc_UWord32 bitrate,
                                             const WebRtc_UWord8 numberOfSSRC,
                                             const WebRtc_UWord32* SSRC) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
               "SetREMBData(bitrate:%d,?,?)", bitrate);
  return _rtcpSender.SetREMBData(bitrate, numberOfSSRC, SSRC);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetMaximumBitrateEstimate(
    const WebRtc_UWord32 bitrate) {
  if (_defaultModule) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "SetMaximumBitrateEstimate - Should be called on default "
                 "module.");
    return -1;
  }
  OnReceivedEstimatedMaxBitrate(bitrate);
  return 0;
}

bool ModuleRtpRtcpImpl::SetRemoteBitrateObserver(
  RtpRemoteBitrateObserver* observer) {
  return _rtcpSender.SetRemoteBitrateObserver(observer);
}

/*
 *   (IJ) Extended jitter report.
 */
bool ModuleRtpRtcpImpl::IJ() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "IJ()");

  return _rtcpSender.IJ();
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetIJStatus(const bool enable) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SetIJStatus(%s)", enable ? "true" : "false");

  return _rtcpSender.SetIJStatus(enable);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterSendRtpHeaderExtension(
  const RTPExtensionType type,
  const WebRtc_UWord8 id) {
  return _rtpSender.RegisterRtpHeaderExtension(type, id);
}

WebRtc_Word32 ModuleRtpRtcpImpl::DeregisterSendRtpHeaderExtension(
  const RTPExtensionType type) {
  return _rtpSender.DeregisterRtpHeaderExtension(type);
}

WebRtc_Word32 ModuleRtpRtcpImpl::RegisterReceiveRtpHeaderExtension(
  const RTPExtensionType type,
  const WebRtc_UWord8 id) {
  return _rtpReceiver.RegisterRtpHeaderExtension(type, id);
}

WebRtc_Word32 ModuleRtpRtcpImpl::DeregisterReceiveRtpHeaderExtension(
  const RTPExtensionType type) {
  return _rtpReceiver.DeregisterRtpHeaderExtension(type);
}

void ModuleRtpRtcpImpl::SetTransmissionSmoothingStatus(const bool enable) {
  _rtpSender.SetTransmissionSmoothingStatus(enable);
}

bool ModuleRtpRtcpImpl::TransmissionSmoothingStatus() const {
  return _rtpSender.TransmissionSmoothingStatus();
}

/*
*   (TMMBR) Temporary Max Media Bit Rate
*/
bool ModuleRtpRtcpImpl::TMMBR() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "TMMBR()");

  return _rtcpSender.TMMBR();
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetTMMBRStatus(const bool enable) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
                 "SetTMMBRStatus(enable)");
  } else {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
                 "SetTMMBRStatus(disable)");
  }
  return _rtcpSender.SetTMMBRStatus(enable);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetTMMBN(const TMMBRSet* boundingSet) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetTMMBN()");

  WebRtc_UWord32 maxBitrateKbit = _rtpSender.MaxConfiguredBitrateVideo() / 1000;
  return _rtcpSender.SetTMMBN(boundingSet, maxBitrateKbit);
}

/*
*   (NACK) Negative acknowledgement
*/

// Is Negative acknowledgement requests on/off?
NACKMethod ModuleRtpRtcpImpl::NACK() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "NACK()");

  NACKMethod childMethod = kNackOff;
  const bool defaultInstance(_childModules.empty() ? false : true);
  if (defaultInstance) {
    // for default we need to check all child modules too
    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    std::list<ModuleRtpRtcpImpl*>::const_iterator it =
      _childModules.begin();
    while (it != _childModules.end()) {
      RtpRtcp* module = *it;
      if (module) {
        NACKMethod nackMethod = module->NACK();
        if (nackMethod != kNackOff) {
          childMethod = nackMethod;
          break;
        }
      }
      it++;
    }
  }

  NACKMethod method = _nackMethod;
  if (childMethod != kNackOff) {
    method = childMethod;
  }
  return method;
}

// Turn negative acknowledgement requests on/off
WebRtc_Word32 ModuleRtpRtcpImpl::SetNACKStatus(NACKMethod method) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SetNACKStatus(%u)", method);

  _nackMethod = method;
  _rtpReceiver.SetNACKStatus(method);
  return 0;
}

// Returns the currently configured retransmission mode.
int ModuleRtpRtcpImpl::SelectiveRetransmissions() const {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SelectiveRetransmissions()");
  return _rtpSender.SelectiveRetransmissions();
}

// Enable or disable a retransmission mode, which decides which packets will
// be retransmitted if NACKed.
int ModuleRtpRtcpImpl::SetSelectiveRetransmissions(uint8_t settings) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SetSelectiveRetransmissions(%u)",
               settings);
  return _rtpSender.SetSelectiveRetransmissions(settings);
}

// Send a Negative acknowledgement packet
WebRtc_Word32 ModuleRtpRtcpImpl::SendNACK(const WebRtc_UWord16* nackList,
                                          const WebRtc_UWord16 size) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SendNACK(size:%u)", size);

  if (size > NACK_PACKETS_MAX_SIZE) {
    RequestKeyFrame();
    return -1;
  }
  WebRtc_UWord16 avgRTT = 0;
  _rtcpReceiver.RTT(_rtpReceiver.SSRC(), NULL, &avgRTT, NULL, NULL);

  WebRtc_UWord32 waitTime = 5 + ((avgRTT * 3) >> 1); // 5 + RTT*1.5
  if (waitTime == 5) {
    waitTime = 100; //During startup we don't have an RTT
  }
  const WebRtc_UWord32 now = _clock.GetTimeInMS();
  const WebRtc_UWord32 timeLimit = now - waitTime;

  if (_nackLastTimeSent < timeLimit) {
    // send list
  } else {
    // only send if extended list
    if (_nackLastSeqNumberSent == nackList[size - 1]) {
      // last seq num is the same don't send list
      return 0;
    } else {
      // send list
    }
  }
  _nackLastTimeSent =  now;
  _nackLastSeqNumberSent = nackList[size - 1];

  switch (_nackMethod) {
    case kNackRtcp:
      return _rtcpSender.SendRTCP(kRtcpNack, size, nackList);
    case kNackOff:
      return -1;
  };
  return -1;
}

// Store the sent packets, needed to answer to a Negative acknowledgement
// requests
WebRtc_Word32 ModuleRtpRtcpImpl::SetStorePacketsStatus(
  const bool enable,
  const WebRtc_UWord16 numberToStore) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
                 "SetStorePacketsStatus(enable, numberToStore:%d)",
                 numberToStore);
  } else {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
                 "SetStorePacketsStatus(disable)");
  }
  return _rtpSender.SetStorePacketsStatus(enable, numberToStore);
}

/*
*   Audio
*/

// Outband TelephoneEvent detection
WebRtc_Word32 ModuleRtpRtcpImpl::SetTelephoneEventStatus(
  const bool enable,
  const bool forwardToDecoder,
  const bool detectEndOfTone) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
               "SetTelephoneEventStatus(enable:%d forwardToDecoder:%d"
               " detectEndOfTone:%d)", enable, forwardToDecoder,
               detectEndOfTone);

  return _rtpReceiver.SetTelephoneEventStatus(enable, forwardToDecoder,
                                              detectEndOfTone);
}

// Is outband TelephoneEvent turned on/off?
bool ModuleRtpRtcpImpl::TelephoneEvent() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "TelephoneEvent()");

  return _rtpReceiver.TelephoneEvent();
}

// Is forwarding of outband telephone events turned on/off?
bool ModuleRtpRtcpImpl::TelephoneEventForwardToDecoder() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
               "TelephoneEventForwardToDecoder()");

  return _rtpReceiver.TelephoneEventForwardToDecoder();
}

// Send a TelephoneEvent tone using RFC 2833 (4733)
WebRtc_Word32 ModuleRtpRtcpImpl::SendTelephoneEventOutband(
    const WebRtc_UWord8 key,
    const WebRtc_UWord16 timeMs,
    const WebRtc_UWord8 level) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
               "SendTelephoneEventOutband(key:%u, timeMs:%u, level:%u)", key,
               timeMs, level);

  return _rtpSender.SendTelephoneEvent(key, timeMs, level);
}

bool ModuleRtpRtcpImpl::SendTelephoneEventActive(
  WebRtc_Word8& telephoneEvent) const {

  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SendTelephoneEventActive()");

  return _rtpSender.SendTelephoneEventActive(telephoneEvent);
}

// set audio packet size, used to determine when it's time to send a DTMF
// packet in silence (CNG)
WebRtc_Word32 ModuleRtpRtcpImpl::SetAudioPacketSize(
  const WebRtc_UWord16 packetSizeSamples) {

  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SetAudioPacketSize(%u)",
               packetSizeSamples);

  return _rtpSender.SetAudioPacketSize(packetSizeSamples);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetRTPAudioLevelIndicationStatus(
  const bool enable,
  const WebRtc_UWord8 ID) {

  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SetRTPAudioLevelIndicationStatus(enable=%d, ID=%u)",
               enable,
               ID);

  if (enable) {
    _rtpReceiver.RegisterRtpHeaderExtension(kRtpExtensionAudioLevel, ID);
  } else {
    _rtpReceiver.DeregisterRtpHeaderExtension(kRtpExtensionAudioLevel);
  }
  return _rtpSender.SetAudioLevelIndicationStatus(enable, ID);
}

WebRtc_Word32 ModuleRtpRtcpImpl::GetRTPAudioLevelIndicationStatus(
  bool& enable,
  WebRtc_UWord8& ID) const {

  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "GetRTPAudioLevelIndicationStatus()");
  return _rtpSender.AudioLevelIndicationStatus(enable, ID);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetAudioLevel(const WebRtc_UWord8 level_dBov) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SetAudioLevel(level_dBov:%u)",
               level_dBov);
  return _rtpSender.SetAudioLevel(level_dBov);
}

// Set payload type for Redundant Audio Data RFC 2198
WebRtc_Word32 ModuleRtpRtcpImpl::SetSendREDPayloadType(
  const WebRtc_Word8 payloadType) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SetSendREDPayloadType(%d)",
               payloadType);

  return _rtpSender.SetRED(payloadType);
}

// Get payload type for Redundant Audio Data RFC 2198
WebRtc_Word32 ModuleRtpRtcpImpl::SendREDPayloadType(
    WebRtc_Word8& payloadType) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SendREDPayloadType()");

  return _rtpSender.RED(payloadType);
}


/*
*   Video
*/
RtpVideoCodecTypes ModuleRtpRtcpImpl::ReceivedVideoCodec() const {
  return _rtpReceiver.VideoCodecType();
}

RtpVideoCodecTypes ModuleRtpRtcpImpl::SendVideoCodec() const {
  return _rtpSender.VideoCodecType();
}

void ModuleRtpRtcpImpl::SetSendBitrate(const WebRtc_UWord32 startBitrate,
                                       const WebRtc_UWord16 minBitrateKbit,
                                       const WebRtc_UWord16 maxBitrateKbit) {

  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SetSendBitrate start:%ubit/s min:%uKbit/s max:%uKbit/s",
               startBitrate, minBitrateKbit, maxBitrateKbit);

  const bool defaultInstance(_childModules.empty() ? false : true);

  if (defaultInstance) {
    // for default we need to update all child modules too
    CriticalSectionScoped lock(_criticalSectionModulePtrs);

    std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
    while (it != _childModules.end()) {
      RtpRtcp* module = *it;
      if (module) {
        module->SetSendBitrate(startBitrate,
                               minBitrateKbit,
                               maxBitrateKbit);
      }
      it++;
    }
  }
  // TODO(henrike): this function also returns a value. It never fails so
  // make it return void.
  _rtpSender.SetTargetSendBitrate(startBitrate);

  _bandwidthManagement.SetSendBitrate(startBitrate, minBitrateKbit,
                                      maxBitrateKbit);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetKeyFrameRequestMethod(
  const KeyFrameRequestMethod method) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SetKeyFrameRequestMethod(method:%u)",
               method);

  _keyFrameReqMethod = method;
  return 0;
}

WebRtc_Word32 ModuleRtpRtcpImpl::RequestKeyFrame() {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "RequestKeyFrame");

  switch (_keyFrameReqMethod) {
    case kKeyFrameReqFirRtp:
      return _rtpSender.SendRTPIntraRequest();
    case kKeyFrameReqPliRtcp:
      return _rtcpSender.SendRTCP(kRtcpPli);
    case kKeyFrameReqFirRtcp:
      return _rtcpSender.SendRTCP(kRtcpFir);
  }
  return -1;
}

WebRtc_Word32 ModuleRtpRtcpImpl::SendRTCPSliceLossIndication(
  const WebRtc_UWord8 pictureID) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SendRTCPSliceLossIndication (pictureID:%d)",
               pictureID);
  return _rtcpSender.SendRTCP(kRtcpSli, 0, 0, false, pictureID);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetCameraDelay(const WebRtc_Word32 delayMS) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               _id,
               "SetCameraDelay(%d)",
               delayMS);
  const bool defaultInstance(_childModules.empty() ? false : true);

  if (defaultInstance) {
    CriticalSectionScoped lock(_criticalSectionModulePtrs);

    std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
    while (it != _childModules.end()) {
      RtpRtcp* module = *it;
      if (module) {
        module->SetCameraDelay(delayMS);
      }
      it++;
    }
    return 0;
  }
  return _rtcpSender.SetCameraDelay(delayMS);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetGenericFECStatus(
  const bool enable,
  const WebRtc_UWord8 payloadTypeRED,
  const WebRtc_UWord8 payloadTypeFEC) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 _id,
                 "SetGenericFECStatus(enable, %u)",
                 payloadTypeRED);
  } else {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 _id,
                 "SetGenericFECStatus(disable)");
  }
  return _rtpSender.SetGenericFECStatus(enable,
                                        payloadTypeRED,
                                        payloadTypeFEC);
}

WebRtc_Word32 ModuleRtpRtcpImpl::GenericFECStatus(
  bool& enable,
  WebRtc_UWord8& payloadTypeRED,
  WebRtc_UWord8& payloadTypeFEC) {

  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "GenericFECStatus()");

  bool childEnabled = false;
  const bool defaultInstance(_childModules.empty() ? false : true);
  if (defaultInstance) {
    // for default we need to check all child modules too
    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
    while (it != _childModules.end()) {
      RtpRtcp* module = *it;
      if (module)  {
        bool enabled = false;
        WebRtc_UWord8 dummyPTypeRED = 0;
        WebRtc_UWord8 dummyPTypeFEC = 0;
        if (module->GenericFECStatus(enabled,
                                     dummyPTypeRED,
                                     dummyPTypeFEC) == 0 && enabled) {
          childEnabled = true;
          break;
        }
      }
      it++;
    }
  }
  WebRtc_Word32 retVal = _rtpSender.GenericFECStatus(enable,
                                                     payloadTypeRED,
                                                     payloadTypeFEC);
  if (childEnabled) {
    // returns true if enabled for any child module
    enable = childEnabled;
  }
  return retVal;
}

WebRtc_Word32 ModuleRtpRtcpImpl::SetFecParameters(
    const FecProtectionParams* delta_params,
    const FecProtectionParams* key_params) {
  const bool defaultInstance(_childModules.empty() ? false : true);
  if (defaultInstance)  {
    // for default we need to update all child modules too
    CriticalSectionScoped lock(_criticalSectionModulePtrs);

    std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
    while (it != _childModules.end()) {
      RtpRtcp* module = *it;
      if (module) {
        module->SetFecParameters(delta_params, key_params);
      }
      it++;
    }
    return 0;
  }
  return _rtpSender.SetFecParameters(delta_params, key_params);
}

void ModuleRtpRtcpImpl::SetRemoteSSRC(const WebRtc_UWord32 SSRC) {
  // inform about the incoming SSRC
  _rtcpSender.SetRemoteSSRC(SSRC);
  _rtcpReceiver.SetRemoteSSRC(SSRC);

  // check for a SSRC collision
  if (_rtpSender.SSRC() == SSRC && !_collisionDetected) {
    // if we detect a collision change the SSRC but only once
    _collisionDetected = true;
    WebRtc_UWord32 newSSRC = _rtpSender.GenerateNewSSRC();
    if (newSSRC == 0) {
      // configured via API ignore
      return;
    }
    if (kRtcpOff != _rtcpSender.Status()) {
      // send RTCP bye on the current SSRC
      _rtcpSender.SendRTCP(kRtcpBye);
    }
    // change local SSRC

    // inform all objects about the new SSRC
    _rtcpSender.SetSSRC(newSSRC);
    _rtcpReceiver.SetSSRC(newSSRC);
  }
}

WebRtc_UWord32 ModuleRtpRtcpImpl::BitrateReceivedNow() const {
  return _rtpReceiver.BitrateNow();
}

void ModuleRtpRtcpImpl::BitrateSent(WebRtc_UWord32* totalRate,
                                    WebRtc_UWord32* videoRate,
                                    WebRtc_UWord32* fecRate,
                                    WebRtc_UWord32* nackRate) const {
  const bool defaultInstance(_childModules.empty() ? false : true);

  if (defaultInstance) {
    // for default we need to update the send bitrate
    CriticalSectionScoped lock(_criticalSectionModulePtrsFeedback);

    if (totalRate != NULL)
      *totalRate = 0;
    if (videoRate != NULL)
      *videoRate = 0;
    if (fecRate != NULL)
      *fecRate = 0;
    if (nackRate != NULL)
      *nackRate = 0;

    std::list<ModuleRtpRtcpImpl*>::const_iterator it =
      _childModules.begin();
    while (it != _childModules.end()) {
      RtpRtcp* module = *it;
      if (module) {
        WebRtc_UWord32 childTotalRate = 0;
        WebRtc_UWord32 childVideoRate = 0;
        WebRtc_UWord32 childFecRate = 0;
        WebRtc_UWord32 childNackRate = 0;
        module->BitrateSent(&childTotalRate,
                            &childVideoRate,
                            &childFecRate,
                            &childNackRate);
        if (totalRate != NULL && childTotalRate > *totalRate)
          *totalRate = childTotalRate;
        if (videoRate != NULL && childVideoRate > *videoRate)
          *videoRate = childVideoRate;
        if (fecRate != NULL && childFecRate > *fecRate)
          *fecRate = childFecRate;
        if (nackRate != NULL && childNackRate > *nackRate)
          *nackRate = childNackRate;
      }
      it++;
    }
    return;
  }
  if (totalRate != NULL)
    *totalRate = _rtpSender.BitrateLast();
  if (videoRate != NULL)
    *videoRate = _rtpSender.VideoBitrateSent();
  if (fecRate != NULL)
    *fecRate = _rtpSender.FecOverheadRate();
  if (nackRate != NULL)
    *nackRate = _rtpSender.NackOverheadRate();
}

int ModuleRtpRtcpImpl::EstimatedSendBandwidth(
    WebRtc_UWord32* available_bandwidth) const {
  return _bandwidthManagement.AvailableBandwidth(available_bandwidth);
}

int ModuleRtpRtcpImpl::EstimatedReceiveBandwidth(
    WebRtc_UWord32* available_bandwidth) const {
  if (!_rtcpSender.ValidBitrateEstimate())
    return -1;
  *available_bandwidth = _rtcpSender.LatestBandwidthEstimate();
  return 0;
}

// for lip sync
void ModuleRtpRtcpImpl::OnReceivedNTP() {
  // don't do anything if we are the audio module
  // video module is responsible for sync
  if (!_audio) {
    WebRtc_Word32 diff = 0;
    WebRtc_UWord32 receivedNTPsecs = 0;
    WebRtc_UWord32 receivedNTPfrac = 0;
    WebRtc_UWord32 RTCPArrivalTimeSecs = 0;
    WebRtc_UWord32 RTCPArrivalTimeFrac = 0;

    if (0 == _rtcpReceiver.NTP(&receivedNTPsecs,
                               &receivedNTPfrac,
                               &RTCPArrivalTimeSecs,
                               &RTCPArrivalTimeFrac)) {
      CriticalSectionScoped lock(_criticalSectionModulePtrs);

      if (_audioModule) {
        if (0 != _audioModule->RemoteNTP(&_receivedNTPsecsAudio,
                                         &_receivedNTPfracAudio,
                                         &_RTCPArrivalTimeSecsAudio,
                                         &_RTCPArrivalTimeFracAudio)) {
          // failed ot get audio NTP
          return;
        }
      }
      if (_receivedNTPfracAudio != 0) {
        // ReceivedNTPxxx is NTP at sender side when sent.
        // RTCPArrivalTimexxx is NTP at receiver side when received.
        // can't use ConvertNTPTimeToMS since calculation can be
        //  negative

        WebRtc_Word32 NTPdiff = (WebRtc_Word32)
                                ((_receivedNTPsecsAudio - receivedNTPsecs) *
                                 1000); // ms
        NTPdiff += (WebRtc_Word32)
                   (_receivedNTPfracAudio / FracMS - receivedNTPfrac / FracMS);

        WebRtc_Word32 RTCPdiff =
            static_cast<WebRtc_Word32> ((_RTCPArrivalTimeSecsAudio -
                                         RTCPArrivalTimeSecs) * 1000);
        RTCPdiff += (WebRtc_Word32)
                    (_RTCPArrivalTimeFracAudio / FracMS -
                     RTCPArrivalTimeFrac / FracMS);

        diff = NTPdiff - RTCPdiff;
        // if diff is + video is behind
        if (diff < -1000 || diff > 1000) {
          // unresonable ignore value.
          diff = 0;
          return;
        }
      }
    }
    // export via callback
    // after release of critsect
    _rtcpReceiver.UpdateLipSync(diff);
  }
}

RateControlRegion ModuleRtpRtcpImpl::OnOverUseStateUpdate(
  const RateControlInput& rateControlInput) {

  bool firstOverUse = false;
  RateControlRegion region = _rtcpSender.UpdateOverUseState(rateControlInput,
                                                            firstOverUse);
  if (firstOverUse) {
    // Send TMMBR or REMB immediately.
    WebRtc_UWord16 RTT = 0;
    _rtcpReceiver.RTT(_rtpReceiver.SSRC(), &RTT, NULL, NULL, NULL);
    // About to send TMMBR, first run remote rate control
    // to get a target bit rate.
    unsigned int target_bitrate =
      _rtcpSender.CalculateNewTargetBitrate(RTT);
    if (REMB()) {
      _rtcpSender.UpdateRemoteBitrateEstimate(target_bitrate);
    } else if (TMMBR()) {
      _rtcpSender.SendRTCP(kRtcpTmmbr);
    }
  }
  return region;
}

// bad state of RTP receiver request a keyframe
void ModuleRtpRtcpImpl::OnRequestIntraFrame() {
  RequestKeyFrame();
}

void ModuleRtpRtcpImpl::OnReceivedIntraFrameRequest(const RtpRtcp* caller) {
  if (_defaultModule) {
    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    if (_defaultModule) {
      // if we use a default module pass this info to the default module
      _defaultModule->OnReceivedIntraFrameRequest(caller);
      return;
    }
  }

  WebRtc_UWord8 streamIdx = 0;
  FrameType frameType = kVideoFrameKey;
  if (_simulcast) {
    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    // loop though child modules and count idx
    std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
    while (it != _childModules.end()) {
      ModuleRtpRtcpImpl* childModule = *it;
      if (childModule == caller) {
        break;
      }
      streamIdx++;
      it++;
    }
  }
  _rtcpReceiver.OnReceivedIntraFrameRequest(frameType, streamIdx);
}

void ModuleRtpRtcpImpl::OnReceivedEstimatedMaxBitrate(
  const WebRtc_UWord32 maxBitrate) {
  // TODO(mflodman) Split this function in two parts. One for the child module
  // and one for the default module.

  // We received a REMB.
  if (_defaultModule) {
    // Send this update to the REMB instance to take actions.
    _rtcpSender.ReceivedRemb(maxBitrate);
    return;
  }

  WebRtc_UWord32 newBitrate = 0;
  WebRtc_UWord8 fractionLost = 0;
  WebRtc_UWord16 roundTripTime = 0;
  WebRtc_UWord16 bwEstimateKbit = WebRtc_UWord16(maxBitrate / 1000);
  if (_bandwidthManagement.UpdateBandwidthEstimate(bwEstimateKbit,
                                                   &newBitrate,
                                                   &fractionLost,
                                                   &roundTripTime) == 0) {
    _rtpReceiver.UpdateBandwidthManagement(newBitrate,
                                           fractionLost,
                                           roundTripTime);

    // We've received a new bandwidth estimate lower than the current send
    // bitrate. For simulcast we need to update the sending bitrate for all
    // streams.
    if (_simulcast) {
      CriticalSectionScoped lock(_criticalSectionModulePtrsFeedback);
      WebRtc_UWord8 idx = 0;
      for (std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
           it != _childModules.end(); ++it) {
        // sanity
        if (idx >= (_sendVideoCodec.numberOfSimulcastStreams - 1)) {
          return;
        }
        ModuleRtpRtcpImpl* module = *it;
        if (newBitrate >= _sendVideoCodec.simulcastStream[idx].maxBitrate) {
          module->_bandwidthManagement.SetSendBitrate(
            _sendVideoCodec.simulcastStream[idx].maxBitrate, 0, 0);
          module->_rtpSender.SetTargetSendBitrate(
            _sendVideoCodec.simulcastStream[idx].maxBitrate);

          newBitrate -= _sendVideoCodec.simulcastStream[idx].maxBitrate;
        } else {
          module->_bandwidthManagement.SetSendBitrate(newBitrate, 0, 0);
          module->_rtpSender.SetTargetSendBitrate(newBitrate);
          newBitrate -= newBitrate;
        }
        idx++;
      }
    }
  }
  // For non-simulcast, update all child modules with the new bandwidth estimate
  // regardless of the new estimate.
  if (!_simulcast) {
    // Update all child modules with the new max bitrate before exiting.
    CriticalSectionScoped lock(_criticalSectionModulePtrsFeedback);
    for (std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
         it != _childModules.end(); ++it) {
      // Update all child modules with the maximum bitrate estimate.
      ModuleRtpRtcpImpl* module = *it;
      WebRtc_UWord32 ignoreBitrate = 0;
      WebRtc_UWord8 ignoreFractionLost = 0;
      WebRtc_UWord16 ignoreRoundTripTime = 0;
      module->_bandwidthManagement.UpdateBandwidthEstimate(
        bwEstimateKbit,
        &ignoreBitrate,
        &ignoreFractionLost,
        &ignoreRoundTripTime);
      // We don't need to take care of a possible lowered bitrate, that is
      // handled earlier in this function for the default module.
    }
  }
}

// received a request for a new SLI
void ModuleRtpRtcpImpl::OnReceivedSliceLossIndication(
  const WebRtc_UWord8 pictureID) {

  if (_defaultModule) {
    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    if (_defaultModule) {
      // if we use a default module pass this info to the default module
      _defaultModule->OnReceivedSliceLossIndication(pictureID);
      return;
    }
  }
  _rtcpReceiver.OnReceivedSliceLossIndication(pictureID);
}

// received a new refereence frame
void ModuleRtpRtcpImpl::OnReceivedReferencePictureSelectionIndication(
  const WebRtc_UWord64 pictureID) {

  if (_defaultModule) {
    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    if (_defaultModule) {
      // if we use a default module pass this info to the default module
      _defaultModule->OnReceivedReferencePictureSelectionIndication(
        pictureID);
      return;
    }
  }
  _rtcpReceiver.OnReceivedReferencePictureSelectionIndication(pictureID);
}

void ModuleRtpRtcpImpl::OnReceivedBandwidthEstimateUpdate(
  const WebRtc_UWord16 bwEstimateKbit) {
  // We received a TMMBR
  if (_audio) {
    return;
  }
  const bool defaultInstance(_childModules.empty() ? false : true);
  if (defaultInstance) {
    ProcessDefaultModuleBandwidth();
    return;
  }
  WebRtc_UWord32 newBitrate = 0;
  WebRtc_UWord8 fractionLost = 0;
  WebRtc_UWord16 roundTripTime = 0;
  if (_bandwidthManagement.UpdateBandwidthEstimate(bwEstimateKbit,
                                                   &newBitrate,
                                                   &fractionLost,
                                                   &roundTripTime) == 0) {
    if (!_defaultModule) {
      // No default module check if we should trigger OnNetworkChanged
      // via video callback
      _rtpReceiver.UpdateBandwidthManagement(newBitrate,
                                             fractionLost,
                                             roundTripTime);
    }
    if (newBitrate > 0) {
      // update bitrate
      _rtpSender.SetTargetSendBitrate(newBitrate);
    }
  }
  if (_defaultModule) {
    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    if (_defaultModule) {
      // if we use a default module pass this info to the default module
      _defaultModule->OnReceivedBandwidthEstimateUpdate(bwEstimateKbit);
      return;
    }
  }
}

// bw estimation
// We received a RTCP report block
void ModuleRtpRtcpImpl::OnPacketLossStatisticsUpdate(
  const WebRtc_UWord8 fractionLost,
  const WebRtc_UWord16 roundTripTime,
  const WebRtc_UWord32 lastReceivedExtendedHighSeqNum) {

  const bool defaultInstance(_childModules.empty() ? false : true);
  if (!defaultInstance) {
    WebRtc_UWord32 newBitrate = 0;
    WebRtc_UWord8 loss = fractionLost;  // local copy since it can change
    WebRtc_UWord32 videoRate = 0;
    WebRtc_UWord32 fecRate = 0;
    WebRtc_UWord32 nackRate = 0;
    BitrateSent(NULL, &videoRate, &fecRate, &nackRate);
    if (_bandwidthManagement.UpdatePacketLoss(
          lastReceivedExtendedHighSeqNum,
          videoRate + fecRate + nackRate,
          roundTripTime,
          &loss,
          &newBitrate,
          _clock.GetTimeInMS()) != 0) {
      // ignore this update
      return;
    }
    // We need to do update RTP sender before calling default module in
    // case we'll strip any layers.
    if (!_simulcast) {
      // the default module will inform all child modules about
      //  their bitrate
      _rtpSender.SetTargetSendBitrate(newBitrate);
    }
    if (_defaultModule) {
      // if we have a default module update it
      CriticalSectionScoped lock(_criticalSectionModulePtrs);
      if (_defaultModule) {  // we need to check again inside the critsect
        // if we use a default module pass this info to the
        // default module
        _defaultModule->OnPacketLossStatisticsUpdate(
          loss,  // send in the filtered loss
          roundTripTime,
          lastReceivedExtendedHighSeqNum);
      }
      return;
    }
    _rtpReceiver.UpdateBandwidthManagement(newBitrate,
                                           fractionLost,
                                           roundTripTime);
  } else {
    if (!_simulcast) {
      ProcessDefaultModuleBandwidth();
    } else {
      // default and simulcast
      WebRtc_UWord32 newBitrate = 0;
      WebRtc_UWord8 loss = fractionLost;  // local copy
      WebRtc_UWord32 videoRate = 0;
      WebRtc_UWord32 fecRate = 0;
      WebRtc_UWord32 nackRate = 0;
      BitrateSent(NULL, &videoRate, &fecRate, &nackRate);
      if (_bandwidthManagement.UpdatePacketLoss(0,  // we can't use this
                                                videoRate + fecRate + nackRate,
                                                roundTripTime,
                                                &loss,
                                                &newBitrate,
                                                _clock.GetTimeInMS()) != 0) {
        // ignore this update
        return;
      }
      _rtpSender.SetTargetSendBitrate(newBitrate);
      _rtpReceiver.UpdateBandwidthManagement(newBitrate,
                                             loss,
                                             roundTripTime);
      // sanity
      if (_sendVideoCodec.codecType == kVideoCodecUnknown) {
        return;
      }
      CriticalSectionScoped lock(_criticalSectionModulePtrsFeedback);
      std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
      WebRtc_UWord8 idx = 0;
      while (it != _childModules.end()) {
        // sanity
        if (idx >= (_sendVideoCodec.numberOfSimulcastStreams - 1)) {
          return;
        }
        ModuleRtpRtcpImpl* module = *it;
        // update all child modules
        if (newBitrate >=
            _sendVideoCodec.simulcastStream[idx].maxBitrate) {
          module->_bandwidthManagement.SetSendBitrate(
            _sendVideoCodec.simulcastStream[idx].maxBitrate, 0, 0);
          module->_rtpSender.SetTargetSendBitrate(
            _sendVideoCodec.simulcastStream[idx].maxBitrate);

          newBitrate -=
            _sendVideoCodec.simulcastStream[idx].maxBitrate;
        } else {
          module->_bandwidthManagement.SetSendBitrate(newBitrate,
                                                      0,
                                                      0);
          module->_rtpSender.SetTargetSendBitrate(newBitrate);
          newBitrate -= newBitrate;
        }
        idx++;
      }
    }
  }
}

void ModuleRtpRtcpImpl::ProcessDefaultModuleBandwidth() {

  WebRtc_UWord32 minBitrateBps = 0xffffffff;
  WebRtc_UWord32 maxBitrateBps = 0;
  WebRtc_UWord32 count = 0;
  WebRtc_UWord32 fractionLostAcc = 0;
  WebRtc_UWord16 maxRoundTripTime = 0;
  {
    // get min and max for the sending channels
    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    for (std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
         it != _childModules.end(); ++ it) {
      // Get child RTP sender and ask for bitrate estimate.
      ModuleRtpRtcpImpl* childModule = *it;
      if (childModule->Sending()) {
        RTPSender& childRtpSender = (*it)->_rtpSender;
        const WebRtc_UWord32 childEstimateBps =
          1000 * childRtpSender.TargetSendBitrateKbit();
        if (childEstimateBps < minBitrateBps) {
          minBitrateBps = childEstimateBps;
        }
        if (childEstimateBps > maxBitrateBps) {
          maxBitrateBps = childEstimateBps;
        }
        RTCPReceiver& childRtcpReceiver = (*it)->_rtcpReceiver;

        std::vector<RTCPReportBlock> rtcp_blocks;
        childRtcpReceiver.StatisticsReceived(&rtcp_blocks);
        for (std::vector<RTCPReportBlock>::iterator rit = rtcp_blocks.begin();
             rit != rtcp_blocks.end(); ++rit) {
          count++;
          fractionLostAcc += rit->fractionLost;
          WebRtc_UWord16 RTT = 0;
          childRtcpReceiver.RTT(rit->remoteSSRC, &RTT, NULL, NULL, NULL);
          maxRoundTripTime = (RTT > maxRoundTripTime) ? RTT : maxRoundTripTime;
        }
      }
    }
  }  // end critsect

  if (count == 0) {
    // No sending modules and no bitrate estimate.
    return;
  }

  // Update RTT to all receive only child modules, they won't have their own RTT
  // estimate. Assume the receive only channels are on similar links as the
  // sending channel and have approximately the same RTT.
  {
    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    for (std::list<ModuleRtpRtcpImpl*>::iterator it = _childModules.begin();
        it != _childModules.end(); ++it) {
      if (!(*it)->Sending()) {
        (*it)->_rtcpReceiver.SetRTT(maxRoundTripTime);
      }
    }
  }

  _bandwidthManagement.SetSendBitrate(minBitrateBps, 0, 0);

  // Update default module bitrate. Don't care about min max.
  WebRtc_UWord8 fractionLostAvg = WebRtc_UWord8(fractionLostAcc / count);
  _rtpReceiver.UpdateBandwidthManagement(minBitrateBps,
                                         fractionLostAvg ,
                                         maxRoundTripTime);
}

void ModuleRtpRtcpImpl::OnRequestSendReport() {
  _rtcpSender.SendRTCP(kRtcpSr);
}

WebRtc_Word32 ModuleRtpRtcpImpl::SendRTCPReferencePictureSelection(
  const WebRtc_UWord64 pictureID) {
  return _rtcpSender.SendRTCP(kRtcpRpsi, 0, 0, false, pictureID);
}

WebRtc_UWord32 ModuleRtpRtcpImpl::SendTimeOfSendReport(
  const WebRtc_UWord32 sendReport) {
  return _rtcpSender.SendTimeOfSendReport(sendReport);
}

void ModuleRtpRtcpImpl::OnReceivedNACK(
  const WebRtc_UWord16 nackSequenceNumbersLength,
  const WebRtc_UWord16* nackSequenceNumbers) {
  if (!_rtpSender.StorePackets() ||
      nackSequenceNumbers == NULL ||
      nackSequenceNumbersLength == 0) {
    return;
  }
  WebRtc_UWord16 avgRTT = 0;
  _rtcpReceiver.RTT(_rtpReceiver.SSRC(), NULL, &avgRTT, NULL, NULL);
  _rtpSender.OnReceivedNACK(nackSequenceNumbersLength,
                            nackSequenceNumbers,
                            avgRTT);
}

WebRtc_Word32 ModuleRtpRtcpImpl::LastReceivedNTP(
  WebRtc_UWord32& RTCPArrivalTimeSecs,  // when we received the last report
  WebRtc_UWord32& RTCPArrivalTimeFrac,
  WebRtc_UWord32& remoteSR) {
  // remote SR: NTP inside the last received (mid 16 bits from sec and frac)
  WebRtc_UWord32 NTPsecs = 0;
  WebRtc_UWord32 NTPfrac = 0;

  if (-1 == _rtcpReceiver.NTP(&NTPsecs,
                              &NTPfrac,
                              &RTCPArrivalTimeSecs,
                              &RTCPArrivalTimeFrac)) {
    return -1;
  }
  remoteSR = ((NTPsecs & 0x0000ffff) << 16) + ((NTPfrac & 0xffff0000) >> 16);
  return 0;
}

bool ModuleRtpRtcpImpl::UpdateRTCPReceiveInformationTimers() {
  // if this returns true this channel has timed out
  // periodically check if this is true and if so call UpdateTMMBR
  return _rtcpReceiver.UpdateRTCPReceiveInformationTimers();
}

// called from RTCPsender
WebRtc_Word32 ModuleRtpRtcpImpl::BoundingSet(bool& tmmbrOwner,
                                             TMMBRSet*& boundingSet) {
  return _rtcpReceiver.BoundingSet(tmmbrOwner, boundingSet);
}

void ModuleRtpRtcpImpl::SendKeyFrame() {
  WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id, "SendKeyFrame()");
  OnReceivedIntraFrameRequest(0);
}

}  // namespace webrtc
