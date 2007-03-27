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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kaie@netscape.com>
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

#include "nsNSSShutDown.h"
#include "nsCOMPtr.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

struct ObjectHashEntry : PLDHashEntryHdr {
  nsNSSShutDownObject *obj;
};

PR_STATIC_CALLBACK(PRBool)
ObjectSetMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                         const void *key)
{
  const ObjectHashEntry *entry = NS_STATIC_CAST(const ObjectHashEntry*, hdr);
  return entry->obj == NS_STATIC_CAST(const nsNSSShutDownObject*, key);
}

PR_STATIC_CALLBACK(PRBool)
ObjectSetInitEntry(PLDHashTable *table, PLDHashEntryHdr *hdr,
                     const void *key)
{
  ObjectHashEntry *entry = NS_STATIC_CAST(ObjectHashEntry*, hdr);
  entry->obj = NS_CONST_CAST(nsNSSShutDownObject*, NS_STATIC_CAST(const nsNSSShutDownObject*, key));
  return PR_TRUE;
}

static PLDHashTableOps gSetOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashVoidPtrKeyStub,
  ObjectSetMatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  ObjectSetInitEntry
};

nsNSSShutDownList *nsNSSShutDownList::singleton = nsnull;

nsNSSShutDownList::nsNSSShutDownList()
{
  mListLock = PR_NewLock();
  mActiveSSLSockets = 0;
  mPK11LogoutCancelObjects.ops = nsnull;
  mObjects.ops = nsnull;
  PL_DHashTableInit(&mObjects, &gSetOps, nsnull,
                    sizeof(ObjectHashEntry), 16);
  PL_DHashTableInit(&mPK11LogoutCancelObjects, &gSetOps, nsnull,
                    sizeof(ObjectHashEntry), 16);
}

nsNSSShutDownList::~nsNSSShutDownList()
{
  if (mListLock) {
    PR_DestroyLock(mListLock);
    mListLock = nsnull;
  }
  if (mObjects.ops) {
    PL_DHashTableFinish(&mObjects);
    mObjects.ops = nsnull;
  }
  if (mPK11LogoutCancelObjects.ops) {
    PL_DHashTableFinish(&mPK11LogoutCancelObjects);
    mPK11LogoutCancelObjects.ops = nsnull;
  }
  PR_ASSERT(this == singleton);
  singleton = nsnull;
}

void nsNSSShutDownList::remember(nsNSSShutDownObject *o)
{
  if (!singleton)
    return;
  
  PR_ASSERT(o);
  PR_Lock(singleton->mListLock);
    PL_DHashTableOperate(&singleton->mObjects, o, PL_DHASH_ADD);
  PR_Unlock(singleton->mListLock);
}

void nsNSSShutDownList::forget(nsNSSShutDownObject *o)
{
  if (!singleton)
    return;
  
  PR_ASSERT(o);
  PR_Lock(singleton->mListLock);
    PL_DHashTableOperate(&singleton->mObjects, o, PL_DHASH_REMOVE);
  PR_Unlock(singleton->mListLock);
}

void nsNSSShutDownList::remember(nsOnPK11LogoutCancelObject *o)
{
  if (!singleton)
    return;
  
  PR_ASSERT(o);
  PR_Lock(singleton->mListLock);
    PL_DHashTableOperate(&singleton->mPK11LogoutCancelObjects, o, PL_DHASH_ADD);
  PR_Unlock(singleton->mListLock);
}

void nsNSSShutDownList::forget(nsOnPK11LogoutCancelObject *o)
{
  if (!singleton)
    return;
  
  PR_ASSERT(o);
  PR_Lock(singleton->mListLock);
    PL_DHashTableOperate(&singleton->mPK11LogoutCancelObjects, o, PL_DHASH_REMOVE);
  PR_Unlock(singleton->mListLock);
}

void nsNSSShutDownList::trackSSLSocketCreate()
{
  if (!singleton)
    return;
  
  PR_Lock(singleton->mListLock);
    ++singleton->mActiveSSLSockets;
  PR_Unlock(singleton->mListLock);
}

