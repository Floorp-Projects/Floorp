/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A type suitable for returning either a value or an error from a function. */

#ifndef mozilla_Result_h
#define mozilla_Result_h

#include <type_traits>
#include "mozilla/Alignment.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/CompactPair.h"
#include "mozilla/Types.h"
#include "mozilla/Variant.h"

namespace mozilla {

/**
 * Empty struct, indicating success for operations that have no return value.
 * For example, if you declare another empty struct `struct OutOfMemory {};`,
 * then `Result<Ok, OutOfMemory>` represents either success or OOM.
 */
struct Ok {};

template <typename E>
class GenericErrorResult;
template <typename V, typename E>
class Result;

namespace detail {

enum class PackingStrategy {
  Variant,
  NullIsOk,
  LowBitTagIsError,
  PackedVariant,
};

template <typename T>
struct UnusedZero;

template <typename V, typename E, PackingStrategy Strategy>
class ResultImplementation;

template <typename V, typename E>
class ResultImplementation<V, E, PackingStrategy::Variant> {
  mozilla::Variant<V, E> mStorage;

 public:
  ResultImplementation(ResultImplementation&&) = default;
  ResultImplementation(const ResultImplementation&) = delete;
  ResultImplementation& operator=(const ResultImplementation&) = delete;
  ResultImplementation& operator=(ResultImplementation&&) = default;

  explicit ResultImplementation(V&& aValue)
      : mStorage(std::forward<V>(aValue)) {}
  explicit ResultImplementation(const V& aValue) : mStorage(aValue) {}
  explicit ResultImplementation(const E& aErrorValue) : mStorage(aErrorValue) {}
  explicit ResultImplementation(E&& aErrorValue)
      : mStorage(std::forward<E>(aErrorValue)) {}

  bool isOk() const { return mStorage.template is<V>(); }

  // The callers of these functions will assert isOk() has the proper value, so
  // these functions (in all ResultImplementation specializations) don't need
  // to do so.
  V unwrap() { return std::move(mStorage.template as<V>()); }
  const V& inspect() const { return mStorage.template as<V>(); }

  E unwrapErr() { return std::move(mStorage.template as<E>()); }
  const E& inspectErr() const { return mStorage.template as<E>(); }
};

/**
 * mozilla::Variant doesn't like storing a reference. This is a specialization
 * to store E as pointer if it's a reference.
 */
template <typename V, typename E>
class ResultImplementation<V, E&, PackingStrategy::Variant> {
  mozilla::Variant<V, E*> mStorage;

 public:
  explicit ResultImplementation(V&& aValue)
      : mStorage(std::forward<V>(aValue)) {}
  explicit ResultImplementation(const V& aValue) : mStorage(aValue) {}
  explicit ResultImplementation(E& aErrorValue) : mStorage(&aErrorValue) {}

  bool isOk() const { return mStorage.template is<V>(); }

  const V& inspect() const { return mStorage.template as<V>(); }
  V unwrap() { return std::move(mStorage.template as<V>()); }

  E& unwrapErr() { return *mStorage.template as<E*>(); }
  const E& inspectErr() const { return *mStorage.template as<E*>(); }
};

// The purpose of EmptyWrapper is to make an empty class look like
// AlignedStorage2 for the purposes of the PackingStrategy::NullIsOk
// specializations of ResultImplementation below. We can't use AlignedStorage2
// itself with an empty class, since it would no longer be empty, and we want to
// avoid changing AlignedStorage2 just for this purpose.
template <typename V>
struct EmptyWrapper : V {
  const V* addr() const { return this; }
  V* addr() { return this; }
};

template <typename V>
using AlignedStorageOrEmpty =
    std::conditional_t<std::is_empty_v<V>, EmptyWrapper<V>, AlignedStorage2<V>>;

template <typename V, typename E>
class ResultImplementationNullIsOkBase {
 protected:
  using ErrorStorageType = typename UnusedZero<E>::StorageType;

  static constexpr auto kNullValue = UnusedZero<E>::nullValue;

  // This is static function rather than a static data member in order to avoid
  // that gcc emits lots of static constructors. It may be changed to a static
  // constexpr data member with C++20.
  static inline auto GetMovedFromMarker() {
    return UnusedZero<E>::GetDefaultValue();
  }

  static_assert(std::is_trivially_copyable_v<ErrorStorageType>);
  static_assert(kNullValue == decltype(kNullValue)(0));

  CompactPair<AlignedStorageOrEmpty<V>, ErrorStorageType> mValue;

