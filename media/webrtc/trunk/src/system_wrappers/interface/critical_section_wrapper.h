/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_CRITICAL_SECTION_WRAPPER_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_CRITICAL_SECTION_WRAPPER_H_

// If the critical section is heavily contended it may be beneficial to use
// read/write locks instead.

#include "common_types.h"

namespace webrtc {
class CriticalSectionWrapper
{
public:
    // Factory method, constructor disabled
    static CriticalSectionWrapper* CreateCriticalSection();

    virtual ~CriticalSectionWrapper() {}

    // Tries to grab lock, beginning of a critical section. Will wait for the
    // lock to become available if the grab failed.
    virtual void Enter() = 0;

    // Returns a grabbed lock, end of critical section.
    virtual void Leave() = 0;
};

// RAII extension of the critical section. Prevents Enter/Leave mismatches and
// provides more compact critical section syntax.
class CriticalSectionScoped
{
public:
    // Deprecated, don't add more users of this constructor.
    // TODO(mflodman) Remove this version of the constructor when no one is
    // using it any longer.
    explicit CriticalSectionScoped(CriticalSectionWrapper& critsec)
        : _ptrCritSec(&critsec)
    {
        _ptrCritSec->Enter();
    }

    explicit CriticalSectionScoped(CriticalSectionWrapper* critsec)
        : _ptrCritSec(critsec)
    {
      _ptrCritSec->Enter();
    }

    ~CriticalSectionScoped()
    {
        if (_ptrCritSec)
        {
            Leave();
        }
    }

private:
    void Leave()
    {
        _ptrCritSec->Leave();
        _ptrCritSec = 0;
    }

    CriticalSectionWrapper* _ptrCritSec;
};
} // namespace webrtc
#endif // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_CRITICAL_SECTION_WRAPPER_H_
