/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StaticPrefs_h
#define mozilla_StaticPrefs_h

#include "gfxPlatform.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/gfx/LoggingConstants.h"
#include "MainThreadUtils.h"
#include <atomic>
#include <cmath>  // for M_PI

namespace mozilla {

class SharedPrefMapBuilder;

// These typedefs are for use within StaticPrefList.h.

typedef const char* String;

typedef Atomic<bool, Relaxed> RelaxedAtomicBool;
typedef Atomic<bool, ReleaseAcquire> ReleaseAcquireAtomicBool;
typedef Atomic<bool, SequentiallyConsistent> SequentiallyConsistentAtomicBool;

typedef Atomic<int32_t, Relaxed> RelaxedAtomicInt32;
typedef Atomic<int32_t, ReleaseAcquire> ReleaseAcquireAtomicInt32;
typedef Atomic<int32_t, SequentiallyConsistent>
    SequentiallyConsistentAtomicInt32;

typedef Atomic<uint32_t, Relaxed> RelaxedAtomicUint32;
typedef Atomic<uint32_t, ReleaseAcquire> ReleaseAcquireAtomicUint32;
typedef Atomic<uint32_t, SequentiallyConsistent>
    SequentiallyConsistentAtomicUint32;

// Atomic<float> currently doesn't work (see bug 1552086), once this tast has
// been completed we will be able to use Atomic<float> instead.
typedef std::atomic<float> AtomicFloat;

template <typename T>
struct StripAtomicImpl {
  typedef T Type;
};

template <typename T, MemoryOrdering Order>
struct StripAtomicImpl<Atomic<T, Order>> {
  typedef T Type;
};

template <typename T>
struct StripAtomicImpl<std::atomic<T>> {
  typedef T Type;
};

template <typename T>
using StripAtomic = typename StripAtomicImpl<T>::Type;

template <typename T>
struct IsAtomic : FalseType {};

template <typename T, MemoryOrdering Order>
struct IsAtomic<Atomic<T, Order>> : TrueType {};

template <typename T>
struct IsAtomic<std::atomic<T>> : TrueType {};

namespace StaticPrefs {

// Enums for the update policy.
enum class UpdatePolicy {
  Skip,  // Set the value to default, skip any Preferences calls.
  Once,  // Evaluate the preference once, unchanged during the session.
  Live   // Evaluate the preference and set callback so it stays current/live.
};

void MaybeInitOncePrefs();

// For a VarCache pref like this:
//
//   VARCACHE_PREF($POLICY, "my.pref", my_pref, int32_t, 99)
//
// we generate an extern variable declaration and three getter
// declarations/definitions.
//
//     extern int32_t sVarCache_my_pref;
//     inline int32_t my_pref() {
//       if (UpdatePolicy::$POLICY != UpdatePolicy::Once) {
//         return sVarCache_my_pref;
//       }
//       MaybeInitOncePrefs();
//       return sVarCache_my_pref();
//     }
//     inline const char* GetPrefName_my_pref() { return "my.pref"; }
//     inline int32_t GetPrefDefault_my_pref() { return 99; }
//
// The extern declaration of the variable is necessary for bindgen to see it
// and generate Rust bindings.
//
#define PREF(name, cpp_type, default_value)
#define VARCACHE_PREF(policy, name, id, cpp_type, default_value) \
  extern cpp_type sVarCache_##id;                                \
  inline StripAtomic<cpp_type> id() {                            \
    if (UpdatePolicy::policy != UpdatePolicy::Once) {            \
      MOZ_DIAGNOSTIC_ASSERT(                                     \
          UpdatePolicy::policy == UpdatePolicy::Skip ||          \
              IsAtomic<cpp_type>::value || NS_IsMainThread(),    \
          "Non-atomic static pref '" name                        \
          "' being accessed on background thread by getter");    \
      return sVarCache_##id;                                     \
    }                                                            \
    MaybeInitOncePrefs();                                        \
    return sVarCache_##id;                                       \
  }                                                              \
  inline const char* GetPrefName_##id() { return name; }         \
  inline StripAtomic<cpp_type> GetPrefDefault_##id() { return default_value; }

#include "mozilla/StaticPrefList.h"
#undef PREF
#undef VARCACHE_PREF

}  // namespace StaticPrefs

}  // namespace mozilla

#endif  // mozilla_StaticPrefs_h
