// Copyright 2021 Google LLC
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

#include "hwy/tests/test_util.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#include <cmath>

#include "hwy/base.h"

namespace hwy {

bool BytesEqual(const void* p1, const void* p2, const size_t size,
                size_t* pos) {
  const uint8_t* bytes1 = reinterpret_cast<const uint8_t*>(p1);
  const uint8_t* bytes2 = reinterpret_cast<const uint8_t*>(p2);
  for (size_t i = 0; i < size; ++i) {
    if (bytes1[i] != bytes2[i]) {
      fprintf(stderr, "Mismatch at byte %" PRIu64 " of %" PRIu64 ": %d != %d\n",
              static_cast<uint64_t>(i), static_cast<uint64_t>(size), bytes1[i],
              bytes2[i]);
      if (pos != nullptr) {
        *pos = i;
      }
      return false;
    }
  }
  return true;
}

void AssertStringEqual(const char* expected, const char* actual,
                       const char* target_name, const char* filename,
                       int line) {
  while (*expected == *actual++) {
    if (*expected++ == '\0') return;
  }

  Abort(filename, line, "%s string mismatch: expected '%s', got '%s'.\n",
        target_name, expected, actual);
}

namespace detail {

bool IsEqual(const TypeInfo& info, const void* expected_ptr,
             const void* actual_ptr) {
  if (!info.is_float) {
    return BytesEqual(expected_ptr, actual_ptr, info.sizeof_t);
  }

  if (info.sizeof_t == 4) {
    float expected, actual;
    CopyBytes<4>(expected_ptr, &expected);
    CopyBytes<4>(actual_ptr, &actual);
    return ComputeUlpDelta(expected, actual) <= 1;
  } else if (info.sizeof_t == 8) {
    double expected, actual;
    CopyBytes<8>(expected_ptr, &expected);
    CopyBytes<8>(actual_ptr, &actual);
    return ComputeUlpDelta(expected, actual) <= 1;
  } else {
    HWY_ABORT("Unexpected float size %" PRIu64 "\n",
              static_cast<uint64_t>(info.sizeof_t));
    return false;
  }
}

void TypeName(const TypeInfo& info, size_t N, char* string100) {
  const char prefix = info.is_float ? 'f' : (info.is_signed ? 'i' : 'u');
  // Omit the xN suffix for scalars.
  if (N == 1) {
    snprintf(string100, 64, "%c%" PRIu64, prefix,
             static_cast<uint64_t>(info.sizeof_t * 8));
  } else {
    snprintf(string100, 64, "%c%" PRIu64 "x%" PRIu64, prefix,
             static_cast<uint64_t>(info.sizeof_t * 8),
             static_cast<uint64_t>(N));
  }
}

void ToString(const TypeInfo& info, const void* ptr, char* string100) {
  if (info.sizeof_t == 1) {
    uint8_t byte;
    CopyBytes<1>(ptr, &byte);  // endian-safe: we ensured sizeof(T)=1.
    snprintf(string100, 100, "0x%02X", byte);
  } else if (info.sizeof_t == 2) {
    uint16_t bits;
    CopyBytes<2>(ptr, &bits);
    snprintf(string100, 100, "0x%04X", bits);
  } else if (info.sizeof_t == 4) {
    if (info.is_float) {
      float value;
      CopyBytes<4>(ptr, &value);
      snprintf(string100, 100, "%g", double(value));
    } else if (info.is_signed) {
      int32_t value;
      CopyBytes<4>(ptr, &value);
      snprintf(string100, 100, "%d", value);
    } else {
      uint32_t value;
      CopyBytes<4>(ptr, &value);
      snprintf(string100, 100, "%u", value);
    }
  } else {
    HWY_ASSERT(info.sizeof_t == 8);
    if (info.is_float) {
      double value;
      CopyBytes<8>(ptr, &value);
      snprintf(string100, 100, "%g", value);
    } else if (info.is_signed) {
      int64_t value;
      CopyBytes<8>(ptr, &value);
      snprintf(string100, 100, "%" PRIi64 "", value);
    } else {
      uint64_t value;
      CopyBytes<8>(ptr, &value);
      snprintf(string100, 100, "%" PRIu64 "", value);
    }
  }
}

void PrintArray(const TypeInfo& info, const char* caption,
                const void* array_void, size_t N, size_t lane_u,
                size_t max_lanes) {
  const uint8_t* array_bytes = reinterpret_cast<const uint8_t*>(array_void);

  char type_name[100];
  TypeName(info, N, type_name);

  const intptr_t lane = intptr_t(lane_u);
  const size_t begin = static_cast<size_t>(HWY_MAX(0, lane - 2));
  const size_t end = HWY_MIN(begin + max_lanes, N);
  fprintf(stderr, "%s %s [%" PRIu64 "+ ->]:\n  ", type_name, caption,
          static_cast<uint64_t>(begin));
  for (size_t i = begin; i < end; ++i) {
    const void* ptr = array_bytes + i * info.sizeof_t;
    char str[100];
    ToString(info, ptr, str);
    fprintf(stderr, "%s,", str);
  }
  if (begin >= end) fprintf(stderr, "(out of bounds)");
  fprintf(stderr, "\n");
}

HWY_NORETURN void PrintMismatchAndAbort(const TypeInfo& info,
                                        const void* expected_ptr,
                                        const void* actual_ptr,
                                        const char* target_name,
                                        const char* filename, int line,
                                        size_t lane, size_t num_lanes) {
  char type_name[100];
  TypeName(info, 1, type_name);
  char expected_str[100];
  ToString(info, expected_ptr, expected_str);
  char actual_str[100];
  ToString(info, actual_ptr, actual_str);
  Abort(filename, line,
        "%s, %sx%" PRIu64 " lane %" PRIu64
        " mismatch: expected '%s', got '%s'.\n",
        target_name, type_name, static_cast<uint64_t>(num_lanes),
        static_cast<uint64_t>(lane), expected_str, actual_str);
}

void AssertArrayEqual(const TypeInfo& info, const void* expected_void,
                      const void* actual_void, size_t N,
                      const char* target_name, const char* filename, int line) {
  const uint8_t* expected_array =
      reinterpret_cast<const uint8_t*>(expected_void);
  const uint8_t* actual_array = reinterpret_cast<const uint8_t*>(actual_void);
  for (size_t i = 0; i < N; ++i) {
    const void* expected_ptr = expected_array + i * info.sizeof_t;
    const void* actual_ptr = actual_array + i * info.sizeof_t;
    if (!IsEqual(info, expected_ptr, actual_ptr)) {
      fprintf(stderr, "\n\n");
      PrintArray(info, "expect", expected_array, N, i);
      PrintArray(info, "actual", actual_array, N, i);

      PrintMismatchAndAbort(info, expected_ptr, actual_ptr, target_name,
                            filename, line, i, N);
    }
  }
}

}  // namespace detail
}  // namespace hwy
