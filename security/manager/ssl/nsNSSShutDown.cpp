/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSShutDown.h"

#include "mozilla/Casting.h"
#include "nsCOMPtr.h"

using namespace mozilla;

extern LazyLogModule gPIPNSSLog;

struct ObjectHashEntry : PLDHashEntryHdr {
  nsNSSShutDownObject *obj;
};

static bool
ObjectSetMatchEntry(const PLDHashEntryHdr *hdr, const void *key)
{
  const ObjectHashEntry *entry = static_cast<const ObjectHashEntry*>(hdr);
  return entry->obj == static_cast<const nsNSSShutDownObject*>(key);
}

static void
ObjectSetInitEntry(PLDHashEntryHdr *hdr, const void *key)
{
  ObjectHashEntry *entry = static_cast<ObjectHashEntry*>(hdr);
  entry->obj = const_cast<nsNSSShutDownObject*>(static_cast<const nsNSSShutDownObject*>(key));
}

static const PLDHashTableOps gSetOps = {
  PLDHashTable::HashVoidPtrKeyStub,
  ObjectSetMatchEntry,
  PLDHashTable::MoveEntryStub,
  PLDHashTable::ClearEntryStub,
  ObjectSetInitEntry
};

StaticMutex sListLock;
Atomic<bool> sInShutdown(false);
nsNSSShutDownList *singleton = nullptr;

nsNSSShutDownList::nsNSSShutDownList()
  : mObjects(&gSetOps, sizeof(ObjectHashEntry))
  , mPK11LogoutCancelObjects(&gSetOps, sizeof(ObjectHashEntry))
{
}

nsNSSShutDownList::~nsNSSShutDownList()
{
  PR_ASSERT(this == singleton);
  singleton = nullptr;
}

void nsNSSShutDownList::remember(nsNSSShutDownObject *o)
{
  StaticMutexAutoLock lock(sListLock);
  if (!nsNSSShutDownList::construct(lock)) {
    return;
  }

  PR_ASSERT(o);
  singleton->mObjects.Add(o, fallible);
}

void nsNSSShutDownList::forget(nsNSSShutDownObject *o)
{
  StaticMutexAutoLock lock(sListLock);
  if (!singleton) {
    return;
  }

  PR_ASSERT(o);
  singleton->mObjects.Remove(o);
}

void nsNSSShutDownList::remember(nsOnPK11LogoutCancelObject *o)
{
  StaticMutexAutoLock lock(sListLock);
  if (!nsNSSShutDownList::construct(lock)) {
    return;
  }

  PR_ASSERT(o);
  singleton->mPK11LogoutCancelObjects.Add(o, fallible);
}

void nsNSSShutDownList::forget(nsOnPK11LogoutCancelObject *o)
{
  StaticMutexAutoLock lock(sListLock);
  if (!singleton) {
    return;
  }

  PR_ASSERT(o);
  singleton->mPK11LogoutCancelObjects.Remove(o);
}

nsresult nsNSSShutDownList::doPK11Logout()
{
  StaticMutexAutoLock lock(sListLock);
  if (!singleton) {
    return NS_OK;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("canceling all open SSL sockets to disallow future IO\n"));

  // During our iteration we will set a bunch of PRBools to true.
  // Nobody else ever modifies that bool, only we do.
  // We only must ensure that our objects do not go away.
  // This is guaranteed by holding the list lock.

  for (auto iter = singleton->mPK11LogoutCancelObjects.Iter();
       !iter.Done();
       iter.Next()) {
    auto entry = static_cast<ObjectHashEntry*>(iter.Get());
    nsOnPK11LogoutCancelObject* pklco =
      BitwiseCast<nsOnPK11LogoutCancelObject*, nsNSSShutDownObject*>(entry->obj);
    if (pklco) {
      pklco->logout();
    }
  }

  return NS_OK;
}

