/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
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

/**
 * This code is heavily based on the sample at:
 *   http://www.sqlite.org/unlock_notify.html
 */

#include "SharedCacheUnlockNotify.h"

#include "mozilla/Mutex.h"
#include "mozilla/CondVar.h"
#include "nsThreadUtils.h"

namespace {

class UnlockNotification
{
public:
  UnlockNotification()
  : mMutex("UnlockNotification mMutex"),
    mCondVar(mMutex, "UnlockNotification condVar"),
    mSignaled(false)
  { }

  void Wait()
  {
    mozilla::MutexAutoLock lock(mMutex);
    while (!mSignaled) {
      mCondVar.Wait();
    }
  }

  void Signal()
  {
    mozilla::MutexAutoLock lock(mMutex);
    mSignaled = true;
    mCondVar.Notify();
  }

private:
  mozilla::Mutex mMutex;
  mozilla::CondVar mCondVar;
  bool mSignaled;
};

void
UnlockNotifyCallback(void** apArg,
                     int nArg)
{
  for (int i = 0; i < nArg; i++) {
    UnlockNotification* notification =
      static_cast<UnlockNotification*>(apArg[i]);
    notification->Signal();
  }
}

int
WaitForUnlockNotify(sqlite3* db)
{
  UnlockNotification notification;
  int srv = ::sqlite3_unlock_notify(db, UnlockNotifyCallback, &notification);
  NS_ASSERTION(srv == SQLITE_LOCKED || srv == SQLITE_OK, "Bad result!");

  if (srv == SQLITE_OK) {
    notification.Wait();
  }

  return srv;
}

} // anonymous namespace

namespace mozilla {
namespace storage {

int
moz_sqlite3_step(sqlite3_stmt* pStmt)
{
  bool checkedMainThread = false;

  int srv;
  while ((srv = ::sqlite3_step(pStmt)) == SQLITE_LOCKED) {
    if (!checkedMainThread) {
      checkedMainThread = true;
      if (NS_IsMainThread()) {
        NS_WARNING("We won't allow blocking on the main thread!");
        break;
      }
    }

    srv = WaitForUnlockNotify(sqlite3_db_handle(pStmt));
    if (srv != SQLITE_OK)
      break;

    sqlite3_reset(pStmt);
  }

  return srv;
}

int
moz_sqlite3_prepare_v2(sqlite3* db,
                       const char* zSql,
                       int nByte,
                       sqlite3_stmt** ppStmt,
                       const char** pzTail)
{
  bool checkedMainThread = false;

  int srv;
  while((srv = ::sqlite3_prepare_v2(db, zSql, nByte, ppStmt, pzTail)) ==
        SQLITE_LOCKED) {
    if (!checkedMainThread) {
      checkedMainThread = true;
      if (NS_IsMainThread()) {
        NS_WARNING("We won't allow blocking on the main thread!");
        break;
      }
    }

    srv = WaitForUnlockNotify(db);
    if (srv != SQLITE_OK)
      break;
  }

  return srv;
}

} /* namespace storage */
} /* namespace mozilla */
