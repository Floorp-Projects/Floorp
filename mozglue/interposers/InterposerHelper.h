/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InterposerHelper_h
#define InterposerHelper_h

#include <type_traits>

#ifdef MOZ_LINKER
#  include "Linker.h"
#else
#  include <dlfcn.h>
#endif

#include "mozilla/Assertions.h"

template <typename T>
static inline T dlsym_wrapper(void* aHandle, const char* aName) {
#ifdef MOZ_LINKER
  return reinterpret_cast<T>(__wrap_dlsym(aHandle, aName));
#else
  return reinterpret_cast<T>(dlsym(aHandle, aName));
#endif  // MOZ_LINKER
}

static inline void* dlopen_wrapper(const char* aPath, int flags) {
#ifdef MOZ_LINKER
  return __wrap_dlopen(aPath, flags);
#else
  return dlopen(aPath, flags);
#endif  // MOZ_LINKER
}

template <typename T>
static T get_real_symbol(const char* aName, T aReplacementSymbol) {
  // T can only be a function pointer
  static_assert(std::is_function<typename std::remove_pointer<T>::type>::value);

  // Find the corresponding function in the linked libraries
  T real_symbol = dlsym_wrapper<T>(RTLD_NEXT, aName);

#if defined(ANDROID)
  if ((real_symbol == nullptr) || (real_symbol == aReplacementSymbol)) {
    // On old versions of Android the application runtime links in libc before
    // we get a chance to link libmozglue, so its symbols don't appear when
    // resolving them with RTLD_NEXT. This behavior differs between the
    // different versions of Android so we'll just look for them directly into
    // libc.so. Note that this won't work if we're trying to interpose
    // functions that are in other libraries, but hopefully we'll never have
    // to do that.
    void* handle = dlopen_wrapper("libc.so", RTLD_LAZY);

    if (handle) {
      real_symbol = dlsym_wrapper<T>(handle, aName);
    }
  }
#endif

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
