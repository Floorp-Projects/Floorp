/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "mozilla/Result.h"
#include "mozilla/UniquePtr.h"

using mozilla::Err;
using mozilla::GenericErrorResult;
using mozilla::Ok;
using mozilla::Result;
using mozilla::UniquePtr;

enum struct UnusedZeroEnum : int32_t { Ok = 0, NotOk = 1 };

namespace mozilla::detail {
template <>
struct UnusedZero<UnusedZeroEnum> {
  using StorageType = UnusedZeroEnum;

  static constexpr bool value = true;
  static constexpr StorageType nullValue = UnusedZeroEnum::Ok;

  static constexpr StorageType GetDefaultValue() {
    return UnusedZeroEnum::NotOk;
  }

  static constexpr void AssertValid(StorageType aValue) {}
  static constexpr const UnusedZeroEnum& Inspect(const StorageType& aValue) {
    return aValue;
  }
  static constexpr UnusedZeroEnum Unwrap(StorageType aValue) { return aValue; }
  static constexpr StorageType Store(UnusedZeroEnum aValue) { return aValue; }
};
}  // namespace mozilla::detail

struct Failed {
  int x;
};

// V is trivially default-constructible, and E has UnusedZero<E>::value == true,
// for a reference type and for a non-reference type
static_assert(mozilla::detail::SelectResultImpl<uintptr_t, Failed&>::value ==
              mozilla::detail::PackingStrategy::NullIsOk);
static_assert(mozilla::detail::SelectResultImpl<Ok, UnusedZeroEnum>::value ==
              mozilla::detail::PackingStrategy::NullIsOk);

static_assert(std::is_trivially_destructible_v<Result<uintptr_t, Failed&>>);
static_assert(std::is_trivially_destructible_v<Result<Ok, UnusedZeroEnum>>);

static_assert(sizeof(Result<Ok, Failed&>) == sizeof(uintptr_t),
              "Result with empty value type should be pointer-sized");
