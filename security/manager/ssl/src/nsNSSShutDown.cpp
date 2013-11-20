/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSShutDown.h"
#include "nsCOMPtr.h"

using namespace mozilla;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

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

static bool
ObjectSetInitEntry(PLDHashTable *table, PLDHashEntryHdr *hdr,
                     const void *key)
{
  ObjectHashEntry *entry = static_cast<ObjectHashEntry*>(hdr);
  entry->obj = const_cast<nsNSSShutDownObject*>(static_cast<const nsNSSShutDownObject*>(key));
  return true;
}

static const PLDHashTableOps gSetOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashVoidPtrKeyStub,
  ObjectSetMatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  ObjectSetInitEntry
};

nsNSSShutDownList *nsNSSShutDownList::singleton = nullptr;

nsNSSShutDownList::nsNSSShutDownList()
:mListLock("nsNSSShutDownList.mListLock")
{
  mActiveSSLSockets = 0;
  mPK11LogoutCancelObjects.ops = nullptr;
  mObjects.ops = nullptr;
  PL_DHashTableInit(&mObjects, &gSetOps, nullptr,
                    sizeof(ObjectHashEntry), 16);
  PL_DHashTableInit(&mPK11LogoutCancelObjects, &gSetOps, nullptr,
                    sizeof(ObjectHashEntry), 16);
}

nsNSSShutDownList::~nsNSSShutDownList()
{
  if (mObjects.ops) {
    PL_DHashTableFinish(&mObjects);
    mObjects.ops = nullptr;
  }
  if (mPK11LogoutCancelObjects.ops) {
    PL_DHashTableFinish(&mPK11LogoutCancelObjects);
    mPK11LogoutCancelObjects.ops = nullptr;
  }
  PR_ASSERT(this == singleton);
  singleton = nullptr;
}

void nsNSSShutDownList::remember(nsNSSShutDownObject *o)
{
  if (!singleton)
    return;
  
  PR_ASSERT(o);
  MutexAutoLock lock(singleton->mListLock);
    PL_DHashTableOperate(&singleton->mObjects, o, PL_DHASH_ADD);
}

void nsNSSShutDownList::forget(nsNSSShutDownObject *o)
{
  if (!singleton)
    return;
  
  PR_ASSERT(o);
  MutexAutoLock lock(singleton->mListLock);
  PL_DHashTableOperate(&singleton->mObjects, o, PL_DHASH_REMOVE);
}

void nsNSSShutDownList::remember(nsOnPK11LogoutCancelObject *o)
{
  if (!singleton)
    return;
  
  PR_ASSERT(o);
  MutexAutoLock lock(singleton->mListLock);
  PL_DHashTableOperate(&singleton->mPK11LogoutCancelObjects, o, PL_DHASH_ADD);
}

void nsNSSShutDownList::forget(nsOnPK11LogoutCancelObject *o)
{
  if (!singleton)
    return;
  
  PR_ASSERT(o);
  MutexAutoLock lock(singleton->mListLock);
  PL_DHashTableOperate(&singleton->mPK11LogoutCancelObjects, o, PL_DHASH_REMOVE);
}

void nsNSSShutDownList::trackSSLSocketCreate()
{
  if (!singleton)
    return;
  
  MutexAutoLock lock(singleton->mListLock);
  ++singleton->mActiveSSLSockets;
}

void nsNSSShutDownList::trackSSLSocketClose()
{
  if (!singleton)
    return;
  
  MutexAutoLock lock(singleton->mListLock);
  --singleton->mActiveSSLSockets;
}
  
bool nsNSSShutDownList::areSSLSocketsActive()
{
  if (!singleton) {
    // I'd rather prefer to be pessimistic and return true.
    // However, maybe we will get called at a time when the singleton
    // has already been freed, and returning true would bring up an 
    // unnecessary warning.
    return false;
  }
  
  MutexAutoLock lock(singleton->mListLock);
  return (singleton->mActiveSSLSockets > 0);
}

nsresult nsNSSShutDownList::doPK11Logout()
{
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("canceling all open SSL sockets to disallow future IO\n"));
  // During our iteration we will set a bunch of PRBools to true.
  // Nobody else ever modifies that bool, only we do.
  // We only must ensure that our objects do not go away.
  // This is guaranteed by holding the list lock.

  MutexAutoLock lock(singleton->mListLock);
  PL_DHashTableEnumerate(&mPK11LogoutCancelObjects, doPK11LogoutHelper, 0);

  return NS_OK;
}

PLDHashOperator
nsNSSShutDownList::doPK11LogoutHelper(PLDHashTable *table, 
  PLDHashEntryHdr *hdr, uint32_t number, void *arg)
{
  ObjectHashEntry *entry = static_cast<ObjectHashEntry*>(hdr);

  nsOnPK11LogoutCancelObject *pklco = 
    reinterpret_cast<nsOnPK11LogoutCancelObject*>(entry->obj);

  if (pklco) {
    pklco->logout();
  }

  return PL_DHASH_NEXT;
}