 public:
  explicit ResultImplementationNullIsOkBase(const V& aSuccessValue)
      : mValue(std::piecewise_construct, std::tuple<>(),
               std::tuple(kNullValue)) {
    if constexpr (!std::is_empty_v<V>) {
      new (mValue.first().addr()) V(aSuccessValue);
    }
  }
  explicit ResultImplementationNullIsOkBase(V&& aSuccessValue)
      : mValue(std::piecewise_construct, std::tuple<>(),
               std::tuple(kNullValue)) {
    if constexpr (!std::is_empty_v<V>) {
      new (mValue.first().addr()) V(std::move(aSuccessValue));
    }
  }
  explicit ResultImplementationNullIsOkBase(E aErrorValue)
      : mValue(std::piecewise_construct, std::tuple<>(),
               std::tuple(UnusedZero<E>::Store(aErrorValue))) {
    MOZ_ASSERT(mValue.second() != kNullValue);
  }

  ResultImplementationNullIsOkBase(ResultImplementationNullIsOkBase&& aOther)
      : mValue(std::piecewise_construct, std::tuple<>(),
               std::tuple(std::move(aOther.mValue.second()))) {
    if constexpr (!std::is_empty_v<V>) {
      if (isOk()) {
        new (mValue.first().addr()) V(std::move(*aOther.mValue.first().addr()));
        aOther.mValue.first().addr()->~V();
        aOther.mValue.second() = GetMovedFromMarker();
      }
    }
  }
  ResultImplementationNullIsOkBase& operator=(
      ResultImplementationNullIsOkBase&& aOther) {
    if constexpr (!std::is_empty_v<V>) {
      if (isOk()) {
        mValue.first().addr()->~V();
      }
    }
    mValue.second() = std::move(aOther.mValue.second());
    if constexpr (!std::is_empty_v<V>) {
      if (isOk()) {
        new (mValue.first().addr()) V(std::move(*aOther.mValue.first().addr()));
        aOther.mValue.first().addr()->~V();
        aOther.mValue.second() = GetMovedFromMarker();
      }
    }
    return *this;
  }

  bool isOk() const { return mValue.second() == kNullValue; }

  const V& inspect() const { return *mValue.first().addr(); }
  V unwrap() { return std::move(*mValue.first().addr()); }

  const E& inspectErr() const {
    return UnusedZero<E>::Inspect(mValue.second());
  }
  E unwrapErr() { return UnusedZero<E>::Unwrap(std::move(mValue.second())); }
};

template <typename V, typename E,
          bool IsVTriviallyDestructible = std::is_trivially_destructible_v<V>>
class ResultImplementationNullIsOk;

template <typename V, typename E>
class ResultImplementationNullIsOk<V, E, true>
    : public ResultImplementationNullIsOkBase<V, E> {
 public:
  using ResultImplementationNullIsOkBase<V,
                                         E>::ResultImplementationNullIsOkBase;
};

template <typename V, typename E>
class ResultImplementationNullIsOk<V, E, false>
    : public ResultImplementationNullIsOkBase<V, E> {
 public:
  using ResultImplementationNullIsOkBase<V,
                                         E>::ResultImplementationNullIsOkBase;

  ResultImplementationNullIsOk(ResultImplementationNullIsOk&&) = default;
  ResultImplementationNullIsOk& operator=(ResultImplementationNullIsOk&&) =
      default;

  ~ResultImplementationNullIsOk() {
    if (this->isOk()) {
      this->mValue.first().addr()->~V();
    }
  }
};

/**
 * Specialization for when the success type is default-constructible and the
 * error type is a value type which can never have the value 0 (as determined by
 * UnusedZero<>).
 */
template <typename V, typename E>
class ResultImplementation<V, E, PackingStrategy::NullIsOk>
    : public ResultImplementationNullIsOk<V, E> {
 public:
  using ResultImplementationNullIsOk<V, E>::ResultImplementationNullIsOk;
};

/**
 * Specialization for when alignment permits using the least significant bit
 * as a tag bit.
 */
template <typename V, typename E>
class ResultImplementation<V*, E&, PackingStrategy::LowBitTagIsError> {
  uintptr_t mBits;

