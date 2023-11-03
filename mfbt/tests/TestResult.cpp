/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include <string.h>
#include "mozilla/ResultVariant.h"
#include "mozilla/Try.h"
#include "mozilla/UniquePtr.h"

using mozilla::Err;
using mozilla::GenericErrorResult;
using mozilla::Ok;
using mozilla::Result;
using mozilla::UniquePtr;

#define MOZ_STATIC_AND_RELEASE_ASSERT(expr) \
  static_assert(expr);                      \
  MOZ_RELEASE_ASSERT(expr)

enum struct TestUnusedZeroEnum : int16_t { Ok = 0, NotOk = 1 };

namespace mozilla::detail {
template <>
struct UnusedZero<TestUnusedZeroEnum> : UnusedZeroEnum<TestUnusedZeroEnum> {};
}  // namespace mozilla::detail

struct Failed {};

namespace mozilla::detail {
template <>
struct UnusedZero<Failed> {
  using StorageType = uintptr_t;

  static constexpr bool value = true;
  static constexpr StorageType nullValue = 0;
  static constexpr StorageType GetDefaultValue() { return 2; }

  static constexpr void AssertValid(StorageType aValue) {}
  static constexpr Failed Inspect(const StorageType& aValue) {
    return Failed{};
  }
  static constexpr Failed Unwrap(StorageType aValue) { return Failed{}; }
  static constexpr StorageType Store(Failed aValue) {
    return GetDefaultValue();
  }
};

}  // namespace mozilla::detail

// V is trivially default-constructible, and E has UnusedZero<E>::value == true,
// for a reference type and for a non-reference type
static_assert(mozilla::detail::SelectResultImpl<uintptr_t, Failed>::value ==
              mozilla::detail::PackingStrategy::NullIsOk);
static_assert(
    mozilla::detail::SelectResultImpl<Ok, TestUnusedZeroEnum>::value ==
    mozilla::detail::PackingStrategy::NullIsOk);
static_assert(mozilla::detail::SelectResultImpl<Ok, Failed>::value ==
              mozilla::detail::PackingStrategy::LowBitTagIsError);

static_assert(std::is_trivially_destructible_v<Result<uintptr_t, Failed>>);
static_assert(std::is_trivially_destructible_v<Result<Ok, TestUnusedZeroEnum>>);
static_assert(std::is_trivially_destructible_v<Result<Ok, Failed>>);

static_assert(
    sizeof(Result<bool, TestUnusedZeroEnum>) <= sizeof(uintptr_t),
    "Result with bool value type should not be larger than pointer-sized");
static_assert(sizeof(Result<Ok, Failed>) == sizeof(uint8_t),
              "Result with empty value type should be size 1");
static_assert(sizeof(Result<int*, Failed>) == sizeof(uintptr_t),
              "Result with two aligned pointer types should be pointer-sized");
static_assert(
    sizeof(Result<char*, Failed*>) > sizeof(char*),
    "Result with unaligned success type `char*` must not be pointer-sized");
static_assert(
    sizeof(Result<int*, char*>) > sizeof(char*),
    "Result with unaligned error type `char*` must not be pointer-sized");

enum Foo8 : uint8_t {};
enum Foo16 : uint16_t {};
enum Foo32 : uint32_t {};
static_assert(sizeof(Result<Ok, Foo8>) <= sizeof(uintptr_t),
              "Result with small types should be pointer-sized");
static_assert(sizeof(Result<Ok, Foo16>) <= sizeof(uintptr_t),
              "Result with small types should be pointer-sized");
static_assert(sizeof(Foo32) >= sizeof(uintptr_t) ||
                  sizeof(Result<Ok, Foo32>) <= sizeof(uintptr_t),
              "Result with small types should be pointer-sized");

static_assert(sizeof(Result<Foo16, Foo8>) <= sizeof(uintptr_t),
              "Result with small types should be pointer-sized");
static_assert(sizeof(Result<Foo8, Foo16>) <= sizeof(uintptr_t),
              "Result with small types should be pointer-sized");
