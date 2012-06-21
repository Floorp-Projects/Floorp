/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef UINIT_TEST_H_
#define UINIT_TEST_H_

#include <gtest/gtest.h>

#define align_buffer_16(var, size) \
  uint8 *var; \
  uint8 *var##_mem; \
  var##_mem = reinterpret_cast<uint8*>(calloc((size)+15, sizeof(uint8))); \
  var = reinterpret_cast<uint8*> \
        ((reinterpret_cast<intptr_t>(var##_mem) + 15) & (~0x0f));

#define free_aligned_buffer_16(var) \
  free(var##_mem);  \
  var = 0;

#ifdef WIN32

#include <windows.h>
static double get_time()
{
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&f);
    return double(t.QuadPart)/double(f.QuadPart);
}

#else

#include <sys/time.h>
#include <sys/resource.h>

static double get_time() {
  struct timeval t;
  struct timezone tzp;
  gettimeofday(&t, &tzp);
  return t.tv_sec + t.tv_usec*1e-6;
}

#endif

class libyuvTest : public ::testing::Test {
 protected:
  libyuvTest();

  const int _rotate_max_w;
  const int _rotate_max_h;

  const int _benchmark_iterations;
  const int _benchmark_width;
  const int _benchmark_height;

};

#endif // UNIT_TEST_H_
