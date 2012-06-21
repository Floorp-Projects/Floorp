/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_ENCRYPTION_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_VIE_ENCRYPTION_IMPL_H_

#include "typedefs.h"
#include "video_engine/include/vie_encryption.h"
#include "video_engine/vie_ref_count.h"

namespace webrtc {

class ViESharedData;

class ViEEncryptionImpl
    : public ViEEncryption,
      public ViERefCount {
 public:
  virtual int Release();

  // Implements ViEEncryption.
  virtual int RegisterExternalEncryption(const int video_channel,
                                         Encryption& encryption);
  virtual int DeregisterExternalEncryption(const int video_channel);

 protected:
  ViEEncryptionImpl(ViESharedData* shared_data);
  virtual ~ViEEncryptionImpl();

 private:
  ViESharedData* shared_data_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_ENCRYPTION_IMPL_H_
