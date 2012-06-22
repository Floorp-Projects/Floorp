/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_RW_LOCK_WINDOWS_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_RW_LOCK_WINDOWS_H_

#include "rw_lock_wrapper.h"

#include <Windows.h>

#if !defined(RTL_SRWLOCK_INIT)
    typedef struct _RTL_SRWLOCK
    {
        void* Ptr;
    } RTL_SRWLOCK, *PRTL_SRWLOCK;
    typedef RTL_SRWLOCK SRWLOCK, *PSRWLOCK;
#endif

namespace webrtc {
class CriticalSectionWrapper;
class ConditionVariableWrapper;

typedef void (WINAPI *PInitializeSRWLock)(PSRWLOCK);

typedef void (WINAPI *PAcquireSRWLockExclusive)(PSRWLOCK);
typedef void (WINAPI *PReleaseSRWLockExclusive)(PSRWLOCK);

typedef void (WINAPI *PAcquireSRWLockShared)(PSRWLOCK);
typedef void (WINAPI *PReleaseSRWLockShared)(PSRWLOCK);


class RWLockWindows :public RWLockWrapper
{
public:
    RWLockWindows();
    virtual ~RWLockWindows();

    virtual void AcquireLockExclusive();
    virtual void ReleaseLockExclusive();

    virtual void AcquireLockShared();
    virtual void ReleaseLockShared();

protected:
    virtual int Init();

private:
    // For native implementation.
    static bool _winSupportRWLockPrimitive;
    SRWLOCK     _lock;

    // Genric implementation, fallback if native is not supported.
    CriticalSectionWrapper*   _critSectPtr;
    ConditionVariableWrapper* _readCondPtr;
    ConditionVariableWrapper* _writeCondPtr;

    int  _readersActive;
    bool _writerActive;
    int  _readersWaiting;
    int  _writersWaiting;
};
} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_RW_LOCK_WINDOWS_H_
