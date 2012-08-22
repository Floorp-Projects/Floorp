/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeleteDir_h__
#define nsDeleteDir_h__

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "mozilla/Mutex.h"
#include "mozilla/CondVar.h"

class nsIFile;
class nsIThread;
class nsITimer;


class nsDeleteDir {
public:
  nsDeleteDir();
  ~nsDeleteDir();

  static nsresult Init();
  static nsresult Shutdown(bool finishDeleting);

  /**
   * This routine attempts to delete a directory that may contain some files
   * that are still in use. This latter point is only an issue on Windows and
   * a few other systems.
   *
   * If the moveToTrash parameter is true we first rename the given directory
   * "foo.Trash123" (where "foo" is the original directory name, and "123" is
   * a random number, in order to support multiple concurrent deletes). The
   * directory is then deleted, file-by-file, on a background thread.
   *
   * If the moveToTrash parameter is false, then the given directory is deleted
   * directly.
   *
   * If 'delay' is non-zero, the directory will not be deleted until the
   * specified number of milliseconds have passed. (The directory is still
   * renamed immediately if 'moveToTrash' is passed, so upon return it is safe
   * to create a directory with the same name).
   */
  static nsresult DeleteDir(nsIFile *dir, bool moveToTrash, uint32_t delay = 0);

  /**
   * Returns the trash directory corresponding to the given directory.
   */
  static nsresult GetTrashDir(nsIFile *dir, nsCOMPtr<nsIFile> *result);

  /**
   * Remove all trashes left from previous run. This function does nothing when
   * called second and more times.
   */
  static nsresult RemoveOldTrashes(nsIFile *cacheDir);

  static void TimerCallback(nsITimer *aTimer, void *arg);

private:
  friend class nsBlockOnBackgroundThreadEvent;
  friend class nsDestroyThreadEvent;

  nsresult InitThread();
  void     DestroyThread();
  nsresult PostTimer(void *arg, uint32_t delay);
  nsresult RemoveDir(nsIFile *file, bool *stopDeleting);

  static nsDeleteDir * gInstance;
  mozilla::Mutex       mLock;
  mozilla::CondVar     mCondVar;
  nsCOMArray<nsITimer> mTimers;
  nsCOMPtr<nsIThread>  mThread;
  bool                 mShutdownPending;
  bool                 mStopDeleting;
};

#endif  // nsDeleteDir_h__
