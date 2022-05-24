/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_MEDIA_TYPES_H_
#define API_MEDIA_TYPES_H_

#include <string>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

enum class MediaType { ANY, AUDIO, VIDEO, DATA, UNSUPPORTED };

}  // namespace webrtc

// The cricket and webrtc have separate definitions for what a media type is.
// They used to be incompatible, but now cricket is defined in terms of the
// webrtc definition.

namespace cricket {

enum MediaType {
  MEDIA_TYPE_AUDIO = static_cast<int>(webrtc::MediaType::AUDIO),
  MEDIA_TYPE_VIDEO = static_cast<int>(webrtc::MediaType::VIDEO),
  MEDIA_TYPE_DATA = static_cast<int>(webrtc::MediaType::DATA),
  MEDIA_TYPE_UNSUPPORTED = static_cast<int>(webrtc::MediaType::UNSUPPORTED),
};

extern const char kMediaTypeAudio[];
extern const char kMediaTypeVideo[];
extern const char kMediaTypeData[];

RTC_EXPORT std::string MediaTypeToString(MediaType type);

}  // namespace cricket

#endif  // API_MEDIA_TYPES_H_
