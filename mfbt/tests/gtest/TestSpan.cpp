///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////////

// Adapted from
// https://github.com/Microsoft/GSL/blob/3819df6e378ffccf0e29465afe99c3b324c2aa70/tests/Span_tests.cpp

#include "gtest/gtest.h"

#include "mozilla/Span.h"

#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Range.h"

#include <type_traits>

#define SPAN_TEST(name) TEST(SpanTest, name)
#define CHECK_THROW(a, b)

using namespace mozilla;

static_assert(std::is_convertible_v<Range<int>, Span<const int>>,
              "Range should convert into const");
static_assert(std::is_convertible_v<Range<const int>, Span<const int>>,
              "const Range should convert into const");
static_assert(!std::is_convertible_v<Range<const int>, Span<int>>,
              "Range should not drop const in conversion");
static_assert(std::is_convertible_v<Span<int>, Range<const int>>,
              "Span should convert into const");
static_assert(std::is_convertible_v<Span<const int>, Range<const int>>,
              "const Span should convert into const");
static_assert(!std::is_convertible_v<Span<const int>, Range<int>>,
              "Span should not drop const in conversion");
static_assert(std::is_convertible_v<Span<const int>, Span<const int>>,
              "const Span should convert into const");
static_assert(std::is_convertible_v<Span<int>, Span<const int>>,
              "Span should convert into const");
static_assert(!std::is_convertible_v<Span<const int>, Span<int>>,
              "Span should not drop const in conversion");
static_assert(std::is_convertible_v<const nsTArray<int>, Span<const int>>,
              "const nsTArray should convert into const");
static_assert(std::is_convertible_v<nsTArray<int>, Span<const int>>,
              "nsTArray should convert into const");
static_assert(!std::is_convertible_v<const nsTArray<int>, Span<int>>,
              "nsTArray should not drop const in conversion");
static_assert(std::is_convertible_v<nsTArray<const int>, Span<const int>>,
              "nsTArray should convert into const");
static_assert(!std::is_convertible_v<nsTArray<const int>, Span<int>>,
              "nsTArray should not drop const in conversion");

/**
 * Rust slice-compatible nullptr replacement value.
 */
#define SLICE_CONST_INT_PTR reinterpret_cast<const int*>(alignof(const int))

/**
 * Rust slice-compatible nullptr replacement value.
 */
#define SLICE_INT_PTR reinterpret_cast<int*>(alignof(int))

/**
 * Rust slice-compatible nullptr replacement value.
 */
#define SLICE_CONST_INT_PTR_PTR \
  reinterpret_cast<const int**>(alignof(const int*))

/**
 * Rust slice-compatible nullptr replacement value.
 */
#define SLICE_INT_PTR_PTR reinterpret_cast<int**>(alignof(int*))

namespace {
struct BaseClass {};
struct DerivedClass : BaseClass {};
}  // namespace

void AssertSpanOfThreeInts(Span<const int> s) {
  ASSERT_EQ(s.size(), 3U);
  ASSERT_EQ(s[0], 1);
  ASSERT_EQ(s[1], 2);
  ASSERT_EQ(s[2], 3);
}

void AssertSpanOfThreeChars(Span<const char> s) {
  ASSERT_EQ(s.size(), 3U);
  ASSERT_EQ(s[0], 'a');
  ASSERT_EQ(s[1], 'b');
  ASSERT_EQ(s[2], 'c');
}

void AssertSpanOfThreeChar16s(Span<const char16_t> s) {
  ASSERT_EQ(s.size(), 3U);
  ASSERT_EQ(s[0], 'a');
  ASSERT_EQ(s[1], 'b');
  ASSERT_EQ(s[2], 'c');
}

void AssertSpanOfThreeCharsViaString(const nsACString& aStr) {
  AssertSpanOfThreeChars(aStr);
}

void AssertSpanOfThreeChar16sViaString(const nsAString& aStr) {
  AssertSpanOfThreeChar16s(aStr);
}

SPAN_TEST(default_constructor) {
  {
    Span<int> s;
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);

    Span<const int> cs;
    ASSERT_EQ(cs.Length(), 0U);
    ASSERT_EQ(cs.data(), SLICE_CONST_INT_PTR);
  }

  {
    Span<int, 0> s;
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);

    Span<const int, 0> cs;
    ASSERT_EQ(cs.Length(), 0U);
    ASSERT_EQ(cs.data(), SLICE_CONST_INT_PTR);
  }

  {
#ifdef CONFIRM_COMPILATION_ERRORS
    Span<int, 1> s;
    ASSERT_EQ(s.Length(), 1U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);  // explains why it can't compile
#endif
  }

  {
    Span<int> s{};
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);

    Span<const int> cs{};
    ASSERT_EQ(cs.Length(), 0U);
    ASSERT_EQ(cs.data(), SLICE_CONST_INT_PTR);
  }
}

SPAN_TEST(size_optimization) {
  {
    Span<int> s;
    ASSERT_EQ(sizeof(s), sizeof(int*) + sizeof(size_t));
  }

  {
    Span<int, 0> s;
    ASSERT_EQ(sizeof(s), sizeof(int*));
  }
}

SPAN_TEST(from_nullptr_constructor) {
  {
    Span<int> s = nullptr;
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);

    Span<const int> cs = nullptr;
    ASSERT_EQ(cs.Length(), 0U);
    ASSERT_EQ(cs.data(), SLICE_CONST_INT_PTR);
  }

  {
    Span<int, 0> s = nullptr;
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);

    Span<const int, 0> cs = nullptr;
    ASSERT_EQ(cs.Length(), 0U);
    ASSERT_EQ(cs.data(), SLICE_CONST_INT_PTR);
  }

  {
#ifdef CONFIRM_COMPILATION_ERRORS
    Span<int, 1> s = nullptr;
    ASSERT_EQ(s.Length(), 1U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);  // explains why it can't compile
#endif
  }

  {
    Span<int> s{nullptr};
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);

    Span<const int> cs{nullptr};
    ASSERT_EQ(cs.Length(), 0U);
    ASSERT_EQ(cs.data(), SLICE_CONST_INT_PTR);
  }

  {
    Span<int*> s{nullptr};
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR_PTR);

    Span<const int*> cs{nullptr};
    ASSERT_EQ(cs.Length(), 0U);
    ASSERT_EQ(cs.data(), SLICE_CONST_INT_PTR_PTR);
  }
}

SPAN_TEST(from_nullptr_length_constructor) {
  {
    Span<int> s{nullptr, static_cast<Span<int>::index_type>(0)};
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);

    Span<const int> cs{nullptr, static_cast<Span<int>::index_type>(0)};
    ASSERT_EQ(cs.Length(), 0U);
    ASSERT_EQ(cs.data(), SLICE_CONST_INT_PTR);
  }

  {
    Span<int, 0> s{nullptr, static_cast<Span<int>::index_type>(0)};
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);

    Span<const int, 0> cs{nullptr, static_cast<Span<int>::index_type>(0)};
    ASSERT_EQ(cs.Length(), 0U);
    ASSERT_EQ(cs.data(), SLICE_CONST_INT_PTR);
  }

#if 0
        {
            auto workaround_macro = []() { Span<int, 1> s{ nullptr, static_cast<Span<int>::index_type>(0) }; };
            CHECK_THROW(workaround_macro(), fail_fast);
        }

        {
            auto workaround_macro = []() { Span<int> s{nullptr, 1}; };
            CHECK_THROW(workaround_macro(), fail_fast);

            auto const_workaround_macro = []() { Span<const int> cs{nullptr, 1}; };
            CHECK_THROW(const_workaround_macro(), fail_fast);
        }

        {
            auto workaround_macro = []() { Span<int, 0> s{nullptr, 1}; };
            CHECK_THROW(workaround_macro(), fail_fast);

            auto const_workaround_macro = []() { Span<const int, 0> s{nullptr, 1}; };
            CHECK_THROW(const_workaround_macro(), fail_fast);
        }
