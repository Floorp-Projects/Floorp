/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Creates a Tainted<> wrapper to enforce data validation before use.
 */

#ifndef mozilla_Tainting_h
#define mozilla_Tainting_h

#include <utility>
#include "mozilla/MacroArgs.h"

namespace mozilla {

template <typename T>
class Tainted;

namespace ipc {
template <typename>
struct IPDLParamTraits;
}

/*
 * The Tainted<> class allows data to be wrapped and considered 'tainted'; which
 * requires explicit validation of the data before it can be used for
 * comparisons or in arithmetic.
 *
 * Tainted<> objects are intended to be passed down callstacks (still in
 * Tainted<> form) to whatever location is appropriate to validate (or complete
 * validation) of the data before finally unwrapping it.
 *
 * Tainting data ensures that validation actually occurs and is not forgotten,
 * increase consideration of validation so it can be as strict as possible, and
 * makes it clear from a code point of view where and what validation is
 * performed.
 */

// ================================================

/*
 * Simple Tainted<foo> class
 *
 * Class should not support any de-reference or comparison operator and instead
 * force all access to the member variable through the MOZ_VALIDATE macros.
 *
 * While the Coerce() function is publicly accessible on the class, it should
 * only be used by the MOZ_VALIDATE macros, and static analysis will prevent
 * it being used elsewhere.
 */

template <typename T>
class Tainted {
 private:
  T mValue;

 public:
  explicit Tainted() = default;

  template <typename U>
  explicit Tainted(U&& aValue) : mValue(std::forward<U>(aValue)) {}

  T& Coerce() { return this->mValue; }
  const T& Coerce() const { return this->mValue; }

  friend struct mozilla::ipc::IPDLParamTraits<Tainted<T>>;
};

// ================================================

/*
 * Macros to validate and un-taint a value.
 *
 * All macros accept the tainted variable as the first argument, and a condition
 * as the second argument. If the condition is satisfied, then the value is
 * considered valid.
 *
 * Usage:
 *   Tainted<int> a(50);
 *
 *
 *   int w = MOZ_VALIDATE_AND_GET(a, a < 10);
 *       Here we check that a is greater than 10 and if so, we
 *       return its value. If it is not greater than 10 we MOZ_RELEASE_ASSERT.
 *
 *       Note that while the comparison of a < 10 works inside the macro,
 *       doing so outside the macro (such as with if (a < 10) will
 *       (intentionally) fail. We do this to ensure that all validation logic
 *       is self-contained.
 *
 *
 *   int x = MOZ_VALIDATE_AND_GET(a, a < 10, "a is too large");
 *       The macro also supports supplying a custom string to the
 *       MOZ_RELEASE_ASSERT
 *
 *
 *   int y = MOZ_VALIDATE_AND_GET(a, [&a] {
 *     bool intermediate_result = someFunction(a);
 *     if (intermediate_result) {
 *       return true;
 *     } else {
 *       return someOtherFunction(a);
 *     }
 *   }());
 *      In this example we use a lambda function to perform a more complicated
 *      validation that cannot be performed succinctly in a single conditional.
 *
 *
 *   int safeOffset = MOZ_VALIDATE_OR(a, a < 10, 0);
 *      MOZ_VALIDATE_OR will provide a default value to use if the tainted value
 *      is invalid.
 *
 *
 *
 *   if (!MOZ_IS_VALID(a, a > 50)) {
 *     return false;
 *   }
 *      If you want to test validity without triggering an assertion, the
 *      MOZ_IS_VALID macro can be used anywhere as a boolean.
 *
 */

#define MOZ_TAINT_GLUE(a, b) a b

// We use the same variable name in the nested scope, shadowing the outer
// scope - this allows the user to write the same variable name in the
// macro's condition without using a magic name like 'value'.
//
// We mark it MOZ_MAYBE_UNUSED because sometimes the condition doesn't
// make use of tainted_value, which causes an unused variable warning.
// That will only happen when we are bypssing validation, which is a
// future ergonomic we will iterate on.
#define MOZ_VALIDATE_AND_GET_HELPER3(tainted_value, condition, \
                                     assertionstring)          \
  [&tainted_value]() {                                         \
    auto& tmp = tainted_value.Coerce();                        \
    auto& MOZ_MAYBE_UNUSED tainted_value = tmp;                \
    MOZ_RELEASE_ASSERT((condition), assertionstring);          \
    return tmp;                                                \
  }()

// This and the next macros are heavy C++ gobblygook to support a two and three
// argument variant.
#define MOZ_VALIDATE_AND_GET_HELPER2(tainted_value, condition)        \
  MOZ_VALIDATE_AND_GET_HELPER3(tainted_value, condition,              \
                               "MOZ_VALIDATE_AND_GET(" #tainted_value \
                               ", " #condition ") has failed")

#define MOZ_VALIDATE_AND_GET(...)                                            \
  MOZ_TAINT_GLUE(MOZ_PASTE_PREFIX_AND_ARG_COUNT(MOZ_VALIDATE_AND_GET_HELPER, \
                                                __VA_ARGS__),                \
                 (__VA_ARGS__))

// This construct uses a lambda expression to create a scope and test the
// condition, returning true or false.
// We use the same variable-shadowing trick.
#define MOZ_IS_VALID(tainted_value, condition)  \
  [&tainted_value]() {                          \
    auto& tmp = tainted_value.Coerce();         \
    auto& MOZ_MAYBE_UNUSED tainted_value = tmp; \
    return (condition);                         \
  }()

// Allows us to test validity and returning a default value if the value is
// invalid.
#define MOZ_VALIDATE_OR(tainted_value, condition, alternate_value) \
  (MOZ_IS_VALID(tainted_value, condition) ? tainted_value.Coerce() \
                                          : alternate_value)

// This allows unsafe removal of the Taint wrapper.
// A justification string is required to explain why this is acceptable
#define MOZ_NO_VALIDATE(tainted_value, justification) tainted_value.Coerce()

/*
 TODO:

  - Figure out if there are helpers that would be useful for Strings and
 Principals
  - Write static analysis to enforce invariants:
    - No use of .Coerce() except in the header file.
    - No constant passed to the condition of MOZ_VALIDATE_AND_GET
 */

}  // namespace mozilla

#endif /* mozilla_Tainting_h */
