/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - Telephone event transmission.
//  - DTMF tone generation.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface(voe);
//  VoEDtmf* dtmf  = VoEDtmf::GetInterface(voe);
//  base->Init();
//  int ch = base->CreateChannel();
//  ...
//  dtmf->SendTelephoneEvent(ch, 7);
//  ...
//  base->DeleteChannel(ch);
//  base->Terminate();
//  base->Release();
//  dtmf->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_DTMF_H
#define WEBRTC_VOICE_ENGINE_VOE_DTMF_H

#include "common_types.h"

namespace webrtc {

class VoiceEngine;

// VoETelephoneEventObserver
class WEBRTC_DLLEXPORT VoETelephoneEventObserver
{
public:
    // This method will be called after the detection of an inband
    // telephone event. The event code is given as output in the
    // |eventCode| parameter.
    virtual void OnReceivedTelephoneEventInband(int channel,
                                                int eventCode,
                                                bool endOfEvent) = 0;

    // This method will be called after the detection of an out-of-band
    // telephone event. The event code is given as output in the
    // |eventCode| parameter.
    virtual void OnReceivedTelephoneEventOutOfBand(
        int channel,
        int eventCode,
        bool endOfEvent) = 0;

protected:
    virtual ~VoETelephoneEventObserver() {}
};

// VoEDtmf
class WEBRTC_DLLEXPORT VoEDtmf
{
public:
    
    // Factory for the VoEDtmf sub-API. Increases an internal
    // reference counter if successful. Returns NULL if the API is not
    // supported or if construction fails.
    static VoEDtmf* GetInterface(VoiceEngine* voiceEngine);

    // Releases the VoEDtmf sub-API and decreases an internal
    // reference counter. Returns the new reference count. This value should
    // be zero for all sub-API:s before the VoiceEngine object can be safely
    // deleted.
    virtual int Release() = 0;

    // Sends telephone events either in-band or out-of-band.
    virtual int SendTelephoneEvent(int channel, int eventCode,
                                   bool outOfBand = true, int lengthMs = 160,
                                   int attenuationDb = 10) = 0;

   
    // Sets the dynamic payload |type| that should be used for telephone
    // events.
    virtual int SetSendTelephoneEventPayloadType(int channel,
                                                 unsigned char type) = 0;

  
    // Gets the currently set dynamic payload |type| for telephone events.
    virtual int GetSendTelephoneEventPayloadType(int channel,
                                                 unsigned char& type) = 0;

    // Enables or disables local tone playout for received DTMF events
    // out-of-band.
    virtual int SetDtmfPlayoutStatus(int channel, bool enable) = 0;

    // Gets the DTMF playout status.
    virtual int GetDtmfPlayoutStatus(int channel, bool& enabled) = 0;

    // Toogles DTMF feedback state: when a DTMF tone is sent, the same tone
    // is played out on the speaker.
    virtual int SetDtmfFeedbackStatus(bool enable,
                                      bool directFeedback = false) = 0;

    // Gets the DTMF feedback status.
    virtual int GetDtmfFeedbackStatus(bool& enabled, bool& directFeedback) = 0;

    // Plays a DTMF feedback tone (only locally).
    virtual int PlayDtmfTone(int eventCode, int lengthMs = 200,
                             int attenuationDb = 10) = 0;

    // Starts playing out a DTMF feedback tone locally.
    // The tone will be played out until the corresponding stop function
    // is called.
    virtual int StartPlayingDtmfTone(int eventCode,
                                     int attenuationDb = 10) = 0;

    // Stops playing out a DTMF feedback tone locally.
    virtual int StopPlayingDtmfTone() = 0;

    // Installs an instance of a VoETelephoneEventObserver derived class and
    // activates detection of telephone events for the specified |channel|.
    virtual int RegisterTelephoneEventDetection(
        int channel, TelephoneEventDetectionMethods detectionMethod,
        VoETelephoneEventObserver& observer) = 0;

    // Removes an instance of a VoETelephoneEventObserver derived class and
    // disables detection of telephone events for the specified |channel|.
    virtual int DeRegisterTelephoneEventDetection(int channel) = 0;

    // Gets the current telephone-event detection status for a specified
    // |channel|.
    virtual int GetTelephoneEventDetectionStatus(
        int channel, bool& enabled,
        TelephoneEventDetectionMethods& detectionMethod) = 0;

protected:
    VoEDtmf() {}
    virtual ~VoEDtmf() {}
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_DTMF_H