#endif
  {
    Span<int*> s{nullptr, static_cast<Span<int>::index_type>(0)};
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR_PTR);

    Span<const int*> cs{nullptr, static_cast<Span<int>::index_type>(0)};
    ASSERT_EQ(cs.Length(), 0U);
    ASSERT_EQ(cs.data(), SLICE_CONST_INT_PTR_PTR);
  }
}

SPAN_TEST(from_pointer_length_constructor) {
  int arr[4] = {1, 2, 3, 4};

  {
    Span<int> s{&arr[0], 2};
    ASSERT_EQ(s.Length(), 2U);
    ASSERT_EQ(s.data(), &arr[0]);
    ASSERT_EQ(s[0], 1);
    ASSERT_EQ(s[1], 2);
  }

  {
    Span<int, 2> s{&arr[0], 2};
    ASSERT_EQ(s.Length(), 2U);
    ASSERT_EQ(s.data(), &arr[0]);
    ASSERT_EQ(s[0], 1);
    ASSERT_EQ(s[1], 2);
  }

  {
    int* p = nullptr;
    Span<int> s{p, static_cast<Span<int>::index_type>(0)};
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);
  }

#if 0
        {
            int* p = nullptr;
            auto workaround_macro = [=]() { Span<int> s{p, 2}; };
            CHECK_THROW(workaround_macro(), fail_fast);
        }
#endif

  {
    auto s = MakeSpan(&arr[0], 2);
    ASSERT_EQ(s.Length(), 2U);
    ASSERT_EQ(s.data(), &arr[0]);
    ASSERT_EQ(s[0], 1);
    ASSERT_EQ(s[1], 2);
  }

  {
    int* p = nullptr;
    auto s = MakeSpan(p, static_cast<Span<int>::index_type>(0));
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);
  }

#if 0
        {
            int* p = nullptr;
            auto workaround_macro = [=]() { MakeSpan(p, 2); };
            CHECK_THROW(workaround_macro(), fail_fast);
        }
#endif
}

SPAN_TEST(from_pointer_pointer_constructor) {
  int arr[4] = {1, 2, 3, 4};

  {
    Span<int> s{&arr[0], &arr[2]};
    ASSERT_EQ(s.Length(), 2U);
    ASSERT_EQ(s.data(), &arr[0]);
    ASSERT_EQ(s[0], 1);
    ASSERT_EQ(s[1], 2);
  }

  {
    Span<int, 2> s{&arr[0], &arr[2]};
    ASSERT_EQ(s.Length(), 2U);
    ASSERT_EQ(s.data(), &arr[0]);
    ASSERT_EQ(s[0], 1);
    ASSERT_EQ(s[1], 2);
  }

  {
    Span<int> s{&arr[0], &arr[0]};
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), &arr[0]);
  }

  {
    Span<int, 0> s{&arr[0], &arr[0]};
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), &arr[0]);
  }

  // this will fail the std::distance() precondition, which asserts on MSVC
  // debug builds
  //{
  //    auto workaround_macro = [&]() { Span<int> s{&arr[1], &arr[0]}; };
  //    CHECK_THROW(workaround_macro(), fail_fast);
  //}

  // this will fail the std::distance() precondition, which asserts on MSVC
  // debug builds
  //{
  //    int* p = nullptr;
  //    auto workaround_macro = [&]() { Span<int> s{&arr[0], p}; };
  //    CHECK_THROW(workaround_macro(), fail_fast);
  //}

  {
    int* p = nullptr;
    Span<int> s{p, p};
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);
  }

  {
    int* p = nullptr;
    Span<int, 0> s{p, p};
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);
  }

  // this will fail the std::distance() precondition, which asserts on MSVC
  // debug builds
  //{
  //    int* p = nullptr;
  //    auto workaround_macro = [&]() { Span<int> s{&arr[0], p}; };
  //    CHECK_THROW(workaround_macro(), fail_fast);
  //}

  {
    auto s = MakeSpan(&arr[0], &arr[2]);
    ASSERT_EQ(s.Length(), 2U);
    ASSERT_EQ(s.data(), &arr[0]);
    ASSERT_EQ(s[0], 1);
    ASSERT_EQ(s[1], 2);
  }

  {
    auto s = MakeSpan(&arr[0], &arr[0]);
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), &arr[0]);
  }

  {
    int* p = nullptr;
    auto s = MakeSpan(p, p);
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), SLICE_INT_PTR);
  }
}

SPAN_TEST(from_array_constructor) {
  int arr[5] = {1, 2, 3, 4, 5};

  {
    Span<int> s{arr};
    ASSERT_EQ(s.Length(), 5U);
    ASSERT_EQ(s.data(), &arr[0]);
  }

  {
    Span<int, 5> s{arr};
    ASSERT_EQ(s.Length(), 5U);
    ASSERT_EQ(s.data(), &arr[0]);
  }

  int arr2d[2][3] = {{1, 2, 3}, {4, 5, 6}};

#ifdef CONFIRM_COMPILATION_ERRORS
  { Span<int, 6> s{arr}; }

  {
    Span<int, 0> s{arr};
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), &arr[0]);
  }

  {
    Span<int> s{arr2d};
    ASSERT_EQ(s.Length(), 6U);
    ASSERT_EQ(s.data(), &arr2d[0][0]);
    ASSERT_EQ(s[0], 1);
    ASSERT_EQ(s[5], 6);
  }

  {
    Span<int, 0> s{arr2d};
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), &arr2d[0][0]);
  }

  { Span<int, 6> s{arr2d}; }
#endif
  {
    Span<int[3]> s{&(arr2d[0]), 1};
    ASSERT_EQ(s.Length(), 1U);
    ASSERT_EQ(s.data(), &arr2d[0]);
  }

  int arr3d[2][3][2] = {{{1, 2}, {3, 4}, {5, 6}}, {{7, 8}, {9, 10}, {11, 12}}};

#ifdef CONFIRM_COMPILATION_ERRORS
  {
    Span<int> s{arr3d};
    ASSERT_EQ(s.Length(), 12U);
    ASSERT_EQ(s.data(), &arr3d[0][0][0]);
    ASSERT_EQ(s[0], 1);
    ASSERT_EQ(s[11], 12);
  }

  {
    Span<int, 0> s{arr3d};
    ASSERT_EQ(s.Length(), 0U);
    ASSERT_EQ(s.data(), &arr3d[0][0][0]);
  }

  { Span<int, 11> s{arr3d}; }

  {
    Span<int, 12> s{arr3d};
    ASSERT_EQ(s.Length(), 12U);
    ASSERT_EQ(s.data(), &arr3d[0][0][0]);
    ASSERT_EQ(s[0], 1);
    ASSERT_EQ(s[5], 6);
  }
#endif
  {
    Span<int[3][2]> s{&arr3d[0], 1};
    ASSERT_EQ(s.Length(), 1U);
    ASSERT_EQ(s.data(), &arr3d[0]);
  }

  {
    auto s = MakeSpan(arr);
    ASSERT_EQ(s.Length(), 5U);
    ASSERT_EQ(s.data(), &arr[0]);
  }

  {
    auto s = MakeSpan(&(arr2d[0]), 1);
    ASSERT_EQ(s.Length(), 1U);
    ASSERT_EQ(s.data(), &arr2d[0]);
  }

  {
    auto s = MakeSpan(&arr3d[0], 1);
    ASSERT_EQ(s.Length(), 1U);
    ASSERT_EQ(s.data(), &arr3d[0]);
  }
}

SPAN_TEST(from_dynamic_array_constructor) {
  double(*arr)[3][4] = new double[100][3][4];

  {
    Span<double> s(&arr[0][0][0], 10);
    ASSERT_EQ(s.Length(), 10U);
    ASSERT_EQ(s.data(), &arr[0][0][0]);
  }

  {
    auto s = MakeSpan(&arr[0][0][0], 10);
    ASSERT_EQ(s.Length(), 10U);
    ASSERT_EQ(s.data(), &arr[0][0][0]);
  }

  delete[] arr;
}

