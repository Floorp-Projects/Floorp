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

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#if CONFIG_MULTITHREAD

#if defined(_WIN32)

#include <windows.h>  // NOLINT
typedef HANDLE pthread_t;
typedef CRITICAL_SECTION pthread_mutex_t;
typedef struct {
  HANDLE waiting_sem_;
  HANDLE received_sem_;
  HANDLE signal_event_;
} pthread_cond_t;

#else

#include <pthread.h> // NOLINT

#endif    /* _WIN32 */
#endif    /* CONFIG_MULTITHREAD */

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

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  // VP9_DECODER_VP9_THREAD_H_