static_assert(sizeof(Foo32) >= sizeof(uintptr_t) ||
                  sizeof(Result<Foo32, Foo16>) <= sizeof(uintptr_t),
              "Result with small types should be pointer-sized");
static_assert(sizeof(Foo32) >= sizeof(uintptr_t) ||
                  sizeof(Result<Foo16, Foo32>) <= sizeof(uintptr_t),
              "Result with small types should be pointer-sized");

#if __cplusplus < 202002L
static_assert(std::is_literal_type_v<Result<int*, Failed>>);
static_assert(std::is_literal_type_v<Result<Ok, Failed>>);
static_assert(std::is_literal_type_v<Result<Ok, Foo8>>);
static_assert(std::is_literal_type_v<Result<Foo8, Foo16>>);
static_assert(!std::is_literal_type_v<Result<Ok, UniquePtr<int>>>);
#endif

static constexpr GenericErrorResult<Failed> Fail() { return Err(Failed{}); }

static constexpr GenericErrorResult<TestUnusedZeroEnum>
FailTestUnusedZeroEnum() {
  return Err(TestUnusedZeroEnum::NotOk);
}

static constexpr Result<Ok, Failed> Task1(bool pass) {
  if (!pass) {
    return Fail();  // implicit conversion from GenericErrorResult to Result
  }
  return Ok();
}

static constexpr Result<Ok, TestUnusedZeroEnum> Task1UnusedZeroEnumErr(
    bool pass) {
  if (!pass) {
    return FailTestUnusedZeroEnum();  // implicit conversion from
                                      // GenericErrorResult to Result
  }
  return Ok();
}

static constexpr Result<int, Failed> Task2(bool pass, int value) {
  MOZ_TRY(
      Task1(pass));  // converts one type of result to another in the error case
  return value;      // implicit conversion from T to Result<T, E>
}

static constexpr Result<int, TestUnusedZeroEnum> Task2UnusedZeroEnumErr(
    bool pass, int value) {
  MOZ_TRY(Task1UnusedZeroEnumErr(
      pass));    // converts one type of result to another in the error case
  return value;  // implicit conversion from T to Result<T, E>
}

static Result<int, Failed> Task3(bool pass1, bool pass2, int value) {
  int x, y;
  MOZ_TRY_VAR(x, Task2(pass1, value));
  MOZ_TRY_VAR(y, Task2(pass2, value));
  return x + y;
}

