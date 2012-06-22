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
//  - External encryption and decryption.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEEncryption* encrypt  = VoEEncryption::GetInterface(voe);
//  ...
//  encrypt->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_ENCRYPTION_H
#define WEBRTC_VOICE_ENGINE_VOE_ENCRYPTION_H

#include "common_types.h"

namespace webrtc {

class VoiceEngine;

class WEBRTC_DLLEXPORT VoEEncryption
{
public:
    // Factory for the VoEEncryption sub-API. Increases an internal
    // reference counter if successful. Returns NULL if the API is not
    // supported or if construction fails.
    static VoEEncryption* GetInterface(VoiceEngine* voiceEngine);

    // Releases the VoEEncryption sub-API and decreases an internal
    // reference counter. Returns the new reference count. This value should
    // be zero for all sub-API:s before the VoiceEngine object can be safely
    // deleted.
    virtual int Release() = 0;

    // Installs an Encryption instance and enables external encryption
    // for the selected |channel|.
    virtual int RegisterExternalEncryption(
        int channel, Encryption& encryption) = 0;

    // Removes an Encryption instance and disables external encryption
    // for the selected |channel|.
    virtual int DeRegisterExternalEncryption(int channel) = 0;

    // Not supported
    virtual int EnableSRTPSend(int channel, CipherTypes cipherType,
        int cipherKeyLength, AuthenticationTypes authType, int authKeyLength,
        int authTagLength, SecurityLevels level, const unsigned char key[30],
        bool useForRTCP = false) = 0;

    // Not supported
    virtual int DisableSRTPSend(int channel) = 0;

    // Not supported
    virtual int EnableSRTPReceive(int channel, CipherTypes cipherType,
        int cipherKeyLength, AuthenticationTypes authType, int authKeyLength,
        int authTagLength, SecurityLevels level, const unsigned char key[30],
        bool useForRTCP = false) = 0;

    // Not supported
    virtual int DisableSRTPReceive(int channel) = 0;

protected:
    VoEEncryption() {}
    virtual ~VoEEncryption() {}
};

}  // namespace webrtc

#endif  //  WEBRTC_VOICE_ENGINE_VOE_ENCRYPTION_H
