/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSShutDown.h"
#include "nsCOMPtr.h"

using namespace mozilla;

extern PRLogModuleInfo* gPIPNSSLog;

struct ObjectHashEntry : PLDHashEntryHdr {
  nsNSSShutDownObject *obj;
};

static bool
ObjectSetMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                         const void *key)
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

nsNSSShutDownList *nsNSSShutDownList::singleton = nullptr;

nsNSSShutDownList::nsNSSShutDownList()
  : mListLock("nsNSSShutDownList.mListLock")
  , mObjects(&gSetOps, sizeof(ObjectHashEntry))
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
  if (!singleton)
    return;
  
  PR_ASSERT(o);
  MutexAutoLock lock(singleton->mListLock);
  singleton->mObjects.Add(o, fallible);
}

void nsNSSShutDownList::forget(nsNSSShutDownObject *o)
{
  if (!singleton)
    return;
  
  PR_ASSERT(o);
  MutexAutoLock lock(singleton->mListLock);
  singleton->mObjects.Remove(o);
}

void nsNSSShutDownList::remember(nsOnPK11LogoutCancelObject *o)
{
  if (!singleton)
    return;
  
  PR_ASSERT(o);
  MutexAutoLock lock(singleton->mListLock);
  singleton->mPK11LogoutCancelObjects.Add(o, fallible);
}

void nsNSSShutDownList::forget(nsOnPK11LogoutCancelObject *o)
{
  if (!singleton)
    return;
  
  PR_ASSERT(o);
  MutexAutoLock lock(singleton->mListLock);
  singleton->mPK11LogoutCancelObjects.Remove(o);
}

nsresult nsNSSShutDownList::doPK11Logout()
{
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("canceling all open SSL sockets to disallow future IO\n"));
  // During our iteration we will set a bunch of PRBools to true.
  // Nobody else ever modifies that bool, only we do.
  // We only must ensure that our objects do not go away.
  // This is guaranteed by holding the list lock.

  MutexAutoLock lock(singleton->mListLock);
  for (auto iter = mPK11LogoutCancelObjects.Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<ObjectHashEntry*>(iter.Get());
    nsOnPK11LogoutCancelObject *pklco =
      reinterpret_cast<nsOnPK11LogoutCancelObject*>(entry->obj);
    if (pklco) {
      pklco->logout();
    }
  }

  return NS_OK;
}

nsresult nsNSSShutDownList::evaporateAllNSSResources()
{
  if (PR_SUCCESS != mActivityState.restrictActivityToCurrentThread()) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("failed to restrict activity to current thread\n"));
    return NS_ERROR_FAILURE;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("now evaporating NSS resources\n"));

  // Never free more than one entry, because other threads might be calling
  // us and remove themselves while we are iterating over the list,
  // and the behaviour of changing the list while iterating is undefined.
  while (true) {
    MutexAutoLock lock(mListLock);
    auto iter = mObjects.Iter();
    if (iter.Done()) {
      break;
    }
    auto entry = static_cast<ObjectHashEntry*>(iter.Get());
    {
      MutexAutoUnlock unlock(singleton->mListLock);
      entry->obj->shutdown(nsNSSShutDownObject::calledFromList);
    }
    iter.Remove();
  }

  mActivityState.releaseCurrentThreadActivityRestriction();
  return NS_OK;
}

nsNSSShutDownList *nsNSSShutDownList::construct()
{
  if (singleton) {
    // we should never ever be called twice
    return nullptr;
  }

  singleton = new nsNSSShutDownList();
  return singleton;
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
  nsNSSActivityState *state = nsNSSShutDownList::getActivityState();
  if (!state)
    return;

  state->enter();
}

nsNSSShutDownPreventionLock::~nsNSSShutDownPreventionLock()
{
  nsNSSActivityState *state = nsNSSShutDownList::getActivityState();
  if (!state)
    return;
  
  state->leave();
}
