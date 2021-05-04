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

#ifndef LIB_JXL_BASE_DATA_PARALLEL_H_
#define LIB_JXL_BASE_DATA_PARALLEL_H_

// Portable, low-overhead C++11 ThreadPool alternative to OpenMP for
// data-parallel computations.

#include <stddef.h>
#include <stdint.h>

#include "jxl/parallel_runner.h"
#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/status.h"

namespace jxl {

class ThreadPool {
 public:
  // Use this type as an InitFunc to skip the initialization step in Run().
  // When this is used the return value of Run() is always true and does not
  // need to be checked.
  struct SkipInit {};

  ThreadPool(JxlParallelRunner runner, void* runner_opaque)
      : runner_(runner ? runner : &ThreadPool::SequentialRunnerStatic),
        runner_opaque_(runner ? runner_opaque : static_cast<void*>(this)) {}

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator&(const ThreadPool&) = delete;

  // Runs init_func(num_threads) followed by data_func(task, thread) on worker
  // thread(s) for every task in [begin, end). init_func() must return a Status
  // indicating whether the initialization succeeded.
  // "thread" is an integer smaller than num_threads.
  // Not thread-safe - no two calls to Run may overlap.
  // Subsequent calls will reuse the same threads.
  //
  // Precondition: begin <= end.
  template <class InitFunc, class DataFunc>
  Status Run(uint32_t begin, uint32_t end, const InitFunc& init_func,
             const DataFunc& data_func, const char* caller = "") {
    JXL_ASSERT(begin <= end);
    if (begin == end) return true;
    RunCallState<InitFunc, DataFunc> call_state(init_func, data_func);
    // The runner_ uses the C convention and returns 0 in case of error, so we
    // convert it to an Status.
    return (*runner_)(runner_opaque_, static_cast<void*>(&call_state),
                      &call_state.CallInitFunc, &call_state.CallDataFunc, begin,
                      end) == 0;
  }

  // Specialization that returns bool when SkipInit is used.
  template <class DataFunc>
  bool Run(uint32_t begin, uint32_t end, const SkipInit /* tag */,
           const DataFunc& data_func, const char* caller = "") {
    return Run(begin, end, ReturnTrueInit, data_func, caller);
  }

 private:
  static Status ReturnTrueInit(size_t num_threads) { return true; }

  // class holding the state of a Run() call to pass to the runner_ as an
  // opaque_jpegxl pointer.
  template <class InitFunc, class DataFunc>
  class RunCallState final {
   public:
    RunCallState(const InitFunc& init_func, const DataFunc& data_func)
        : init_func_(init_func), data_func_(data_func) {}

    // JxlParallelRunInit interface.
    static int CallInitFunc(void* jpegxl_opaque, size_t num_threads) {
      const auto* self =
          static_cast<RunCallState<InitFunc, DataFunc>*>(jpegxl_opaque);
      // Returns -1 when the internal init function returns false Status to
      // indicate an error.
      return self->init_func_(num_threads) ? 0 : -1;
    }

    // JxlParallelRunFunction interface.
    static void CallDataFunc(void* jpegxl_opaque, uint32_t value,
                             size_t thread_id) {
      const auto* self =
          static_cast<RunCallState<InitFunc, DataFunc>*>(jpegxl_opaque);
      return self->data_func_(value, thread_id);
    }

   private:
    const InitFunc& init_func_;
    const DataFunc& data_func_;
  };

  // Default JxlParallelRunner used when no runner is provided by the
  // caller. This runner doesn't use any threading and thread_id is always 0.
  static JxlParallelRetCode SequentialRunnerStatic(
      void* runner_opaque, void* jpegxl_opaque, JxlParallelRunInit init,
      JxlParallelRunFunction func, uint32_t start_range, uint32_t end_range);

  // The caller supplied runner function and its opaque void*.
  const JxlParallelRunner runner_;
  void* const runner_opaque_;
};

void TraceRunBegin(const char* caller, double* t0);
void TraceRunEnd(const char* caller, double t0);

// TODO(deymo): Convert the return value to a Status when not using SkipInit.
template <class InitFunc, class DataFunc>
bool RunOnPool(ThreadPool* pool, const uint32_t begin, const uint32_t end,
               const InitFunc& init_func, const DataFunc& data_func,
               const char* caller) {
  Status ret = true;
  double t0;
  TraceRunBegin(caller, &t0);
  if (pool == nullptr) {
    ThreadPool default_pool(nullptr, nullptr);
    ret = default_pool.Run(begin, end, init_func, data_func, caller);
  } else {
    ret = pool->Run(begin, end, init_func, data_func, caller);
  }
  TraceRunEnd(caller, t0);
  return ret;
}

// Accelerates multiple unsigned 32-bit divisions with the same divisor by
// precomputing a multiplier. This is useful for splitting a contiguous range of
// indices (the task index) into 2D indices. Exhaustively tested on dividends
// up to 4M with non-power of two divisors up to 2K.
class Divider {
 public:
  // "d" is the divisor (what to divide by).
  explicit Divider(const uint32_t d) : shift_(FloorLog2Nonzero(d)) {
    // Power of two divisors (including 1) are not supported because it is more
    // efficient to special-case them at a higher level.
    JXL_ASSERT((d & (d - 1)) != 0);

    // ceil_log2 = floor_log2 + 1 because we ruled out powers of two above.
    const uint64_t next_pow2 = 1ULL << (shift_ + 1);

    mul_ = ((next_pow2 - d) << 32) / d + 1;
  }

  // "n" is the numerator (what is being divided).
  inline uint32_t operator()(const uint32_t n) const {
    // Algorithm from "Division by Invariant Integers using Multiplication".
    // Its "sh1" is hardcoded to 1 because we don't need to handle d=1.
    const uint32_t hi = (uint64_t(mul_) * n) >> 32;
    return (hi + ((n - hi) >> 1)) >> shift_;
  }

 private:
  uint32_t mul_;
  const int shift_;
};

}  // namespace jxl

#endif  // LIB_JXL_BASE_DATA_PARALLEL_H_
