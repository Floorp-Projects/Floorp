/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Michal Novotny <michal.novotny@gmail.com>
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
  static nsresult DeleteDir(nsIFile *dir, bool moveToTrash, PRUint32 delay = 0);

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
  nsresult PostTimer(void *arg, PRUint32 delay);
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
