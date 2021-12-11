// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_BASE_THREAD_POOL_INTERNAL_H_
#define LIB_JXL_BASE_THREAD_POOL_INTERNAL_H_

#include <stddef.h>

#include <cmath>

#include "jxl/parallel_runner.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/threads/thread_parallel_runner_internal.h"

namespace jxl {

// Helper class to pass an internal ThreadPool-like object using threads. This
// is only suitable for tests or tools that access the internal API of JPEG XL.
// In other cases the caller will provide a JxlParallelRunner() for handling
// this. This class uses jpegxl::ThreadParallelRunner (from jpegxl_threads
// library). For interface details check jpegxl::ThreadParallelRunner.
class ThreadPoolInternal : public ThreadPool {
 public:
  // Starts the given number of worker threads and blocks until they are ready.
  // "num_worker_threads" defaults to one per hyperthread. If zero, all tasks
  // run on the main thread.
  explicit ThreadPoolInternal(
      int num_worker_threads = std::thread::hardware_concurrency())
      : ThreadPool(&jpegxl::ThreadParallelRunner::Runner,
                   static_cast<void*>(&runner_)),
        runner_(num_worker_threads) {}

  ThreadPoolInternal(const ThreadPoolInternal&) = delete;
  ThreadPoolInternal& operator&(const ThreadPoolInternal&) = delete;

  size_t NumThreads() const { return runner_.NumThreads(); }
  size_t NumWorkerThreads() const { return runner_.NumWorkerThreads(); }

  template <class Func>
  void RunOnEachThread(const Func& func) {
    runner_.RunOnEachThread(func);
  }

 private:
  jpegxl::ThreadParallelRunner runner_;
};

}  // namespace jxl

#endif  // LIB_JXL_BASE_THREAD_POOL_INTERNAL_H_