static void BasicTests() {
  MOZ_STATIC_AND_RELEASE_ASSERT(Task1(true).isOk());
  MOZ_STATIC_AND_RELEASE_ASSERT(!Task1(true).isErr());
  MOZ_STATIC_AND_RELEASE_ASSERT(!Task1(false).isOk());
  MOZ_STATIC_AND_RELEASE_ASSERT(Task1(false).isErr());

  MOZ_STATIC_AND_RELEASE_ASSERT(Task1UnusedZeroEnumErr(true).isOk());
  MOZ_STATIC_AND_RELEASE_ASSERT(!Task1UnusedZeroEnumErr(true).isErr());
  MOZ_STATIC_AND_RELEASE_ASSERT(!Task1UnusedZeroEnumErr(false).isOk());
  MOZ_STATIC_AND_RELEASE_ASSERT(Task1UnusedZeroEnumErr(false).isErr());
  MOZ_STATIC_AND_RELEASE_ASSERT(TestUnusedZeroEnum::NotOk ==
                                Task1UnusedZeroEnumErr(false).inspectErr());
  MOZ_STATIC_AND_RELEASE_ASSERT(TestUnusedZeroEnum::NotOk ==
                                Task1UnusedZeroEnumErr(false).unwrapErr());

  // MOZ_TRY works.
  MOZ_STATIC_AND_RELEASE_ASSERT(Task2(true, 3).isOk());
  MOZ_STATIC_AND_RELEASE_ASSERT(Task2(true, 3).unwrap() == 3);
  MOZ_STATIC_AND_RELEASE_ASSERT(Task2(true, 3).unwrapOr(6) == 3);
  MOZ_RELEASE_ASSERT(Task2(false, 3).isErr());
  MOZ_RELEASE_ASSERT(Task2(false, 3).unwrapOr(6) == 6);

  MOZ_STATIC_AND_RELEASE_ASSERT(Task2UnusedZeroEnumErr(true, 3).isOk());
  MOZ_STATIC_AND_RELEASE_ASSERT(Task2UnusedZeroEnumErr(true, 3).unwrap() == 3);
  MOZ_STATIC_AND_RELEASE_ASSERT(Task2UnusedZeroEnumErr(true, 3).unwrapOr(6) ==
                                3);
  MOZ_STATIC_AND_RELEASE_ASSERT(Task2UnusedZeroEnumErr(false, 3).isErr());
  MOZ_STATIC_AND_RELEASE_ASSERT(Task2UnusedZeroEnumErr(false, 3).unwrapOr(6) ==
                                6);

  // MOZ_TRY_VAR works.
  MOZ_RELEASE_ASSERT(Task3(true, true, 3).isOk());
  MOZ_RELEASE_ASSERT(Task3(true, true, 3).unwrap() == 6);
  MOZ_RELEASE_ASSERT(Task3(true, false, 3).isErr());
  MOZ_RELEASE_ASSERT(Task3(false, true, 3).isErr());
  MOZ_RELEASE_ASSERT(Task3(false, true, 3).unwrapOr(6) == 6);

  // Lvalues should work too.
  {
    constexpr Result<Ok, Failed> res1 = Task1(true);
    MOZ_STATIC_AND_RELEASE_ASSERT(res1.isOk());
    MOZ_STATIC_AND_RELEASE_ASSERT(!res1.isErr());

    constexpr Result<Ok, Failed> res2 = Task1(false);
    MOZ_STATIC_AND_RELEASE_ASSERT(!res2.isOk());
    MOZ_STATIC_AND_RELEASE_ASSERT(res2.isErr());
  }

  {
    Result<int, Failed> res = Task2(true, 3);
    MOZ_RELEASE_ASSERT(res.isOk());
    MOZ_RELEASE_ASSERT(res.unwrap() == 3);

    res = Task2(false, 4);
    MOZ_RELEASE_ASSERT(res.isErr());
  }

  // Some tests for pointer tagging.
  {
    int i = 123;

    Result<int*, Failed> res = &i;
    static_assert(sizeof(res) == sizeof(uintptr_t),
                  "should use pointer tagging to fit in a word");

    MOZ_RELEASE_ASSERT(res.isOk());
    MOZ_RELEASE_ASSERT(*res.unwrap() == 123);

    res = Err(Failed());
    MOZ_RELEASE_ASSERT(res.isErr());
  }
}

struct NonCopyableNonMovable {
  explicit constexpr NonCopyableNonMovable(uint32_t aValue) : mValue(aValue) {}

  NonCopyableNonMovable(const NonCopyableNonMovable&) = delete;
  NonCopyableNonMovable(NonCopyableNonMovable&&) = delete;
  NonCopyableNonMovable& operator=(const NonCopyableNonMovable&) = delete;
  NonCopyableNonMovable& operator=(NonCopyableNonMovable&&) = delete;

  uint32_t mValue;
};

static void InPlaceConstructionTests() {
  {
    // PackingStrategy == NullIsOk
    static_assert(mozilla::detail::SelectResultImpl<NonCopyableNonMovable,
                                                    Failed>::value ==
                  mozilla::detail::PackingStrategy::NullIsOk);
    constexpr Result<NonCopyableNonMovable, Failed> result{std::in_place, 42u};
    MOZ_STATIC_AND_RELEASE_ASSERT(42 == result.inspect().mValue);
  }

  {
    // PackingStrategy == Variant
    static_assert(
        mozilla::detail::SelectResultImpl<NonCopyableNonMovable, int>::value ==
        mozilla::detail::PackingStrategy::Variant);
    const Result<NonCopyableNonMovable, int> result{std::in_place, 42};
    MOZ_RELEASE_ASSERT(42 == result.inspect().mValue);
  }
}

/* * */

struct Snafu : Failed {};

static Result<Ok, Snafu*> Explode() {
  static Snafu snafu;
  return Err(&snafu);
}

