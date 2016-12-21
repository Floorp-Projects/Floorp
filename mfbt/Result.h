/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A type suitable for returning either a value or an error from a function. */

#ifndef mozilla_Result_h
#define mozilla_Result_h

#include "mozilla/Alignment.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Types.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/Variant.h"

namespace mozilla {

/**
 * Empty struct, indicating success for operations that have no return value.
 * For example, if you declare another empty struct `struct OutOfMemory {};`,
 * then `Result<Ok, OutOfMemory>` represents either success or OOM.
 */
struct Ok {};

template <typename E> class GenericErrorResult;

namespace detail {

enum class VEmptiness { IsEmpty, IsNotEmpty };
enum class Alignedness { IsAligned, IsNotAligned };

template <typename V, typename E, VEmptiness EmptinessOfV, Alignedness Aligned>
class ResultImplementation
{
  mozilla::Variant<V, E> mStorage;

public:
  explicit ResultImplementation(V aValue) : mStorage(aValue) {}
  explicit ResultImplementation(E aErrorValue) : mStorage(aErrorValue) {}

  bool isOk() const { return mStorage.template is<V>(); }

  // The callers of these functions will assert isOk() has the proper value, so
  // these functions (in all ResultImplementation specializations) don't need
  // to do so.
  V unwrap() const { return mStorage.template as<V>(); }
  E unwrapErr() const { return mStorage.template as<E>(); }
};

/**
 * mozilla::Variant doesn't like storing a reference. This is a specialization
 * to store E as pointer if it's a reference.
 */
template <typename V, typename E, VEmptiness EmptinessOfV, Alignedness Aligned>
class ResultImplementation<V, E&, EmptinessOfV, Aligned>
{
  mozilla::Variant<V, E*> mStorage;

public:
  explicit ResultImplementation(V aValue) : mStorage(aValue) {}
  explicit ResultImplementation(E& aErrorValue) : mStorage(&aErrorValue) {}

  bool isOk() const { return mStorage.template is<V>(); }
  V unwrap() const { return mStorage.template as<V>(); }
  E& unwrapErr() const { return *mStorage.template as<E*>(); }
};

/**
 * Specialization for when the success type is Ok (or another empty class) and
 * the error type is a reference.
 */
template <typename V, typename E, Alignedness Aligned>
class ResultImplementation<V, E&, VEmptiness::IsEmpty, Aligned>
{
  E* mErrorValue;

public:
  explicit ResultImplementation(V) : mErrorValue(nullptr) {}
  explicit ResultImplementation(E& aErrorValue) : mErrorValue(&aErrorValue) {}

  bool isOk() const { return mErrorValue == nullptr; }

  V unwrap() const { return V(); }
  E& unwrapErr() const { return *mErrorValue; }
};

/**
 * Specialization for when alignment permits using the least significant bit as
 * a tag bit.
 */
template <typename V, typename E, VEmptiness EmptinessOfV>
class ResultImplementation<V*, E&, EmptinessOfV, Alignedness::IsAligned>
{
  uintptr_t mBits;

public:
  explicit ResultImplementation(V* aValue)
    : mBits(reinterpret_cast<uintptr_t>(aValue))
  {
    MOZ_ASSERT((uintptr_t(aValue) % MOZ_ALIGNOF(V)) == 0,
               "Result value pointers must not be misaligned");
  }
  explicit ResultImplementation(E& aErrorValue)
    : mBits(reinterpret_cast<uintptr_t>(&aErrorValue) | 1)
  {
    MOZ_ASSERT((uintptr_t(&aErrorValue) % MOZ_ALIGNOF(E)) == 0,
               "Result errors must not be misaligned");
  }

  bool isOk() const { return (mBits & 1) == 0; }

  V* unwrap() const { return reinterpret_cast<V*>(mBits); }
  E& unwrapErr() const { return *reinterpret_cast<E*>(mBits & ~uintptr_t(1)); }
};

// A bit of help figuring out which of the above specializations to use.
//
// We begin by safely assuming types don't have a spare bit.
template <typename T> struct HasFreeLSB { static const bool value = false; };

// The lowest bit of a properly-aligned pointer is always zero if the pointee
// type is greater than byte-aligned. That bit is free to use if it's masked
// out of such pointers before they're dereferenced.
template <typename T> struct HasFreeLSB<T*> {
  static const bool value = (MOZ_ALIGNOF(T) & 1) == 0;
};

// We store references as pointers, so they have a free bit if a pointer would
// have one.
template <typename T> struct HasFreeLSB<T&> {
  static const bool value = HasFreeLSB<T*>::value;
};

} // namespace detail

/**
 * Result<V, E> represents the outcome of an operation that can either succeed
 * or fail. It contains either a success value of type V or an error value of
 * type E.
 *
 * All Result methods are const, so results are basically immutable.
 * This is just like Variant<V, E> but with a slightly different API, and the
 * following cases are optimized so Result can be stored more efficiently:
 *
 * - If the success type is Ok (or another empty class) and the error type is a
 *   reference, Result<V, E&> is guaranteed to be pointer-sized and all zero
 *   bits on success. Do not change this representation! There is JIT code that
 *   depends on it.
 *
 * - If the success type is a pointer type and the error type is a reference
 *   type, and the least significant bit is unused for both types when stored
 *   as a pointer (due to alignment rules), Result<V*, E&> is guaranteed to be
 *   pointer-sized. In this case, we use the lowest bit as tag bit: 0 to
 *   indicate the Result's bits are a V, 1 to indicate the Result's bits (with
 *   the 1 masked out) encode an E*.
 *
 * The purpose of Result is to reduce the screwups caused by using `false` or
 * `nullptr` to indicate errors.
 * What screwups? See <https://bugzilla.mozilla.org/show_bug.cgi?id=912928> for
 * a partial list.
 */
template <typename V, typename E>
class MOZ_MUST_USE_TYPE Result final
{
  using Impl =
    detail::ResultImplementation<V, E,
                                 IsEmpty<V>::value
                                   ? detail::VEmptiness::IsEmpty
                                   : detail::VEmptiness::IsNotEmpty,
                                 (detail::HasFreeLSB<V>::value &&
                                  detail::HasFreeLSB<E>::value)
                                   ? detail::Alignedness::IsAligned
                                   : detail::Alignedness::IsNotAligned>;
  Impl mImpl;

public:
  /**
   * Create a success result.
   */
  MOZ_IMPLICIT Result(V aValue) : mImpl(aValue) { MOZ_ASSERT(isOk()); }