SPAN_TEST(from_std_array_constructor) {
  std::array<int, 4> arr = {{1, 2, 3, 4}};

  {
    Span<int> s{arr};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.size()));
    ASSERT_EQ(s.data(), arr.data());

    Span<const int> cs{arr};
    ASSERT_EQ(cs.size(), narrow_cast<size_t>(arr.size()));
    ASSERT_EQ(cs.data(), arr.data());
  }

  {
    Span<int, 4> s{arr};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.size()));
    ASSERT_EQ(s.data(), arr.data());

    Span<const int, 4> cs{arr};
    ASSERT_EQ(cs.size(), narrow_cast<size_t>(arr.size()));
    ASSERT_EQ(cs.data(), arr.data());
  }

#ifdef CONFIRM_COMPILATION_ERRORS
  {
    Span<int, 2> s{arr};
    ASSERT_EQ(s.size(), 2U);
    ASSERT_EQ(s.data(), arr.data());

    Span<const int, 2> cs{arr};
    ASSERT_EQ(cs.size(), 2U);
    ASSERT_EQ(cs.data(), arr.data());
  }

  {
    Span<int, 0> s{arr};
    ASSERT_EQ(s.size(), 0U);
    ASSERT_EQ(s.data(), arr.data());

    Span<const int, 0> cs{arr};
    ASSERT_EQ(cs.size(), 0U);
    ASSERT_EQ(cs.data(), arr.data());
  }

  { Span<int, 5> s{arr}; }

  {
    auto get_an_array = []() -> std::array<int, 4> { return {1, 2, 3, 4}; };
    auto take_a_Span = [](Span<int> s) { static_cast<void>(s); };
    // try to take a temporary std::array
    take_a_Span(get_an_array());
  }
#endif

  {
    auto get_an_array = []() -> std::array<int, 4> { return {{1, 2, 3, 4}}; };
    auto take_a_Span = [](Span<const int> s) { static_cast<void>(s); };
    // try to take a temporary std::array
    take_a_Span(get_an_array());
  }

  {
    auto s = MakeSpan(arr);
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.size()));
    ASSERT_EQ(s.data(), arr.data());
  }
}

SPAN_TEST(from_const_std_array_constructor) {
  const std::array<int, 4> arr = {{1, 2, 3, 4}};

  {
    Span<const int> s{arr};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.size()));
    ASSERT_EQ(s.data(), arr.data());
  }

  {
    Span<const int, 4> s{arr};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.size()));
    ASSERT_EQ(s.data(), arr.data());
  }

#ifdef CONFIRM_COMPILATION_ERRORS
  {
    Span<const int, 2> s{arr};
    ASSERT_EQ(s.size(), 2U);
    ASSERT_EQ(s.data(), arr.data());
  }

  {
    Span<const int, 0> s{arr};
    ASSERT_EQ(s.size(), 0U);
    ASSERT_EQ(s.data(), arr.data());
  }

  { Span<const int, 5> s{arr}; }
#endif

  {
    auto get_an_array = []() -> const std::array<int, 4> {
      return {{1, 2, 3, 4}};
    };
    auto take_a_Span = [](Span<const int> s) { static_cast<void>(s); };
    // try to take a temporary std::array
    take_a_Span(get_an_array());
  }

  {
    auto s = MakeSpan(arr);
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.size()));
    ASSERT_EQ(s.data(), arr.data());
  }
}

SPAN_TEST(from_std_array_const_constructor) {
  std::array<const int, 4> arr = {{1, 2, 3, 4}};

  {
    Span<const int> s{arr};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.size()));
    ASSERT_EQ(s.data(), arr.data());
  }

  {
    Span<const int, 4> s{arr};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.size()));
    ASSERT_EQ(s.data(), arr.data());
  }

#ifdef CONFIRM_COMPILATION_ERRORS
  {
    Span<const int, 2> s{arr};
    ASSERT_EQ(s.size(), 2U);
    ASSERT_EQ(s.data(), arr.data());
  }

  {
    Span<const int, 0> s{arr};
    ASSERT_EQ(s.size(), 0U);
    ASSERT_EQ(s.data(), arr.data());
  }

  { Span<const int, 5> s{arr}; }

  { Span<int, 4> s{arr}; }
#endif

  {
    auto s = MakeSpan(arr);
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.size()));
    ASSERT_EQ(s.data(), arr.data());
  }
}

SPAN_TEST(from_mozilla_array_constructor) {
  mozilla::Array<int, 4> arr(1, 2, 3, 4);

  {
    Span<int> s{arr};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.cend() - arr.cbegin()));
    ASSERT_EQ(s.data(), &arr[0]);

    Span<const int> cs{arr};
    ASSERT_EQ(cs.size(), narrow_cast<size_t>(arr.cend() - arr.cbegin()));
    ASSERT_EQ(cs.data(), &arr[0]);
  }

  {
    Span<int, 4> s{arr};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.cend() - arr.cbegin()));
    ASSERT_EQ(s.data(), &arr[0]);

    Span<const int, 4> cs{arr};
    ASSERT_EQ(cs.size(), narrow_cast<size_t>(arr.cend() - arr.cbegin()));
    ASSERT_EQ(cs.data(), &arr[0]);
  }

#ifdef CONFIRM_COMPILATION_ERRORS
  {
    Span<int, 2> s{arr};
    ASSERT_EQ(s.size(), 2U);
    ASSERT_EQ(s.data(), &arr[0]);

    Span<const int, 2> cs{arr};
    ASSERT_EQ(cs.size(), 2U);
    ASSERT_EQ(cs.data(), &arr[0]);
  }

  {
    Span<int, 0> s{arr};
    ASSERT_EQ(s.size(), 0U);
    ASSERT_EQ(s.data(), &arr[0]);

    Span<const int, 0> cs{arr};
    ASSERT_EQ(cs.size(), 0U);
    ASSERT_EQ(cs.data(), &arr[0]);
  }

  { Span<int, 5> s{arr}; }

  {
    auto get_an_array = []() -> mozilla::Array<int, 4> { return {1, 2, 3, 4}; };
    auto take_a_Span = [](Span<int> s) { static_cast<void>(s); };
    // try to take a temporary mozilla::Array
    take_a_Span(get_an_array());
  }
#endif

  {
    auto get_an_array = []() -> mozilla::Array<int, 4> { return {1, 2, 3, 4}; };
    auto take_a_Span = [](Span<const int> s) { static_cast<void>(s); };
    // try to take a temporary mozilla::Array
    take_a_Span(get_an_array());
  }

  {
    auto s = MakeSpan(arr);
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.cend() - arr.cbegin()));
    ASSERT_EQ(s.data(), &arr[0]);
  }
}

SPAN_TEST(from_const_mozilla_array_constructor) {
  const mozilla::Array<int, 4> arr(1, 2, 3, 4);

  {
    Span<const int> s{arr};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.cend() - arr.cbegin()));
    ASSERT_EQ(s.data(), &arr[0]);
  }

  {
    Span<const int, 4> s{arr};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.cend() - arr.cbegin()));
    ASSERT_EQ(s.data(), &arr[0]);
  }

#ifdef CONFIRM_COMPILATION_ERRORS
  {
    Span<const int, 2> s{arr};
    ASSERT_EQ(s.size(), 2U);
    ASSERT_EQ(s.data(), &arr[0]);
  }

  {
    Span<const int, 0> s{arr};
    ASSERT_EQ(s.size(), 0U);
    ASSERT_EQ(s.data(), &arr[0]);
  }

  { Span<const int, 5> s{arr}; }
#endif

#if 0
  {
    auto get_an_array = []() -> const mozilla::Array<int, 4> {
      return { 1, 2, 3, 4 };
    };
    auto take_a_Span = [](Span<const int> s) { static_cast<void>(s); };
    // try to take a temporary mozilla::Array
    take_a_Span(get_an_array());
  }
#endif

  {
    auto s = MakeSpan(arr);
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.cend() - arr.cbegin()));
    ASSERT_EQ(s.data(), &arr[0]);
  }
}

