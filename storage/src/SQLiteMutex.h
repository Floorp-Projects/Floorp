/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#ifndef DEBUG
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
class NS_STACK_CLASS SQLiteMutexAutoLock
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
class NS_STACK_CLASS SQLiteMutexAutoUnlock
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
