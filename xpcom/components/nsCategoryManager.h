/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSCATEGORYMANAGER_H
#define NSCATEGORYMANAGER_H

#include "prio.h"
#include "nsClassHashtable.h"
#include "nsICategoryManager.h"
#include "nsIMemoryReporter.h"
#include "mozilla/ArenaAllocator.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/Attributes.h"

class nsIMemoryReporter;

typedef mozilla::ArenaAllocator<1024 * 8, 8> CategoryAllocator;

/* 16d222a6-1dd2-11b2-b693-f38b02c021b2 */
#define NS_CATEGORYMANAGER_CID                       \
  {                                                  \
    0x16d222a6, 0x1dd2, 0x11b2, {                    \
      0xb6, 0x93, 0xf3, 0x8b, 0x02, 0xc0, 0x21, 0xb2 \
    }                                                \
  }

/**
 * a "leaf-node", managed by the nsCategoryNode hashtable.
 *
 * we need to keep a "persistent value" (which will be written to the registry)
 * and a non-persistent value (for the current runtime): these are usually
 * the same, except when aPersist==false. The strings are permanently arena-
 * allocated, and will never go away.
 */
class CategoryLeaf : public nsDepCharHashKey {
 public:
  explicit CategoryLeaf(const char* aKey)
      : nsDepCharHashKey(aKey), value(nullptr) {}
  const char* value;
};

/**
 * CategoryNode keeps a hashtable of its entries.
 * the CategoryNode itself is permanently allocated in
 * the arena.
 */
class CategoryNode {
 public:
  nsresult GetLeaf(const nsACString& aEntryName, nsACString& aResult);

  nsresult AddLeaf(const nsACString& aEntryName, const nsACString& aValue,
                   bool aReplace, nsACString& aResult,
                   CategoryAllocator* aArena);

  void DeleteLeaf(const nsACString& aEntryName);

  void Clear() {
    mozilla::MutexAutoLock lock(mLock);
    mTable.Clear();
  }

  uint32_t Count() {
    mozilla::MutexAutoLock lock(mLock);
    uint32_t tCount = mTable.Count();
    return tCount;
  }

  nsresult Enumerate(nsISimpleEnumerator** aResult);

  // CategoryNode is arena-allocated, with the strings
  static CategoryNode* Create(CategoryAllocator* aArena);
  ~CategoryNode();
  void operator delete(void*) {}

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf);

 private:
  CategoryNode() : mLock("CategoryLeaf") {}

  void* operator new(size_t aSize, CategoryAllocator* aArena);

  nsTHashtable<CategoryLeaf> mTable MOZ_GUARDED_BY(mLock);
  mozilla::Mutex mLock;
};

/**
 * The main implementation of nsICategoryManager.
 *
 * This implementation is thread-safe.
 */
class nsCategoryManager final : public nsICategoryManager,
                                public nsIMemoryReporter {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICATEGORYMANAGER
  NS_DECL_NSIMEMORYREPORTER

  /**
   * Suppress or unsuppress notifications of category changes to the
   * observer service. This is to be used by nsComponentManagerImpl
   * on startup while reading the stored category list.
   */
  nsresult SuppressNotifications(bool aSuppress);

  void AddCategoryEntry(const nsACString& aCategory, const nsACString& aKey,
                        const nsACString& aValue, bool aReplace,
                        nsACString& aOldValue);

  void AddCategoryEntry(const nsACString& aCategory, const nsACString& aKey,
                        const nsACString& aValue, bool aReplace = true) {
    nsCString oldValue;
    return AddCategoryEntry(aCategory, aKey, aValue, aReplace, oldValue);
  }

  static nsresult Create(REFNSIID aIID, void** aResult);
  void InitMemoryReporter();

  static nsCategoryManager* GetSingleton();
  static void Destroy();

 private:
  static nsCategoryManager* gCategoryManager;

  nsCategoryManager();
  ~nsCategoryManager();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

  CategoryNode* get_category(const nsACString& aName) MOZ_REQUIRES(mLock);
  void NotifyObservers(
      const char* aTopic,
      const nsACString& aCategoryName,  // must be a static string
      const nsACString& aEntryName);

  CategoryAllocator mArena;  // Mainthread only
  nsClassHashtable<nsDepCharHashKey, CategoryNode> mTable MOZ_GUARDED_BY(mLock);
  mozilla::Mutex mLock;
  bool mSuppressNotifications;  // Mainthread only
};

#endif
