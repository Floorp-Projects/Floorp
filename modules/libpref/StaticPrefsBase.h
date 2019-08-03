/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StaticPrefsBase_h
#define mozilla_StaticPrefsBase_h

#include "mozilla/Atomics.h"

namespace mozilla {

class SharedPrefMapBuilder;

// These typedefs are for use within init/StaticPrefList*.h.

typedef const char* String;

typedef Atomic<bool, Relaxed, recordreplay::Behavior::DontPreserve>
    RelaxedAtomicBool;
typedef Atomic<bool, ReleaseAcquire, recordreplay::Behavior::DontPreserve>
    ReleaseAcquireAtomicBool;
typedef Atomic<bool, SequentiallyConsistent,
               recordreplay::Behavior::DontPreserve>
    SequentiallyConsistentAtomicBool;

typedef Atomic<int32_t, Relaxed, recordreplay::Behavior::DontPreserve>
    RelaxedAtomicInt32;
typedef Atomic<int32_t, ReleaseAcquire, recordreplay::Behavior::DontPreserve>
    ReleaseAcquireAtomicInt32;
typedef Atomic<int32_t, SequentiallyConsistent,
               recordreplay::Behavior::DontPreserve>
    SequentiallyConsistentAtomicInt32;

typedef Atomic<uint32_t, Relaxed, recordreplay::Behavior::DontPreserve>
    RelaxedAtomicUint32;
typedef Atomic<uint32_t, ReleaseAcquire, recordreplay::Behavior::DontPreserve>
    ReleaseAcquireAtomicUint32;
typedef Atomic<uint32_t, SequentiallyConsistent,
               recordreplay::Behavior::DontPreserve>
    SequentiallyConsistentAtomicUint32;

// XXX: Atomic<float> currently doesn't work (see bug 1552086). Once it's
// supported we will be able to use Atomic<float> here.
typedef std::atomic<float> AtomicFloat;

template <typename T>
struct StripAtomicImpl {
  typedef T Type;
};

template <typename T, MemoryOrdering Order, recordreplay::Behavior Recording>
struct StripAtomicImpl<Atomic<T, Order, Recording>> {
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

template <typename T, MemoryOrdering Order, recordreplay::Behavior Recording>
struct IsAtomic<Atomic<T, Order, Recording>> : TrueType {};

template <typename T>
struct IsAtomic<std::atomic<T>> : TrueType {};

namespace StaticPrefs {

void MaybeInitOncePrefs();

}  // namespace StaticPrefs

}  // namespace mozilla

#endif  // mozilla_StaticPrefsBase_h
