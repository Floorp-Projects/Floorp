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

class StaticPrefs {
  // For a VarCache pref like this:
  //
  //   VARCACHE_PREF("my.varcache", my_varcache, int32_t, 99)
  //
  // we generate a static variable declaration, a getter and a setter
  // definition. A StaticPref can be set by using the corresponding Set method.
  // For example, if the accessor is Foo() then calling SetFoo(...) will update
  // the preference and also change the return value of subsequent Foo() calls.
  // Changing StaticPrefs is only allowed on the parent process' main thread.
  //
  //   private:
  //     static int32_t sVarCache_my_varcache;
  //   public:
  //     static int32_t my_varcache();
  //     static void Setmy_varcache(int32_t aValue);
  //     static const char* Getmy_varcachePrefName() { return "my.varcache"; }
  //     static int32_t Getmy_varcachePrefDefault() { return 99; }
  //

 public:
  // Enums for the update policy.
  enum class UpdatePolicy {
    Skip,  // Set the value to default, skip any Preferences calls.
    Once,  // Evaluate the preference once, unchanged during the session.
    Live   // Evaluate the preference and set callback so it stays current/live.
  };

#define PREF(str, cpp_type, default_value)
#define VARCACHE_PREF(policy, str, id, cpp_type, default_value) \
 private:                                                       \
  static cpp_type sVarCache_##id;                               \
                                                                \
 public:                                                        \
  static StripAtomic<cpp_type> id();                            \
  static void Set##id(StripAtomic<cpp_type> aValue);            \
  static const char* Get##id##PrefName() { return str; }        \
  static StripAtomic<cpp_type> Get##id##PrefDefault() { return default_value; }

#include "mozilla/StaticPrefList.h"
#undef PREF
#undef VARCACHE_PREF

 private:
  friend class Preferences;
  static void InitAll(bool aIsStartup);
  static void InitOncePrefs();
  static void InitOncePrefsFromShared();
  static void MaybeInitOncePrefs();
  static void RegisterOncePrefs(SharedPrefMapBuilder& aBuilder);
};

}  // namespace mozilla

#endif  // mozilla_StaticPrefs_h