static_assert(sizeof(Result<int*, Failed&>) == sizeof(uintptr_t),
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

static GenericErrorResult<Failed&> Fail() {
  static Failed failed;
  return Err<Failed&>(failed);
}

static GenericErrorResult<UnusedZeroEnum> FailUnusedZeroEnum() {
  return Err(UnusedZeroEnum::NotOk);
}

static Result<Ok, Failed&> Task1(bool pass) {
  if (!pass) {
    return Fail();  // implicit conversion from GenericErrorResult to Result
  }
  return Ok();
}

static Result<Ok, UnusedZeroEnum> Task1UnusedZeroEnumErr(bool pass) {
  if (!pass) {
    return FailUnusedZeroEnum();  // implicit conversion from GenericErrorResult
                                  // to Result
  }
  return Ok();
}

static Result<int, Failed&> Task2(bool pass, int value) {
  MOZ_TRY(
      Task1(pass));  // converts one type of result to another in the error case
  return value;      // implicit conversion from T to Result<T, E>
}

static Result<int, UnusedZeroEnum> Task2UnusedZeroEnumErr(bool pass,
                                                          int value) {
  MOZ_TRY(Task1UnusedZeroEnumErr(
      pass));    // converts one type of result to another in the error case
  return value;  // implicit conversion from T to Result<T, E>
}

static Result<int, Failed&> Task3(bool pass1, bool pass2, int value) {
  int x, y;
  MOZ_TRY_VAR(x, Task2(pass1, value));
  MOZ_TRY_VAR(y, Task2(pass2, value));
  return x + y;
}

static void BasicTests() {
  MOZ_RELEASE_ASSERT(Task1(true).isOk());
  MOZ_RELEASE_ASSERT(!Task1(true).isErr());
  MOZ_RELEASE_ASSERT(!Task1(false).isOk());
  MOZ_RELEASE_ASSERT(Task1(false).isErr());

  MOZ_RELEASE_ASSERT(Task1UnusedZeroEnumErr(true).isOk());
  MOZ_RELEASE_ASSERT(!Task1UnusedZeroEnumErr(true).isErr());
  MOZ_RELEASE_ASSERT(!Task1UnusedZeroEnumErr(false).isOk());
  MOZ_RELEASE_ASSERT(Task1UnusedZeroEnumErr(false).isErr());
  MOZ_RELEASE_ASSERT(UnusedZeroEnum::NotOk ==
                     Task1UnusedZeroEnumErr(false).inspectErr());
  MOZ_RELEASE_ASSERT(UnusedZeroEnum::NotOk ==
                     Task1UnusedZeroEnumErr(false).unwrapErr());

  // MOZ_TRY works.
  MOZ_RELEASE_ASSERT(Task2(true, 3).isOk());
  MOZ_RELEASE_ASSERT(Task2(true, 3).unwrap() == 3);
  MOZ_RELEASE_ASSERT(Task2(true, 3).unwrapOr(6) == 3);
  MOZ_RELEASE_ASSERT(Task2(false, 3).isErr());
  MOZ_RELEASE_ASSERT(Task2(false, 3).unwrapOr(6) == 6);

  MOZ_RELEASE_ASSERT(Task2UnusedZeroEnumErr(true, 3).isOk());
  MOZ_RELEASE_ASSERT(Task2UnusedZeroEnumErr(true, 3).unwrap() == 3);
  MOZ_RELEASE_ASSERT(Task2UnusedZeroEnumErr(true, 3).unwrapOr(6) == 3);
  MOZ_RELEASE_ASSERT(Task2UnusedZeroEnumErr(false, 3).isErr());
  MOZ_RELEASE_ASSERT(Task2UnusedZeroEnumErr(false, 3).unwrapOr(6) == 6);

  // MOZ_TRY_VAR works.
  MOZ_RELEASE_ASSERT(Task3(true, true, 3).isOk());
  MOZ_RELEASE_ASSERT(Task3(true, true, 3).unwrap() == 6);
  MOZ_RELEASE_ASSERT(Task3(true, false, 3).isErr());
  MOZ_RELEASE_ASSERT(Task3(false, true, 3).isErr());
  MOZ_RELEASE_ASSERT(Task3(false, true, 3).unwrapOr(6) == 6);

  // Lvalues should work too.
  {
    Result<Ok, Failed&> res = Task1(true);
    MOZ_RELEASE_ASSERT(res.isOk());
    MOZ_RELEASE_ASSERT(!res.isErr());

    res = Task1(false);
    MOZ_RELEASE_ASSERT(!res.isOk());
    MOZ_RELEASE_ASSERT(res.isErr());
  }

  {
    Result<int, Failed&> res = Task2(true, 3);
    MOZ_RELEASE_ASSERT(res.isOk());
    MOZ_RELEASE_ASSERT(res.unwrap() == 3);

    res = Task2(false, 4);
    MOZ_RELEASE_ASSERT(res.isErr());
  }

  // Some tests for pointer tagging.
  {
    int i = 123;
    double d = 3.14;

    Result<int*, double&> res = &i;
    static_assert(sizeof(res) == sizeof(uintptr_t),
                  "should use pointer tagging to fit in a word");

    MOZ_RELEASE_ASSERT(res.isOk());
    MOZ_RELEASE_ASSERT(*res.unwrap() == 123);

    res = Err(d);
    MOZ_RELEASE_ASSERT(res.isErr());
    MOZ_RELEASE_ASSERT(&res.unwrapErr() == &d);
    MOZ_RELEASE_ASSERT(res.unwrapErr() == 3.14);
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
}

static void EmptyValueTest() {
  struct Fine {};
  mozilla::Result<Fine, int&> res((Fine()));
  res.unwrap();
  MOZ_RELEASE_ASSERT(res.isOk());
  static_assert(sizeof(res) == sizeof(uintptr_t),
                "Result with empty value type should be pointer-sized");
}

static void ReferenceTest() {
  struct MyError {
    int x = 0;
  };
  MyError merror;
  Result<int, MyError&> res(merror);
  MOZ_RELEASE_ASSERT(&res.unwrapErr() == &merror);
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

    explicit MyError(int y) : x(y) {}
  };

  struct MyError2 {
    int a;

    explicit MyError2(int b) : a(b) {}
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
  Result<int, const char*> r1(10);
  Result<int, const char*> r2 =
      r1.andThen([](int x) { return Result<int, const char*>(x + 1); });
  MOZ_RELEASE_ASSERT(r2.isOk());
  MOZ_RELEASE_ASSERT(r2.unwrap() == 11);

  // `andThen`ing over error results.
  Result<int, const char*> r3("error");
  Result<int, const char*> r4 = r3.andThen([](int x) {
    MOZ_RELEASE_ASSERT(false);
    return Result<int, const char*>(1);
  });
  MOZ_RELEASE_ASSERT(r4.isErr());
  MOZ_RELEASE_ASSERT(r3.unwrapErr() == r4.unwrapErr());
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

/* * */

int main() {
  BasicTests();
  TypeConversionTests();
  EmptyValueTest();
  ReferenceTest();
  MapTest();
  MapErrTest();
  OrElseTest();
  AndThenTest();
  UniquePtrTest();
  return 0;
}