SPAN_TEST(from_mozilla_array_const_constructor) {
  mozilla::Array<const int, 4> arr(1, 2, 3, 4);

  {
    Span<const int> s{arr};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.cend() - arr.cbegin()));
    ASSERT_EQ(s.data(), &arr[0]);
  }

  {
    Span<const int, 4> s{arr};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.cend() - arr.cbegin()));
    ASSERT_EQ(s.data(), &arr[0]);
  }

#ifdef CONFIRM_COMPILATION_ERRORS
  {
    Span<const int, 2> s{arr};
    ASSERT_EQ(s.size(), 2U);
    ASSERT_EQ(s.data(), &arr[0]);
  }

  {
    Span<const int, 0> s{arr};
    ASSERT_EQ(s.size(), 0U);
    ASSERT_EQ(s.data(), &arr[0]);
  }

  { Span<const int, 5> s{arr}; }

  { Span<int, 4> s{arr}; }
#endif

  {
    auto s = MakeSpan(arr);
    ASSERT_EQ(s.size(), narrow_cast<size_t>(arr.cend() - arr.cbegin()));
    ASSERT_EQ(s.data(), &arr[0]);
  }
}

SPAN_TEST(from_container_constructor) {
  std::vector<int> v = {1, 2, 3};
  const std::vector<int> cv = v;

  {
    AssertSpanOfThreeInts(v);

    Span<int> s{v};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(v.size()));
    ASSERT_EQ(s.data(), v.data());

    Span<const int> cs{v};
    ASSERT_EQ(cs.size(), narrow_cast<size_t>(v.size()));
    ASSERT_EQ(cs.data(), v.data());
  }

  std::string str = "hello";
  const std::string cstr = "hello";

  {
#ifdef CONFIRM_COMPILATION_ERRORS
    Span<char> s{str};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(str.size()));
    ASSERT_EQ(s.data(), str.data());
#endif
    Span<const char> cs{str};
    ASSERT_EQ(cs.size(), narrow_cast<size_t>(str.size()));
    ASSERT_EQ(cs.data(), str.data());
  }

  {
#ifdef CONFIRM_COMPILATION_ERRORS
    Span<char> s{cstr};
#endif
    Span<const char> cs{cstr};
    ASSERT_EQ(cs.size(), narrow_cast<size_t>(cstr.size()));
    ASSERT_EQ(cs.data(), cstr.data());
  }

  {
#ifdef CONFIRM_COMPILATION_ERRORS
    auto get_temp_vector = []() -> std::vector<int> { return {}; };
    auto use_Span = [](Span<int> s) { static_cast<void>(s); };
    use_Span(get_temp_vector());
#endif
  }

  {
    auto get_temp_vector = []() -> std::vector<int> { return {}; };
    auto use_Span = [](Span<const int> s) { static_cast<void>(s); };
    use_Span(get_temp_vector());
  }

  {
#ifdef CONFIRM_COMPILATION_ERRORS
    auto get_temp_string = []() -> std::string { return {}; };
    auto use_Span = [](Span<char> s) { static_cast<void>(s); };
    use_Span(get_temp_string());
#endif
  }

  {
    auto get_temp_string = []() -> std::string { return {}; };
    auto use_Span = [](Span<const char> s) { static_cast<void>(s); };
    use_Span(get_temp_string());
  }

  {
#ifdef CONFIRM_COMPILATION_ERRORS
    auto get_temp_vector = []() -> const std::vector<int> { return {}; };
    auto use_Span = [](Span<const char> s) { static_cast<void>(s); };
    use_Span(get_temp_vector());
#endif
  }

  {
    auto get_temp_string = []() -> const std::string { return {}; };
    auto use_Span = [](Span<const char> s) { static_cast<void>(s); };
    use_Span(get_temp_string());
  }

  {
#ifdef CONFIRM_COMPILATION_ERRORS
    std::map<int, int> m;
    Span<int> s{m};
#endif
  }

  {
    auto s = MakeSpan(v);
    ASSERT_EQ(s.size(), narrow_cast<size_t>(v.size()));
    ASSERT_EQ(s.data(), v.data());

    auto cs = MakeSpan(cv);
    ASSERT_EQ(cs.size(), narrow_cast<size_t>(cv.size()));
    ASSERT_EQ(cs.data(), cv.data());
  }
}

SPAN_TEST(from_xpcom_collections) {
  {
    nsTArray<int> v;
    v.AppendElement(1);
    v.AppendElement(2);
    v.AppendElement(3);

    AssertSpanOfThreeInts(v);

    Span<int> s{v};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(v.Length()));
    ASSERT_EQ(s.data(), v.Elements());
    ASSERT_EQ(s[2], 3);

    Span<const int> cs{v};
    ASSERT_EQ(cs.size(), narrow_cast<size_t>(v.Length()));
    ASSERT_EQ(cs.data(), v.Elements());
    ASSERT_EQ(cs[2], 3);
  }
  {
    nsTArray<int> v;
    v.AppendElement(1);
    v.AppendElement(2);
    v.AppendElement(3);

    AssertSpanOfThreeInts(v);

    auto s = MakeSpan(v);
    ASSERT_EQ(s.size(), narrow_cast<size_t>(v.Length()));
    ASSERT_EQ(s.data(), v.Elements());
    ASSERT_EQ(s[2], 3);
  }
  {
    AutoTArray<int, 5> v;
    v.AppendElement(1);
    v.AppendElement(2);
    v.AppendElement(3);

    AssertSpanOfThreeInts(v);

    Span<int> s{v};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(v.Length()));
    ASSERT_EQ(s.data(), v.Elements());
    ASSERT_EQ(s[2], 3);

    Span<const int> cs{v};
    ASSERT_EQ(cs.size(), narrow_cast<size_t>(v.Length()));
    ASSERT_EQ(cs.data(), v.Elements());
    ASSERT_EQ(cs[2], 3);
  }
  {
    AutoTArray<int, 5> v;
    v.AppendElement(1);
    v.AppendElement(2);
    v.AppendElement(3);

    AssertSpanOfThreeInts(v);

    auto s = MakeSpan(v);
    ASSERT_EQ(s.size(), narrow_cast<size_t>(v.Length()));
    ASSERT_EQ(s.data(), v.Elements());
    ASSERT_EQ(s[2], 3);
  }
  {
    FallibleTArray<int> v;
    *(v.AppendElement(fallible)) = 1;
    *(v.AppendElement(fallible)) = 2;
    *(v.AppendElement(fallible)) = 3;

    AssertSpanOfThreeInts(v);

    Span<int> s{v};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(v.Length()));
    ASSERT_EQ(s.data(), v.Elements());
    ASSERT_EQ(s[2], 3);

    Span<const int> cs{v};
    ASSERT_EQ(cs.size(), narrow_cast<size_t>(v.Length()));
    ASSERT_EQ(cs.data(), v.Elements());
    ASSERT_EQ(cs[2], 3);
  }
  {
    FallibleTArray<int> v;
    *(v.AppendElement(fallible)) = 1;
    *(v.AppendElement(fallible)) = 2;
    *(v.AppendElement(fallible)) = 3;

    AssertSpanOfThreeInts(v);

    auto s = MakeSpan(v);
    ASSERT_EQ(s.size(), narrow_cast<size_t>(v.Length()));
    ASSERT_EQ(s.data(), v.Elements());
    ASSERT_EQ(s[2], 3);
  }
  {
    nsAutoString str;
    str.AssignLiteral("abc");

    AssertSpanOfThreeChar16s(str);
    AssertSpanOfThreeChar16sViaString(str);

    Span<char16_t> s{str};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(str.Length()));
    ASSERT_EQ(s.data(), str.BeginWriting());
    ASSERT_EQ(s[2], 'c');

    Span<const char16_t> cs{str};
    ASSERT_EQ(cs.size(), narrow_cast<size_t>(str.Length()));
    ASSERT_EQ(cs.data(), str.BeginReading());
    ASSERT_EQ(cs[2], 'c');
  }
  {
    nsAutoString str;
    str.AssignLiteral("abc");

    AssertSpanOfThreeChar16s(str);
    AssertSpanOfThreeChar16sViaString(str);

    auto s = MakeSpan(str);
    ASSERT_EQ(s.size(), narrow_cast<size_t>(str.Length()));
    ASSERT_EQ(s.data(), str.BeginWriting());
    ASSERT_EQ(s[2], 'c');
  }
  {
    nsAutoCString str;
    str.AssignLiteral("abc");

    AssertSpanOfThreeChars(str);
    AssertSpanOfThreeCharsViaString(str);

    Span<uint8_t> s{str};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(str.Length()));
    ASSERT_EQ(s.data(), reinterpret_cast<uint8_t*>(str.BeginWriting()));
    ASSERT_EQ(s[2], 'c');

    Span<const uint8_t> cs{str};
    ASSERT_EQ(cs.size(), narrow_cast<size_t>(str.Length()));
    ASSERT_EQ(cs.data(), reinterpret_cast<const uint8_t*>(str.BeginReading()));
    ASSERT_EQ(cs[2], 'c');
  }
  {
    nsAutoCString str;
    str.AssignLiteral("abc");

    AssertSpanOfThreeChars(str);
    AssertSpanOfThreeCharsViaString(str);

    auto s = MakeSpan(str);
    ASSERT_EQ(s.size(), narrow_cast<size_t>(str.Length()));
    ASSERT_EQ(s.data(), str.BeginWriting());
    ASSERT_EQ(s[2], 'c');
  }
  {
    nsTArray<int> v;
    v.AppendElement(1);
    v.AppendElement(2);
    v.AppendElement(3);

    Range<int> r(v.Elements(), v.Length());

    AssertSpanOfThreeInts(r);

    Span<int> s{r};
    ASSERT_EQ(s.size(), narrow_cast<size_t>(v.Length()));
    ASSERT_EQ(s.data(), v.Elements());
    ASSERT_EQ(s[2], 3);

    Span<const int> cs{r};
    ASSERT_EQ(cs.size(), narrow_cast<size_t>(v.Length()));
    ASSERT_EQ(cs.data(), v.Elements());
    ASSERT_EQ(cs[2], 3);
  }
  {
    nsTArray<int> v;
    v.AppendElement(1);
    v.AppendElement(2);
    v.AppendElement(3);

    Range<int> r(v.Elements(), v.Length());

    AssertSpanOfThreeInts(r);

    auto s = MakeSpan(r);
    ASSERT_EQ(s.size(), narrow_cast<size_t>(v.Length()));
    ASSERT_EQ(s.data(), v.Elements());
    ASSERT_EQ(s[2], 3);
  }
}

