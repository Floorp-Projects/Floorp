/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StaticPrefs_h
#define mozilla_StaticPrefs_h

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/TypeTraits.h"
#include "MainThreadUtils.h"

namespace mozilla {

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

template<typename T>
struct StripAtomicImpl
{
  typedef T Type;
};

template<typename T, MemoryOrdering Order>
struct StripAtomicImpl<Atomic<T, Order>>
{
  typedef T Type;
};

template<typename T>
using StripAtomic = typename StripAtomicImpl<T>::Type;

template<typename T>
struct IsAtomic : FalseType
{
};

template<typename T, MemoryOrdering Order>
struct IsAtomic<Atomic<T, Order>> : TrueType
{
};

class StaticPrefs
{
// For a VarCache pref like this:
//
//   VARCACHE_PREF("my.varcache", my_varcache, int32_t, 99)
//
// we generate a static variable declaration and a getter definition:
//
//   private:
//     static int32_t sVarCache_my_varcache;
//   public:
//     static int32_t my_varcache() { return sVarCache_my_varcache; }
//
#define PREF(str, cpp_type, default_value)
#define VARCACHE_PREF(str, id, cpp_type, default_value)                        \
private:                                                                       \
  static cpp_type sVarCache_##id;                                              \
                                                                               \
public:                                                                        \
  static StripAtomic<cpp_type> id()                                            \
  {                                                                            \
    MOZ_ASSERT(IsAtomic<cpp_type>::value || NS_IsMainThread(),                 \
               "Non-atomic static pref '" str                                  \
               "' being accessed on background thread");                       \
    return sVarCache_##id;                                                     \
  }
#include "mozilla/StaticPrefList.h"
#undef PREF
#undef VARCACHE_PREF

public:
  static void InitAll(bool aIsStartup);
};

} // namespace mozilla

#endif // mozilla_StaticPrefs_h
