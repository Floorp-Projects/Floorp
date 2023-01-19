/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a cppunittest, rather than a gtest, in order to assert that no
// additional DLL needs to be linked in to use the function(s) tested herein.

#include <algorithm>
#include <iostream>
#include <optional>
#include <sstream>
#include <type_traits>

#include "mozmemory_utils.h"
#include "mozilla/Likely.h"

static bool TESTS_FAILED = false;

// Introduce iostream output operators for std::optional, for convenience's
// sake.
//
// (This is technically undefined behavior per [namespace.std], but it's
// unlikely to have any surprising effects when confined to this compilation
// unit.)
namespace std {
template <typename T>
std::ostream& operator<<(std::ostream& o, std::optional<T> const& s) {
  if (s) {
    return o << "std::optional{" << s.value() << "}";
  }
  return o << "std::nullopt";
}
std::ostream& operator<<(std::ostream& o, std::nullopt_t const& s) {
  return o << "std::nullopt";
}
}  // namespace std

// EXPECT_EQ
//
// Assert that two expressions are equal. Print them, and their values, on
// failure. (Based on the GTest macro of the same name.)
template <typename X, typename Y, size_t Xn, size_t Yn>
void AssertEqualImpl_(X&& x, Y&& y, const char* file, size_t line,
                      const char (&xStr)[Xn], const char (&yStr)[Yn],
                      const char* explanation = nullptr) {
  if (MOZ_LIKELY(x == y)) return;

  TESTS_FAILED = true;

  std::stringstream sstr;
  sstr << file << ':' << line << ": ";
  if (explanation) sstr << explanation << "\n\t";
  sstr << "expected " << xStr << " (" << x << ") == " << yStr << " (" << y
       << ")\n";
  std::cerr << sstr.str() << std::flush;
}

#define EXPECT_EQ(x, y)                                 \
  do {                                                  \
    AssertEqualImpl_(x, y, __FILE__, __LINE__, #x, #y); \
  } while (0)

// STATIC_ASSERT_VALUE_IS_OF_TYPE
//
// Assert that a value `v` is of type `t` (ignoring cv-qualification).
#define STATIC_ASSERT_VALUE_IS_OF_TYPE(v, t) \
  static_assert(std::is_same_v<std::remove_cv_t<decltype(v)>, t>)

// MockSleep
//
// Mock replacement for ::Sleep that merely logs its calls.
struct MockSleep {
  size_t calls = 0;
  size_t sum = 0;

  void operator()(size_t val) {
    ++calls;
    sum += val;
  }

  bool operator==(MockSleep const& that) const {
    return calls == that.calls && sum == that.sum;
  }
};
std::ostream& operator<<(std::ostream& o, MockSleep const& s) {
  return o << "MockSleep { count: " << s.calls << ", sum: " << s.sum << " }";
}

// MockAlloc
//
// Mock memory allocation mechanism. Eventually returns a value.
template <typename T>
struct MockAlloc {
  size_t count;
  T value;

  std::optional<T> operator()() {
    if (!count--) return value;
    return std::nullopt;
  }
};

int main() {
  using mozilla::StallSpecs;

  const StallSpecs stall = {.maxAttempts = 10, .delayMs = 50};

  // semantic test: stalls as requested but still yields a value,
  // up until it doesn't
  for (size_t i = 0; i < 20; ++i) {
    MockSleep sleep;
    auto const ret =
        stall.StallAndRetry(sleep, MockAlloc<int>{.count = i, .value = 5});
    STATIC_ASSERT_VALUE_IS_OF_TYPE(ret, std::optional<int>);

    if (i < 10) {
      EXPECT_EQ(ret, std::optional<int>(5));
    } else {
      EXPECT_EQ(ret, std::nullopt);
    }
    size_t const expectedCalls = std::min<size_t>(i + 1, 10);
    EXPECT_EQ(sleep,
              (MockSleep{.calls = expectedCalls, .sum = 50 * expectedCalls}));
  }

  // syntactic test: inline capturing lambda is accepted for aOperation
  {
    MockSleep sleep;
    std::optional<int> value{42};
    auto const ret = stall.StallAndRetry(sleep, [&]() { return value; });

    STATIC_ASSERT_VALUE_IS_OF_TYPE(ret, std::optional<int>);
    EXPECT_EQ(ret, std::optional(42));
    EXPECT_EQ(sleep, (MockSleep{.calls = 1, .sum = 50}));
  }

  // syntactic test: inline capturing lambda is accepted for aDelayFunc
  {
    MockSleep sleep;
    auto const ret =
        stall.StallAndRetry([&](size_t time) { sleep(time); },
                            MockAlloc<int>{.count = 0, .value = 105});

    STATIC_ASSERT_VALUE_IS_OF_TYPE(ret, std::optional<int>);
    EXPECT_EQ(ret, std::optional(105));
    EXPECT_EQ(sleep, (MockSleep{.calls = 1, .sum = 50}));
  }

  return TESTS_FAILED ? 1 : 0;
}
