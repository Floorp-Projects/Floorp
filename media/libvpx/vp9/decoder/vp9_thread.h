// Copyright 2013 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Multi-threaded worker
//
// Original source:
//  http://git.chromium.org/webm/libwebp.git
//  100644 blob 13a61a4c84194c3374080cbf03d881d3cd6af40d  src/utils/thread.h


#ifndef VP9_DECODER_VP9_THREAD_H_
#define VP9_DECODER_VP9_THREAD_H_

#include "./vpx_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_MULTITHREAD

#if defined(_WIN32)
#include <errno.h>  // NOLINT
#include <process.h>  // NOLINT
#include <windows.h>  // NOLINT
typedef HANDLE pthread_t;
typedef CRITICAL_SECTION pthread_mutex_t;
typedef struct {
  HANDLE waiting_sem_;
  HANDLE received_sem_;
  HANDLE signal_event_;
} pthread_cond_t;

//------------------------------------------------------------------------------
// simplistic pthread emulation layer

// _beginthreadex requires __stdcall
#define THREADFN unsigned int __stdcall
#define THREAD_RETURN(val) (unsigned int)((DWORD_PTR)val)

static INLINE int pthread_create(pthread_t* const thread, const void* attr,
                                 unsigned int (__stdcall *start)(void*),
                                 void* arg) {
  (void)attr;
  *thread = (pthread_t)_beginthreadex(NULL,   /* void *security */
                                      0,      /* unsigned stack_size */
                                      start,
                                      arg,
                                      0,      /* unsigned initflag */
                                      NULL);  /* unsigned *thrdaddr */
  if (*thread == NULL) return 1;
  SetThreadPriority(*thread, THREAD_PRIORITY_ABOVE_NORMAL);
  return 0;
}

static INLINE int pthread_join(pthread_t thread, void** value_ptr) {
  (void)value_ptr;
  return (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0 ||
          CloseHandle(thread) == 0);
}

// Mutex
static INLINE int pthread_mutex_init(pthread_mutex_t *const mutex,
                                     void* mutexattr) {
  (void)mutexattr;
  InitializeCriticalSection(mutex);
  return 0;
}

static INLINE int pthread_mutex_trylock(pthread_mutex_t *const mutex) {
  return TryEnterCriticalSection(mutex) ? 0 : EBUSY;
}

static INLINE int pthread_mutex_lock(pthread_mutex_t *const mutex) {
  EnterCriticalSection(mutex);
  return 0;
}

static INLINE int pthread_mutex_unlock(pthread_mutex_t *const mutex) {
  LeaveCriticalSection(mutex);
  return 0;
}

static INLINE int pthread_mutex_destroy(pthread_mutex_t *const mutex) {
  DeleteCriticalSection(mutex);
  return 0;
}

// Condition
static INLINE int pthread_cond_destroy(pthread_cond_t *const condition) {
  int ok = 1;
  ok &= (CloseHandle(condition->waiting_sem_) != 0);
  ok &= (CloseHandle(condition->received_sem_) != 0);
  ok &= (CloseHandle(condition->signal_event_) != 0);
  return !ok;
}

static INLINE int pthread_cond_init(pthread_cond_t *const condition,
                                    void* cond_attr) {
  (void)cond_attr;
  condition->waiting_sem_ = CreateSemaphore(NULL, 0, 1, NULL);
  condition->received_sem_ = CreateSemaphore(NULL, 0, 1, NULL);
  condition->signal_event_ = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (condition->waiting_sem_ == NULL ||
      condition->received_sem_ == NULL ||
      condition->signal_event_ == NULL) {
    pthread_cond_destroy(condition);
    return 1;
  }
  return 0;
}

static INLINE int pthread_cond_signal(pthread_cond_t *const condition) {
  int ok = 1;
  if (WaitForSingleObject(condition->waiting_sem_, 0) == WAIT_OBJECT_0) {
    // a thread is waiting in pthread_cond_wait: allow it to be notified
    ok = SetEvent(condition->signal_event_);
    // wait until the event is consumed so the signaler cannot consume
    // the event via its own pthread_cond_wait.
    ok &= (WaitForSingleObject(condition->received_sem_, INFINITE) !=
           WAIT_OBJECT_0);
  }
  return !ok;
}

static INLINE int pthread_cond_wait(pthread_cond_t *const condition,
                                    pthread_mutex_t *const mutex) {
  int ok;
  // note that there is a consumer available so the signal isn't dropped in
  // pthread_cond_signal
  if (!ReleaseSemaphore(condition->waiting_sem_, 1, NULL))
    return 1;
  // now unlock the mutex so pthread_cond_signal may be issued
  pthread_mutex_unlock(mutex);
  ok = (WaitForSingleObject(condition->signal_event_, INFINITE) ==
        WAIT_OBJECT_0);
  ok &= ReleaseSemaphore(condition->received_sem_, 1, NULL);
  pthread_mutex_lock(mutex);
  return !ok;
}
#else  // _WIN32
#include <pthread.h> // NOLINT
# define THREADFN void*
# define THREAD_RETURN(val) val
#endif

#endif  // CONFIG_MULTITHREAD

// State of the worker thread object
typedef enum {
  NOT_OK = 0,   // object is unusable
  OK,           // ready to work
  WORK          // busy finishing the current task
} VP9WorkerStatus;

// Function to be called by the worker thread. Takes two opaque pointers as
// arguments (data1 and data2), and should return false in case of error.
typedef int (*VP9WorkerHook)(void*, void*);

// Synchronize object used to launch job in the worker thread
typedef struct {
#if CONFIG_MULTITHREAD
  pthread_mutex_t mutex_;
  pthread_cond_t  condition_;
  pthread_t       thread_;
#endif
  VP9WorkerStatus status_;
  VP9WorkerHook hook;     // hook to call
  void* data1;            // first argument passed to 'hook'
  void* data2;            // second argument passed to 'hook'
  int had_error;          // return value of the last call to 'hook'
} VP9Worker;

// Must be called first, before any other method.
void vp9_worker_init(VP9Worker* const worker);
// Must be called to initialize the object and spawn the thread. Re-entrant.
// Will potentially launch the thread. Returns false in case of error.
int vp9_worker_reset(VP9Worker* const worker);
// Makes sure the previous work is finished. Returns true if worker->had_error
// was not set and no error condition was triggered by the working thread.
int vp9_worker_sync(VP9Worker* const worker);
// Triggers the thread to call hook() with data1 and data2 argument. These
// hook/data1/data2 can be changed at any time before calling this function,
// but not be changed afterward until the next call to vp9_worker_sync().
void vp9_worker_launch(VP9Worker* const worker);
// This function is similar to vp9_worker_launch() except that it calls the
// hook directly instead of using a thread. Convenient to bypass the thread
// mechanism while still using the VP9Worker structs. vp9_worker_sync() must
// still be called afterward (for error reporting).
void vp9_worker_execute(VP9Worker* const worker);
// Kill the thread and terminate the object. To use the object again, one
// must call vp9_worker_reset() again.
void vp9_worker_end(VP9Worker* const worker);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  // VP9_DECODER_VP9_THREAD_H_