 public:
  explicit ResultImplementation(V* aValue)
      : mBits(reinterpret_cast<uintptr_t>(aValue)) {
    MOZ_ASSERT((uintptr_t(aValue) % MOZ_ALIGNOF(V)) == 0,
               "Result value pointers must not be misaligned");
  }
  explicit ResultImplementation(E& aErrorValue)
      : mBits(reinterpret_cast<uintptr_t>(&aErrorValue) | 1) {
    MOZ_ASSERT((uintptr_t(&aErrorValue) % MOZ_ALIGNOF(E)) == 0,
               "Result errors must not be misaligned");
  }

  bool isOk() const { return (mBits & 1) == 0; }

  V* inspect() const { return reinterpret_cast<V*>(mBits); }
  V* unwrap() { return inspect(); }

  E& inspectErr() const { return *reinterpret_cast<E*>(mBits ^ 1); }
  E& unwrapErr() { return inspectErr(); }
};

// Return true if any of the struct can fit in a word.
template <typename V, typename E>
struct IsPackableVariant {
  struct VEbool {
    V v;
    E e;
    bool ok;
  };
  struct EVbool {
    E e;
    V v;
    bool ok;
  };

  using Impl =
      std::conditional_t<sizeof(VEbool) <= sizeof(EVbool), VEbool, EVbool>;

  static const bool value = sizeof(Impl) <= sizeof(uintptr_t);
};

/**
 * Specialization for when both type are not using all the bytes, in order to
 * use one byte as a tag.
 */
template <typename V, typename E>
class ResultImplementation<V, E, PackingStrategy::PackedVariant> {
  using Impl = typename IsPackableVariant<V, E>::Impl;
  Impl data;

 public:
  explicit ResultImplementation(V aValue) {
    data.v = std::move(aValue);
    data.ok = true;
  }
  explicit ResultImplementation(E aErrorValue) {
    data.e = std::move(aErrorValue);
    data.ok = false;
  }

  bool isOk() const { return data.ok; }

  const V& inspect() const { return data.v; }
  V unwrap() { return std::move(data.v); }

  const E& inspectErr() const { return data.e; }
  E unwrapErr() { return std::move(data.e); }
};

// To use nullptr as a special value, we need the counter part to exclude zero
// from its range of valid representations.
//
// By default assume that zero can be represented.
template <typename T>
struct UnusedZero {
  static const bool value = false;
};

// References can't be null.
template <typename T>
struct UnusedZero<T&> {
  using StorageType = T*;

  static constexpr bool value = true;

  // This is static function rather than a static data member in order to avoid
  // that gcc emits lots of static constructors. It may be changed to a static
  // constexpr data member using bit_cast with C++20.
  static inline StorageType GetDefaultValue() {
    return reinterpret_cast<StorageType>(~ptrdiff_t(0));
  }

  static constexpr StorageType nullValue = nullptr;

  static constexpr const T& Inspect(StorageType aValue) {
    AssertValid(aValue);
    return *aValue;
  }
  static constexpr T& Unwrap(StorageType aValue) {
    AssertValid(aValue);
    return *aValue;
  }
  static constexpr StorageType Store(T& aValue) { return &aValue; }

 private:
  static constexpr void AssertValid(StorageType aValue) {
    MOZ_ASSERT(aValue != GetDefaultValue());
  }
};

// A bit of help figuring out which of the above specializations to use.
//
// We begin by safely assuming types don't have a spare bit.
template <typename T>
struct HasFreeLSB {
  static const bool value = false;
};

// As an incomplete type, void* does not have a spare bit.
template <>
struct HasFreeLSB<void*> {
  static const bool value = false;
};

// The lowest bit of a properly-aligned pointer is always zero if the pointee
// type is greater than byte-aligned. That bit is free to use if it's masked
// out of such pointers before they're dereferenced.
template <typename T>
struct HasFreeLSB<T*> {
  static const bool value = (alignof(T) & 1) == 0;
};

// We store references as pointers, so they have a free bit if a pointer would
// have one.
template <typename T>
struct HasFreeLSB<T&> {
  static const bool value = HasFreeLSB<T*>::value;
};

// Select one of the previous result implementation based on the properties of
// the V and E types.
template <typename V, typename E>
struct SelectResultImpl {
  static const PackingStrategy value =
      (HasFreeLSB<V>::value && HasFreeLSB<E>::value)
          ? PackingStrategy::LowBitTagIsError
          : (UnusedZero<E>::value && sizeof(E) <= sizeof(uintptr_t))
                ? PackingStrategy::NullIsOk
                : (std::is_default_constructible_v<V> &&
                   std::is_default_constructible_v<E> &&
                   IsPackableVariant<V, E>::value)
                      ? PackingStrategy::PackedVariant
                      : PackingStrategy::Variant;

