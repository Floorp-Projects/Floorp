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

#include "lib/jxl/base/data_parallel.h"

#define DATA_PARALLEL_TRACE 0

#if DATA_PARALLEL_TRACE
#include <stdlib.h>

#include "lib/jxl/base/time.h"
#endif  // DATA_PARALLEL_TRACE

namespace jxl {

// static
JxlParallelRetCode ThreadPool::SequentialRunnerStatic(
    void* runner_opaque, void* jpegxl_opaque, JxlParallelRunInit init,
    JxlParallelRunFunction func, uint32_t start_range, uint32_t end_range) {
  JxlParallelRetCode init_ret = (*init)(jpegxl_opaque, 1);
  if (init_ret != 0) return init_ret;

  for (uint32_t i = start_range; i < end_range; i++) {
    (*func)(jpegxl_opaque, i, 0);
  }
  return 0;
}

#if DATA_PARALLEL_TRACE
void TraceRunBegin(const char* /*caller*/, double* t0) { *t0 = Now(); }

void TraceRunEnd(const char* caller, double t0) {
  const double elapsed = Now() - t0;
  fprintf(stderr, "%27s: %5.1f ms\n", caller, elapsed * 1E3);
}
#else
void TraceRunBegin(const char* /*caller*/, double* /*t0*/) {}
void TraceRunEnd(const char* /*caller*/, double /*t0*/) {}
#endif

}  // namespace jxl
