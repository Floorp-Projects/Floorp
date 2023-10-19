/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef replace_malloc_bridge_h
#define replace_malloc_bridge_h

// The replace-malloc bridge allows bidirectional method calls between
// a program and the replace-malloc library that has been loaded for it.
// In Firefox, this is used to allow method calls between code in libxul
// and code in the replace-malloc library, without libxul needing to link
// against that library or vice-versa.
//
// Subsystems can add methods for their own need. Replace-malloc libraries
// can decide to implement those methods or not.
//
// Replace-malloc libraries can provide such a bridge by implementing
// a ReplaceMallocBridge-derived class, and a get_bridge function
// returning an instance of that class. The default methods in
// ReplaceMallocBridge are expected to return values that callers would
// understand as "the bridge doesn't implement this method", so that a
// replace-malloc library doesn't have to implement all methods.
//
// The ReplaceMallocBridge class contains definitions for methods for
// all replace-malloc libraries. Each library picks the methods it wants
// to reply to in its ReplaceMallocBridge-derived class instance.
// All methods of ReplaceMallocBridge must be virtual. Similarly,
// anything passed as an argument to those methods must be plain data, or
// an instance of a class with only virtual methods.
//
// Binary compatibility is expected to be maintained, such that a newer
// Firefox can be used with an old replace-malloc library, or an old
// Firefox can be used with a newer replace-malloc library. As such, only
// new virtual methods should be added to ReplaceMallocBridge, and
// each change should have a corresponding bump of the mVersion value.
// At the same time, each virtual method should have a corresponding
// wrapper calling the virtual method on the instance from
// ReplaceMallocBridge::Get(), giving it the version the virtual method
// was added.
//
// Parts that are not relevant to the replace-malloc library end of the
// bridge are hidden when REPLACE_MALLOC_IMPL is not defined, which is
// the case when including replace_malloc.h.

struct ReplaceMallocBridge;

#include "mozilla/Types.h"

MOZ_BEGIN_EXTERN_C

#ifndef REPLACE_MALLOC_IMPL
// Returns the replace-malloc bridge if there is one to be returned.
MFBT_API ReplaceMallocBridge* get_bridge();
#endif

// Table of malloc functions.
//   e.g. void* (*malloc)(size_t), etc.

#define MALLOC_DECL(name, return_type, ...) \
  typedef return_type(name##_impl_t)(__VA_ARGS__);

#include "malloc_decls.h"

#define MALLOC_DECL(name, return_type, ...) name##_impl_t* name;

typedef struct {
#include "malloc_decls.h"
} malloc_table_t;

MOZ_END_EXTERN_C

#ifdef __cplusplus

// Table of malloc hook functions.
// Those functions are called with the arguments and results of malloc
// functions after they are called.
//   e.g. void* (*malloc_hook)(void*, size_t), etc.
// They can either return the result they're given, or alter it before
// returning it.
// The hooks corresponding to functions, like free(void*), that return no
// value, don't take an extra argument.
// The table must at least contain a pointer for malloc_hook and free_hook
// functions. They will be used as fallback if no pointer is given for
// other allocation functions, like calloc_hook.
namespace mozilla {
namespace detail {
template <typename R, typename... Args>
struct AllocHookType {
  using Type = R (*)(R, Args...);
};

template <typename... Args>
struct AllocHookType<void, Args...> {
  using Type = void (*)(Args...);
};

}  // namespace detail
}  // namespace mozilla

#  define MALLOC_DECL(name, return_type, ...)                                 \
    typename mozilla::detail::AllocHookType<return_type, ##__VA_ARGS__>::Type \
        name##_hook;

typedef struct {
#  include "malloc_decls.h"
  // Like free_hook, but called before realloc_hook. free_hook is called
  // instead of not given.
  void (*realloc_hook_before)(void* aPtr);
} malloc_hook_table_t;

namespace mozilla {
namespace dmd {
struct DMDFuncs;
}  // namespace dmd

namespace phc {

class AddrInfo;

}  // namespace phc

// Callbacks to register debug file handles for Poison IO interpose.
// See Mozilla(|Un)RegisterDebugHandle in xpcom/build/PoisonIOInterposer.h
struct DebugFdRegistry {
  virtual void RegisterHandle(intptr_t aFd);

  virtual void UnRegisterHandle(intptr_t aFd);
};
}  // namespace mozilla

struct ReplaceMallocBridge {
  ReplaceMallocBridge() : mVersion(6) {}

  // This method was added in version 1 of the bridge.
  virtual mozilla::dmd::DMDFuncs* GetDMDFuncs() { return nullptr; }

  // Send a DebugFdRegistry instance to the replace-malloc library so that
  // it can register/unregister file descriptors whenever needed. The
  // instance is valid until the process dies.
  // This method was added in version 2 of the bridge.
  virtual void InitDebugFd(mozilla::DebugFdRegistry&) {}

  // Register a list of malloc functions and hook functions to the
  // replace-malloc library so that it can choose to dispatch to them
  // when needed. The details of what is dispatched when is left to the
  // replace-malloc library.
  // Passing a nullptr for either table will unregister a previously
  // registered table under the same name.
  // Returns nullptr if registration failed.
  // If registration succeeded, a table of "pure" malloc functions is
  // returned. Those "pure" malloc functions won't call hooks.
  // /!\ Do not rely on registration/unregistration to be instantaneous.
  // Functions from a previously registered table may still be called for
  // a brief time after RegisterHook returns.
  // This method was added in version 3 of the bridge.
  virtual const malloc_table_t* RegisterHook(
      const char* aName, const malloc_table_t* aTable,
      const malloc_hook_table_t* aHookTable) {
    return nullptr;
  }

#  ifndef REPLACE_MALLOC_IMPL
  // Returns the replace-malloc bridge if its version is at least the
  // requested one.
  static ReplaceMallocBridge* Get(int aMinimumVersion) {
    static ReplaceMallocBridge* sSingleton = get_bridge();
    return (sSingleton && sSingleton->mVersion >= aMinimumVersion) ? sSingleton
                                                                   : nullptr;
  }
#  endif

 protected:
  const int mVersion;
};

#  ifndef REPLACE_MALLOC_IMPL
// Class containing wrappers for calls to ReplaceMallocBridge methods.
// Those wrappers need to be static methods in a class because compilers
// complain about unused static global functions, and linkers complain
// about multiple definitions of non-static global functions.
// Using a separate class from ReplaceMallocBridge allows the function
// names to be identical.
struct ReplaceMalloc {
  // Don't call this method from performance critical code. Use
  // mozilla::dmd::DMDFuncs::Get() instead, it has less overhead.
  static mozilla::dmd::DMDFuncs* GetDMDFuncs() {
    auto singleton = ReplaceMallocBridge::Get(/* minimumVersion */ 1);
    return singleton ? singleton->GetDMDFuncs() : nullptr;
  }

  static void InitDebugFd(mozilla::DebugFdRegistry& aRegistry) {
    auto singleton = ReplaceMallocBridge::Get(/* minimumVersion */ 2);
    if (singleton) {
      singleton->InitDebugFd(aRegistry);
    }
  }

  static const malloc_table_t* RegisterHook(
      const char* aName, const malloc_table_t* aTable,
      const malloc_hook_table_t* aHookTable) {
    auto singleton = ReplaceMallocBridge::Get(/* minimumVersion */ 3);
    return singleton ? singleton->RegisterHook(aName, aTable, aHookTable)
                     : nullptr;
  }
};
#  endif

#endif  // __cplusplus

#endif  // replace_malloc_bridge_h
