/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_storage_SQLiteMutex_h_
#define mozilla_storage_SQLiteMutex_h_

#include "mozilla/BlockingResourceBase.h"
#include "sqlite3.h"

namespace mozilla {
namespace storage {

/**
 * Wrapper class for sqlite3_mutexes.  To be used whenever we want to use a
 * sqlite3_mutex.
 *
 * @warning Never EVER wrap the same sqlite3_mutex with a different SQLiteMutex.
 *          If you do this, you void the deadlock detector's warranty!
 */
class SQLiteMutex : private BlockingResourceBase
{
public:
  /**
   * Constructs a wrapper for a sqlite3_mutex that has deadlock detecting.
   *
   * @param aName
   *        A name which can be used to reference this mutex.
   */
  SQLiteMutex(const char *aName)
  : BlockingResourceBase(aName, eMutex)
  , mMutex(NULL)
  {
  }

  /**
   * Sets the mutex that we are wrapping.  We generally do not have access to
   * our mutex at class construction, so we have to set it once we get access to
   * it.
   *
   * @param aMutex
   *        The sqlite3_mutex that we are going to wrap.
   */
  void initWithMutex(sqlite3_mutex *aMutex)
  {
    NS_ASSERTION(aMutex, "You must pass in a valid mutex!");
    NS_ASSERTION(!mMutex, "A mutex has already been set for this!");
    mMutex = aMutex;
  }

#if !defined(DEBUG) || defined(MOZ_NATIVE_SQLITE)
  /**
   * Acquires the mutex.
   */
  void lock()
  {
    sqlite3_mutex_enter(mMutex);
  }

  /**
   * Releases the mutex.
   */
  void unlock()
  {
    sqlite3_mutex_leave(mMutex);
  }

  /**
   * Asserts that the current thread owns the mutex.
   */
  void assertCurrentThreadOwns()
  {
  }

  /**
   * Asserts that the current thread does not own the mutex.
   */
  void assertNotCurrentThreadOwns()
  {
  }

#else
  void lock()
  {
    NS_ASSERTION(mMutex, "No mutex associated with this wrapper!");

    // While SQLite Mutexes may be recursive, in our own code we do not want to
    // treat them as such.
    CallStack callContext = CallStack();

    CheckAcquire(callContext);
    sqlite3_mutex_enter(mMutex);
    Acquire(callContext); // Call is protected by us holding the mutex.
  }

  void unlock()
  {
    NS_ASSERTION(mMutex, "No mutex associated with this wrapper!");

    // While SQLite Mutexes may be recursive, in our own code we do not want to
    // treat them as such.
    Release(); // Call is protected by us holding the mutex.
    sqlite3_mutex_leave(mMutex);
  }

  void assertCurrentThreadOwns()
  {
    NS_ASSERTION(mMutex, "No mutex associated with this wrapper!");
    NS_ASSERTION(sqlite3_mutex_held(mMutex),
                 "Mutex is not held, but we expect it to be!");
  }

  void assertNotCurrentThreadOwns()
  {
    NS_ASSERTION(mMutex, "No mutex associated with this wrapper!");
    NS_ASSERTION(sqlite3_mutex_notheld(mMutex),
                 "Mutex is held, but we expect it to not be!");
  }
#endif // ifndef DEBUG

private:
  sqlite3_mutex *mMutex;
};

/**
 * Automatically acquires the mutex when it enters scope, and releases it when
 * it leaves scope.
 */
class MOZ_STACK_CLASS SQLiteMutexAutoLock
{
public:
  SQLiteMutexAutoLock(SQLiteMutex &aMutex)
  : mMutex(aMutex)
  {
    mMutex.lock();
  }

  ~SQLiteMutexAutoLock()
  {
    mMutex.unlock();
  }

private:
  SQLiteMutex &mMutex;
};

/**
 * Automatically releases the mutex when it enters scope, and acquires it when
 * it leaves scope.
 */
class MOZ_STACK_CLASS SQLiteMutexAutoUnlock
{
public:
  SQLiteMutexAutoUnlock(SQLiteMutex &aMutex)
  : mMutex(aMutex)
  {
    mMutex.unlock();
  }

  ~SQLiteMutexAutoUnlock()
  {
    mMutex.lock();
  }

private:
  SQLiteMutex &mMutex;
};

} // namespace storage
} // namespace mozilla

#endif // mozilla_storage_SQLiteMutex_h_
