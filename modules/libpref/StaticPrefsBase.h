/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StaticPrefsBase_h
#define mozilla_StaticPrefsBase_h

#include <type_traits>

#include "mozilla/Atomics.h"
#include "mozilla/DataMutex.h"
#include "nsString.h"

namespace mozilla {

class SharedPrefMapBuilder;

// These typedefs are for use within init/StaticPrefList*.h.

typedef const char* String;

using DataMutexString = StaticDataMutex<nsCString>;

template <typename T>
struct IsString : std::false_type {};

template <>
struct IsString<String> : std::true_type {};

template <>
struct IsString<DataMutexString> : std::true_type {};

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

// XXX: Atomic<float> currently doesn't work (see bug 1552086). Once it's
// supported we will be able to use Atomic<float> here.
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

template <>
struct StripAtomicImpl<DataMutexString> {
  typedef nsCString Type;
};

template <typename T>
using StripAtomic = typename StripAtomicImpl<T>::Type;

template <typename T>
struct IsAtomic : std::false_type {};

template <typename T, MemoryOrdering Order>
struct IsAtomic<Atomic<T, Order>> : std::true_type {};

template <typename T>
struct IsAtomic<std::atomic<T>> : std::true_type {};

namespace StaticPrefs {

void MaybeInitOncePrefs();

}  // namespace StaticPrefs

}  // namespace mozilla

#endif  // mozilla_StaticPrefsBase_h