static Result<Ok, Failed*> ErrorGeneralization() {
  MOZ_TRY(Explode());  // change error type from Snafu* to more general Failed*
  return Ok();
}

static void TypeConversionTests() {
  MOZ_RELEASE_ASSERT(ErrorGeneralization().isErr());

  {
    const Result<Ok, Failed*> res = Explode();
    MOZ_RELEASE_ASSERT(res.isErr());
  }

  {
    const Result<Ok, Failed*> res = Result<Ok, Snafu*>{Ok{}};
    MOZ_RELEASE_ASSERT(res.isOk());
  }
}

static void EmptyValueTest() {
  struct Fine {};
  mozilla::Result<Fine, Failed> res((Fine()));
  res.unwrap();
  MOZ_RELEASE_ASSERT(res.isOk());
  static_assert(sizeof(res) == sizeof(uint8_t),
                "Result with empty value and error types should be size 1");
}

static void MapTest() {
  struct MyError {
    int x;

    explicit MyError(int y) : x(y) {}
  };

  // Mapping over success values, to the same success type.
  {
    Result<int, MyError> res(5);
    bool invoked = false;
    auto res2 = res.map([&invoked](int x) {
      MOZ_RELEASE_ASSERT(x == 5);
      invoked = true;
      return 6;
    });
    MOZ_RELEASE_ASSERT(res2.isOk());
    MOZ_RELEASE_ASSERT(invoked);
    MOZ_RELEASE_ASSERT(res2.unwrap() == 6);
  }

  // Mapping over success values, to a different success type.
  {
    Result<int, MyError> res(5);
    bool invoked = false;
    auto res2 = res.map([&invoked](int x) {
      MOZ_RELEASE_ASSERT(x == 5);
      invoked = true;
      return "hello";
    });
    MOZ_RELEASE_ASSERT(res2.isOk());
    MOZ_RELEASE_ASSERT(invoked);
    MOZ_RELEASE_ASSERT(strcmp(res2.unwrap(), "hello") == 0);
  }

  // Mapping over success values (constexpr).
  {
    constexpr uint64_t kValue = 42u;
    constexpr auto res2a = Result<int32_t, Failed>{5}.map([](int32_t x) {
      MOZ_RELEASE_ASSERT(x == 5);
      return kValue;
    });
    MOZ_STATIC_AND_RELEASE_ASSERT(res2a.isOk());
    MOZ_STATIC_AND_RELEASE_ASSERT(kValue == res2a.inspect());
  }

  // Mapping over error values.
  {
    MyError err(1);
    Result<char, MyError> res(err);
    MOZ_RELEASE_ASSERT(res.isErr());
    Result<char, MyError> res2 = res.map([](int x) {
      MOZ_RELEASE_ASSERT(false);
      return 'a';
    });
    MOZ_RELEASE_ASSERT(res2.isErr());
    MOZ_RELEASE_ASSERT(res2.unwrapErr().x == err.x);
  }

  // Function pointers instead of lambdas as the mapping function.
  {
    Result<const char*, MyError> res("hello");
    auto res2 = res.map(strlen);
    MOZ_RELEASE_ASSERT(res2.isOk());
    MOZ_RELEASE_ASSERT(res2.unwrap() == 5);
  }
}

