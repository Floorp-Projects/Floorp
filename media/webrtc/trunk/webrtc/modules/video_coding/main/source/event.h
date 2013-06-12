/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_EVENT_H_
#define WEBRTC_MODULES_VIDEO_CODING_EVENT_H_

#include "event_wrapper.h"

namespace webrtc
{

//#define EVENT_DEBUG

class VCMEvent : public EventWrapper
{
public:
    VCMEvent() : _event(*EventWrapper::Create()) {};

    virtual ~VCMEvent() { delete &_event; };

    /**
    *   Release waiting threads
    */
    bool Set() { return _event.Set(); };

    bool Reset() { return _event.Reset(); };

    /**
    *   Wait for this event
    */
    EventTypeWrapper Wait(unsigned long maxTime)
    {
#ifdef EVENT_DEBUG
        return kEventTimeout;
#else
        return _event.Wait(maxTime);
#endif
    };

    /**
    *   Start a timer
    */
    bool StartTimer(bool periodic, unsigned long time)
                   { return _event.StartTimer(periodic, time); };
    /**
    *   Stop the timer
    */
    bool StopTimer() { return _event.StopTimer(); };

private:
    EventWrapper&      _event;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_EVENT_H_
