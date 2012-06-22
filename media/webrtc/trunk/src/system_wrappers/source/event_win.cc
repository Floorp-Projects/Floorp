/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "event_win.h"

#include "Mmsystem.h"

namespace webrtc {
EventWindows::EventWindows()
    : _event(::CreateEvent(NULL  /* security attributes */,
                           FALSE /* manual reset */,
                           FALSE /* initial state */,
                           NULL  /* name of event */)),
      _timerID(NULL)
{
}

EventWindows::~EventWindows()
{
    CloseHandle(_event);
}

bool EventWindows::Set()
{
    // Note: setting an event that is already set has no effect.
    return SetEvent(_event) == 1 ? true : false;
}

bool EventWindows::Reset()
{
    return ResetEvent(_event) == 1 ? true : false;
}

EventTypeWrapper EventWindows::Wait(unsigned long maxTime)
{
    unsigned long res = WaitForSingleObject(_event, maxTime);
    switch(res)
    {
    case WAIT_OBJECT_0:
        return kEventSignaled;
    case WAIT_TIMEOUT:
        return kEventTimeout;
    default:
        return kEventError;
    }
}

bool EventWindows::StartTimer(bool periodic, unsigned long time)
{
    if (_timerID != NULL)
    {
        timeKillEvent(_timerID);
        _timerID=NULL;
    }
    if (periodic)
    {
        _timerID=timeSetEvent(time, 0,(LPTIMECALLBACK)HANDLE(_event),0,
                              TIME_PERIODIC|TIME_CALLBACK_EVENT_PULSE);
    } else {
        _timerID=timeSetEvent(time, 0,(LPTIMECALLBACK)HANDLE(_event),0,
                              TIME_ONESHOT|TIME_CALLBACK_EVENT_SET);
    }

    if (_timerID == NULL)
    {
        return false;
    }
    return true;
}

bool EventWindows::StopTimer()
{
    timeKillEvent(_timerID);
    _timerID = NULL;
    return true;
}
} // namespace webrtc