static void MapErrTest() {
  struct MyError {
    int x;

    explicit MyError(int y) : x(y) {}
  };

  struct MyError2 {
    int a;

    explicit MyError2(int b) : a(b) {}
  };

  // Mapping over error values, to the same error type.
  {
    MyError err(1);
    Result<char, MyError> res(err);
    MOZ_RELEASE_ASSERT(res.isErr());
    bool invoked = false;
    auto res2 = res.mapErr([&invoked](const auto err) {
      MOZ_RELEASE_ASSERT(err.x == 1);
      invoked = true;
      return MyError(2);
    });
    MOZ_RELEASE_ASSERT(res2.isErr());
    MOZ_RELEASE_ASSERT(invoked);
    MOZ_RELEASE_ASSERT(res2.unwrapErr().x == 2);
  }

  // Mapping over error values, to a different error type.
  {
    MyError err(1);
    Result<char, MyError> res(err);
    MOZ_RELEASE_ASSERT(res.isErr());
    bool invoked = false;
    auto res2 = res.mapErr([&invoked](const auto err) {
      MOZ_RELEASE_ASSERT(err.x == 1);
      invoked = true;
      return MyError2(2);
    });
    MOZ_RELEASE_ASSERT(res2.isErr());
    MOZ_RELEASE_ASSERT(invoked);
    MOZ_RELEASE_ASSERT(res2.unwrapErr().a == 2);
  }

  // Mapping over success values.
  {
    Result<int, MyError> res(5);
    auto res2 = res.mapErr([](const auto err) {
      MOZ_RELEASE_ASSERT(false);
      return MyError(1);
    });
    MOZ_RELEASE_ASSERT(res2.isOk());
    MOZ_RELEASE_ASSERT(res2.unwrap() == 5);
  }

  // Function pointers instead of lambdas as the mapping function.
  {
    Result<Ok, const char*> res("hello");
    auto res2 = res.mapErr(strlen);
    MOZ_RELEASE_ASSERT(res2.isErr());
    MOZ_RELEASE_ASSERT(res2.unwrapErr() == 5);
  }
}

static Result<Ok, size_t> strlen_ResultWrapper(const char* aValue) {
  return Err(strlen(aValue));
}

static void OrElseTest() {
  struct MyError {
    int x;

    explicit constexpr MyError(int y) : x(y) {}
  };

  struct MyError2 {
    int a;

    explicit constexpr MyError2(int b) : a(b) {}
  };

  // `orElse`ing over error values, to Result<V, E> (the same error type) error
  // variant.
  {
    MyError err(1);
    Result<char, MyError> res(err);
    MOZ_RELEASE_ASSERT(res.isErr());
    bool invoked = false;
    auto res2 = res.orElse([&invoked](const auto err) -> Result<char, MyError> {
      MOZ_RELEASE_ASSERT(err.x == 1);
      invoked = true;
      if (err.x != 42) {
        return Err(MyError(2));
      }
      return 'a';
    });
    MOZ_RELEASE_ASSERT(res2.isErr());
    MOZ_RELEASE_ASSERT(invoked);
    MOZ_RELEASE_ASSERT(res2.unwrapErr().x == 2);
  }

  // `orElse`ing over error values, to Result<V, E> (the same error type)
  // success variant.
  {
    MyError err(42);
    Result<char, MyError> res(err);
    MOZ_RELEASE_ASSERT(res.isErr());
    bool invoked = false;
    auto res2 = res.orElse([&invoked](const auto err) -> Result<char, MyError> {
      MOZ_RELEASE_ASSERT(err.x == 42);
      invoked = true;
      if (err.x != 42) {
        return Err(MyError(2));
      }
      return 'a';
    });
    MOZ_RELEASE_ASSERT(res2.isOk());
    MOZ_RELEASE_ASSERT(invoked);
    MOZ_RELEASE_ASSERT(res2.unwrap() == 'a');
  }

  // `orElse`ing over error values, to Result<V, E2> (a different error type)
  // error variant.
  {
    MyError err(1);
    Result<char, MyError> res(err);
    MOZ_RELEASE_ASSERT(res.isErr());
    bool invoked = false;
    auto res2 =
        res.orElse([&invoked](const auto err) -> Result<char, MyError2> {
          MOZ_RELEASE_ASSERT(err.x == 1);
          invoked = true;
          if (err.x != 42) {
            return Err(MyError2(2));
          }
          return 'a';
        });
    MOZ_RELEASE_ASSERT(res2.isErr());
    MOZ_RELEASE_ASSERT(invoked);
    MOZ_RELEASE_ASSERT(res2.unwrapErr().a == 2);
  }

  // `orElse`ing over error values, to Result<V, E2> (a different error type)
  // success variant.
  {
    MyError err(42);
    Result<char, MyError> res(err);
    MOZ_RELEASE_ASSERT(res.isErr());
    bool invoked = false;
    auto res2 =
        res.orElse([&invoked](const auto err) -> Result<char, MyError2> {
          MOZ_RELEASE_ASSERT(err.x == 42);
          invoked = true;
          if (err.x != 42) {
            return Err(MyError2(2));
          }
          return 'a';
        });
    MOZ_RELEASE_ASSERT(res2.isOk());
    MOZ_RELEASE_ASSERT(invoked);
    MOZ_RELEASE_ASSERT(res2.unwrap() == 'a');
  }

  // `orElse`ing over success values.
  {
    Result<int, MyError> res(5);
    auto res2 = res.orElse([](const auto err) -> Result<int, MyError> {
      MOZ_RELEASE_ASSERT(false);
      return Err(MyError(1));
    });
    MOZ_RELEASE_ASSERT(res2.isOk());
    MOZ_RELEASE_ASSERT(res2.unwrap() == 5);
  }

  // Function pointers instead of lambdas as the `orElse`ing function.
  {
    Result<Ok, const char*> res("hello");
    auto res2 = res.orElse(strlen_ResultWrapper);
    MOZ_RELEASE_ASSERT(res2.isErr());
    MOZ_RELEASE_ASSERT(res2.unwrapErr() == 5);
  }
}