void nsNSSShutDownList::trackSSLSocketClose()
{
  if (!singleton)
    return;
  
  PR_Lock(singleton->mListLock);
    --singleton->mActiveSSLSockets;
  PR_Unlock(singleton->mListLock);
}
  
PRBool nsNSSShutDownList::areSSLSocketsActive()
{
  if (!singleton) {
    // I'd rather prefer to be pessimistic and return PR_TRUE.
    // However, maybe we will get called at a time when the singleton
    // has already been freed, and returning PR_TRUE would bring up an 
    // unnecessary warning.
    return PR_FALSE;
  }
  
  PRBool retval;
  PR_Lock(singleton->mListLock);
    retval = (singleton->mActiveSSLSockets > 0);
  PR_Unlock(singleton->mListLock);

  return retval;
}

nsresult nsNSSShutDownList::doPK11Logout()
{
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("canceling all open SSL sockets to disallow future IO\n"));
  // During our iteration we will set a bunch of PRBools to PR_TRUE.
  // Nobody else ever modifies that PRBool, only we do.
  // We only must ensure that our objects do not go away.
  // This is guaranteed by holding the list lock.

  PR_Lock(mListLock);
    PL_DHashTableEnumerate(&mPK11LogoutCancelObjects, doPK11LogoutHelper, 0);
  PR_Unlock(mListLock);

  return NS_OK;
}

PLDHashOperator PR_CALLBACK
nsNSSShutDownList::doPK11LogoutHelper(PLDHashTable *table, 
  PLDHashEntryHdr *hdr, PRUint32 number, void *arg)
{
  ObjectHashEntry *entry = NS_STATIC_CAST(ObjectHashEntry*, hdr);

  nsOnPK11LogoutCancelObject *pklco = 
    NS_REINTERPRET_CAST(nsOnPK11LogoutCancelObject*, entry->obj);

  if (pklco) {
    pklco->logout();
  }

  return PL_DHASH_NEXT;
}

PRBool nsNSSShutDownList::isUIActive()
{
  PRBool canDisallow = mActivityState.ifPossibleDisallowUI(nsNSSActivityState::test_only);
  PRBool bIsUIActive = !canDisallow;
  return bIsUIActive;
}

PRBool nsNSSShutDownList::ifPossibleDisallowUI()
{
  PRBool isNowDisallowed = mActivityState.ifPossibleDisallowUI(nsNSSActivityState::do_it_for_real);
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
    PR_Lock(mListLock);
      removedCount = PL_DHashTableEnumerate(&mObjects, evaporateAllNSSResourcesHelper, 0);
    PR_Unlock(mListLock);
  } while (removedCount > 0);

  mActivityState.releaseCurrentThreadActivityRestriction();
  return NS_OK;
}

PLDHashOperator PR_CALLBACK
nsNSSShutDownList::evaporateAllNSSResourcesHelper(PLDHashTable *table, 
  PLDHashEntryHdr *hdr, PRUint32 number, void *arg)
{
    ObjectHashEntry *entry = NS_STATIC_CAST(ObjectHashEntry*, hdr);
    PR_Unlock(singleton->mListLock);

  entry->obj->shutdown(nsNSSShutDownObject::calledFromList);

    PR_Lock(singleton->mListLock);

    // Never free more than one entry, because other threads might be calling
    // us and remove themselves while we are iterating over the list,
    // and the behaviour of changing the list while iterating is undefined.
    return (PLDHashOperator)(PL_DHASH_STOP | PL_DHASH_REMOVE);
}

nsNSSShutDownList *nsNSSShutDownList::construct()
{
  if (singleton) {
    // we should never ever be called twice
    return nsnull;
  }

  singleton = new nsNSSShutDownList();
  return singleton;
}

nsNSSActivityState::nsNSSActivityState()
:mNSSActivityStateLock(nsnull), 
 mNSSActivityChanged(nsnull),
 mNSSActivityCounter(0),
 mBlockingUICounter(0),
 mIsUIForbidden(PR_FALSE),
 mNSSRestrictedThread(nsnull)
{
  mNSSActivityStateLock = PR_NewLock();
  if (!mNSSActivityStateLock)
    return;

  mNSSActivityChanged = PR_NewCondVar(mNSSActivityStateLock);
}

