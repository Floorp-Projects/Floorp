/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InterposerHelper_h
#define InterposerHelper_h

#include <type_traits>

#include <dlfcn.h>

#include "mozilla/Assertions.h"

template <typename T>
static T get_real_symbol(const char* aName, T aReplacementSymbol) {
  // T can only be a function pointer
  static_assert(std::is_function<typename std::remove_pointer<T>::type>::value);

  // Find the corresponding function in the linked libraries
  T real_symbol = reinterpret_cast<T>(dlsym(RTLD_NEXT, aName));

  if (real_symbol == nullptr) {
    MOZ_CRASH_UNSAFE_PRINTF(
        "%s() interposition failed but the interposer function is "
        "still being called, this won't work!",
        aName);
  }

  if (real_symbol == aReplacementSymbol) {
    MOZ_CRASH_UNSAFE_PRINTF(
        "We could not obtain the real %s(). Calling the symbol we "
        "got would make us enter an infinite loop so stop here instead.",
        aName);
  }

  return real_symbol;
}

#define GET_REAL_SYMBOL(name) get_real_symbol(#name, name)

#endif  // InterposerHelper_h
