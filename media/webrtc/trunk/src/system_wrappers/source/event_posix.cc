/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "event_posix.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

namespace webrtc {
const long int E6 = 1000000;
const long int E9 = 1000 * E6;

EventWrapper* EventPosix::Create()
{
    EventPosix* ptr = new EventPosix;
    if (!ptr)
    {
        return NULL;
    }

    const int error = ptr->Construct();
    if (error)
    {
        delete ptr;
        return NULL;
    }
    return ptr;
}


EventPosix::EventPosix()
    : _timerThread(0),
      _timerEvent(0),
      _periodic(false),
      _time(0),
      _count(0),
      _state(kDown)
{
}

int EventPosix::Construct()
{
    // Set start time to zero
    memset(&_tCreate, 0, sizeof(_tCreate));

    int result = pthread_mutex_init(&mutex, 0);
    if (result != 0)
    {
        return -1;
    }
#ifdef WEBRTC_CLOCK_TYPE_REALTIME
    result = pthread_cond_init(&cond, 0);
    if (result != 0)
    {
        return -1;
    }
#else
    pthread_condattr_t condAttr;
    result = pthread_condattr_init(&condAttr);
    if (result != 0)
    {
        return -1;
    }
    result = pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    if (result != 0)
    {
        return -1;
    }
    result = pthread_cond_init(&cond, &condAttr);
    if (result != 0)
    {
        return -1;
    }
    result = pthread_condattr_destroy(&condAttr);
    if (result != 0)
    {
        return -1;
    }
#endif
    return 0;
}

EventPosix::~EventPosix()
{
    StopTimer();
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
}

bool EventPosix::Reset()
{
    if (0 != pthread_mutex_lock(&mutex))
    {
        return false;
    }
    _state = kDown;
    pthread_mutex_unlock(&mutex);
    return true;
}

bool EventPosix::Set()
{
    if (0 != pthread_mutex_lock(&mutex))
    {
        return false;
    }
    _state = kUp;
     // Release all waiting threads
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
    return true;
}

EventTypeWrapper EventPosix::Wait(unsigned long timeout)
{
    int retVal = 0;
    if (0 != pthread_mutex_lock(&mutex))
    {
        return kEventError;
    }

    if (kDown == _state)
    {
        if (WEBRTC_EVENT_INFINITE != timeout)
        {
            timespec tEnd;
#ifndef WEBRTC_MAC
#ifdef WEBRTC_CLOCK_TYPE_REALTIME
            clock_gettime(CLOCK_REALTIME, &tEnd);
#else
            clock_gettime(CLOCK_MONOTONIC, &tEnd);
#endif
#else
            timeval tVal;
            struct timezone tZone;
            tZone.tz_minuteswest = 0;
            tZone.tz_dsttime = 0;
            gettimeofday(&tVal,&tZone);
            TIMEVAL_TO_TIMESPEC(&tVal,&tEnd);
#endif
            tEnd.tv_sec  += timeout / 1000;
            tEnd.tv_nsec += (timeout - (timeout / 1000) * 1000) * E6;

            if (tEnd.tv_nsec >= E9)
            {
                tEnd.tv_sec++;
                tEnd.tv_nsec -= E9;
            }
            retVal = pthread_cond_timedwait(&cond, &mutex, &tEnd);
        } else {
            retVal = pthread_cond_wait(&cond, &mutex);
        }
    }

    _state = kDown;
    pthread_mutex_unlock(&mutex);

    switch(retVal)
    {
    case 0:
        return kEventSignaled;
    case ETIMEDOUT:
        return kEventTimeout;
    default:
        return kEventError;
    }
}

EventTypeWrapper EventPosix::Wait(timespec& tPulse)
{
    int retVal = 0;
    if (0 != pthread_mutex_lock(&mutex))
    {
        return kEventError;
    }

    if (kUp != _state)
    {
        retVal = pthread_cond_timedwait(&cond, &mutex, &tPulse);
    }
    _state = kDown;

    pthread_mutex_unlock(&mutex);

    switch(retVal)
    {
    case 0:
        return kEventSignaled;
    case ETIMEDOUT:
        return kEventTimeout;
    default:
        return kEventError;
    }
}

bool EventPosix::StartTimer(bool periodic, unsigned long time)
{
    if (_timerThread)
    {
        if(_periodic)
        {
            // Timer already started.
            return false;
        } else  {
            // New one shot timer
            _time = time;
            _tCreate.tv_sec = 0;
            _timerEvent->Set();
            return true;
        }
    }

    // Start the timer thread
    _timerEvent = static_cast<EventPosix*>(EventWrapper::Create());
    const char* threadName = "WebRtc_event_timer_thread";
    _timerThread = ThreadWrapper::CreateThread(Run, this, kRealtimePriority,
                                               threadName);
    _periodic = periodic;
    _time = time;
    unsigned int id = 0;
    if (_timerThread->Start(id))
    {
        return true;
    }
    return false;
}

bool EventPosix::Run(ThreadObj obj)
{
    return static_cast<EventPosix*>(obj)->Process();
}

bool EventPosix::Process()
{
    if (_tCreate.tv_sec == 0)
    {
#ifndef WEBRTC_MAC
#ifdef WEBRTC_CLOCK_TYPE_REALTIME
        clock_gettime(CLOCK_REALTIME, &_tCreate);
#else
        clock_gettime(CLOCK_MONOTONIC, &_tCreate);
#endif
#else
        timeval tVal;
        struct timezone tZone;
        tZone.tz_minuteswest = 0;
        tZone.tz_dsttime = 0;
        gettimeofday(&tVal,&tZone);
        TIMEVAL_TO_TIMESPEC(&tVal,&_tCreate);
#endif
        _count=0;
    }

    timespec tEnd;
    unsigned long long time = _time * ++_count;
    tEnd.tv_sec  = _tCreate.tv_sec + time/1000;
    tEnd.tv_nsec = _tCreate.tv_nsec + (time - (time/1000)*1000)*E6;

    if ( tEnd.tv_nsec >= E9 )
    {
        tEnd.tv_sec++;
        tEnd.tv_nsec -= E9;
    }

    switch(_timerEvent->Wait(tEnd))
    {
    case kEventSignaled:
        return true;
    case kEventError:
        return false;
    case kEventTimeout:
        break;
    }
    if(_periodic || _count==1)
    {
        Set();
    }
    return true;
}

bool EventPosix::StopTimer()
{
    if(_timerThread)
    {
        _timerThread->SetNotAlive();
    }
    if (_timerEvent)
    {
        _timerEvent->Set();
    }
    if (_timerThread)
    {
        if(!_timerThread->Stop())
        {
            return false;
        }

        delete _timerThread;
        _timerThread = 0;
    }
    if (_timerEvent)
    {
        delete _timerEvent;
        _timerEvent = 0;
    }

    // Set time to zero to force new reference time for the timer.
    memset(&_tCreate, 0, sizeof(_tCreate));
    _count=0;
    return true;
}
} // namespace webrtc