SPAN_TEST(from_cstring) {
  {
    const char* str = nullptr;
    auto cs = MakeStringSpan(str);
    ASSERT_EQ(cs.size(), 0U);
  }
  {
    const char* str = "abc";

    auto cs = MakeStringSpan(str);
    ASSERT_EQ(cs.size(), 3U);
    ASSERT_EQ(cs.data(), str);
    ASSERT_EQ(cs[2], 'c');

#ifdef CONFIRM_COMPILATION_ERRORS
    Span<const char> scccl("literal");  // error

    Span<const char> sccel;
    sccel = "literal";  // error

    cs = MakeSpan("literal");  // error
#endif
  }
  {
    char arr[4] = {'a', 'b', 'c', 0};

    auto cs = MakeStringSpan(arr);
    ASSERT_EQ(cs.size(), 3U);
    ASSERT_EQ(cs.data(), arr);
    ASSERT_EQ(cs[2], 'c');

    cs = MakeSpan(arr);
    ASSERT_EQ(cs.size(), 4U);  // zero terminator is part of the array span.
    ASSERT_EQ(cs.data(), arr);
    ASSERT_EQ(cs[2], 'c');
    ASSERT_EQ(cs[3], '\0');  // zero terminator is part of the array span.

#ifdef CONFIRM_COMPILATION_ERRORS
    Span<char> scca(arr);         // error
    Span<const char> sccca(arr);  // error

    Span<const char> scccea;
    scccea = arr;  // error
#endif
  }
  {
    const char16_t* str = nullptr;
    auto cs = MakeStringSpan(str);
    ASSERT_EQ(cs.size(), 0U);
  }
  {
    char16_t arr[4] = {'a', 'b', 'c', 0};
    const char16_t* str = arr;

    auto cs = MakeStringSpan(str);
    ASSERT_EQ(cs.size(), 3U);
    ASSERT_EQ(cs.data(), str);
    ASSERT_EQ(cs[2], 'c');

    cs = MakeStringSpan(arr);
    ASSERT_EQ(cs.size(), 3U);
    ASSERT_EQ(cs.data(), str);
    ASSERT_EQ(cs[2], 'c');

    cs = MakeSpan(arr);
    ASSERT_EQ(cs.size(), 4U);  // zero terminator is part of the array span.
    ASSERT_EQ(cs.data(), str);
    ASSERT_EQ(cs[2], 'c');
    ASSERT_EQ(cs[3], '\0');  // zero terminator is part of the array span.

#ifdef CONFIRM_COMPILATION_ERRORS
    Span<char16_t> scca(arr);  // error

    Span<const char16_t> scccea;
    scccea = arr;  // error

    Span<const char16_t> scccl(u"literal");  // error

    Span<const char16_t>* sccel;
    *sccel = u"literal";  // error

    cs = MakeSpan(u"literal");  // error
#endif
  }
}

SPAN_TEST(from_convertible_Span_constructor){{Span<DerivedClass> avd;
Span<const DerivedClass> avcd = avd;
static_cast<void>(avcd);
}

{
#ifdef CONFIRM_COMPILATION_ERRORS
  Span<DerivedClass> avd;
  Span<BaseClass> avb = avd;
  static_cast<void>(avb);
#endif
}

#ifdef CONFIRM_COMPILATION_ERRORS
{
  Span<int> s;
  Span<unsigned int> s2 = s;
  static_cast<void>(s2);
}

{
  Span<int> s;
  Span<const unsigned int> s2 = s;
  static_cast<void>(s2);
}

{
  Span<int> s;
  Span<short> s2 = s;
  static_cast<void>(s2);
}
#endif
}

SPAN_TEST(copy_move_and_assignment) {
  Span<int> s1;
  ASSERT_TRUE(s1.empty());

  int arr[] = {3, 4, 5};

  Span<const int> s2 = arr;
  ASSERT_EQ(s2.Length(), 3U);
  ASSERT_EQ(s2.data(), &arr[0]);

  s2 = s1;
  ASSERT_TRUE(s2.empty());

  auto get_temp_Span = [&]() -> Span<int> { return {&arr[1], 2}; };
  auto use_Span = [&](Span<const int> s) {
    ASSERT_EQ(s.Length(), 2U);
    ASSERT_EQ(s.data(), &arr[1]);
  };
  use_Span(get_temp_Span());

  s1 = get_temp_Span();
  ASSERT_EQ(s1.Length(), 2U);
  ASSERT_EQ(s1.data(), &arr[1]);
}

SPAN_TEST(first) {
  int arr[5] = {1, 2, 3, 4, 5};

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.First<2>().Length(), 2U);
    ASSERT_EQ(av.First(2).Length(), 2U);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.First<0>().Length(), 0U);
    ASSERT_EQ(av.First(0).Length(), 0U);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.First<5>().Length(), 5U);
    ASSERT_EQ(av.First(5).Length(), 5U);
  }

#if 0
        {
            Span<int, 5> av = arr;
#  ifdef CONFIRM_COMPILATION_ERRORS
            ASSERT_EQ(av.First<6>().Length() , 6U);
            ASSERT_EQ(av.First<-1>().Length() , -1);
#  endif
            CHECK_THROW(av.First(6).Length(), fail_fast);
        }
#endif

  {
    Span<int> av;
    ASSERT_EQ(av.First<0>().Length(), 0U);
    ASSERT_EQ(av.First(0).Length(), 0U);
  }
}

