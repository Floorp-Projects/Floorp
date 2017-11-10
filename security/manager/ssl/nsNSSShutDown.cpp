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
  : mPK11LogoutCancelObjects(&gSetOps, sizeof(ObjectHashEntry))
{
}

nsNSSShutDownList::~nsNSSShutDownList()
{
  MOZ_ASSERT(this == singleton);
  MOZ_ASSERT(sInShutdown,
             "evaporateAllNSSResourcesAndShutDown() should have been called");
  singleton = nullptr;
}

void nsNSSShutDownList::remember(nsOnPK11LogoutCancelObject *o)
{
  StaticMutexAutoLock lock(sListLock);
  if (!nsNSSShutDownList::construct(lock)) {
    return;
  }

  MOZ_ASSERT(o);
  singleton->mPK11LogoutCancelObjects.Add(o, fallible);
}

void nsNSSShutDownList::forget(nsOnPK11LogoutCancelObject *o)
{
  StaticMutexAutoLock lock(sListLock);
  if (!singleton) {
    return;
  }

  MOZ_ASSERT(o);
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

nsresult nsNSSShutDownList::evaporateAllNSSResourcesAndShutDown()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  // This makes this function idempotent and protects against reentering it
  // (which really shouldn't happen anyway, but just in case).
  if (sInShutdown) {
    return NS_OK;
  }

  StaticMutexAutoLock lock(sListLock);
  if (!singleton) {
    return NS_OK;
  }
  sInShutdown = true;
  delete singleton;
  return NS_OK;
}

bool nsNSSShutDownList::construct(const StaticMutexAutoLock& /*proofOfLock*/)
{
  if (sInShutdown) {
    return false;
  }

  if (!singleton && XRE_IsParentProcess()) {
    singleton = new nsNSSShutDownList();
  }

  return !!singleton;
}

bool
nsNSSShutDownObject::isAlreadyShutDown() const
{
  return false;
}
