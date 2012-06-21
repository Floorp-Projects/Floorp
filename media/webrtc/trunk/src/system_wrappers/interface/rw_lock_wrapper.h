/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_RW_LOCK_WRAPPER_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_RW_LOCK_WRAPPER_H_

// Note, Windows pre-Vista version of RW locks are not supported nativly. For
// these OSs regular critical sections have been used to approximate RW lock
// functionality and will therefore have worse performance.

namespace webrtc {
class RWLockWrapper
{
public:
    static RWLockWrapper* CreateRWLock();
    virtual ~RWLockWrapper();

    virtual void AcquireLockExclusive() = 0;
    virtual void ReleaseLockExclusive() = 0;

    virtual void AcquireLockShared() = 0;
    virtual void ReleaseLockShared() = 0;

protected:
    virtual int Init() = 0;
};

// RAII extensions of the RW lock. Prevents Acquire/Release missmatches and
// provides more compact locking syntax.
class ReadLockScoped
{
public:
    ReadLockScoped(RWLockWrapper& rwLock)
        :
        _rwLock(rwLock)
    {
        _rwLock.AcquireLockShared();
    }

    ~ReadLockScoped()
    {
        _rwLock.ReleaseLockShared();
    }

private:
    RWLockWrapper& _rwLock;
};

class WriteLockScoped
{
public:
    WriteLockScoped(RWLockWrapper& rwLock)
        :
        _rwLock(rwLock)
    {
        _rwLock.AcquireLockExclusive();
    }

    ~WriteLockScoped()
    {
        _rwLock.ReleaseLockExclusive();
    }

private:
    RWLockWrapper& _rwLock;
};
} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_RW_LOCK_WRAPPER_H_
