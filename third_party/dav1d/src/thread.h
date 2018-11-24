/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __DAV1D_THREAD_H__
# define __DAV1D_THREAD_H__

#if defined(_WIN32)

#include <windows.h>

#define PTHREAD_ONCE_INIT INIT_ONCE_STATIC_INIT

typedef SRWLOCK pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;
typedef INIT_ONCE pthread_once_t;
typedef void *pthread_t;
typedef void *pthread_mutexattr_t;
typedef void *pthread_condattr_t;
typedef void *pthread_attr_t;

int dav1d_pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                         void*(*proc)(void*), void* param);
void dav1d_pthread_join(pthread_t thread, void** res);
int dav1d_pthread_once(pthread_once_t *once_control,
                       void (*init_routine)(void));

#define pthread_create dav1d_pthread_create
#define pthread_join   dav1d_pthread_join
#define pthread_once   dav1d_pthread_once

static inline void pthread_mutex_init(pthread_mutex_t* mutex,
                                      const pthread_mutexattr_t* attr)
{
    (void)attr;
    InitializeSRWLock(mutex);
}

static inline void pthread_mutex_destroy(pthread_mutex_t* mutex) {
    (void)mutex;
}

static inline void pthread_mutex_lock(pthread_mutex_t* mutex) {
    AcquireSRWLockExclusive(mutex);
}

static inline void pthread_mutex_unlock(pthread_mutex_t* mutex) {
    ReleaseSRWLockExclusive(mutex);
}

static inline void pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
    (void)attr;
    InitializeConditionVariable(cond);
}

static inline void pthread_cond_destroy(pthread_cond_t* cond) {
    (void)cond;
}

static inline void pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    SleepConditionVariableSRW(cond, mutex, INFINITE, 0);
}

static inline void pthread_cond_signal(pthread_cond_t* cond) {
    WakeConditionVariable(cond);
}

static inline void pthread_cond_broadcast(pthread_cond_t* cond) {
    WakeAllConditionVariable(cond);
}

#else

#include <pthread.h>

#endif

#endif // __DAV1D_THREAD_H__
