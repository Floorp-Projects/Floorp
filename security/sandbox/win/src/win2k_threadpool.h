// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_WIN2K_THREADPOOL_H_
#define SANDBOX_SRC_WIN2K_THREADPOOL_H_

#include <list>
#include <algorithm>
#include "sandbox/win/src/crosscall_server.h"

namespace sandbox {

// Win2kThreadPool a simple implementation of a thread provider as required
// for the sandbox IPC subsystem. See sandbox\crosscall_server.h for the details
// and requirements of this interface.
//
// Implementing the thread provider as a thread pool is desirable in the case
// of shared memory IPC because it can generate a large number of waitable
// events: as many as channels. A thread pool does not create a thread per
// event, instead maintains a few idle threads but can create more if the need
// arises.
//
// This implementation simply thunks to the nice thread pool API of win2k.
class Win2kThreadPool : public ThreadProvider {
 public:
  Win2kThreadPool() {
    ::InitializeCriticalSection(&lock_);
  }
  virtual ~Win2kThreadPool();

  virtual bool RegisterWait(const void* cookie, HANDLE waitable_object,
                            CrossCallIPCCallback callback,
                            void* context);

  virtual bool UnRegisterWaits(void* cookie);

  // Returns the total number of wait objects associated with
  // the thread pool.
  size_t OutstandingWaits();

 private:
  // record to keep track of a wait and its associated cookie.
  struct PoolObject {
    const void* cookie;
    HANDLE wait;
  };
  // The list of pool wait objects.
  typedef std::list<PoolObject> PoolObjects;
  PoolObjects pool_objects_;
  // This lock protects the list of pool wait objects.
  CRITICAL_SECTION lock_;
  DISALLOW_COPY_AND_ASSIGN(Win2kThreadPool);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_WIN2K_THREADPOOL_H_
