/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TIMESTAMP_MAP_H_
#define WEBRTC_MODULES_VIDEO_CODING_TIMESTAMP_MAP_H_

#include "typedefs.h"

namespace webrtc
{

struct VCMTimestampDataTuple
{
    WebRtc_UWord32    timestamp;
    void*             data;
};

class VCMTimestampMap
{
public:
    // Constructor. Optional parameter specifies maximum number of
    // timestamps in map.
    VCMTimestampMap(const WebRtc_Word32 length = 10);

    // Destructor.
    ~VCMTimestampMap();

    // Empty the map
    void Reset();

    WebRtc_Word32 Add(WebRtc_UWord32 timestamp, void*  data);
    void* Pop(WebRtc_UWord32 timestamp);

private:
    bool IsEmpty() const;

    VCMTimestampDataTuple* _map;
    WebRtc_Word32                   _nextAddIx;
    WebRtc_Word32                   _nextPopIx;
    WebRtc_Word32                   _length;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_TIMESTAMP_MAP_H_