  using Type = ResultImplementation<V, E, value>;
};

template <typename T>
struct IsResult : std::false_type {};

template <typename V, typename E>
struct IsResult<Result<V, E>> : std::true_type {};

}  // namespace detail

template <typename V, typename E>
auto ToResult(Result<V, E>&& aValue)
    -> decltype(std::forward<Result<V, E>>(aValue)) {
  return std::forward<Result<V, E>>(aValue);
}

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
 *
 * Result<const V, E> or Result<V, const E> are not meaningful. The success or
 * error values in a Result instance are non-modifiable in-place anyway. This
 * guarantee must also be maintained when evolving Result. They can be
 * unwrap()ped, but this loses const qualification. However, Result<const V, E>
 * or Result<V, const E> may be misleading and prevent movability. Just use
 * Result<V, E>. (Result<const V*, E> may make sense though, just Result<const
 * V* const, E> is not possible.)
 */
template <typename V, typename E>
class MOZ_MUST_USE_TYPE Result final {
  // See class comment on Result<const V, E> and Result<V, const E>.
  static_assert(!std::is_const_v<V>);
  static_assert(!std::is_const_v<E>);

  using Impl = typename detail::SelectResultImpl<V, E>::Type;

  Impl mImpl;

 public:
  using ok_type = V;
  using err_type = E;

  /** Create a success result. */
  MOZ_IMPLICIT Result(V&& aValue) : mImpl(std::forward<V>(aValue)) {
    MOZ_ASSERT(isOk());
  }

  /** Create a success result. */
  MOZ_IMPLICIT Result(const V& aValue) : mImpl(aValue) { MOZ_ASSERT(isOk()); }

  /** Create an error result. */
  explicit Result(E aErrorValue) : mImpl(std::forward<E>(aErrorValue)) {
    MOZ_ASSERT(isErr());
  }

  /**
   * Implementation detail of MOZ_TRY().
   * Create an error result from another error result.
   */
  template <typename E2>
  MOZ_IMPLICIT Result(GenericErrorResult<E2>&& aErrorResult)
      : mImpl(std::forward<E2>(aErrorResult.mErrorValue)) {
    static_assert(std::is_convertible_v<E2, E>, "E2 must be convertible to E");
    MOZ_ASSERT(isErr());
  }

  /**
   * Implementation detail of MOZ_TRY().
   * Create an error result from another error result.
   */
  template <typename E2>
  MOZ_IMPLICIT Result(const GenericErrorResult<E2>& aErrorResult)
      : mImpl(aErrorResult.mErrorValue) {
    static_assert(std::is_convertible_v<E2, E>, "E2 must be convertible to E");
    MOZ_ASSERT(isErr());
  }

  Result(const Result&) = delete;
  Result(Result&&) = default;
  Result& operator=(const Result&) = delete;
  Result& operator=(Result&&) = default;

  /** True if this Result is a success result. */
  bool isOk() const { return mImpl.isOk(); }

  /** True if this Result is an error result. */
  bool isErr() const { return !mImpl.isOk(); }

  /** Take the success value from this Result, which must be a success result.
   */
  V unwrap() {
    MOZ_ASSERT(isOk());
    return mImpl.unwrap();
  }

  /**
   * Take the success value from this Result, which must be a success result.
   * If it is an error result, then return the aValue.
   */
  V unwrapOr(V aValue) {
    return MOZ_LIKELY(isOk()) ? mImpl.unwrap() : std::move(aValue);
  }

  /** Take the error value from this Result, which must be an error result. */
  E unwrapErr() {
    MOZ_ASSERT(isErr());
    return mImpl.unwrapErr();
  }

  /** See the success value from this Result, which must be a success result. */
  const V& inspect() const { return mImpl.inspect(); }

  /** See the error value from this Result, which must be an error result. */
  const E& inspectErr() const {
    MOZ_ASSERT(isErr());
    return mImpl.inspectErr();
  }

  /** Propagate the error value from this Result, which must be an error result.
   *
   * This can be used to propagate an error from a function call to the caller
   * with a different value type, but the same error type:
   *
   *    Result<T1, E> Func1() {
   *       Result<T2, E> res = Func2();
   *       if (res.isErr()) { return res.propagateErr(); }
   *    }
   */
  GenericErrorResult<E> propagateErr() {
    MOZ_ASSERT(isErr());
    return GenericErrorResult<E>{mImpl.unwrapErr()};
  }

