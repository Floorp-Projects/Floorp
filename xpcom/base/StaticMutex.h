/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StaticMutex_h
#define mozilla_StaticMutex_h

#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"

namespace mozilla {

/**
 * StaticMutex is a Mutex that can (and in fact, must) be used as a
 * global/static variable.
 *
 * The main reason to use StaticMutex as opposed to
 * StaticAutoPtr<OffTheBooksMutex> is that we instantiate the StaticMutex in a
 * thread-safe manner the first time it's used.
 *
 * The same caveats that apply to StaticAutoPtr apply to StaticMutex.  In
 * particular, do not use StaticMutex as a stack variable or a class instance
 * variable, because this class relies on the fact that global variablies are
 * initialized to 0 in order to initialize mMutex.  It is only safe to use
 * StaticMutex as a global or static variable.
 */
class MOZ_ONLY_USED_TO_AVOID_STATIC_CONSTRUCTORS StaticMutex
{
public:
  // In debug builds, check that mMutex is initialized for us as we expect by
  // the compiler.  In non-debug builds, don't declare a constructor so that
  // the compiler can see that the constructor is trivial.
#ifdef DEBUG
  StaticMutex()
  {
    MOZ_ASSERT(!mMutex);
  }
#endif

  void Lock()
  {
    Mutex()->Lock();
  }

  void Unlock()
  {
    Mutex()->Unlock();
  }

  void AssertCurrentThreadOwns()
  {
#ifdef DEBUG
    Mutex()->AssertCurrentThreadOwns();
#endif
  }

private:
  OffTheBooksMutex* Mutex()
  {
    if (mMutex) {
      return mMutex;
    }

    OffTheBooksMutex* mutex = new OffTheBooksMutex("StaticMutex");
    if (!mMutex.compareExchange(nullptr, mutex)) {
      delete mutex;
    }

    return mMutex;
  }

  Atomic<OffTheBooksMutex*> mMutex;


  // Disallow copy constructor, but only in debug mode.  We only define
  // a default constructor in debug mode (see above); if we declared
  // this constructor always, the compiler wouldn't generate a trivial
  // default constructor for us in non-debug mode.
#ifdef DEBUG
  StaticMutex(StaticMutex& aOther);
#endif

  // Disallow these operators.
  StaticMutex& operator=(StaticMutex* aRhs);
  static void* operator new(size_t) CPP_THROW_NEW;
  static void operator delete(void*);
};

typedef BaseAutoLock<StaticMutex> StaticMutexAutoLock;
typedef BaseAutoUnlock<StaticMutex> StaticMutexAutoUnlock;

} // namespace mozilla

#endif