static void AndThenTest() {
  // `andThen`ing over success results.
  {
    Result<int, const char*> r1(10);
    Result<int, const char*> r2 =
        r1.andThen([](int x) { return Result<int, const char*>(x + 1); });
    MOZ_RELEASE_ASSERT(r2.isOk());
    MOZ_RELEASE_ASSERT(r2.unwrap() == 11);
  }

  // `andThen`ing over success results (constexpr).
  {
    constexpr Result<int, Failed> r2a = Result<int, Failed>{10}.andThen(
        [](int x) { return Result<int, Failed>(x + 1); });
    MOZ_STATIC_AND_RELEASE_ASSERT(r2a.isOk());
    MOZ_STATIC_AND_RELEASE_ASSERT(r2a.inspect() == 11);
  }

  // `andThen`ing over error results.
  {
    Result<int, const char*> r3("error");
    Result<int, const char*> r4 = r3.andThen([](int x) {
      MOZ_RELEASE_ASSERT(false);
      return Result<int, const char*>(1);
    });
    MOZ_RELEASE_ASSERT(r4.isErr());
    MOZ_RELEASE_ASSERT(r3.unwrapErr() == r4.unwrapErr());
  }

  // andThen with a function accepting an rvalue
  {
    Result<int, const char*> r1(10);
    Result<int, const char*> r2 =
        r1.andThen([](int&& x) { return Result<int, const char*>(x + 1); });
    MOZ_RELEASE_ASSERT(r2.isOk());
    MOZ_RELEASE_ASSERT(r2.unwrap() == 11);
  }

  // `andThen`ing over error results (constexpr).
  {
    constexpr Result<int, Failed> r4a =
        Result<int, Failed>{Failed{}}.andThen([](int x) {
          MOZ_RELEASE_ASSERT(false);
          return Result<int, Failed>(1);
        });
    MOZ_STATIC_AND_RELEASE_ASSERT(r4a.isErr());
  }
}

using UniqueResult = Result<UniquePtr<int>, const char*>;

static UniqueResult UniqueTask() { return mozilla::MakeUnique<int>(3); }
static UniqueResult UniqueTaskError() { return Err("bad"); }

using UniqueErrorResult = Result<int, UniquePtr<int>>;
static UniqueErrorResult UniqueError() {
  return Err(mozilla::MakeUnique<int>(4));
}

static Result<Ok, UniquePtr<int>> TryUniqueErrorResult() {
  MOZ_TRY(UniqueError());
  return Ok();
}

