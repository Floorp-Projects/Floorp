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
 * The Initial Developer of the Original Code is
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kengert@redhat.com>
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

#include "nsPSMBackgroundThread.h"
#include "nsThreadUtils.h"

using namespace mozilla;

void PR_CALLBACK nsPSMBackgroundThread::nsThreadRunner(void *arg)
{
  nsPSMBackgroundThread *self = static_cast<nsPSMBackgroundThread *>(arg);
  self->Run();
}

nsPSMBackgroundThread::nsPSMBackgroundThread()
: mThreadHandle(nsnull),
  mMutex("nsPSMBackgroundThread.mMutex"),
  mCond(mMutex, "nsPSMBackgroundThread.mCond"),
  mExitState(ePSMThreadRunning)
{
}

nsresult nsPSMBackgroundThread::startThread()
{
  mThreadHandle = PR_CreateThread(PR_USER_THREAD, nsThreadRunner, static_cast<void*>(this), 
    PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);

  NS_ASSERTION(mThreadHandle, "Could not create nsPSMBackgroundThread\n");
  
  if (!mThreadHandle)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

nsPSMBackgroundThread::~nsPSMBackgroundThread()
{
}

bool
nsPSMBackgroundThread::exitRequested(const MutexAutoLock & /*proofOfLock*/) const
{
  return exitRequestedNoLock();
}

nsresult
nsPSMBackgroundThread::postStoppedEventToMainThread(
    MutexAutoLock const & /*proofOfLock*/)
{
  NS_ASSERTION(PR_GetCurrentThread() == mThreadHandle,
               "Background thread stopped from another thread");

  mExitState = ePSMThreadStopped;
  // requestExit is waiting for an event, so give it one.
  return NS_DispatchToMainThread(new nsRunnable());
}

void nsPSMBackgroundThread::requestExit()
{
  NS_ASSERTION(NS_IsMainThread(),
               "nsPSMBackgroundThread::requestExit called off main thread.");

  if (!mThreadHandle)
    return;

  {
    MutexAutoLock threadLock(mMutex);
    if (mExitState < ePSMThreadStopRequested) {
      mExitState = ePSMThreadStopRequested;
      mCond.NotifyAll();
    }
  }
  
  nsCOMPtr<nsIThread> mainThread = do_GetCurrentThread();
  for (;;) {
    {
      MutexAutoLock threadLock(mMutex);
      if (mExitState == ePSMThreadStopped)
        break;
    }
    NS_ProcessPendingEvents(mainThread, PR_MillisecondsToInterval(50));
  }

  PR_JoinThread(mThreadHandle);
  mThreadHandle = nsnull;
}
