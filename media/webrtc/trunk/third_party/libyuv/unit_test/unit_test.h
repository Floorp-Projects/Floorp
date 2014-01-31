/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef UNIT_TEST_UNIT_TEST_H_
#define UNIT_TEST_UNIT_TEST_H_

#include <gtest/gtest.h>

#define align_buffer_16(var, size)                                             \
  uint8* var;                                                                  \
  uint8* var##_mem;                                                            \
  var##_mem = reinterpret_cast<uint8*>(malloc((size) + 15));                   \
  var = reinterpret_cast<uint8*>                                               \
        ((reinterpret_cast<intptr_t>(var##_mem) + 15) & ~15);

#define free_aligned_buffer_16(var) \
  free(var##_mem);  \
  var = 0;


#define align_buffer_page_end(var, size)                                       \
  uint8* var;                                                                  \
  uint8* var##_mem;                                                            \
  var##_mem = reinterpret_cast<uint8*>(malloc(((size) + 4095) & ~4095));       \
  var = var##_mem + (-(size) & 4095);

#define free_aligned_buffer_page_end(var) \
  free(var##_mem);  \
  var = 0;

#ifdef WIN32
#include <windows.h>
static inline double get_time() {
  LARGE_INTEGER t, f;
  QueryPerformanceCounter(&t);
  QueryPerformanceFrequency(&f);
  return static_cast<double>(t.QuadPart) / static_cast<double>(f.QuadPart);
}

#define random rand
#define srandom srand
#else

#include <sys/time.h>
#include <sys/resource.h>

static inline double get_time() {
  struct timeval t;
  struct timezone tzp;
  gettimeofday(&t, &tzp);
  return t.tv_sec + t.tv_usec * 1e-6;
}
#endif

class libyuvTest : public ::testing::Test {
 protected:
  libyuvTest();

  const int rotate_max_w_;
  const int rotate_max_h_;

  int benchmark_iterations_;
  const int benchmark_width_;
  const int benchmark_height_;
};

#endif  // UNIT_TEST_UNIT_TEST_H_