static void UniquePtrTest() {
  {
    auto result = UniqueTask();
    MOZ_RELEASE_ASSERT(result.isOk());
    auto ptr = result.unwrap();
    MOZ_RELEASE_ASSERT(ptr);
    MOZ_RELEASE_ASSERT(*ptr == 3);
    auto moved = result.unwrap();
    MOZ_RELEASE_ASSERT(!moved);
  }

  {
    auto err = UniqueTaskError();
    MOZ_RELEASE_ASSERT(err.isErr());
    auto ptr = err.unwrapOr(mozilla::MakeUnique<int>(4));
    MOZ_RELEASE_ASSERT(ptr);
    MOZ_RELEASE_ASSERT(*ptr == 4);
  }

  {
    auto result = UniqueTaskError();
    result = UniqueResult(mozilla::MakeUnique<int>(6));
    MOZ_RELEASE_ASSERT(result.isOk());
    MOZ_RELEASE_ASSERT(result.inspect() && *result.inspect() == 6);
  }

  {
    auto result = UniqueError();
    MOZ_RELEASE_ASSERT(result.isErr());
    MOZ_RELEASE_ASSERT(result.inspectErr());
    MOZ_RELEASE_ASSERT(*result.inspectErr() == 4);
    auto err = result.unwrapErr();
    MOZ_RELEASE_ASSERT(!result.inspectErr());
    MOZ_RELEASE_ASSERT(err);
    MOZ_RELEASE_ASSERT(*err == 4);

    result = UniqueErrorResult(0);
    MOZ_RELEASE_ASSERT(result.isOk() && result.unwrap() == 0);
  }

  {
    auto result = TryUniqueErrorResult();
    MOZ_RELEASE_ASSERT(result.isErr());
    auto err = result.unwrapErr();
    MOZ_RELEASE_ASSERT(err && *err == 4);
    MOZ_RELEASE_ASSERT(!result.inspectErr());
  }
}

struct ZeroIsUnusedStructForPointer {
  int x = 1;
};
enum class ZeroIsUnusedEnum1 : uint8_t {
  V1 = 1,
  V2 = 2,
};
enum class ZeroIsUnusedEnum2 : uint16_t {
  V1 = 1,
  V2 = 2,
};
enum class ZeroIsUnusedEnum4 : uint32_t {
  V1 = 1,
  V2 = 2,
};
enum class ZeroIsUnusedEnum8 : uint64_t {
  V1 = 1,
  V2 = 2,
};
struct EmptyErrorStruct {};

template <>
struct mozilla::detail::UnusedZero<ZeroIsUnusedStructForPointer*> {
  static const bool value = true;
};
template <>
struct mozilla::detail::UnusedZero<ZeroIsUnusedEnum1> {
  static const bool value = true;
};
template <>
struct mozilla::detail::UnusedZero<ZeroIsUnusedEnum2> {
  static const bool value = true;
};
template <>
struct mozilla::detail::UnusedZero<ZeroIsUnusedEnum4> {
  static const bool value = true;
};
template <>
struct mozilla::detail::UnusedZero<ZeroIsUnusedEnum8> {
  static const bool value = true;
};

