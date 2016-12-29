/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_TEST_PROTOBUF_UTILS_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_TEST_PROTOBUF_UTILS_H_

#include "webrtc/audio_processing/debug.pb.h"
#include "webrtc/base/scoped_ptr.h"

namespace webrtc {

// Allocates new memory in the scoped_ptr to fit the raw message and returns the
// number of bytes read.
size_t ReadMessageBytesFromFile(FILE* file, rtc::scoped_ptr<uint8_t[]>* bytes);

// Returns true on success, false on error or end-of-file.
bool ReadMessageFromFile(FILE* file, ::google::protobuf::MessageLite* msg);

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_TEST_PROTOBUF_UTILS_H_
