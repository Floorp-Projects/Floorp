// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_SAFE_MATH_H_
#define BASE_NUMERICS_SAFE_MATH_H_

#include <stddef.h>

#include <limits>
#include <type_traits>

#include "base/logging.h"
#include "base/numerics/safe_math_impl.h"

namespace base {

namespace internal {

// CheckedNumeric implements all the logic and operators for detecting integer
// boundary conditions such as overflow, underflow, and invalid conversions.
// The CheckedNumeric type implicitly converts from floating point and integer
// data types, and contains overloads for basic arithmetic operations (i.e.: +,
// -, *, /, %).
//
// The following methods convert from CheckedNumeric to standard numeric values:
// IsValid() - Returns true if the underlying numeric value is valid (i.e. has
//             has not wrapped and is not the result of an invalid conversion).
// ValueOrDie() - Returns the underlying value. If the state is not valid this
//                call will crash on a CHECK.
// ValueOrDefault() - Returns the current value, or the supplied default if the
//                    state is not valid.
// ValueFloating() - Returns the underlying floating point value (valid only
//                   only for floating point CheckedNumeric types).
//
// Bitwise operations are explicitly not supported, because correct
// handling of some cases (e.g. sign manipulation) is ambiguous. Comparison
// operations are explicitly not supported because they could result in a crash
// on a CHECK condition. You should use patterns like the following for these
// operations:
// Bitwise operation:
//     CheckedNumeric<int> checked_int = untrusted_input_value;
//     int x = checked_int.ValueOrDefault(0) | kFlagValues;
// Comparison:
//   CheckedNumeric<size_t> checked_size = untrusted_input_value;
//   checked_size += HEADER LENGTH;
//   if (checked_size.IsValid() && checked_size.ValueOrDie() < buffer_size)
//     Do stuff...
template <typename T>
class CheckedNumeric {
  static_assert(std::is_arithmetic<T>::value,
                "CheckedNumeric<T>: T must be a numeric type.");

 public:
  typedef T type;

  CheckedNumeric() {}

  // Copy constructor.
  template <typename Src>
  CheckedNumeric(const CheckedNumeric<Src>& rhs)
      : state_(rhs.ValueUnsafe(), rhs.IsValid()) {}

  template <typename Src>
  CheckedNumeric(Src value, bool is_valid) : state_(value, is_valid) {}

  // This is not an explicit constructor because we implicitly upgrade regular
  // numerics to CheckedNumerics to make them easier to use.
  template <typename Src>
  CheckedNumeric(Src value)  // NOLINT(runtime/explicit)
      : state_(value) {
    static_assert(std::numeric_limits<Src>::is_specialized,
                  "Argument must be numeric.");
  }

  // This is not an explicit constructor because we want a seamless conversion
  // from StrictNumeric types.
  template <typename Src>
  CheckedNumeric(StrictNumeric<Src> value)  // NOLINT(runtime/explicit)
      : state_(static_cast<Src>(value)) {
  }

  // IsValid() is the public API to test if a CheckedNumeric is currently valid.
  bool IsValid() const { return state_.is_valid(); }

  // ValueOrDie() The primary accessor for the underlying value. If the current
  // state is not valid it will CHECK and crash.
  T ValueOrDie() const {
    CHECK(IsValid());
    return state_.value();
  }

  // ValueOrDefault(T default_value) A convenience method that returns the
  // current value if the state is valid, and the supplied default_value for
  // any other state.
  T ValueOrDefault(T default_value) const {
    return IsValid() ? state_.value() : default_value;
  }

  // ValueFloating() - Since floating point values include their validity state,
  // we provide an easy method for extracting them directly, without a risk of
  // crashing on a CHECK.
  T ValueFloating() const {
    static_assert(std::numeric_limits<T>::is_iec559, "Argument must be float.");
    return CheckedNumeric<T>::cast(*this).ValueUnsafe();
  }

  // ValueUnsafe() - DO NOT USE THIS IN EXTERNAL CODE - It is public right now
  // for tests and to avoid a big matrix of friend operator overloads. But the
  // values it returns are unintuitive and likely to change in the future.
  // Returns: the raw numeric value, regardless of the current state.
  T ValueUnsafe() const { return state_.value(); }

  // Prototypes for the supported arithmetic operator overloads.
  template <typename Src>
  CheckedNumeric& operator+=(Src rhs);
  template <typename Src>
  CheckedNumeric& operator-=(Src rhs);
  template <typename Src>
  CheckedNumeric& operator*=(Src rhs);
  template <typename Src>
  CheckedNumeric& operator/=(Src rhs);
  template <typename Src>
  CheckedNumeric& operator%=(Src rhs);
  template <typename Src>
  CheckedNumeric& operator<<=(Src rhs);
  template <typename Src>
  CheckedNumeric& operator>>=(Src rhs);

  CheckedNumeric operator-() const {
    // Negation is always valid for floating point.
    T value = 0;
    bool is_valid = (std::numeric_limits<T>::is_iec559 || IsValid()) &&
                    CheckedNeg(state_.value(), &value);
    return CheckedNumeric<T>(value, is_valid);
  }

  CheckedNumeric Abs() const {
    // Absolute value is always valid for floating point.
    T value = 0;
    bool is_valid = (std::numeric_limits<T>::is_iec559 || IsValid()) &&
                    CheckedAbs(state_.value(), &value);
    return CheckedNumeric<T>(value, is_valid);
  }

