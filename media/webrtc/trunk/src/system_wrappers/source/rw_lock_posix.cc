/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rw_lock_posix.h"

namespace webrtc {
RWLockPosix::RWLockPosix() : _lock()
{
}

RWLockPosix::~RWLockPosix()
{
    pthread_rwlock_destroy(&_lock);
}

int RWLockPosix::Init()
{
    return pthread_rwlock_init(&_lock, 0);
}

void RWLockPosix::AcquireLockExclusive()
{
    pthread_rwlock_wrlock(&_lock);
}

void RWLockPosix::ReleaseLockExclusive()
{
    pthread_rwlock_unlock(&_lock);
}

void RWLockPosix::AcquireLockShared()
{
    pthread_rwlock_rdlock(&_lock);
}

void RWLockPosix::ReleaseLockShared()
{
    pthread_rwlock_unlock(&_lock);
}
} // namespace webrtc