static void ZeroIsEmptyErrorTest() {
  {
    ZeroIsUnusedStructForPointer s;

    using V = ZeroIsUnusedStructForPointer*;

    mozilla::Result<V, EmptyErrorStruct> result(&s);
    MOZ_RELEASE_ASSERT(sizeof(result) == sizeof(V));

    MOZ_RELEASE_ASSERT(result.isOk());
    MOZ_RELEASE_ASSERT(result.inspect() == &s);
  }

  {
    using V = ZeroIsUnusedStructForPointer*;

    mozilla::Result<V, EmptyErrorStruct> result(Err(EmptyErrorStruct{}));

    MOZ_RELEASE_ASSERT(result.isErr());
    MOZ_RELEASE_ASSERT(*reinterpret_cast<V*>(&result) == nullptr);
  }

  {
    ZeroIsUnusedEnum1 e = ZeroIsUnusedEnum1::V1;

    using V = ZeroIsUnusedEnum1;

    mozilla::Result<V, EmptyErrorStruct> result(e);
    MOZ_RELEASE_ASSERT(sizeof(result) == sizeof(V));

    MOZ_RELEASE_ASSERT(result.isOk());
    MOZ_RELEASE_ASSERT(result.inspect() == e);
  }

  {
    using V = ZeroIsUnusedEnum1;

    mozilla::Result<V, EmptyErrorStruct> result(Err(EmptyErrorStruct()));

    MOZ_RELEASE_ASSERT(result.isErr());
    MOZ_RELEASE_ASSERT(*reinterpret_cast<uint8_t*>(&result) == 0);
  }

  {
    ZeroIsUnusedEnum2 e = ZeroIsUnusedEnum2::V1;

    using V = ZeroIsUnusedEnum2;

    mozilla::Result<V, EmptyErrorStruct> result(e);
    MOZ_RELEASE_ASSERT(sizeof(result) == sizeof(V));

    MOZ_RELEASE_ASSERT(result.isOk());
    MOZ_RELEASE_ASSERT(result.inspect() == e);
  }

  {
    using V = ZeroIsUnusedEnum2;

    mozilla::Result<V, EmptyErrorStruct> result(Err(EmptyErrorStruct()));

    MOZ_RELEASE_ASSERT(result.isErr());
    MOZ_RELEASE_ASSERT(*reinterpret_cast<uint16_t*>(&result) == 0);
  }

  {
    ZeroIsUnusedEnum4 e = ZeroIsUnusedEnum4::V1;

    using V = ZeroIsUnusedEnum4;

    mozilla::Result<V, EmptyErrorStruct> result(e);
    MOZ_RELEASE_ASSERT(sizeof(result) == sizeof(V));

    MOZ_RELEASE_ASSERT(result.isOk());
    MOZ_RELEASE_ASSERT(result.inspect() == e);
  }

  {
    using V = ZeroIsUnusedEnum4;

    mozilla::Result<V, EmptyErrorStruct> result(Err(EmptyErrorStruct()));

    MOZ_RELEASE_ASSERT(result.isErr());
    MOZ_RELEASE_ASSERT(*reinterpret_cast<uint32_t*>(&result) == 0);
  }

  {
    ZeroIsUnusedEnum8 e = ZeroIsUnusedEnum8::V1;

    using V = ZeroIsUnusedEnum8;

    mozilla::Result<V, EmptyErrorStruct> result(e);
    MOZ_RELEASE_ASSERT(sizeof(result) == sizeof(V));

    MOZ_RELEASE_ASSERT(result.isOk());
    MOZ_RELEASE_ASSERT(result.inspect() == e);
  }

  {
    using V = ZeroIsUnusedEnum8;

    mozilla::Result<V, EmptyErrorStruct> result(Err(EmptyErrorStruct()));

    MOZ_RELEASE_ASSERT(result.isErr());
    MOZ_RELEASE_ASSERT(*reinterpret_cast<uint64_t*>(&result) == 0);
  }
}

class Foo {};

class C1 {};
class C2 : public C1 {};

class E1 {};
class E2 : public E1 {};

void UpcastTest() {
  {
    C2 c2;

    mozilla::Result<C2*, Failed> result(&c2);
    mozilla::Result<C1*, Failed> copied(std::move(result));

    MOZ_RELEASE_ASSERT(copied.inspect() == &c2);
  }

  {
    E2 e2;

    mozilla::Result<Foo, E2*> result(Err(&e2));
    mozilla::Result<Foo, E1*> copied(std::move(result));

    MOZ_RELEASE_ASSERT(copied.inspectErr() == &e2);
  }

  {
    C2 c2;

    mozilla::Result<C2*, E2*> result(&c2);
    mozilla::Result<C1*, E1*> copied(std::move(result));

    MOZ_RELEASE_ASSERT(copied.inspect() == &c2);
  }

  {
    E2 e2;

    mozilla::Result<C2*, E2*> result(Err(&e2));
    mozilla::Result<C1*, E1*> copied(std::move(result));

    MOZ_RELEASE_ASSERT(copied.inspectErr() == &e2);
  }
}

/* * */

int main() {
  BasicTests();
  InPlaceConstructionTests();
  TypeConversionTests();
  EmptyValueTest();
  MapTest();
  MapErrTest();
  OrElseTest();
  AndThenTest();
  UniquePtrTest();
  ZeroIsEmptyErrorTest();
  UpcastTest();
  return 0;
}
