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
    uint32_t    timestamp;
    void*             data;
};

class VCMTimestampMap
{
public:
    // Constructor. Optional parameter specifies maximum number of
    // timestamps in map.
    VCMTimestampMap(const int32_t length = 10);

    // Destructor.
    ~VCMTimestampMap();

    // Empty the map
    void Reset();

    int32_t Add(uint32_t timestamp, void*  data);
    void* Pop(uint32_t timestamp);

private:
    bool IsEmpty() const;

    VCMTimestampDataTuple* _map;
    int32_t                   _nextAddIx;
    int32_t                   _nextPopIx;
    int32_t                   _length;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_TIMESTAMP_MAP_H_
