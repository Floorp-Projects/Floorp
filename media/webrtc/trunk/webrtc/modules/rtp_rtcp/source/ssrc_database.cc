/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/ssrc_database.h"

#include <assert.h>
#include <stdlib.h>

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef _WIN32
    #include <windows.h>
    #include <MMSystem.h> //timeGetTime

// TODO(hellner): investigate if it is necessary to disable these warnings.
    #pragma warning(disable:4311)
    #pragma warning(disable:4312)
#else
    #include <stdio.h>
    #include <string.h>
    #include <time.h>
    #include <sys/time.h>
#endif

namespace webrtc {
SSRCDatabase*
SSRCDatabase::StaticInstance(CountOperation count_operation)
{
  SSRCDatabase* impl =
      GetStaticInstance<SSRCDatabase>(count_operation);
  return impl;
}

SSRCDatabase*
SSRCDatabase::GetSSRCDatabase()
{
    return StaticInstance(kAddRef);
}

void
SSRCDatabase::ReturnSSRCDatabase()
{
    StaticInstance(kRelease);
}

uint32_t
SSRCDatabase::CreateSSRC()
{
    CriticalSectionScoped lock(_critSect);

    uint32_t ssrc = GenerateRandom();

#ifndef WEBRTC_NO_STL

    while(_ssrcMap.find(ssrc) != _ssrcMap.end())
    {
        ssrc = GenerateRandom();
    }
    _ssrcMap[ssrc] = 0;

#else
    if(_sizeOfSSRC <= _numberOfSSRC)
    {
        // allocate more space
        const int newSize = _sizeOfSSRC + 10;
        uint32_t* tempSSRCVector = new uint32_t[newSize];
        memcpy(tempSSRCVector, _ssrcVector, _sizeOfSSRC*sizeof(uint32_t));
        delete [] _ssrcVector;

        _ssrcVector = tempSSRCVector;
        _sizeOfSSRC = newSize;
    }

    // check if in DB
    if(_ssrcVector)
    {
        for (int i=0; i<_numberOfSSRC; i++)
        {
            if (_ssrcVector[i] == ssrc)
            {
                // we have a match
                i = 0; // start over with a new ssrc
                ssrc = GenerateRandom();
            }

        }
        //  add to database
        _ssrcVector[_numberOfSSRC] = ssrc;
        _numberOfSSRC++;
    }
#endif
    return ssrc;
}

int32_t
SSRCDatabase::RegisterSSRC(const uint32_t ssrc)
{
    CriticalSectionScoped lock(_critSect);

#ifndef WEBRTC_NO_STL

    _ssrcMap[ssrc] = 0;

#else
    if(_sizeOfSSRC <= _numberOfSSRC)
    {
        // allocate more space
        const int newSize = _sizeOfSSRC + 10;
        uint32_t* tempSSRCVector = new uint32_t[newSize];
        memcpy(tempSSRCVector, _ssrcVector, _sizeOfSSRC*sizeof(uint32_t));
        delete [] _ssrcVector;

        _ssrcVector = tempSSRCVector;
        _sizeOfSSRC = newSize;
    }
    // check if in DB
    if(_ssrcVector)
    {
        for (int i=0; i<_numberOfSSRC; i++)
        {
            if (_ssrcVector[i] == ssrc)
            {
                // we have a match
                return -1;
            }
        }
        //  add to database
        _ssrcVector[_numberOfSSRC] = ssrc;
        _numberOfSSRC++;
    }
#endif
    return 0;
}

int32_t
SSRCDatabase::ReturnSSRC(const uint32_t ssrc)
{
    CriticalSectionScoped lock(_critSect);

#ifndef WEBRTC_NO_STL
    _ssrcMap.erase(ssrc);

#else
    if(_ssrcVector)
    {
        for (int i=0; i<_numberOfSSRC; i++)
        {
            if (_ssrcVector[i] == ssrc)
            {
                // we have a match
                // remove from database
                _ssrcVector[i] = _ssrcVector[_numberOfSSRC-1];
                _numberOfSSRC--;
                break;
            }
        }
    }
#endif
    return 0;
}

SSRCDatabase::SSRCDatabase()
{
    // we need to seed the random generator, otherwise we get 26500 each time, hardly a random value :)
#ifdef _WIN32
    srand(timeGetTime());
#else
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    srand(tv.tv_usec);
#endif

#ifdef WEBRTC_NO_STL
    _sizeOfSSRC = 10;
    _numberOfSSRC = 0;
    _ssrcVector = new uint32_t[10];
#endif
    _critSect = CriticalSectionWrapper::CreateCriticalSection();

    WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, -1, "%s created", __FUNCTION__);
}

SSRCDatabase::~SSRCDatabase()
{
#ifdef WEBRTC_NO_STL
    delete [] _ssrcVector;
#else
    _ssrcMap.clear();
#endif
    delete _critSect;

    WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, -1, "%s deleted", __FUNCTION__);
}

uint32_t SSRCDatabase::GenerateRandom()
{
    uint32_t ssrc = 0;
    do
    {
        ssrc = rand();
        ssrc = ssrc <<16;
        ssrc += rand();

    } while (ssrc == 0 || ssrc == 0xffffffff);

    return ssrc;
}
}  // namespace webrtc
