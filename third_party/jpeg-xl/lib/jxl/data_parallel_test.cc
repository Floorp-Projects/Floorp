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

#include "gtest/gtest.h"
#include "lib/jxl/base/thread_pool_internal.h"
#include "lib/jxl/test_utils.h"

namespace jxl {
namespace {

class DataParallelTest : public ::testing::Test {
 protected:
  // A fake class to verify that DataParallel is properly calling the
  // client-provided runner functions.
  static int FakeRunner(void* runner_opaque, void* jpegxl_opaque,
                        JxlParallelRunInit init, JxlParallelRunFunction func,
                        uint32_t start_range, uint32_t end_range) {
    DataParallelTest* self = static_cast<DataParallelTest*>(runner_opaque);
    self->runner_called_++;
    self->jpegxl_opaque_ = jpegxl_opaque;
    self->init_ = init;
    self->func_ = func;
    self->start_range_ = start_range;
    self->end_range_ = end_range;
    return self->runner_return_;
  }

  ThreadPool pool_{&DataParallelTest::FakeRunner, this};

  // Number of times FakeRunner() was called.
  int runner_called_ = 0;

  // Parameters passed to FakeRunner.
  void* jpegxl_opaque_ = nullptr;
  JxlParallelRunInit init_ = nullptr;
  JxlParallelRunFunction func_ = nullptr;
  uint32_t start_range_ = -1;
  uint32_t end_range_ = -1;

  // Return value that FakeRunner will return.
  int runner_return_ = 0;
};

// JxlParallelRunInit interface.
typedef int (*JxlParallelRunInit)();
int TestInit(void* jpegxl_opaque, size_t num_threads) { return 0; }

}  // namespace

TEST_F(DataParallelTest, RunnerCalledParamenters) {
  EXPECT_TRUE(pool_.Run(
      1234, 5678, [](const size_t num_threads) { return true; },
      [](const int task, const int thread) { return; }));
  EXPECT_EQ(1, runner_called_);
  EXPECT_NE(nullptr, init_);
  EXPECT_NE(nullptr, func_);
  EXPECT_NE(nullptr, jpegxl_opaque_);
  EXPECT_EQ(1234u, start_range_);
  EXPECT_EQ(5678u, end_range_);
}

TEST_F(DataParallelTest, RunnerFailurePropagates) {
  runner_return_ = -1;  // FakeRunner return value.
  EXPECT_FALSE(pool_.Run(
      1234, 5678, [](const size_t num_threads) { return false; },
      [](const int task, const int thread) { return; }));
  EXPECT_FALSE(RunOnPool(
      nullptr, 1234, 5678, [](const size_t num_threads) { return false; },
      [](const int task, const int thread) { return; }, "Test"));
}

TEST_F(DataParallelTest, RunnerNotCalledOnEmptyRange) {
  runner_return_ = -1;  // FakeRunner return value.
  EXPECT_TRUE(pool_.Run(
      123, 123, [](const size_t num_threads) { return false; },
      [](const int task, const int thread) { return; }));
  EXPECT_TRUE(RunOnPool(
      nullptr, 123, 123, [](const size_t num_threads) { return false; },
      [](const int task, const int thread) { return; }, "Test"));
  // We don't call the external runner when the range is empty. We don't even
  // need to call the init function.
  EXPECT_EQ(0, runner_called_);
}

// The TestDivider is slow when compiled in debug mode.
TEST_F(DataParallelTest, JXL_SLOW_TEST(TestDivider)) {
  jxl::ThreadPoolInternal pool(8);
  // 1, 2 are powers of two.
  pool.Run(3, 2 * 1024, ThreadPool::SkipInit(),
           [](const int d, const int thread) {
             // powers of two are not supported.
             if ((d & (d - 1)) == 0) return;

             const Divider div(d);
#ifdef NDEBUG
             const int max_dividend = 4 * 1024 * 1024;
#else
             const int max_dividend = 2 * 1024 + 1;
#endif
             for (int x = 0; x < max_dividend; ++x) {
               const int q = div(x);
               ASSERT_EQ(x / d, q) << x << "/" << d;
             }
           });
}

}  // namespace jxl
