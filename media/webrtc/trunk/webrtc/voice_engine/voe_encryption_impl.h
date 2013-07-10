/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_ENCRYPTION_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_ENCRYPTION_IMPL_H

#include "voe_encryption.h"

#include "shared_data.h"

namespace webrtc {

class VoEEncryptionImpl : public VoEEncryption
{
public:
    // External encryption
    virtual int RegisterExternalEncryption(
        int channel,
        Encryption& encryption);

    virtual int DeRegisterExternalEncryption(int channel);

protected:
    VoEEncryptionImpl(voe::SharedData* shared);
    virtual ~VoEEncryptionImpl();

private:
    voe::SharedData* _shared;
};

}   // namespace webrtc

#endif  // #ifndef WEBRTC_VOICE_ENGINE_VOE_ENCRYPTION_IMPL_H