nsresult nsNSSShutDownList::evaporateAllNSSResources()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  StaticMutexAutoLock lock(sListLock);
  // Other threads can acquire an nsNSSShutDownPreventionLock and cause this
  // thread to block when it calls restructActivityToCurrentThread, below. If
  // those other threads then attempt to create an object that must be
  // remembered by the shut down list, they will call
  // nsNSSShutDownList::remember, which attempts to acquire sListLock.
  // Consequently, holding sListLock while we're in
  // restrictActivityToCurrentThread would result in deadlock. sListLock
  // protects the singleton, so if we enforce that the singleton only be created
  // and destroyed on the main thread, and if we similarly enforce that this
  // function is only called on the main thread, what we can do is check that
  // the singleton hasn't already gone away and then we don't actually have to
  // hold sListLock while calling restrictActivityToCurrentThread.
  if (!singleton) {
    return NS_OK;
  }

  {
    StaticMutexAutoUnlock unlock(sListLock);
    PRStatus rv = singleton->mActivityState.restrictActivityToCurrentThread();
    if (rv != PR_SUCCESS) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("failed to restrict activity to current thread"));
      return NS_ERROR_FAILURE;
    }
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("now evaporating NSS resources"));

  // Never free more than one entry, because other threads might be calling
  // us and remove themselves while we are iterating over the list,
  // and the behaviour of changing the list while iterating is undefined.
  while (singleton) {
    auto iter = singleton->mObjects.Iter();
    if (iter.Done()) {
      break;
    }
    auto entry = static_cast<ObjectHashEntry*>(iter.Get());
    {
      StaticMutexAutoUnlock unlock(sListLock);
      entry->obj->shutdown(nsNSSShutDownObject::ShutdownCalledFrom::List);
    }
    iter.Remove();
  }

  if (!singleton) {
    return NS_ERROR_FAILURE;
  }

  singleton->mActivityState.releaseCurrentThreadActivityRestriction();
  return NS_OK;
}

void nsNSSShutDownList::enterActivityState()
{
  StaticMutexAutoLock lock(sListLock);
  if (nsNSSShutDownList::construct(lock)) {
    singleton->mActivityState.enter();
  }
}

void nsNSSShutDownList::leaveActivityState()
{
  StaticMutexAutoLock lock(sListLock);
  if (singleton) {
    singleton->mActivityState.leave();
  }
}

bool nsNSSShutDownList::construct(const StaticMutexAutoLock& /*proofOfLock*/)
{
  if (!singleton && !sInShutdown && XRE_IsParentProcess()) {
    singleton = new nsNSSShutDownList();
  }

  return !!singleton;
}

void nsNSSShutDownList::shutdown()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  StaticMutexAutoLock lock(sListLock);
  sInShutdown = true;

  if (singleton) {
    delete singleton;
  }
}

nsNSSActivityState::nsNSSActivityState()
:mNSSActivityStateLock("nsNSSActivityState.mNSSActivityStateLock"), 
 mNSSActivityChanged(mNSSActivityStateLock,
                     "nsNSSActivityState.mNSSActivityStateLock"),
 mNSSActivityCounter(0),
 mNSSRestrictedThread(nullptr)
{
}

nsNSSActivityState::~nsNSSActivityState()
{
}

void nsNSSActivityState::enter()
{
  MutexAutoLock lock(mNSSActivityStateLock);

  while (mNSSRestrictedThread && mNSSRestrictedThread != PR_GetCurrentThread()) {
    mNSSActivityChanged.Wait();
  }

  ++mNSSActivityCounter;
}

void nsNSSActivityState::leave()
{
  MutexAutoLock lock(mNSSActivityStateLock);

  --mNSSActivityCounter;

  mNSSActivityChanged.NotifyAll();
}

PRStatus nsNSSActivityState::restrictActivityToCurrentThread()
{
  MutexAutoLock lock(mNSSActivityStateLock);

  while (mNSSActivityCounter > 0) {
    mNSSActivityChanged.Wait(PR_TicksPerSecond());
  }

  mNSSRestrictedThread = PR_GetCurrentThread();

  return PR_SUCCESS;
}

void nsNSSActivityState::releaseCurrentThreadActivityRestriction()
{
  MutexAutoLock lock(mNSSActivityStateLock);

  mNSSRestrictedThread = nullptr;

  mNSSActivityChanged.NotifyAll();
}

nsNSSShutDownPreventionLock::nsNSSShutDownPreventionLock()
{
  nsNSSShutDownList::enterActivityState();
}

nsNSSShutDownPreventionLock::~nsNSSShutDownPreventionLock()
{
  nsNSSShutDownList::leaveActivityState();
}
