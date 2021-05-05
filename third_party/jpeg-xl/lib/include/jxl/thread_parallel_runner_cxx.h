// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// @file thread_parallel_runner_cxx.h
/// @brief C++ header-only helper for @ref thread_parallel_runner.h.
///
/// There's no binary library associated with the header since this is a header
/// only library.

#ifndef JXL_THREAD_PARALLEL_RUNNER_CXX_H_
#define JXL_THREAD_PARALLEL_RUNNER_CXX_H_

#include <memory>

#include "jxl/thread_parallel_runner.h"

#if !(defined(__cplusplus) || defined(c_plusplus))
#error \
    "This a C++ only header. Use jxl/jxl_thread_parallel_runner.h from C" \
    "sources."
#endif

/// Struct to call JxlThreadParallelRunnerDestroy from the
/// JxlThreadParallelRunnerPtr unique_ptr.
struct JxlThreadParallelRunnerDestroyStruct {
  /// Calls @ref JxlThreadParallelRunnerDestroy() on the passed runner.
  void operator()(void* runner) { JxlThreadParallelRunnerDestroy(runner); }
};

/// std::unique_ptr<> type that calls JxlThreadParallelRunnerDestroy() when
/// releasing the runner.
///
/// Use this helper type from C++ sources to ensure the runner is destroyed and
/// their internal resources released.
typedef std::unique_ptr<void, JxlThreadParallelRunnerDestroyStruct>
    JxlThreadParallelRunnerPtr;

/// Creates an instance of JxlThreadParallelRunner into a
/// JxlThreadParallelRunnerPtr and initializes it.
///
/// This function returns a unique_ptr that will call
/// JxlThreadParallelRunnerDestroy() when releasing the pointer. See @ref
/// JxlThreadParallelRunnerCreate for details on the instance creation.
///
/// @param memory_manager custom allocator function. It may be NULL. The memory
///        manager will be copied internally.
/// @param num_worker_threads the number of worker threads to create.
/// @return a @c NULL JxlThreadParallelRunnerPtr if the instance can not be
/// allocated or initialized
/// @return initialized JxlThreadParallelRunnerPtr instance otherwise.
static inline JxlThreadParallelRunnerPtr JxlThreadParallelRunnerMake(
    const JxlMemoryManager* memory_manager, size_t num_worker_threads) {
  return JxlThreadParallelRunnerPtr(
      JxlThreadParallelRunnerCreate(memory_manager, num_worker_threads));
}

#endif  // JXL_THREAD_PARALLEL_RUNNER_CXX_H_
