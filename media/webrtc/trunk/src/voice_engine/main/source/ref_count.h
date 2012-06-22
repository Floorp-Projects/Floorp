/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_REF_COUNT_H
#define WEBRTC_VOICE_ENGINE_REF_COUNT_H

namespace webrtc {
class CriticalSectionWrapper;

namespace voe {

class RefCount
{
public:
    RefCount();
    ~RefCount();
    RefCount& operator++(int);
    RefCount& operator--(int);
    void Reset();
    int GetCount() const;
private:
    volatile int _count;
    CriticalSectionWrapper& _crit;
};

}  // namespace voe

}  // namespace webrtc
#endif    // #ifndef WEBRTC_VOICE_ENGINE_REF_COUNT_H