  /**
   * Map a function V -> V2 over this result's success variant. If this result
   * is an error, do not invoke the function and propagate the error.
   *
   * Mapping over success values invokes the function to produce a new success
   * value:
   *
   *     // Map Result<int, E> to another Result<int, E>
   *     Result<int, E> res(5);
   *     Result<int, E> res2 = res.map([](int x) { return x * x; });
   *     MOZ_ASSERT(res.isOk());
   *     MOZ_ASSERT(res2.unwrap() == 25);
   *
   *     // Map Result<const char*, E> to Result<size_t, E>
   *     Result<const char*, E> res("hello, map!");
   *     Result<size_t, E> res2 = res.map(strlen);
   *     MOZ_ASSERT(res.isOk());
   *     MOZ_ASSERT(res2.unwrap() == 11);
   *
   * Mapping over an error does not invoke the function and propagates the
   * error:
   *
   *     Result<V, int> res(5);
   *     MOZ_ASSERT(res.isErr());
   *     Result<V2, int> res2 = res.map([](V v) { ... });
   *     MOZ_ASSERT(res2.isErr());
   *     MOZ_ASSERT(res2.unwrapErr() == 5);
   */
  template <typename F>
  auto map(F f) -> Result<std::result_of_t<F(V)>, E> {
    using RetResult = Result<std::result_of_t<F(V)>, E>;
    return MOZ_LIKELY(isOk()) ? RetResult(f(unwrap())) : RetResult(unwrapErr());
  }

  /**
   * Map a function E -> E2 over this result's error variant. If this result is
   * a success, do not invoke the function and move the success over.
   *
   * Mapping over error values invokes the function to produce a new error
   * value:
   *
   *     // Map Result<V, int> to another Result<V, int>
   *     Result<V, int> res(5);
   *     Result<V, int> res2 = res.mapErr([](int x) { return x * x; });
   *     MOZ_ASSERT(res2.isErr());
   *     MOZ_ASSERT(res2.unwrapErr() == 25);
   *
   *     // Map Result<V, const char*> to Result<V, size_t>
   *     Result<V, const char*> res("hello, mapErr!");
   *     Result<V, size_t> res2 = res.mapErr(strlen);
   *     MOZ_ASSERT(res2.isErr());
   *     MOZ_ASSERT(res2.unwrapErr() == 14);
   *
   * Mapping over a success does not invoke the function and moves the success:
   *
   *     Result<int, E> res(5);
   *     MOZ_ASSERT(res.isOk());
   *     Result<int, E2> res2 = res.mapErr([](E e) { ... });
   *     MOZ_ASSERT(res2.isOk());
   *     MOZ_ASSERT(res2.unwrap() == 5);
   */
  template <typename F>
  auto mapErr(F f) -> Result<V, std::result_of_t<F(E)>> {
    using RetResult = Result<V, std::result_of_t<F(E)>>;
    return MOZ_UNLIKELY(isErr()) ? RetResult(f(unwrapErr()))
                                 : RetResult(unwrap());
  }

  /**
   * Map a function E -> Result<V, E2> over this result's error variant. If
   * this result is a success, do not invoke the function and move the success
   * over.
   *
   * `orElse`ing over error values invokes the function to produce a new
   * result:
   *
   *     // `orElse` Result<V, int> error variant to another Result<V, int>
   *     // error variant or Result<V, int> success variant
   *     auto orElse = [](int x) -> Result<V, int> {
   *       if (x != 6) {
   *         return Err(x * x);
   *       }
   *       return V(...);
   *     };
   *
   *     Result<V, int> res(5);
   *     auto res2 = res.orElse(orElse);
   *     MOZ_ASSERT(res2.isErr());
   *     MOZ_ASSERT(res2.unwrapErr() == 25);
   *
   *     Result<V, int> res3(6);
   *     auto res4 = res3.orElse(orElse);
   *     MOZ_ASSERT(res4.isOk());
   *     MOZ_ASSERT(res4.unwrap() == ...);
   *
   *     // `orElse` Result<V, const char*> error variant to Result<V, size_t>
   *     // error variant or Result<V, size_t> success variant
   *     auto orElse = [](const char* s) -> Result<V, size_t> {
   *       if (strcmp(s, "foo")) {
   *         return Err(strlen(s));
   *       }
   *       return V(...);
   *     };
   *
   *     Result<V, const char*> res("hello, orElse!");
   *     auto res2 = res.orElse(orElse);
   *     MOZ_ASSERT(res2.isErr());
   *     MOZ_ASSERT(res2.unwrapErr() == 14);
   *
   *     Result<V, const char*> res3("foo");
   *     auto res4 = ress.orElse(orElse);
   *     MOZ_ASSERT(res4.isOk());
   *     MOZ_ASSERT(res4.unwrap() == ...);
   *
   * `orElse`ing over a success does not invoke the function and moves the
   * success:
   *
   *     Result<int, E> res(5);
   *     MOZ_ASSERT(res.isOk());
   *     Result<int, E2> res2 = res.orElse([](E e) { ... });
   *     MOZ_ASSERT(res2.isOk());
   *     MOZ_ASSERT(res2.unwrap() == 5);
   */
  template <typename F>
  auto orElse(F f) -> Result<V, typename std::result_of_t<F(E)>::err_type> {
    return MOZ_UNLIKELY(isErr()) ? f(unwrapErr()) : unwrap();
  }

