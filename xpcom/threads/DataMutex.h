/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DataMutex_h__
#define DataMutex_h__

#include <utility>
#include "mozilla/Mutex.h"
#include "mozilla/StaticMutex.h"

namespace mozilla {

// A template to wrap a type with a mutex so that accesses to the type's
// data are required to take the lock before accessing it. This ensures
// that a mutex is explicitly associated with the data that it protects,
// and makes it impossible to access the data without first taking the
// associated mutex.
//
// This is based on Rust's std::sync::Mutex, which operates under the
// strategy of locking data, rather than code.
//
// Examples:
//
//    DataMutex<uint32_t> u32DataMutex(1, "u32DataMutex");
//    auto x = u32DataMutex.Lock();
//    *x = 4;
//    assert(*x, 4u);
//
//    DataMutex<nsTArray<uint32_t>> arrayDataMutex("arrayDataMutex");
//    auto a = arrayDataMutex.Lock();
//    auto& x = a.ref();
//    x.AppendElement(1u);
//    assert(x[0], 1u);
//
template <typename T, typename MutexType>
class DataMutexBase {
 public:
  template <typename V>
  class MOZ_STACK_CLASS AutoLockBase {
   public:
    V* operator->() const& { return &ref(); }
    V* operator->() const&& = delete;

    V& operator*() const& { return ref(); }
    V& operator*() const&& = delete;

    // Like RefPtr, make this act like its underlying raw pointer type
    // whenever it is used in a context where a raw pointer is expected.
    operator V*() const& { return &ref(); }

    // Like RefPtr, don't allow implicit conversion of temporary to raw pointer.
    operator V*() const&& = delete;

    V& ref() const& {
      MOZ_ASSERT(mOwner);
      return mOwner->mValue;
    }
    V& ref() const&& = delete;

    AutoLockBase(AutoLockBase&& aOther) : mOwner(aOther.mOwner) {
      aOther.mOwner = nullptr;
    }

    ~AutoLockBase() {
      if (mOwner) {
        mOwner->mMutex.Unlock();
        mOwner = nullptr;
      }
    }

   private:
    friend class DataMutexBase;

    AutoLockBase(const AutoLockBase& aOther) = delete;

    explicit AutoLockBase(DataMutexBase<T, MutexType>* aDataMutex)
        : mOwner(aDataMutex) {
      MOZ_ASSERT(!!mOwner);
      mOwner->mMutex.Lock();
    }

    DataMutexBase<T, MutexType>* mOwner;
  };

  using AutoLock = AutoLockBase<T>;
  using ConstAutoLock = AutoLockBase<const T>;

  explicit DataMutexBase(const char* aName) : mMutex(aName) {}

  DataMutexBase(T&& aValue, const char* aName)
      : mMutex(aName), mValue(std::move(aValue)) {}

  AutoLock Lock() { return AutoLock(this); }
  ConstAutoLock ConstLock() { return ConstAutoLock(this); }

  const MutexType& Mutex() const { return mMutex; }

 private:
  MutexType mMutex;
  T mValue;
};

// Craft a version of StaticMutex that takes a const char* in its ctor.
// We need this so it works interchangeably with Mutex which requires a const
// char* aName in its ctor.
class StaticMutexNameless : public StaticMutex {
 public:
  explicit StaticMutexNameless(const char* aName) : StaticMutex() {}

 private:
  // Disallow copy construction, `=`, `new`, and `delete` like BaseStaticMutex.
#ifdef DEBUG
  StaticMutexNameless(StaticMutexNameless& aOther);
#endif  // DEBUG
  StaticMutexNameless& operator=(StaticMutexNameless* aRhs);
  static void* operator new(size_t) noexcept(true);
  static void operator delete(void*);
};

template <typename T>
using DataMutex = DataMutexBase<T, Mutex>;
template <typename T>
using StaticDataMutex = DataMutexBase<T, StaticMutexNameless>;

}  // namespace mozilla

#endif  // DataMutex_h__