bool nsNSSShutDownList::isUIActive()
{
  bool canDisallow = mActivityState.ifPossibleDisallowUI(nsNSSActivityState::test_only);
  bool bIsUIActive = !canDisallow;
  return bIsUIActive;
}

bool nsNSSShutDownList::ifPossibleDisallowUI()
{
  bool isNowDisallowed = mActivityState.ifPossibleDisallowUI(nsNSSActivityState::do_it_for_real);
  return isNowDisallowed;
}

void nsNSSShutDownList::allowUI()
{
  mActivityState.allowUI();
}

nsresult nsNSSShutDownList::evaporateAllNSSResources()
{
  if (PR_SUCCESS != mActivityState.restrictActivityToCurrentThread()) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("failed to restrict activity to current thread\n"));
    return NS_ERROR_FAILURE;
  }

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("now evaporating NSS resources\n"));
  int removedCount;
  do {
    MutexAutoLock lock(mListLock);
    removedCount = PL_DHashTableEnumerate(&mObjects, evaporateAllNSSResourcesHelper, 0);
  } while (removedCount > 0);

  mActivityState.releaseCurrentThreadActivityRestriction();
  return NS_OK;
}

PLDHashOperator
nsNSSShutDownList::evaporateAllNSSResourcesHelper(PLDHashTable *table, 
  PLDHashEntryHdr *hdr, uint32_t number, void *arg)
{
  ObjectHashEntry *entry = static_cast<ObjectHashEntry*>(hdr);
  {
    MutexAutoUnlock unlock(singleton->mListLock);
    entry->obj->shutdown(nsNSSShutDownObject::calledFromList);
  }
  // Never free more than one entry, because other threads might be calling
  // us and remove themselves while we are iterating over the list,
  // and the behaviour of changing the list while iterating is undefined.
  return (PLDHashOperator)(PL_DHASH_STOP | PL_DHASH_REMOVE);
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
 mBlockingUICounter(0),
 mIsUIForbidden(false),
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

void nsNSSActivityState::enterBlockingUIState()
{
  MutexAutoLock lock(mNSSActivityStateLock);

  ++mBlockingUICounter;
}

void nsNSSActivityState::leaveBlockingUIState()
{
  MutexAutoLock lock(mNSSActivityStateLock);

  --mBlockingUICounter;
}

bool nsNSSActivityState::isBlockingUIActive()
{
  MutexAutoLock lock(mNSSActivityStateLock);
  return (mBlockingUICounter > 0);
}

bool nsNSSActivityState::isUIForbidden()
{
  MutexAutoLock lock(mNSSActivityStateLock);
  return mIsUIForbidden;
}

bool nsNSSActivityState::ifPossibleDisallowUI(RealOrTesting rot)
{
  bool retval = false;
  MutexAutoLock lock(mNSSActivityStateLock);

  // Checking and disallowing the UI must be done atomically.

  if (!mBlockingUICounter) {
    // No UI is currently shown, we are able to evaporate.
    retval = true;
    if (rot == do_it_for_real) {
      // Remember to disallow UI.
      mIsUIForbidden = true;
        
      // to clear the "forbidden" state,
      // one must either call 
      // restrictActivityToCurrentThread() + releaseCurrentThreadActivityRestriction()
      // or cancel the operation by calling
      // unprepareCurrentThreadRestriction()
    }
  }
  return retval;
}

void nsNSSActivityState::allowUI()
{
  MutexAutoLock lock(mNSSActivityStateLock);

  mIsUIForbidden = false;
}

PRStatus nsNSSActivityState::restrictActivityToCurrentThread()
{
  PRStatus retval = PR_FAILURE;
  MutexAutoLock lock(mNSSActivityStateLock);
  
  if (!mBlockingUICounter) {
    while (0 < mNSSActivityCounter && !mBlockingUICounter) {
      mNSSActivityChanged.Wait(PR_TicksPerSecond());
    }
      
    if (mBlockingUICounter) {
      // This should never happen.
      // If we arrive here, our logic is broken.
      PR_ASSERT(0);
    }
    else {
      mNSSRestrictedThread = PR_GetCurrentThread();
      retval = PR_SUCCESS;
    }
  }

  return retval;
}

void nsNSSActivityState::releaseCurrentThreadActivityRestriction()
{
  MutexAutoLock lock(mNSSActivityStateLock);

  mNSSRestrictedThread = nullptr;
  mIsUIForbidden = false;

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

nsPSMUITracker::nsPSMUITracker()
{
  nsNSSActivityState *state = nsNSSShutDownList::getActivityState();
  if (!state)
    return;
  
  state->enterBlockingUIState();
}

nsPSMUITracker::~nsPSMUITracker()
{
  nsNSSActivityState *state = nsNSSShutDownList::getActivityState();
  if (!state)
    return;
  
  state->leaveBlockingUIState();
}

bool nsPSMUITracker::isUIForbidden()
{
  nsNSSActivityState *state = nsNSSShutDownList::getActivityState();
  if (!state)
    return false;

  return state->isUIForbidden();
}
