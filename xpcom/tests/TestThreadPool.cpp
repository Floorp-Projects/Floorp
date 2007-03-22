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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#include <stdio.h>
#include <stdlib.h>
#include "nsXPCOM.h"
#include "nsXPCOMCIDInternal.h"
#include "nsIThreadPool.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIRunnable.h"

class Task : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  Task(int i) : mIndex(i) {}

  NS_IMETHOD Run()
  {
    printf("###(%d) running from thread: %p\n", mIndex, (void *) PR_GetCurrentThread());
    int r = (int) ((float) rand() * 200 / RAND_MAX);
    PR_Sleep(PR_MillisecondsToInterval(r));
    printf("###(%d) exiting from thread: %p\n", mIndex, (void *) PR_GetCurrentThread());
    return NS_OK;
  }

private:
  int mIndex;
};
NS_IMPL_THREADSAFE_ISUPPORTS1(Task, nsIRunnable)

static nsresult
RunTests()
{
  nsCOMPtr<nsIThreadPool> pool = do_CreateInstance(NS_THREADPOOL_CONTRACTID);
  NS_ENSURE_STATE(pool);

  for (int i = 0; i < 100; ++i) {
    nsCOMPtr<nsIRunnable> task = new Task(i);
    NS_ENSURE_TRUE(task, NS_ERROR_OUT_OF_MEMORY);

    pool->Dispatch(task, NS_DISPATCH_NORMAL);
  }

  pool->Shutdown();
  return NS_OK;
}

int
main(int argc, char **argv)
{
  if (NS_FAILED(NS_InitXPCOM2(nsnull, nsnull, nsnull)))
    return -1;
  RunTests();
  NS_ShutdownXPCOM(nsnull);
  return 0;
}
