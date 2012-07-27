/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeleteDir.h"
#include "nsIFile.h"
#include "nsString.h"
#include "mozilla/Telemetry.h"
#include "nsITimer.h"
#include "nsISimpleEnumerator.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsISupportsPriority.h"
#include <time.h>

using namespace mozilla;

class nsBlockOnBackgroundThreadEvent : public nsRunnable {
public:
  nsBlockOnBackgroundThreadEvent() {}
  NS_IMETHOD Run()
  {
    MutexAutoLock lock(nsDeleteDir::gInstance->mLock);
    nsDeleteDir::gInstance->mCondVar.Notify();
    return NS_OK;
  }
};

class nsDestroyThreadEvent : public nsRunnable {
public:
  nsDestroyThreadEvent(nsIThread *thread)
    : mThread(thread)
  {}
  NS_IMETHOD Run()
  {
    mThread->Shutdown();
    return NS_OK;
  }
private:
  nsCOMPtr<nsIThread> mThread;
};


nsDeleteDir * nsDeleteDir::gInstance = nsnull;

nsDeleteDir::nsDeleteDir()
  : mLock("nsDeleteDir.mLock"),
    mCondVar(mLock, "nsDeleteDir.mCondVar"),
    mShutdownPending(false),
    mStopDeleting(false)
{
  NS_ASSERTION(gInstance==nsnull, "multiple nsCacheService instances!");
}

nsDeleteDir::~nsDeleteDir()
{
  gInstance = nsnull;
}

nsresult
nsDeleteDir::Init()
{
  if (gInstance)
    return NS_ERROR_ALREADY_INITIALIZED;

  gInstance = new nsDeleteDir();
  return NS_OK;
}

nsresult
nsDeleteDir::Shutdown(bool finishDeleting)
{
  if (!gInstance)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMArray<nsIFile> dirsToRemove;
  nsCOMPtr<nsIThread> thread;
  {
    MutexAutoLock lock(gInstance->mLock);
    NS_ASSERTION(!gInstance->mShutdownPending,
                 "Unexpected state in nsDeleteDir::Shutdown()");
    gInstance->mShutdownPending = true;

    if (!finishDeleting)
      gInstance->mStopDeleting = true;

    // remove all pending timers
    for (PRInt32 i = gInstance->mTimers.Count(); i > 0; i--) {
      nsCOMPtr<nsITimer> timer = gInstance->mTimers[i-1];
      gInstance->mTimers.RemoveObjectAt(i-1);
      timer->Cancel();

      nsCOMArray<nsIFile> *arg;
      timer->GetClosure((reinterpret_cast<void**>(&arg)));

      if (finishDeleting)
        dirsToRemove.AppendObjects(*arg);

      // delete argument passed to the timer
      delete arg;
    }

    thread.swap(gInstance->mThread);
    if (thread) {
      // dispatch event and wait for it to run and notify us, so we know thread
      // has completed all work and can be shutdown
      nsCOMPtr<nsIRunnable> event = new nsBlockOnBackgroundThreadEvent();
      nsresult rv = thread->Dispatch(event, NS_DISPATCH_NORMAL);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed dispatching block-event");
        return NS_ERROR_UNEXPECTED;
      }

      rv = gInstance->mCondVar.Wait();
      thread->Shutdown();
    }
  }

  delete gInstance;

  for (PRInt32 i = 0; i < dirsToRemove.Count(); i++)
    dirsToRemove[i]->Remove(true);

  return NS_OK;
}

nsresult
nsDeleteDir::InitThread()
{
  if (mThread)
    return NS_OK;

  nsresult rv = NS_NewNamedThread("Cache Deleter", getter_AddRefs(mThread));
  if (NS_FAILED(rv)) {
    NS_WARNING("Can't create background thread");
    return rv;
  }

  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(mThread);
  if (p) {
    p->SetPriority(nsISupportsPriority::PRIORITY_LOWEST);
  }
  return NS_OK;
}

void
nsDeleteDir::DestroyThread()
{
  if (!mThread)
    return;

  if (mTimers.Count())
    // more work to do, so don't delete thread.
    return;

  NS_DispatchToMainThread(new nsDestroyThreadEvent(mThread));
  mThread = nsnull;
}