SPAN_TEST(last) {
  int arr[5] = {1, 2, 3, 4, 5};

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.Last<2>().Length(), 2U);
    ASSERT_EQ(av.Last(2).Length(), 2U);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.Last<0>().Length(), 0U);
    ASSERT_EQ(av.Last(0).Length(), 0U);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.Last<5>().Length(), 5U);
    ASSERT_EQ(av.Last(5).Length(), 5U);
  }

#if 0
        {
            Span<int, 5> av = arr;
#  ifdef CONFIRM_COMPILATION_ERRORS
            ASSERT_EQ(av.Last<6>().Length() , 6U);
#  endif
            CHECK_THROW(av.Last(6).Length(), fail_fast);
        }
#endif

  {
    Span<int> av;
    ASSERT_EQ(av.Last<0>().Length(), 0U);
    ASSERT_EQ(av.Last(0).Length(), 0U);
  }
}

SPAN_TEST(from_to) {
  int arr[5] = {1, 2, 3, 4, 5};

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.From(3).Length(), 2U);
    ASSERT_EQ(av.From(2)[1], 4);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.From(5).Length(), 0U);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.From(0).Length(), 5U);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.To(3).Length(), 3U);
    ASSERT_EQ(av.To(3)[1], 2);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.To(0).Length(), 0U);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.To(5).Length(), 5U);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.FromTo(1, 4).Length(), 3U);
    ASSERT_EQ(av.FromTo(1, 4)[1], 3);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.FromTo(2, 2).Length(), 0U);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.FromTo(0, 5).Length(), 5U);
  }
}

SPAN_TEST(Subspan) {
  int arr[5] = {1, 2, 3, 4, 5};

  {
    Span<int, 5> av = arr;
    ASSERT_EQ((av.Subspan<2, 2>().Length()), 2U);
    ASSERT_EQ(av.Subspan(2, 2).Length(), 2U);
    ASSERT_EQ(av.Subspan(2, 3).Length(), 3U);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ((av.Subspan<0, 0>().Length()), 0U);
    ASSERT_EQ(av.Subspan(0, 0).Length(), 0U);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ((av.Subspan<0, 5>().Length()), 5U);
    ASSERT_EQ(av.Subspan(0, 5).Length(), 5U);
    CHECK_THROW(av.Subspan(0, 6).Length(), fail_fast);
    CHECK_THROW(av.Subspan(1, 5).Length(), fail_fast);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ((av.Subspan<4, 0>().Length()), 0U);
    ASSERT_EQ(av.Subspan(4, 0).Length(), 0U);
    ASSERT_EQ(av.Subspan(5, 0).Length(), 0U);
    CHECK_THROW(av.Subspan(6, 0).Length(), fail_fast);
  }

  {
    Span<int> av;
    ASSERT_EQ((av.Subspan<0, 0>().Length()), 0U);
    ASSERT_EQ(av.Subspan(0, 0).Length(), 0U);
    CHECK_THROW((av.Subspan<1, 0>().Length()), fail_fast);
  }

  {
    Span<int> av;
    ASSERT_EQ(av.Subspan(0).Length(), 0U);
    CHECK_THROW(av.Subspan(1).Length(), fail_fast);
  }

  {
    Span<int> av = arr;
    ASSERT_EQ(av.Subspan(0).Length(), 5U);
    ASSERT_EQ(av.Subspan(1).Length(), 4U);
    ASSERT_EQ(av.Subspan(4).Length(), 1U);
    ASSERT_EQ(av.Subspan(5).Length(), 0U);
    CHECK_THROW(av.Subspan(6).Length(), fail_fast);
    auto av2 = av.Subspan(1);
    for (int i = 0; i < 4; ++i) ASSERT_EQ(av2[i], i + 2);
  }

  {
    Span<int, 5> av = arr;
    ASSERT_EQ(av.Subspan(0).Length(), 5U);
    ASSERT_EQ(av.Subspan(1).Length(), 4U);
    ASSERT_EQ(av.Subspan(4).Length(), 1U);
    ASSERT_EQ(av.Subspan(5).Length(), 0U);
    CHECK_THROW(av.Subspan(6).Length(), fail_fast);
    auto av2 = av.Subspan(1);
    for (int i = 0; i < 4; ++i) ASSERT_EQ(av2[i], i + 2);
  }
}

SPAN_TEST(at_call) {
  int arr[4] = {1, 2, 3, 4};

  {
    Span<int> s = arr;
    ASSERT_EQ(s.at(0), 1);
    CHECK_THROW(s.at(5), fail_fast);
  }

  {
    int arr2d[2] = {1, 6};
    Span<int, 2> s = arr2d;
    ASSERT_EQ(s.at(0), 1);
    ASSERT_EQ(s.at(1), 6);
    CHECK_THROW(s.at(2), fail_fast);
  }
}

SPAN_TEST(operator_function_call) {
  int arr[4] = {1, 2, 3, 4};

  {
    Span<int> s = arr;
    ASSERT_EQ(s(0), 1);
    CHECK_THROW(s(5), fail_fast);
  }

  {
    int arr2d[2] = {1, 6};
    Span<int, 2> s = arr2d;
    ASSERT_EQ(s(0), 1);
    ASSERT_EQ(s(1), 6);
    CHECK_THROW(s(2), fail_fast);
  }
}

SPAN_TEST(iterator_default_init) {
  Span<int>::iterator it1;
  Span<int>::iterator it2;
  ASSERT_EQ(it1, it2);
}

SPAN_TEST(const_iterator_default_init) {
  Span<int>::const_iterator it1;
  Span<int>::const_iterator it2;
  ASSERT_EQ(it1, it2);
}

SPAN_TEST(iterator_conversions) {
  Span<int>::iterator badIt;
  Span<int>::const_iterator badConstIt;
  ASSERT_EQ(badIt, badConstIt);

  int a[] = {1, 2, 3, 4};
  Span<int> s = a;

  auto it = s.begin();
  auto cit = s.cbegin();

  ASSERT_EQ(it, cit);
  ASSERT_EQ(cit, it);

  Span<int>::const_iterator cit2 = it;
  ASSERT_EQ(cit2, cit);

  Span<int>::const_iterator cit3 = it + 4;
  ASSERT_EQ(cit3, s.cend());
}

SPAN_TEST(iterator_comparisons) {
  int a[] = {1, 2, 3, 4};
  {
    Span<int> s = a;
    Span<int>::iterator it = s.begin();
    auto it2 = it + 1;
    Span<int>::const_iterator cit = s.cbegin();

    ASSERT_EQ(it, cit);
    ASSERT_EQ(cit, it);
    ASSERT_EQ(it, it);
    ASSERT_EQ(cit, cit);
    ASSERT_EQ(cit, s.begin());
    ASSERT_EQ(s.begin(), cit);
    ASSERT_EQ(s.cbegin(), cit);
    ASSERT_EQ(it, s.begin());
    ASSERT_EQ(s.begin(), it);

    ASSERT_NE(it, it2);
    ASSERT_NE(it2, it);
    ASSERT_NE(it, s.end());
    ASSERT_NE(it2, s.end());
    ASSERT_NE(s.end(), it);
    ASSERT_NE(it2, cit);
    ASSERT_NE(cit, it2);

    ASSERT_LT(it, it2);
    ASSERT_LE(it, it2);
    ASSERT_LE(it2, s.end());
    ASSERT_LT(it, s.end());
    ASSERT_LE(it, cit);
    ASSERT_LE(cit, it);
    ASSERT_LT(cit, it2);
    ASSERT_LE(cit, it2);
    ASSERT_LT(cit, s.end());
    ASSERT_LE(cit, s.end());

    ASSERT_GT(it2, it);
    ASSERT_GE(it2, it);
    ASSERT_GT(s.end(), it2);
    ASSERT_GE(s.end(), it2);
    ASSERT_GT(it2, cit);
    ASSERT_GE(it2, cit);
  }
}

