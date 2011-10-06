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
 *  Jason Duell <jduell.mcbugs@gmail.com>
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

#include "nsDeleteDir.h"
#include "nsIFile.h"
#include "nsString.h"
#include "prthread.h"
#include "mozilla/Telemetry.h"
#include "nsITimer.h"

using namespace mozilla;

static void DeleteDirThreadFunc(void *arg)
{
  Telemetry::AutoTimer<Telemetry::NETWORK_DISK_CACHE_DELETEDIR> timer;
  nsIFile *dir = static_cast<nsIFile *>(arg);
  dir->Remove(PR_TRUE);
  NS_RELEASE(dir);
}

static void CreateDeleterThread(nsITimer *aTimer, void *arg)
{
  nsIFile *dir = static_cast<nsIFile *>(arg);

  // create the worker thread
  PR_CreateThread(PR_USER_THREAD, DeleteDirThreadFunc, dir, PR_PRIORITY_LOW,
                  PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);
}

nsresult DeleteDir(nsIFile *dirIn, bool moveToTrash, bool sync,
                   PRUint32 delay)
{
  Telemetry::AutoTimer<Telemetry::NETWORK_DISK_CACHE_TRASHRENAME> timer;
  nsresult rv;
  nsCOMPtr<nsIFile> trash, dir;

  // Need to make a clone of this since we don't want to modify the input
  // file object.
  rv = dirIn->Clone(getter_AddRefs(dir));
  if (NS_FAILED(rv))
    return rv;

  if (moveToTrash) {
    rv = GetTrashDir(dir, &trash);
    if (NS_FAILED(rv))
      return rv;
    nsCAutoString leaf;
    rv = trash->GetNativeLeafName(leaf);
    if (NS_FAILED(rv))
      return rv;

    // Important: must rename directory w/o changing parent directory: else on
    // NTFS we'll wait (with cache lock) while nsIFile's ACL reset walks file
    // tree: was hanging GUI for *minutes* on large cache dirs.
    rv = dir->MoveToNative(nsnull, leaf);
    if (NS_FAILED(rv)) {
      nsresult rvMove = rv;
      // TrashDir may already exist (if we crashed while deleting it, etc.)
      // In that case current Cache dir should be small--just move it to
      // subdirectory of TrashDir and eat the NTFS ACL overhead.
      leaf.AppendInt(rand()); // support this happening multiple times
      rv = dir->MoveToNative(trash, leaf);
      if (NS_FAILED(rv))
        return rvMove;
      // Be paranoid and delete immediately if we're seeing old trash when
      // we're creating a new one
      delay = 0;
    }
  } else {
    // we want to pass a clone of the original off to the worker thread.
    trash.swap(dir);
  }

  // Steal ownership of trash directory; let the thread release it.
  nsIFile *trashRef = nsnull;
  trash.swap(trashRef);

  if (sync) {
    DeleteDirThreadFunc(trashRef);
  } else {
    if (delay) {
      nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1", &rv);
      if (NS_FAILED(rv))
        return NS_ERROR_UNEXPECTED;
      timer->InitWithFuncCallback(CreateDeleterThread, trashRef, delay,
                                  nsITimer::TYPE_ONE_SHOT);
    } else {
      CreateDeleterThread(nsnull, trashRef);
    }
  }

  return NS_OK;
}

nsresult GetTrashDir(nsIFile *target, nsCOMPtr<nsIFile> *result)
{
  nsresult rv = target->Clone(getter_AddRefs(*result));
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString leaf;
  rv = (*result)->GetNativeLeafName(leaf);
  if (NS_FAILED(rv))
    return rv;
  leaf.AppendLiteral(".Trash");

  return (*result)->SetNativeLeafName(leaf);
}