  /**
   * Given a function V -> Result<V2, E>, apply it to this result's success
   * value and return its result. If this result is an error value, it is
   * propagated.
   *
   * This is sometimes called "flatMap" or ">>=" in other contexts.
   *
   * `andThen`ing over success values invokes the function to produce a new
   * result:
   *
   *     Result<const char*, Error> res("hello, andThen!");
   *     Result<HtmlFreeString, Error> res2 = res.andThen([](const char* s) {
   *       return containsHtmlTag(s)
   *         ? Result<HtmlFreeString, Error>(Error("Invalid: contains HTML"))
   *         : Result<HtmlFreeString, Error>(HtmlFreeString(s));
   *       }
   *     });
   *     MOZ_ASSERT(res2.isOk());
   *     MOZ_ASSERT(res2.unwrap() == HtmlFreeString("hello, andThen!");
   *
   * `andThen`ing over error results does not invoke the function, and just
   * propagates the error result:
   *
   *     Result<int, const char*> res("some error");
   *     auto res2 = res.andThen([](int x) { ... });
   *     MOZ_ASSERT(res2.isErr());
   *     MOZ_ASSERT(res.unwrapErr() == res2.unwrapErr());
   */
  template <typename F, typename = std::enable_if_t<detail::IsResult<
                            decltype((*((F*)nullptr))(*((V*)nullptr)))>::value>>
  auto andThen(F f) -> decltype(f(*((V*)nullptr))) {
    return MOZ_LIKELY(isOk()) ? f(unwrap()) : propagateErr();
  }
};

/**
 * A type that auto-converts to an error Result. This is like a Result without
 * a success type. It's the best return type for functions that always return
 * an error--functions designed to build and populate error objects. It's also
 * useful in error-handling macros; see MOZ_TRY for an example.
 */
template <typename E>
class MOZ_MUST_USE_TYPE GenericErrorResult {
  E mErrorValue;

  template <typename V, typename E2>
  friend class Result;

 public:
  explicit GenericErrorResult(E&& aErrorValue)
      : mErrorValue(std::forward<E>(aErrorValue)) {}
};

template <typename E>
inline GenericErrorResult<E> Err(E&& aErrorValue) {
  return GenericErrorResult<E>(std::forward<E>(aErrorValue));
}

}  // namespace mozilla

/**
 * MOZ_TRY(expr) is the C++ equivalent of Rust's `try!(expr);`. First, it
 * evaluates expr, which must produce a Result value. On success, it
 * discards the result altogether. On error, it immediately returns an error
 * Result from the enclosing function.
 */
#define MOZ_TRY(expr)                                   \
  do {                                                  \
    auto mozTryTempResult_ = ::mozilla::ToResult(expr); \
    if (MOZ_UNLIKELY(mozTryTempResult_.isErr())) {      \
      return mozTryTempResult_.propagateErr();          \
    }                                                   \
  } while (0)

/**
 * MOZ_TRY_VAR(target, expr) is the C++ equivalent of Rust's `target =
 * try!(expr);`. First, it evaluates expr, which must produce a Result value. On
 * success, the result's success value is assigned to target. On error,
 * immediately returns the error result. |target| must be an lvalue.
 */
#define MOZ_TRY_VAR(target, expr)                     \
  do {                                                \
    auto mozTryVarTempResult_ = (expr);               \
    if (MOZ_UNLIKELY(mozTryVarTempResult_.isErr())) { \
      return mozTryVarTempResult_.propagateErr();     \
    }                                                 \
    (target) = mozTryVarTempResult_.unwrap();         \
  } while (0)

#endif  // mozilla_Result_h
