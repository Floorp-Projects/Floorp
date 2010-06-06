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

static void DeleteDirThreadFunc(void *arg)
{
  nsIFile *dir = static_cast<nsIFile *>(arg);
  dir->Remove(PR_TRUE);
  NS_RELEASE(dir);
}

nsresult DeleteDir(nsIFile *dirIn, PRBool moveToTrash, PRBool sync)
{
  nsresult rv;
  nsCOMPtr<nsIFile> trash, dir;

  // Need to make a clone of this since we don't want to modify the input
  // file object.
  rv = dirIn->Clone(getter_AddRefs(dir));
  if (NS_FAILED(rv))
    return rv;

  if (moveToTrash)
  {
    rv = GetTrashDir(dir, &trash);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIFile> subDir;
    rv = trash->Clone(getter_AddRefs(subDir));
    if (NS_FAILED(rv))
      return rv;

    rv = subDir->AppendNative(NS_LITERAL_CSTRING("Trash"));
    if (NS_FAILED(rv))
      return rv;

    rv = subDir->CreateUnique(nsIFile::DIRECTORY_TYPE, 0700);
    if (NS_FAILED(rv))
      return rv;

    rv = dir->MoveToNative(subDir, EmptyCString());
    if (NS_FAILED(rv))
      return rv;
  }
  else
  {
    // we want to pass a clone of the original off to the worker thread.
    trash.swap(dir);
  }

  // Steal ownership of trash directory; let the thread release it.
  nsIFile *trashRef = nsnull;
  trash.swap(trashRef);

  if (sync)
  {
    DeleteDirThreadFunc(trashRef);
  }
  else
  {
    // now, invoke the worker thread
    PRThread *thread = PR_CreateThread(PR_USER_THREAD,
                                       DeleteDirThreadFunc,
                                       trashRef,
                                       PR_PRIORITY_LOW,
                                       PR_GLOBAL_THREAD,
                                       PR_UNJOINABLE_THREAD,
                                       0);
    if (!thread)
      return NS_ERROR_UNEXPECTED;
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