void
nsDeleteDir::TimerCallback(nsITimer *aTimer, void *arg)
{
  Telemetry::AutoTimer<Telemetry::NETWORK_DISK_CACHE_DELETEDIR> timer;
  {
    MutexAutoLock lock(gInstance->mLock);

    PRInt32 idx = gInstance->mTimers.IndexOf(aTimer);
    if (idx == -1) {
      // Timer was canceled and removed during shutdown.
      return;
    }

    gInstance->mTimers.RemoveObjectAt(idx);
  }

  nsAutoPtr<nsCOMArray<nsIFile> > dirList;
  dirList = static_cast<nsCOMArray<nsIFile> *>(arg);

  bool shuttingDown = false;

  // Intentional extra braces to control variable sope.
  {
    // Low IO priority can only be set when running in the context of the
    // current thread.  So this shouldn't be moved to where we set the priority
    // of the Cache deleter thread using the nsThread's NSPR priority constants.
    nsAutoLowPriorityIO autoLowPriority;
    for (PRInt32 i = 0; i < dirList->Count() && !shuttingDown; i++) {
      gInstance->RemoveDir((*dirList)[i], &shuttingDown);
    }
  }

  {
    MutexAutoLock lock(gInstance->mLock);
    gInstance->DestroyThread();
  }
}

nsresult
nsDeleteDir::DeleteDir(nsIFile *dirIn, bool moveToTrash, PRUint32 delay)
{
  Telemetry::AutoTimer<Telemetry::NETWORK_DISK_CACHE_TRASHRENAME> timer;

  if (!gInstance)
    return NS_ERROR_NOT_INITIALIZED;

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
    nsCAutoString origLeaf;
    rv = trash->GetNativeLeafName(origLeaf);
    if (NS_FAILED(rv))
      return rv;

    // Append random number to the trash directory and check if it exists.
    srand(PR_Now());
    nsCAutoString leaf;
    for (PRInt32 i = 0; i < 10; i++) {
      leaf = origLeaf;
      leaf.AppendInt(rand());
      rv = trash->SetNativeLeafName(leaf);
      if (NS_FAILED(rv))
        return rv;

      bool exists;
      if (NS_SUCCEEDED(trash->Exists(&exists)) && !exists) {
        break;
      }

      leaf.Truncate();
    }

    // Fail if we didn't find unused trash directory within the limit
    if (!leaf.Length())
      return NS_ERROR_FAILURE;

#if defined(MOZ_WIDGET_ANDROID)
    nsCOMPtr<nsIFile> parent;
    rv = trash->GetParent(getter_AddRefs(parent));
    if (NS_FAILED(rv))
      return rv;
    rv = dir->MoveToNative(parent, leaf);
#else
    // Important: must rename directory w/o changing parent directory: else on
    // NTFS we'll wait (with cache lock) while nsIFile's ACL reset walks file
    // tree: was hanging GUI for *minutes* on large cache dirs.
    rv = dir->MoveToNative(nsnull, leaf);
#endif
    if (NS_FAILED(rv))
      return rv;
  } else {
    // we want to pass a clone of the original off to the worker thread.
    trash.swap(dir);
  }

  nsAutoPtr<nsCOMArray<nsIFile> > arg(new nsCOMArray<nsIFile>);
  arg->AppendObject(trash);

  rv = gInstance->PostTimer(arg, delay);
  if (NS_FAILED(rv))
    return rv;

  arg.forget();
  return NS_OK;
}

nsresult
nsDeleteDir::GetTrashDir(nsIFile *target, nsCOMPtr<nsIFile> *result)
{
  nsresult rv;
#if defined(MOZ_WIDGET_ANDROID)
  // Try to use the app cache folder for cache trash on Android
  char* cachePath = getenv("CACHE_DIRECTORY");
  if (cachePath) {
    rv = NS_NewNativeLocalFile(nsDependentCString(cachePath),
                               true, getter_AddRefs(*result));
    if (NS_FAILED(rv))
      return rv;

    // Add a sub folder with the cache folder name
    nsCAutoString leaf;
    rv = target->GetNativeLeafName(leaf);
    (*result)->AppendNative(leaf);
  } else
#endif
  {
    rv = target->Clone(getter_AddRefs(*result));
  }
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString leaf;
  rv = (*result)->GetNativeLeafName(leaf);
  if (NS_FAILED(rv))
    return rv;
  leaf.AppendLiteral(".Trash");

  return (*result)->SetNativeLeafName(leaf);
}

