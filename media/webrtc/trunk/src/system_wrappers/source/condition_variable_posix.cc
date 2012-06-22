/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "condition_variable_posix.h"

#if defined(WEBRTC_LINUX)
#include <ctime>
#else
#include <sys/time.h>
#endif

#include <errno.h>

#include "critical_section_posix.h"

namespace webrtc {
ConditionVariableWrapper* ConditionVariablePosix::Create()
{
    ConditionVariablePosix* ptr = new ConditionVariablePosix;
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

ConditionVariablePosix::ConditionVariablePosix()
{
}

int ConditionVariablePosix::Construct()
{
    int result = 0;
#ifdef WEBRTC_CLOCK_TYPE_REALTIME
    result = pthread_cond_init(&_cond, NULL);
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
    result = pthread_cond_init(&_cond, &condAttr);
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

ConditionVariablePosix::~ConditionVariablePosix()
{
    pthread_cond_destroy(&_cond);
}

void ConditionVariablePosix::SleepCS(CriticalSectionWrapper& critSect)
{
    CriticalSectionPosix* cs = reinterpret_cast<CriticalSectionPosix*>(
                                   &critSect);
    pthread_cond_wait(&_cond, &cs->_mutex);
}


bool
ConditionVariablePosix::SleepCS(
    CriticalSectionWrapper& critSect,
    unsigned long maxTimeInMS)
{
    const unsigned long INFINITE =  0xFFFFFFFF;

    const int MILLISECONDS_PER_SECOND      = 1000;
#ifndef WEBRTC_LINUX
    const int MICROSECONDS_PER_MILLISECOND = 1000;
#endif
    const int NANOSECONDS_PER_SECOND       = 1000000000;
    const int NANOSECONDS_PER_MILLISECOND  = 1000000;

    CriticalSectionPosix* cs = reinterpret_cast<CriticalSectionPosix*>(
                                   &critSect);

    if (maxTimeInMS != INFINITE)
    {
        timespec ts;
#ifndef WEBRTC_MAC
#ifdef WEBRTC_CLOCK_TYPE_REALTIME
        clock_gettime(CLOCK_REALTIME, &ts);
#else
        clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
#else
        struct timeval tv;
        gettimeofday(&tv, 0);
        ts.tv_sec  = tv.tv_sec;
        ts.tv_nsec = tv.tv_usec * MICROSECONDS_PER_MILLISECOND;
#endif

        ts.tv_sec += maxTimeInMS / MILLISECONDS_PER_SECOND;
        ts.tv_nsec += (maxTimeInMS - ((maxTimeInMS / MILLISECONDS_PER_SECOND)*
                      MILLISECONDS_PER_SECOND)) * NANOSECONDS_PER_MILLISECOND;

        if (ts.tv_nsec >= NANOSECONDS_PER_SECOND)
        {
            ts.tv_sec += ts.tv_nsec / NANOSECONDS_PER_SECOND;
            ts.tv_nsec %= NANOSECONDS_PER_SECOND;
        }
        const int res = pthread_cond_timedwait(&_cond, &cs->_mutex, &ts);
        return (res == ETIMEDOUT) ? false : true;
    }
    else
    {
        pthread_cond_wait(&_cond, &cs->_mutex);
        return true;
    }
}

void ConditionVariablePosix::Wake()
{
    pthread_cond_signal(&_cond);
}

void ConditionVariablePosix::WakeAll()
{
    pthread_cond_broadcast(&_cond);
}
} // namespace webrtc