nsNSSActivityState::~nsNSSActivityState()
{
  if (mNSSActivityChanged) {
    PR_DestroyCondVar(mNSSActivityChanged);
    mNSSActivityChanged = nsnull;
  }

  if (mNSSActivityStateLock) {
    PR_DestroyLock(mNSSActivityStateLock);
    mNSSActivityStateLock = nsnull;
  }
}

void nsNSSActivityState::enter()
{
  PR_Lock(mNSSActivityStateLock);

    while (mNSSRestrictedThread && mNSSRestrictedThread != PR_GetCurrentThread()) {
      PR_WaitCondVar(mNSSActivityChanged, PR_INTERVAL_NO_TIMEOUT);
    }

    ++mNSSActivityCounter;
  
  PR_Unlock(mNSSActivityStateLock);
}

void nsNSSActivityState::leave()
{
  PR_Lock(mNSSActivityStateLock);

    --mNSSActivityCounter;

    if (!mNSSActivityCounter) {
      PR_NotifyAllCondVar(mNSSActivityChanged);
    }

  PR_Unlock(mNSSActivityStateLock);
}

void nsNSSActivityState::enterBlockingUIState()
{
  PR_Lock(mNSSActivityStateLock);

    ++mBlockingUICounter;

  PR_Unlock(mNSSActivityStateLock);
}

void nsNSSActivityState::leaveBlockingUIState()
{
  PR_Lock(mNSSActivityStateLock);

    --mBlockingUICounter;

  PR_Unlock(mNSSActivityStateLock);
}

PRBool nsNSSActivityState::isBlockingUIActive()
{
  PRBool retval;

  PR_Lock(mNSSActivityStateLock);
    retval = (mBlockingUICounter > 0);
  PR_Unlock(mNSSActivityStateLock);

  return retval;
}

PRBool nsNSSActivityState::isUIForbidden()
{
  PRBool retval;
  
  PR_Lock(mNSSActivityStateLock);
    retval = mIsUIForbidden;
  PR_Unlock(mNSSActivityStateLock);

  return retval;
}

PRBool nsNSSActivityState::ifPossibleDisallowUI(RealOrTesting rot)
{
  PRBool retval = PR_FALSE;

  PR_Lock(mNSSActivityStateLock);

    // Checking and disallowing the UI must be done atomically.

    if (!mBlockingUICounter) {
      // No UI is currently shown, we are able to evaporate.
      retval = PR_TRUE;
      if (rot == do_it_for_real) {
        // Remember to disallow UI.
        mIsUIForbidden = PR_TRUE;
        
        // to clear the "forbidden" state,
        // one must either call 
        // restrictActivityToCurrentThread() + releaseCurrentThreadActivityRestriction()
        // or cancel the operation by calling
        // unprepareCurrentThreadRestriction()
      }
    }
  
  PR_Unlock(mNSSActivityStateLock);

  return retval;
}

void nsNSSActivityState::allowUI()
{
  PR_Lock(mNSSActivityStateLock);

    mIsUIForbidden = PR_FALSE;
  
  PR_Unlock(mNSSActivityStateLock);
}

PRStatus nsNSSActivityState::restrictActivityToCurrentThread()
{
  PRStatus retval = PR_FAILURE;

  PR_Lock(mNSSActivityStateLock);
  
    if (!mBlockingUICounter) {
      while (0 < mNSSActivityCounter && !mBlockingUICounter) {
        PR_WaitCondVar(mNSSActivityChanged, PR_TicksPerSecond());
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
  
  PR_Unlock(mNSSActivityStateLock);

  return retval;
}

void nsNSSActivityState::releaseCurrentThreadActivityRestriction()
{
  PR_Lock(mNSSActivityStateLock);

    mNSSRestrictedThread = nsnull;
    mIsUIForbidden = PR_FALSE;

    PR_NotifyAllCondVar(mNSSActivityChanged);

  PR_Unlock(mNSSActivityStateLock);
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

PRBool nsPSMUITracker::isUIForbidden()
{
  nsNSSActivityState *state = nsNSSShutDownList::getActivityState();
  if (!state)
    return PR_FALSE;

  return state->isUIForbidden();
}
