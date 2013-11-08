/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - Enables full duplex VoIP sessions via RTP using G.711 (mu-Law or A-Law).
//  - Initialization and termination.
//  - Trace information on text files or via callbacks.
//  - Multi-channel support (mixing, sending to multiple destinations etc.).
//
// To support other codecs than G.711, the VoECodec sub-API must be utilized.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface(voe);
//  base->Init();
//  int ch = base->CreateChannel();
//  base->StartPlayout(ch);
//  ...
//  base->DeleteChannel(ch);
//  base->Terminate();
//  base->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_BASE_H
#define WEBRTC_VOICE_ENGINE_VOE_BASE_H

#include "webrtc/common_types.h"

namespace webrtc {

class AudioDeviceModule;
class AudioProcessing;
class Config;

const int kVoEDefault = -1;

// VoiceEngineObserver
class WEBRTC_DLLEXPORT VoiceEngineObserver
{
public:
    // This method will be called after the occurrence of any runtime error
    // code, or warning notification, when the observer interface has been
    // installed using VoEBase::RegisterVoiceEngineObserver().
    virtual void CallbackOnError(int channel, int errCode) = 0;

protected:
    virtual ~VoiceEngineObserver() {}
};

// VoiceEngine
class WEBRTC_DLLEXPORT VoiceEngine
{
public:
    // Creates a VoiceEngine object, which can then be used to acquire
    // sub-APIs. Returns NULL on failure.
    static VoiceEngine* Create();
    static VoiceEngine* Create(const Config& config);

    // Deletes a created VoiceEngine object and releases the utilized resources.
    // Note that if there are outstanding references held via other interfaces,
    // the voice engine instance will not actually be deleted until those
    // references have been released.
    static bool Delete(VoiceEngine*& voiceEngine);

    // Specifies the amount and type of trace information which will be
    // created by the VoiceEngine.
    static int SetTraceFilter(unsigned int filter);

    // Sets the name of the trace file and enables non-encrypted trace messages.
    static int SetTraceFile(const char* fileNameUTF8,
                            bool addFileCounter = false);

    // Installs the TraceCallback implementation to ensure that the user
    // receives callbacks for generated trace messages.
    static int SetTraceCallback(TraceCallback* callback);

    static int SetAndroidObjects(void* javaVM, void* env, void* context);

protected:
    VoiceEngine() {}
    ~VoiceEngine() {}
};

// VoEBase
class WEBRTC_DLLEXPORT VoEBase
{
public:
    // Factory for the VoEBase sub-API. Increases an internal reference
    // counter if successful. Returns NULL if the API is not supported or if
    // construction fails.
    static VoEBase* GetInterface(VoiceEngine* voiceEngine);

    // Releases the VoEBase sub-API and decreases an internal reference
    // counter. Returns the new reference count. This value should be zero
    // for all sub-APIs before the VoiceEngine object can be safely deleted.
    virtual int Release() = 0;

    // Installs the observer class to enable runtime error control and
    // warning notifications.
    virtual int RegisterVoiceEngineObserver(VoiceEngineObserver& observer) = 0;

    // Removes and disables the observer class for runtime error control
    // and warning notifications.
    virtual int DeRegisterVoiceEngineObserver() = 0;

    // Initializes all common parts of the VoiceEngine; e.g. all
    // encoders/decoders, the sound card and core receiving components.
    // This method also makes it possible to install some user-defined external
    // modules:
    // - The Audio Device Module (ADM) which implements all the audio layer
    // functionality in a separate (reference counted) module.
    // - The AudioProcessing module handles capture-side processing. VoiceEngine
    // takes ownership of this object.
    // If NULL is passed for any of these, VoiceEngine will create its own.
    // TODO(ajm): Remove default NULLs.
    virtual int Init(AudioDeviceModule* external_adm = NULL,
                     AudioProcessing* audioproc = NULL) = 0;

    // Returns NULL before Init() is called.
    virtual AudioProcessing* audio_processing() = 0;

    // Terminates all VoiceEngine functions and releses allocated resources.
    virtual int Terminate() = 0;

    // Creates a new channel and allocates the required resources for it.
    virtual int CreateChannel() = 0;

    // Deletes an existing channel and releases the utilized resources.
    virtual int DeleteChannel(int channel) = 0;

    // Prepares and initiates the VoiceEngine for reception of
    // incoming RTP/RTCP packets on the specified |channel|.
    virtual int StartReceive(int channel) = 0;

    // Stops receiving incoming RTP/RTCP packets on the specified |channel|.
    virtual int StopReceive(int channel) = 0;

    // Starts forwarding the packets to the mixer/soundcard for a
    // specified |channel|.
    virtual int StartPlayout(int channel) = 0;

    // Stops forwarding the packets to the mixer/soundcard for a
    // specified |channel|.
    virtual int StopPlayout(int channel) = 0;

    // Starts sending packets to an already specified IP address and
    // port number for a specified |channel|.
    virtual int StartSend(int channel) = 0;

    // Stops sending packets from a specified |channel|.
    virtual int StopSend(int channel) = 0;

    // Gets the version information for VoiceEngine and its components.
    virtual int GetVersion(char version[1024]) = 0;

    // Gets the last VoiceEngine error code.
    virtual int LastError() = 0;

    // Stops or resumes playout and transmission on a temporary basis.
    virtual int SetOnHoldStatus(int channel, bool enable,
                                OnHoldModes mode = kHoldSendAndPlay) = 0;

    // Gets the current playout and transmission status.
    virtual int GetOnHoldStatus(int channel, bool& enabled,
                                OnHoldModes& mode) = 0;

    // Sets the NetEQ playout mode for a specified |channel| number.
    virtual int SetNetEQPlayoutMode(int channel, NetEqModes mode) = 0;

    // Gets the NetEQ playout mode for a specified |channel| number.
    virtual int GetNetEQPlayoutMode(int channel, NetEqModes& mode) = 0;

protected:
    VoEBase() {}
    virtual ~VoEBase() {}
};

}  // namespace webrtc

#endif  //  WEBRTC_VOICE_ENGINE_VOE_BASE_H
