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
//  100644 blob eff8f2a8c20095aade3c292b0e9292dac6cb3587  src/utils/thread.c


#include <assert.h>
#include <string.h>   // for memset()
#include "./vp9_thread.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#if CONFIG_MULTITHREAD

//------------------------------------------------------------------------------

static THREADFN thread_loop(void *ptr) {    // thread loop
  VP9Worker* const worker = (VP9Worker*)ptr;
  int done = 0;
  while (!done) {
    pthread_mutex_lock(&worker->mutex_);
    while (worker->status_ == OK) {   // wait in idling mode
      pthread_cond_wait(&worker->condition_, &worker->mutex_);
    }
    if (worker->status_ == WORK) {
      vp9_worker_execute(worker);
      worker->status_ = OK;
    } else if (worker->status_ == NOT_OK) {   // finish the worker
      done = 1;
    }
    // signal to the main thread that we're done (for Sync())
    pthread_cond_signal(&worker->condition_);
    pthread_mutex_unlock(&worker->mutex_);
  }
  return THREAD_RETURN(NULL);    // Thread is finished
}

// main thread state control
static void change_state(VP9Worker* const worker,
                         VP9WorkerStatus new_status) {
  // no-op when attempting to change state on a thread that didn't come up
  if (worker->status_ < OK) return;

  pthread_mutex_lock(&worker->mutex_);
  // wait for the worker to finish
  while (worker->status_ != OK) {
    pthread_cond_wait(&worker->condition_, &worker->mutex_);
  }
  // assign new status and release the working thread if needed
  if (new_status != OK) {
    worker->status_ = new_status;
    pthread_cond_signal(&worker->condition_);
  }
  pthread_mutex_unlock(&worker->mutex_);
}

#endif  // CONFIG_MULTITHREAD

//------------------------------------------------------------------------------

void vp9_worker_init(VP9Worker* const worker) {
  memset(worker, 0, sizeof(*worker));
  worker->status_ = NOT_OK;
}

int vp9_worker_sync(VP9Worker* const worker) {
#if CONFIG_MULTITHREAD
  change_state(worker, OK);
#endif
  assert(worker->status_ <= OK);
  return !worker->had_error;
}

int vp9_worker_reset(VP9Worker* const worker) {
  int ok = 1;
  worker->had_error = 0;
  if (worker->status_ < OK) {
#if CONFIG_MULTITHREAD
    if (pthread_mutex_init(&worker->mutex_, NULL) ||
        pthread_cond_init(&worker->condition_, NULL)) {
      return 0;
    }
    pthread_mutex_lock(&worker->mutex_);
    ok = !pthread_create(&worker->thread_, NULL, thread_loop, worker);
    if (ok) worker->status_ = OK;
    pthread_mutex_unlock(&worker->mutex_);
#else
    worker->status_ = OK;
#endif
  } else if (worker->status_ > OK) {
    ok = vp9_worker_sync(worker);
  }
  assert(!ok || (worker->status_ == OK));
  return ok;
}

void vp9_worker_execute(VP9Worker* const worker) {
  if (worker->hook != NULL) {
    worker->had_error |= !worker->hook(worker->data1, worker->data2);
  }
}

void vp9_worker_launch(VP9Worker* const worker) {
#if CONFIG_MULTITHREAD
  change_state(worker, WORK);
#else
  vp9_worker_execute(worker);
#endif
}

void vp9_worker_end(VP9Worker* const worker) {
  if (worker->status_ >= OK) {
#if CONFIG_MULTITHREAD
    change_state(worker, NOT_OK);
    pthread_join(worker->thread_, NULL);
    pthread_mutex_destroy(&worker->mutex_);
    pthread_cond_destroy(&worker->condition_);
#else
    worker->status_ = NOT_OK;
#endif
  }
  assert(worker->status_ == NOT_OK);
}

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
