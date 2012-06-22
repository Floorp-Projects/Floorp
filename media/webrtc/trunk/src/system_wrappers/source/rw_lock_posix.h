/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_RW_LOCK_POSIX_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_RW_LOCK_POSIX_H_

#include "rw_lock_wrapper.h"

#include <pthread.h>

namespace webrtc {
class RWLockPosix : public RWLockWrapper
{
public:
    RWLockPosix();
    virtual ~RWLockPosix();

    virtual void AcquireLockExclusive();
    virtual void ReleaseLockExclusive();

    virtual void AcquireLockShared();
    virtual void ReleaseLockShared();

protected:
    virtual int Init();

private:
    pthread_rwlock_t _lock;
};
} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_RW_LOCK_POSIX_H_