  /**
   * Create an error result.
   */
  explicit Result(E aErrorValue) : mImpl(aErrorValue) { MOZ_ASSERT(isErr()); }

  /**
   * Implementation detail of MOZ_TRY().
   * Create an error result from another error result.
   */
  template <typename E2>
  MOZ_IMPLICIT Result(const GenericErrorResult<E2>& aErrorResult)
    : mImpl(aErrorResult.mErrorValue)
  {
    static_assert(mozilla::IsConvertible<E2, E>::value,
                  "E2 must be convertible to E");
    MOZ_ASSERT(isErr());
  }

  Result(const Result&) = default;
  Result& operator=(const Result&) = default;

  /** True if this Result is a success result. */
  bool isOk() const { return mImpl.isOk(); }

  /** True if this Result is an error result. */
  bool isErr() const { return !mImpl.isOk(); }

  /** Get the success value from this Result, which must be a success result. */
  V unwrap() const {
    MOZ_ASSERT(isOk());
    return mImpl.unwrap();
  }

  /** Get the error value from this Result, which must be an error result. */
  E unwrapErr() const {
    MOZ_ASSERT(isErr());
    return mImpl.unwrapErr();
  }

  /**
   * Map a function V -> W over this result's success variant. If this result is
   * an error, do not invoke the function and return a copy of the error.
   *
   * Mapping over success values invokes the function to produce a new success
   * value:
   *
   *     // Map Result<int, E> to another Result<int, E>
   *     Result<int, E> res(5);
   *     Result<int, E> res2 = res.map([](int x) { return x * x; });
   *     MOZ_ASSERT(res2.unwrap() == 25);
   *
   *     // Map Result<const char*, E> to Result<size_t, E>
   *     Result<const char*, E> res("hello, map!");
   *     Result<size_t, E> res2 = res.map(strlen);
   *     MOZ_ASSERT(res2.unwrap() == 11);
   *
   * Mapping over an error does not invoke the function and copies the error:
   *
   *     Result<V, int> res(5);
   *     MOZ_ASSERT(res.isErr());
   *     Result<W, int> res2 = res.map([](V v) { ... });
   *     MOZ_ASSERT(res2.isErr());
   *     MOZ_ASSERT(res2.unwrapErr() == 5);
   */
  template<typename F>
  auto map(F f) const -> Result<decltype(f(*((V*) nullptr))), E> {
      using RetResult = Result<decltype(f(*((V*) nullptr))), E>;
      return isOk() ? RetResult(f(unwrap())) : RetResult(unwrapErr());
  }
};

/**
 * A type that auto-converts to an error Result. This is like a Result without
 * a success type. It's the best return type for functions that always return
 * an error--functions designed to build and populate error objects. It's also
 * useful in error-handling macros; see MOZ_TRY for an example.
 */
template <typename E>
class MOZ_MUST_USE_TYPE GenericErrorResult
{
  E mErrorValue;

  template<typename V, typename E2> friend class Result;

public:
  explicit GenericErrorResult(E aErrorValue) : mErrorValue(aErrorValue) {}
};

template <typename E>
inline GenericErrorResult<E>
MakeGenericErrorResult(E&& aErrorValue)
{
  return GenericErrorResult<E>(aErrorValue);
}

} // namespace mozilla

/**
 * MOZ_TRY(expr) is the C++ equivalent of Rust's `try!(expr);`. First, it
 * evaluates expr, which must produce a Result value. On success, it
 * discards the result altogether. On error, it immediately returns an error
 * Result from the enclosing function.
 */
#define MOZ_TRY(expr) \
  do { \
    auto mozTryTempResult_ = (expr); \
    if (mozTryTempResult_.isErr()) { \
      return ::mozilla::MakeGenericErrorResult(mozTryTempResult_.unwrapErr()); \
    } \
  } while (0)

/**
 * MOZ_TRY_VAR(target, expr) is the C++ equivalent of Rust's `target = try!(expr);`.
 * First, it evaluates expr, which must produce a Result value.
 * On success, the result's success value is assigned to target.
 * On error, immediately returns the error result.
 * |target| must evaluate to a reference without any side effects.
 */
#define MOZ_TRY_VAR(target, expr) \
  do { \
    auto mozTryVarTempResult_ = (expr); \
    if (mozTryVarTempResult_.isErr()) { \
      return ::mozilla::MakeGenericErrorResult( \
          mozTryVarTempResult_.unwrapErr()); \
    } \
    (target) = mozTryVarTempResult_.unwrap(); \
  } while (0)

#endif // mozilla_Result_h
