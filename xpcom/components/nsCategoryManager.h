/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef NSCATEGORYMANAGER_H
#define NSCATEGORYMANAGER_H

#include "prio.h"
#include "plarena.h"
#include "nsClassHashtable.h"
#include "nsICategoryManager.h"
#include "nsIMemoryReporter.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/Attributes.h"

class nsIMemoryReporter;

/* 16d222a6-1dd2-11b2-b693-f38b02c021b2 */
#define NS_CATEGORYMANAGER_CID \
{ 0x16d222a6, 0x1dd2, 0x11b2, \
  {0xb6, 0x93, 0xf3, 0x8b, 0x02, 0xc0, 0x21, 0xb2} }

/**
 * a "leaf-node", managed by the nsCategoryNode hashtable.
 *
 * we need to keep a "persistent value" (which will be written to the registry)
 * and a non-persistent value (for the current runtime): these are usually
 * the same, except when aPersist==false. The strings are permanently arena-
 * allocated, and will never go away.
 */
class CategoryLeaf : public nsDepCharHashKey
{
public:
  explicit CategoryLeaf(const char* aKey) : nsDepCharHashKey(aKey), value(nullptr) {}
  const char* value;
};


/**
 * CategoryNode keeps a hashtable of its entries.
 * the CategoryNode itself is permanently allocated in
 * the arena.
 */
class CategoryNode
{
public:
  NS_METHOD GetLeaf(const char* aEntryName,
                    char** aResult);

  NS_METHOD AddLeaf(const char* aEntryName,
                    const char* aValue,
                    bool aReplace,
                    char** aResult,
                    PLArenaPool* aArena);

  void DeleteLeaf(const char* aEntryName);

  void Clear()
  {
    mozilla::MutexAutoLock lock(mLock);
    mTable.Clear();
  }

  uint32_t Count()
  {
    mozilla::MutexAutoLock lock(mLock);
    uint32_t tCount = mTable.Count();
    return tCount;
  }

  NS_METHOD Enumerate(nsISimpleEnumerator** aResult);

  // CategoryNode is arena-allocated, with the strings
  static CategoryNode* Create(PLArenaPool* aArena);
  ~CategoryNode();
  void operator delete(void*) {}

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf);

private:
  CategoryNode() : mLock("CategoryLeaf") {}

  void* operator new(size_t aSize, PLArenaPool* aArena);

  nsTHashtable<CategoryLeaf> mTable;
  mozilla::Mutex mLock;
};


/**
 * The main implementation of nsICategoryManager.
 *
 * This implementation is thread-safe.
 */
class nsCategoryManager MOZ_FINAL
  : public nsICategoryManager
  , public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICATEGORYMANAGER
  NS_DECL_NSIMEMORYREPORTER

  /**
   * Suppress or unsuppress notifications of category changes to the
   * observer service. This is to be used by nsComponentManagerImpl
   * on startup while reading the stored category list.
   */
  NS_METHOD SuppressNotifications(bool aSuppress);

  void AddCategoryEntry(const char* aCategory,
                        const char* aKey,
                        const char* aValue,
                        bool aReplace = true,
                        char** aOldValue = nullptr);

  static nsresult Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);
  void InitMemoryReporter();

  static nsCategoryManager* GetSingleton();
  static void Destroy();

private:
  static nsCategoryManager* gCategoryManager;

  nsCategoryManager();
  ~nsCategoryManager();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

  CategoryNode* get_category(const char* aName);
  void NotifyObservers(const char* aTopic,
                       const char* aCategoryName, // must be a static string
                       const char* aEntryName);

  PLArenaPool mArena;
  nsClassHashtable<nsDepCharHashKey, CategoryNode> mTable;
  mozilla::Mutex mLock;
  bool mSuppressNotifications;
};

#endif