SPAN_TEST(begin_end) {
  {
    int a[] = {1, 2, 3, 4};
    Span<int> s = a;

    Span<int>::iterator it = s.begin();
    Span<int>::iterator it2 = std::begin(s);
    ASSERT_EQ(it, it2);

    it = s.end();
    it2 = std::end(s);
    ASSERT_EQ(it, it2);
  }

  {
    int a[] = {1, 2, 3, 4};
    Span<int> s = a;

    auto it = s.begin();
    auto first = it;
    ASSERT_EQ(it, first);
    ASSERT_EQ(*it, 1);

    auto beyond = s.end();
    ASSERT_NE(it, beyond);
    CHECK_THROW(*beyond, fail_fast);

    ASSERT_EQ(beyond - first, 4U);
    ASSERT_EQ(first - first, 0U);
    ASSERT_EQ(beyond - beyond, 0U);

    ++it;
    ASSERT_EQ(it - first, 1U);
    ASSERT_EQ(*it, 2);
    *it = 22;
    ASSERT_EQ(*it, 22);
    ASSERT_EQ(beyond - it, 3U);

    it = first;
    ASSERT_EQ(it, first);
    while (it != s.end()) {
      *it = 5;
      ++it;
    }

    ASSERT_EQ(it, beyond);
    ASSERT_EQ(it - beyond, 0U);

    for (auto& n : s) {
      ASSERT_EQ(n, 5);
    }
  }
}

SPAN_TEST(cbegin_cend) {
#if 0
          {
              int a[] = { 1, 2, 3, 4 };
              Span<int> s = a;

              Span<int>::const_iterator cit = s.cbegin();
              Span<int>::const_iterator cit2 = std::cbegin(s);
              ASSERT_EQ(cit , cit2);

              cit = s.cend();
              cit2 = std::cend(s);
              ASSERT_EQ(cit , cit2);
          }
#endif
  {
    int a[] = {1, 2, 3, 4};
    Span<int> s = a;

    auto it = s.cbegin();
    auto first = it;
    ASSERT_EQ(it, first);
    ASSERT_EQ(*it, 1);

    auto beyond = s.cend();
    ASSERT_NE(it, beyond);
    CHECK_THROW(*beyond, fail_fast);

    ASSERT_EQ(beyond - first, 4U);
    ASSERT_EQ(first - first, 0U);
    ASSERT_EQ(beyond - beyond, 0U);

    ++it;
    ASSERT_EQ(it - first, 1U);
    ASSERT_EQ(*it, 2);
    ASSERT_EQ(beyond - it, 3U);

    int last = 0;
    it = first;
    ASSERT_EQ(it, first);
    while (it != s.cend()) {
      ASSERT_EQ(*it, last + 1);

      last = *it;
      ++it;
    }

    ASSERT_EQ(it, beyond);
    ASSERT_EQ(it - beyond, 0U);
  }
}

SPAN_TEST(rbegin_rend) {
  {
    int a[] = {1, 2, 3, 4};
    Span<int> s = a;

    auto it = s.rbegin();
    auto first = it;
    ASSERT_EQ(it, first);
    ASSERT_EQ(*it, 4);

    auto beyond = s.rend();
    ASSERT_NE(it, beyond);
    CHECK_THROW(*beyond, fail_fast);

    ASSERT_EQ(beyond - first, 4U);
    ASSERT_EQ(first - first, 0U);
    ASSERT_EQ(beyond - beyond, 0U);

    ++it;
    ASSERT_EQ(it - first, 1U);
    ASSERT_EQ(*it, 3);
    *it = 22;
    ASSERT_EQ(*it, 22);
    ASSERT_EQ(beyond - it, 3U);

    it = first;
    ASSERT_EQ(it, first);
    while (it != s.rend()) {
      *it = 5;
      ++it;
    }

    ASSERT_EQ(it, beyond);
    ASSERT_EQ(it - beyond, 0U);

    for (auto& n : s) {
      ASSERT_EQ(n, 5);
    }
  }
}

SPAN_TEST(crbegin_crend) {
  {
    int a[] = {1, 2, 3, 4};
    Span<int> s = a;

    auto it = s.crbegin();
    auto first = it;
    ASSERT_EQ(it, first);
    ASSERT_EQ(*it, 4);

    auto beyond = s.crend();
    ASSERT_NE(it, beyond);
    CHECK_THROW(*beyond, fail_fast);

    ASSERT_EQ(beyond - first, 4U);
    ASSERT_EQ(first - first, 0U);
    ASSERT_EQ(beyond - beyond, 0U);

    ++it;
    ASSERT_EQ(it - first, 1U);
    ASSERT_EQ(*it, 3);
    ASSERT_EQ(beyond - it, 3U);

    it = first;
    ASSERT_EQ(it, first);
    int last = 5;
    while (it != s.crend()) {
      ASSERT_EQ(*it, last - 1);
      last = *it;

      ++it;
    }

    ASSERT_EQ(it, beyond);
    ASSERT_EQ(it - beyond, 0U);
  }
}

SPAN_TEST(comparison_operators) {
  {
    Span<int> s1 = nullptr;
    Span<int> s2 = nullptr;
    ASSERT_EQ(s1, s2);
    ASSERT_FALSE(s1 != s2);
    ASSERT_FALSE(s1 < s2);
    ASSERT_LE(s1, s2);
    ASSERT_FALSE(s1 > s2);
    ASSERT_GE(s1, s2);
    ASSERT_EQ(s2, s1);
    ASSERT_FALSE(s2 != s1);
    ASSERT_FALSE(s2 < s1);
    ASSERT_LE(s2, s1);
    ASSERT_FALSE(s2 > s1);
    ASSERT_GE(s2, s1);
  }

  {
    int arr[] = {2, 1};
    Span<int> s1 = arr;
    Span<int> s2 = arr;

    ASSERT_EQ(s1, s2);
    ASSERT_FALSE(s1 != s2);
    ASSERT_FALSE(s1 < s2);
    ASSERT_LE(s1, s2);
    ASSERT_FALSE(s1 > s2);
    ASSERT_GE(s1, s2);
    ASSERT_EQ(s2, s1);
    ASSERT_FALSE(s2 != s1);
    ASSERT_FALSE(s2 < s1);
    ASSERT_LE(s2, s1);
    ASSERT_FALSE(s2 > s1);
    ASSERT_GE(s2, s1);
  }

  {
    int arr[] = {2, 1};  // bigger

    Span<int> s1 = nullptr;
    Span<int> s2 = arr;

    ASSERT_NE(s1, s2);
    ASSERT_NE(s2, s1);
    ASSERT_NE(s1, s2);
    ASSERT_NE(s2, s1);
    ASSERT_LT(s1, s2);
    ASSERT_FALSE(s2 < s1);
    ASSERT_LE(s1, s2);
    ASSERT_FALSE(s2 <= s1);
    ASSERT_GT(s2, s1);
    ASSERT_FALSE(s1 > s2);
    ASSERT_GE(s2, s1);
    ASSERT_FALSE(s1 >= s2);
  }

  {
    int arr1[] = {1, 2};
    int arr2[] = {1, 2};
    Span<int> s1 = arr1;
    Span<int> s2 = arr2;

    ASSERT_EQ(s1, s2);
    ASSERT_FALSE(s1 != s2);
    ASSERT_FALSE(s1 < s2);
    ASSERT_LE(s1, s2);
    ASSERT_FALSE(s1 > s2);
    ASSERT_GE(s1, s2);
    ASSERT_EQ(s2, s1);
    ASSERT_FALSE(s2 != s1);
    ASSERT_FALSE(s2 < s1);
    ASSERT_LE(s2, s1);
    ASSERT_FALSE(s2 > s1);
    ASSERT_GE(s2, s1);
  }

  {
    int arr[] = {1, 2, 3};

    AssertSpanOfThreeInts(arr);

    Span<int> s1 = {&arr[0], 2};  // shorter
    Span<int> s2 = arr;           // longer

    ASSERT_NE(s1, s2);
    ASSERT_NE(s2, s1);
    ASSERT_NE(s1, s2);
    ASSERT_NE(s2, s1);
    ASSERT_LT(s1, s2);
    ASSERT_FALSE(s2 < s1);
    ASSERT_LE(s1, s2);
    ASSERT_FALSE(s2 <= s1);
    ASSERT_GT(s2, s1);
    ASSERT_FALSE(s1 > s2);
    ASSERT_GE(s2, s1);
    ASSERT_FALSE(s1 >= s2);
  }

  {
    int arr1[] = {1, 2};  // smaller
    int arr2[] = {2, 1};  // bigger

    Span<int> s1 = arr1;
    Span<int> s2 = arr2;

    ASSERT_NE(s1, s2);
    ASSERT_NE(s2, s1);
    ASSERT_NE(s1, s2);
    ASSERT_NE(s2, s1);
    ASSERT_LT(s1, s2);
    ASSERT_FALSE(s2 < s1);
    ASSERT_LE(s1, s2);
    ASSERT_FALSE(s2 <= s1);
    ASSERT_GT(s2, s1);
    ASSERT_FALSE(s1 > s2);
    ASSERT_GE(s2, s1);
    ASSERT_FALSE(s1 >= s2);
  }
}