nsresult
nsDeleteDir::RemoveOldTrashes(nsIFile *cacheDir)
{
  if (!gInstance)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv;

  static bool firstRun = true;

  if (!firstRun)
    return NS_OK;

  firstRun = false;

  nsCOMPtr<nsIFile> trash;
  rv = GetTrashDir(cacheDir, &trash);
  if (NS_FAILED(rv))
    return rv;

  nsAutoString trashName;
  rv = trash->GetLeafName(trashName);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFile> parent;
#if defined(MOZ_WIDGET_ANDROID)
  rv = trash->GetParent(getter_AddRefs(parent));
#else
  rv = cacheDir->GetParent(getter_AddRefs(parent));
#endif
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISimpleEnumerator> iter;
  rv = parent->GetDirectoryEntries(getter_AddRefs(iter));
  if (NS_FAILED(rv))
    return rv;

  bool more;
  nsCOMPtr<nsISupports> elem;
  nsAutoPtr<nsCOMArray<nsIFile> > dirList;

  while (NS_SUCCEEDED(iter->HasMoreElements(&more)) && more) {
    rv = iter->GetNext(getter_AddRefs(elem));
    if (NS_FAILED(rv))
      continue;

    nsCOMPtr<nsIFile> file = do_QueryInterface(elem);
    if (!file)
      continue;

    nsAutoString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_FAILED(rv))
      continue;

    // match all names that begin with the trash name (i.e. "Cache.Trash")
    if (Substring(leafName, 0, trashName.Length()).Equals(trashName)) {
      if (!dirList)
        dirList = new nsCOMArray<nsIFile>;
      dirList->AppendObject(file);
    }
  }

  if (dirList) {
    rv = gInstance->PostTimer(dirList, 90000);
    if (NS_FAILED(rv))
      return rv;

    dirList.forget();
  }

  return NS_OK;
}

nsresult
nsDeleteDir::PostTimer(void *arg, PRUint32 delay)
{
  nsresult rv;

  nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_FAILED(rv))
    return NS_ERROR_UNEXPECTED;

  MutexAutoLock lock(mLock);

  rv = InitThread();
  if (NS_FAILED(rv))
    return rv;

  rv = timer->SetTarget(mThread);
  if (NS_FAILED(rv))
    return rv;

  rv = timer->InitWithFuncCallback(TimerCallback, arg, delay,
                                   nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv))
    return rv;

  mTimers.AppendObject(timer);
  return NS_OK;
}

nsresult
nsDeleteDir::RemoveDir(nsIFile *file, bool *stopDeleting)
{
  nsresult rv;
  bool isLink;

  rv = file->IsSymlink(&isLink);
  if (NS_FAILED(rv) || isLink)
    return NS_ERROR_UNEXPECTED;

  bool isDir;
  rv = file->IsDirectory(&isDir);
  if (NS_FAILED(rv))
    return rv;

  if (isDir) {
    nsCOMPtr<nsISimpleEnumerator> iter;
    rv = file->GetDirectoryEntries(getter_AddRefs(iter));
    if (NS_FAILED(rv))
      return rv;

    bool more;
    nsCOMPtr<nsISupports> elem;
    while (NS_SUCCEEDED(iter->HasMoreElements(&more)) && more) {
      rv = iter->GetNext(getter_AddRefs(elem));
      if (NS_FAILED(rv)) {
        NS_WARNING("Unexpected failure in nsDeleteDir::RemoveDir");
        continue;
      }

      nsCOMPtr<nsIFile> file2 = do_QueryInterface(elem);
      if (!file2) {
        NS_WARNING("Unexpected failure in nsDeleteDir::RemoveDir");
        continue;
      }

      RemoveDir(file2, stopDeleting);
      // No check for errors to remove as much as possible

      if (*stopDeleting)
        return NS_OK;
    }
  }

  file->Remove(false);
  // No check for errors to remove as much as possible

  MutexAutoLock lock(mLock);
  if (mStopDeleting)
    *stopDeleting = true;

  return NS_OK;
}