  // This function is available only for integral types. It returns an unsigned
  // integer of the same width as the source type, containing the absolute value
  // of the source, and properly handling signed min.
  CheckedNumeric<typename UnsignedOrFloatForSize<T>::type> UnsignedAbs() const {
    return CheckedNumeric<typename UnsignedOrFloatForSize<T>::type>(
        SafeUnsignedAbs(state_.value()), state_.is_valid());
  }

  CheckedNumeric& operator++() {
    *this += 1;
    return *this;
  }

  CheckedNumeric operator++(int) {
    CheckedNumeric value = *this;
    *this += 1;
    return value;
  }

  CheckedNumeric& operator--() {
    *this -= 1;
    return *this;
  }

  CheckedNumeric operator--(int) {
    CheckedNumeric value = *this;
    *this -= 1;
    return value;
  }

  // These static methods behave like a convenience cast operator targeting
  // the desired CheckedNumeric type. As an optimization, a reference is
  // returned when Src is the same type as T.
  template <typename Src>
  static CheckedNumeric<T> cast(
      Src u,
      typename std::enable_if<std::numeric_limits<Src>::is_specialized,
                              int>::type = 0) {
    return u;
  }

  template <typename Src>
  static CheckedNumeric<T> cast(
      const CheckedNumeric<Src>& u,
      typename std::enable_if<!std::is_same<Src, T>::value, int>::type = 0) {
    return u;
  }

  static const CheckedNumeric<T>& cast(const CheckedNumeric<T>& u) { return u; }

 private:
  CheckedNumericState<T> state_;
};

// This is the boilerplate for the standard arithmetic operator overloads. A
// macro isn't the prettiest solution, but it beats rewriting these five times.
// Some details worth noting are:
//  * We apply the standard arithmetic promotions.
//  * We skip range checks for floating points.
//  * We skip range checks for destination integers with sufficient range.
// TODO(jschuh): extract these out into templates.
#define BASE_NUMERIC_ARITHMETIC_OPERATORS(NAME, OP, COMPOUND_OP, PROMOTION)   \
  /* Binary arithmetic operator for CheckedNumerics of the same type. */      \
  template <typename L, typename R>                                           \
  CheckedNumeric<typename ArithmeticPromotion<PROMOTION, L, R>::type>         \
  operator OP(const CheckedNumeric<L>& lhs, const CheckedNumeric<R>& rhs) {   \
    using P = typename ArithmeticPromotion<PROMOTION, L, R>::type;            \
    if (!rhs.IsValid() || !lhs.IsValid())                                     \
      return CheckedNumeric<P>(0, false);                                     \
    /* Floating point always takes the fast path */                           \
    if (std::is_floating_point<L>::value || std::is_floating_point<R>::value) \
      return CheckedNumeric<P>(lhs.ValueUnsafe() OP rhs.ValueUnsafe());       \
    P result = 0;                                                             \
    bool is_valid =                                                           \
        Checked##NAME(lhs.ValueUnsafe(), rhs.ValueUnsafe(), &result);         \
    return CheckedNumeric<P>(result, is_valid);                               \
  }                                                                           \
  /* Assignment arithmetic operator implementation from CheckedNumeric. */    \
  template <typename L>                                                       \
  template <typename R>                                                       \
  CheckedNumeric<L>& CheckedNumeric<L>::operator COMPOUND_OP(R rhs) {         \
    *this = *this OP rhs;                                                     \
    return *this;                                                             \
  }                                                                           \
  /* Binary arithmetic operator for left CheckedNumeric and right numeric. */ \
  template <typename L, typename R,                                           \
            typename std::enable_if<std::is_arithmetic<R>::value>::type* =    \
                nullptr>                                                      \
  CheckedNumeric<typename ArithmeticPromotion<PROMOTION, L, R>::type>         \
  operator OP(const CheckedNumeric<L>& lhs, R rhs) {                          \
    return lhs OP CheckedNumeric<R>(rhs);                                     \
  }                                                                           \
  /* Binary arithmetic operator for left numeric and right CheckedNumeric. */ \
  template <typename L, typename R,                                           \
            typename std::enable_if<std::is_arithmetic<L>::value>::type* =    \
                nullptr>                                                      \
  CheckedNumeric<typename ArithmeticPromotion<PROMOTION, L, R>::type>         \
  operator OP(L lhs, const CheckedNumeric<R>& rhs) {                          \
    return CheckedNumeric<L>(lhs) OP rhs;                                     \
  }

BASE_NUMERIC_ARITHMETIC_OPERATORS(Add, +, +=, MAX_EXPONENT_PROMOTION)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Sub, -, -=, MAX_EXPONENT_PROMOTION)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Mul, *, *=, MAX_EXPONENT_PROMOTION)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Div, /, /=, MAX_EXPONENT_PROMOTION)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Mod, %, %=, MAX_EXPONENT_PROMOTION)
BASE_NUMERIC_ARITHMETIC_OPERATORS(LeftShift, <<, <<=, LEFT_PROMOTION)
BASE_NUMERIC_ARITHMETIC_OPERATORS(RightShift, >>, >>=, LEFT_PROMOTION)

#undef BASE_NUMERIC_ARITHMETIC_OPERATORS

}  // namespace internal

using internal::CheckedNumeric;

}  // namespace base

#endif  // BASE_NUMERICS_SAFE_MATH_H_