SPAN_TEST(as_bytes) {
  int a[] = {1, 2, 3, 4};

  {
    Span<const int> s = a;
    ASSERT_EQ(s.Length(), 4U);
    Span<const uint8_t> bs = AsBytes(s);
    ASSERT_EQ(static_cast<const void*>(bs.data()),
              static_cast<const void*>(s.data()));
    ASSERT_EQ(bs.Length(), s.LengthBytes());
  }

  {
    Span<int> s;
    auto bs = AsBytes(s);
    ASSERT_EQ(bs.Length(), s.Length());
    ASSERT_EQ(bs.Length(), 0U);
    ASSERT_EQ(bs.size_bytes(), 0U);
    ASSERT_EQ(static_cast<const void*>(bs.data()),
              static_cast<const void*>(s.data()));
    ASSERT_EQ(bs.data(), reinterpret_cast<const uint8_t*>(SLICE_INT_PTR));
  }

  {
    Span<int> s = a;
    auto bs = AsBytes(s);
    ASSERT_EQ(static_cast<const void*>(bs.data()),
              static_cast<const void*>(s.data()));
    ASSERT_EQ(bs.Length(), s.LengthBytes());
  }
}

SPAN_TEST(as_writable_bytes) {
  int a[] = {1, 2, 3, 4};

  {
#ifdef CONFIRM_COMPILATION_ERRORS
    // you should not be able to get writeable bytes for const objects
    Span<const int> s = a;
    ASSERT_EQ(s.Length(), 4U);
    Span<const byte> bs = AsWritableBytes(s);
    ASSERT_EQ(static_cast<void*>(bs.data()), static_cast<void*>(s.data()));
    ASSERT_EQ(bs.Length(), s.LengthBytes());
#endif
  }

  {
    Span<int> s;
    auto bs = AsWritableBytes(s);
    ASSERT_EQ(bs.Length(), s.Length());
    ASSERT_EQ(bs.Length(), 0U);
    ASSERT_EQ(bs.size_bytes(), 0U);
    ASSERT_EQ(static_cast<void*>(bs.data()), static_cast<void*>(s.data()));
    ASSERT_EQ(bs.data(), reinterpret_cast<uint8_t*>(SLICE_INT_PTR));
  }

  {
    Span<int> s = a;
    auto bs = AsWritableBytes(s);
    ASSERT_EQ(static_cast<void*>(bs.data()), static_cast<void*>(s.data()));
    ASSERT_EQ(bs.Length(), s.LengthBytes());
  }
}

SPAN_TEST(as_chars) {
  const uint8_t a[] = {1, 2, 3, 4};
  Span<const uint8_t> u = MakeSpan(a);
  Span<const char> c = AsChars(u);
  ASSERT_EQ(static_cast<const void*>(u.data()),
            static_cast<const void*>(c.data()));
  ASSERT_EQ(u.size(), c.size());
}

SPAN_TEST(as_writable_chars) {
  uint8_t a[] = {1, 2, 3, 4};
  Span<uint8_t> u = MakeSpan(a);
  Span<char> c = AsWritableChars(u);
  ASSERT_EQ(static_cast<void*>(u.data()), static_cast<void*>(c.data()));
  ASSERT_EQ(u.size(), c.size());
}

SPAN_TEST(fixed_size_conversions) {
  int arr[] = {1, 2, 3, 4};

  // converting to an Span from an equal size array is ok
  Span<int, 4> s4 = arr;
  ASSERT_EQ(s4.Length(), 4U);

  // converting to dynamic_range is always ok
  {
    Span<int> s = s4;
    ASSERT_EQ(s.Length(), s4.Length());
    static_cast<void>(s);
  }

// initialization or assignment to static Span that REDUCES size is NOT ok
#ifdef CONFIRM_COMPILATION_ERRORS
  { Span<int, 2> s = arr; }
  {
    Span<int, 2> s2 = s4;
    static_cast<void>(s2);
  }
#endif

#if 0
        // even when done dynamically
        {
            Span<int> s = arr;
            auto f = [&]() {
                Span<int, 2> s2 = s;
                static_cast<void>(s2);
            };
            CHECK_THROW(f(), fail_fast);
        }
#endif

  // but doing so explicitly is ok

  // you can convert statically
  {
    Span<int, 2> s2 = {arr, 2};
    static_cast<void>(s2);
  }
  {
    Span<int, 1> s1 = s4.First<1>();
    static_cast<void>(s1);
  }

  // ...or dynamically
  {
    // NB: implicit conversion to Span<int,1> from Span<int>
    Span<int, 1> s1 = s4.First(1);
    static_cast<void>(s1);
  }

#if 0
        // initialization or assignment to static Span that requires size INCREASE is not ok.
        int arr2[2] = {1, 2};
#endif

#ifdef CONFIRM_COMPILATION_ERRORS
  { Span<int, 4> s3 = arr2; }
  {
    Span<int, 2> s2 = arr2;
    Span<int, 4> s4a = s2;
  }
#endif

#if 0
        {
            auto f = [&]() {
                Span<int, 4> _s4 = {arr2, 2};
                static_cast<void>(_s4);
            };
            CHECK_THROW(f(), fail_fast);
        }

        // this should fail - we are trying to assign a small dynamic Span to a fixed_size larger one
        Span<int> av = arr2;
        auto f = [&]() {
            Span<int, 4> _s4 = av;
            static_cast<void>(_s4);
        };
        CHECK_THROW(f(), fail_fast);
#endif
}

#if 0
    SPAN_TEST(interop_with_std_regex)
    {
        char lat[] = { '1', '2', '3', '4', '5', '6', 'E', 'F', 'G' };
        Span<char> s = lat;
        auto f_it = s.begin() + 7;

        std::match_results<Span<char>::iterator> match;

        std::regex_match(s.begin(), s.end(), match, std::regex(".*"));
        ASSERT_EQ(match.ready());
        ASSERT_TRUE(!match.empty());
        ASSERT_TRUE(match[0].matched);
        ASSERT_TRUE(match[0].first , s.begin());
        ASSERT_EQ(match[0].second , s.end());

        std::regex_search(s.begin(), s.end(), match, std::regex("F"));
        ASSERT_TRUE(match.ready());
        ASSERT_TRUE(!match.empty());
        ASSERT_TRUE(match[0].matched);
        ASSERT_EQ(match[0].first , f_it);
        ASSERT_EQ(match[0].second , (f_it + 1));
    }

SPAN_TEST(interop_with_gsl_at)
{
  int arr[5] = { 1, 2, 3, 4, 5 };
  Span<int> s{ arr };
  ASSERT_EQ(at(s, 0) , 1 );
ASSERT_EQ(at(s, 1) , 2U);
}
#endif

SPAN_TEST(default_constructible) {
  ASSERT_TRUE((std::is_default_constructible<Span<int>>::value));
  ASSERT_TRUE((std::is_default_constructible<Span<int, 0>>::value));
  ASSERT_TRUE((!std::is_default_constructible<Span<int, 42>>::value));
}
