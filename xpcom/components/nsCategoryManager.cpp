/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define PL_ARENA_CONST_ALIGN_MASK 7

#include "nsICategoryManager.h"
#include "nsCategoryManager.h"

#include "plarena.h"
#include "prio.h"
#include "prprf.h"
#include "prlock.h"
#include "nsCOMPtr.h"
#include "nsTHashtable.h"
#include "nsClassHashtable.h"
#include "nsIFactory.h"
#include "nsIStringEnumerator.h"
#include "nsSupportsPrimitives.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsQuickSort.h"
#include "nsEnumeratorUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Services.h"

#include "ManifestParser.h"
#include "nsISimpleEnumerator.h"

using namespace mozilla;
class nsIComponentLoaderManager;

/*
  CategoryDatabase
  contains 0 or more 1-1 mappings of string to Category
  each Category contains 0 or more 1-1 mappings of string keys to string values

  In other words, the CategoryDatabase is a tree, whose root is a hashtable.
  Internal nodes (or Categories) are hashtables. Leaf nodes are strings.

  The leaf strings are allocated in an arena, because we assume they're not
  going to change much ;)
*/

#define NS_CATEGORYMANAGER_ARENA_SIZE (1024 * 8)

// pulled in from nsComponentManager.cpp
char* ArenaStrdup(const char* aStr, PLArenaPool* aArena);

//
// BaseStringEnumerator is subclassed by EntryEnumerator and
// CategoryEnumerator
//
class BaseStringEnumerator
  : public nsISimpleEnumerator
  , private nsIUTF8StringEnumerator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR
  NS_DECL_NSIUTF8STRINGENUMERATOR

protected:
  // Callback function for NS_QuickSort to sort mArray
  static int SortCallback(const void*, const void*, void*);

  BaseStringEnumerator()
    : mArray(nullptr)
    , mCount(0)
    , mSimpleCurItem(0)
    , mStringCurItem(0)
  {
  }

  // A virtual destructor is needed here because subclasses of
  // BaseStringEnumerator do not implement their own Release() method.

  virtual ~BaseStringEnumerator()
  {
    delete [] mArray;
  }

  void Sort();

  const char** mArray;
  uint32_t mCount;
  uint32_t mSimpleCurItem;
  uint32_t mStringCurItem;
};

NS_IMPL_ISUPPORTS(BaseStringEnumerator, nsISimpleEnumerator,
                  nsIUTF8StringEnumerator)

NS_IMETHODIMP
BaseStringEnumerator::HasMoreElements(bool* aResult)
{
  *aResult = (mSimpleCurItem < mCount);

  return NS_OK;
}

NS_IMETHODIMP
BaseStringEnumerator::GetNext(nsISupports** aResult)
{
  if (mSimpleCurItem >= mCount) {
    return NS_ERROR_FAILURE;
  }

  nsSupportsDependentCString* str =
    new nsSupportsDependentCString(mArray[mSimpleCurItem++]);
  if (!str) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aResult = str;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
BaseStringEnumerator::HasMore(bool* aResult)
{
  *aResult = (mStringCurItem < mCount);

  return NS_OK;
}

NS_IMETHODIMP
BaseStringEnumerator::GetNext(nsACString& aResult)
{
  if (mStringCurItem >= mCount) {
    return NS_ERROR_FAILURE;
  }

  aResult = nsDependentCString(mArray[mStringCurItem++]);
  return NS_OK;
}

int
BaseStringEnumerator::SortCallback(const void* aE1, const void* aE2,
                                   void* /*unused*/)
{
  char const* const* s1 = reinterpret_cast<char const* const*>(aE1);
  char const* const* s2 = reinterpret_cast<char const* const*>(aE2);

  return strcmp(*s1, *s2);
}

void
BaseStringEnumerator::Sort()
{
  NS_QuickSort(mArray, mCount, sizeof(mArray[0]), SortCallback, nullptr);
}

//
// EntryEnumerator is the wrapper that allows nsICategoryManager::EnumerateCategory
//
class EntryEnumerator
  : public BaseStringEnumerator
{
public:
  static EntryEnumerator* Create(nsTHashtable<CategoryLeaf>& aTable);
};


EntryEnumerator*
EntryEnumerator::Create(nsTHashtable<CategoryLeaf>& aTable)
{
  EntryEnumerator* enumObj = new EntryEnumerator();
  if (!enumObj) {
    return nullptr;
  }

  enumObj->mArray = new char const* [aTable.Count()];
  if (!enumObj->mArray) {
    delete enumObj;
    return nullptr;
  }

  for (auto iter = aTable.Iter(); !iter.Done(); iter.Next()) {
    CategoryLeaf* leaf = iter.Get();
    if (leaf->value) {
      enumObj->mArray[enumObj->mCount++] = leaf->GetKey();
    }
  }

  enumObj->Sort();

  return enumObj;
}


//
// CategoryNode implementations
//

CategoryNode*
CategoryNode::Create(PLArenaPool* aArena)
{
  return new (aArena) CategoryNode();
}

CategoryNode::~CategoryNode()
{
}

void*
CategoryNode::operator new(size_t aSize, PLArenaPool* aArena)
{
  void* p;
  PL_ARENA_ALLOCATE(p, aArena, aSize);
  return p;
}

NS_METHOD
CategoryNode::GetLeaf(const char* aEntryName,
                      char** aResult)
{
  MutexAutoLock lock(mLock);
  nsresult rv = NS_ERROR_NOT_AVAILABLE;
  CategoryLeaf* ent = mTable.GetEntry(aEntryName);

  if (ent && ent->value) {
    *aResult = NS_strdup(ent->value);
    if (*aResult) {
      rv = NS_OK;
    }
  }

  return rv;
}

NS_METHOD
CategoryNode::AddLeaf(const char* aEntryName,
                      const char* aValue,
                      bool aReplace,
                      char** aResult,
                      PLArenaPool* aArena)
{
  if (aResult) {
    *aResult = nullptr;
  }

  MutexAutoLock lock(mLock);
  CategoryLeaf* leaf = mTable.GetEntry(aEntryName);

  if (!leaf) {
    const char* arenaEntryName = ArenaStrdup(aEntryName, aArena);
    if (!arenaEntryName) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    leaf = mTable.PutEntry(arenaEntryName);
    if (!leaf) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (leaf->value && !aReplace) {
    return NS_ERROR_INVALID_ARG;
  }

  const char* arenaValue = ArenaStrdup(aValue, aArena);
  if (!arenaValue) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (aResult && leaf->value) {
    *aResult = ToNewCString(nsDependentCString(leaf->value));
    if (!*aResult) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  leaf->value = arenaValue;
  return NS_OK;
}

void
CategoryNode::DeleteLeaf(const char* aEntryName)
{
  // we don't throw any errors, because it normally doesn't matter
  // and it makes JS a lot cleaner
  MutexAutoLock lock(mLock);

  // we can just remove the entire hash entry without introspection
  mTable.RemoveEntry(aEntryName);
}

NS_METHOD
CategoryNode::Enumerate(nsISimpleEnumerator** aResult)
{
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  MutexAutoLock lock(mLock);
  EntryEnumerator* enumObj = EntryEnumerator::Create(mTable);

  if (!enumObj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aResult = enumObj;
  NS_ADDREF(*aResult);
  return NS_OK;
}

size_t
CategoryNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf)
{
  // We don't measure the strings pointed to by the entries because the
  // pointers are non-owning.
  return mTable.SizeOfExcludingThis(nullptr, aMallocSizeOf);
}

//
// CategoryEnumerator class
//

class CategoryEnumerator
  : public BaseStringEnumerator
{
public:
  static CategoryEnumerator* Create(nsClassHashtable<nsDepCharHashKey,
                                                     CategoryNode>& aTable);
};

CategoryEnumerator*
CategoryEnumerator::Create(nsClassHashtable<nsDepCharHashKey, CategoryNode>&
                           aTable)
{
  CategoryEnumerator* enumObj = new CategoryEnumerator();
  if (!enumObj) {
    return nullptr;
  }

  enumObj->mArray = new const char* [aTable.Count()];
  if (!enumObj->mArray) {
    delete enumObj;
    return nullptr;
  }

  for (auto iter = aTable.Iter(); !iter.Done(); iter.Next()) {
    // if a category has no entries, we pretend it doesn't exist
    CategoryNode* aNode = iter.GetUserData();
    if (aNode->Count()) {
      const char* str = iter.GetKey();
      enumObj->mArray[enumObj->mCount++] = str;
    }
  }

  return enumObj;
}


//
// nsCategoryManager implementations
//

NS_IMPL_QUERY_INTERFACE(nsCategoryManager, nsICategoryManager, nsIMemoryReporter)

NS_IMETHODIMP_(MozExternalRefCountType)
nsCategoryManager::AddRef()
{
  return 2;
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsCategoryManager::Release()
{
  return 1;
}

nsCategoryManager* nsCategoryManager::gCategoryManager;

/* static */ nsCategoryManager*
nsCategoryManager::GetSingleton()
{
  if (!gCategoryManager) {
    gCategoryManager = new nsCategoryManager();
  }
  return gCategoryManager;
}

/* static */ void
nsCategoryManager::Destroy()
{
  delete gCategoryManager;
  gCategoryManager = nullptr;
}

nsresult
nsCategoryManager::Create(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  return GetSingleton()->QueryInterface(aIID, aResult);
}

nsCategoryManager::nsCategoryManager()
  : mLock("nsCategoryManager")
  , mSuppressNotifications(false)
{
  PL_INIT_ARENA_POOL(&mArena, "CategoryManagerArena",
                     NS_CATEGORYMANAGER_ARENA_SIZE);
}

void
nsCategoryManager::InitMemoryReporter()
{
#if !defined(MOZILLA_XPCOMRT_API)
  RegisterStrongMemoryReporter(this);
#endif // !defined(MOZILLA_XPCOMRT_API)
}

nsCategoryManager::~nsCategoryManager()
{
  // the hashtable contains entries that must be deleted before the arena is
  // destroyed, or else you will have PRLocks undestroyed and other Really
  // Bad Stuff (TM)
  mTable.Clear();

  PL_FinishArenaPool(&mArena);
}

inline CategoryNode*
nsCategoryManager::get_category(const char* aName)
{
  CategoryNode* node;
  if (!mTable.Get(aName, &node)) {
    return nullptr;
  }
  return node;
}

MOZ_DEFINE_MALLOC_SIZE_OF(CategoryManagerMallocSizeOf)

NS_IMETHODIMP
nsCategoryManager::CollectReports(nsIHandleReportCallback* aHandleReport,
                                  nsISupports* aData, bool aAnonymize)
{
  return MOZ_COLLECT_REPORT("explicit/xpcom/category-manager",
                            KIND_HEAP, UNITS_BYTES,
                            SizeOfIncludingThis(CategoryManagerMallocSizeOf),
                            "Memory used for the XPCOM category manager.");
}

static size_t
SizeOfCategoryManagerTableEntryExcludingThis(nsDepCharHashKey::KeyType aKey,
                                             const nsAutoPtr<CategoryNode>& aData,
                                             MallocSizeOf aMallocSizeOf,
                                             void* aUserArg)
{
  // We don't measure the string pointed to by aKey because it's a non-owning
  // pointer.
  return aData.get()->SizeOfExcludingThis(aMallocSizeOf);
}

size_t
nsCategoryManager::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  size_t n = aMallocSizeOf(this);

  n += PL_SizeOfArenaPoolExcludingPool(&mArena, aMallocSizeOf);

  n += mTable.SizeOfExcludingThis(SizeOfCategoryManagerTableEntryExcludingThis,
                                  aMallocSizeOf);

  return n;
}

namespace {

class CategoryNotificationRunnable : public nsRunnable
{
public:
  CategoryNotificationRunnable(nsISupports* aSubject,
                               const char* aTopic,
                               const char* aData)
    : mSubject(aSubject)
    , mTopic(aTopic)
    , mData(aData)
  {
  }

  NS_DECL_NSIRUNNABLE

private:
  nsCOMPtr<nsISupports> mSubject;
  const char* mTopic;
  NS_ConvertUTF8toUTF16 mData;
};

NS_IMETHODIMP
CategoryNotificationRunnable::Run()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(mSubject, mTopic, mData.get());
  }

  return NS_OK;
}

} // namespace


void
nsCategoryManager::NotifyObservers(const char* aTopic,
                                   const char* aCategoryName,
                                   const char* aEntryName)
{
  if (mSuppressNotifications) {
    return;
  }

  nsRefPtr<CategoryNotificationRunnable> r;

  if (aEntryName) {
    nsCOMPtr<nsISupportsCString> entry =
      do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID);
    if (!entry) {
      return;
    }

    nsresult rv = entry->SetData(nsDependentCString(aEntryName));
    if (NS_FAILED(rv)) {
      return;
    }

    r = new CategoryNotificationRunnable(entry, aTopic, aCategoryName);
  } else {
    r = new CategoryNotificationRunnable(NS_ISUPPORTS_CAST(nsICategoryManager*,
                                                           this),
                                         aTopic, aCategoryName);
  }

  NS_DispatchToMainThread(r);
}

NS_IMETHODIMP
nsCategoryManager::GetCategoryEntry(const char* aCategoryName,
                                    const char* aEntryName,
                                    char** aResult)
{
  if (NS_WARN_IF(!aCategoryName) ||
      NS_WARN_IF(!aEntryName) ||
      NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult status = NS_ERROR_NOT_AVAILABLE;

  CategoryNode* category;
  {
    MutexAutoLock lock(mLock);
    category = get_category(aCategoryName);
  }

  if (category) {
    status = category->GetLeaf(aEntryName, aResult);
  }

  return status;
}

NS_IMETHODIMP
nsCategoryManager::AddCategoryEntry(const char* aCategoryName,
                                    const char* aEntryName,
                                    const char* aValue,
                                    bool aPersist,
                                    bool aReplace,
                                    char** aResult)
{
  if (aPersist) {
    NS_ERROR("Category manager doesn't support persistence.");
    return NS_ERROR_INVALID_ARG;
  }

  AddCategoryEntry(aCategoryName, aEntryName, aValue, aReplace, aResult);
  return NS_OK;
}

void
nsCategoryManager::AddCategoryEntry(const char* aCategoryName,
                                    const char* aEntryName,
                                    const char* aValue,
                                    bool aReplace,
                                    char** aOldValue)
{
  if (aOldValue) {
    *aOldValue = nullptr;
  }

  // Before we can insert a new entry, we'll need to
  //  find the |CategoryNode| to put it in...
  CategoryNode* category;
  {
    MutexAutoLock lock(mLock);
    category = get_category(aCategoryName);

    if (!category) {
      // That category doesn't exist yet; let's make it.
      category = CategoryNode::Create(&mArena);

      char* categoryName = ArenaStrdup(aCategoryName, &mArena);
      mTable.Put(categoryName, category);
    }
  }

  if (!category) {
    return;
  }

  // We will need the return value of AddLeaf even if the called doesn't want it
  char* oldEntry = nullptr;

  nsresult rv = category->AddLeaf(aEntryName,
                                  aValue,
                                  aReplace,
                                  &oldEntry,
                                  &mArena);

  if (NS_SUCCEEDED(rv)) {
    if (oldEntry) {
      NotifyObservers(NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID,
                      aCategoryName, aEntryName);
    }
    NotifyObservers(NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID,
                    aCategoryName, aEntryName);

    if (aOldValue) {
      *aOldValue = oldEntry;
    } else {
      free(oldEntry);
    }
  }
}

NS_IMETHODIMP
nsCategoryManager::DeleteCategoryEntry(const char* aCategoryName,
                                       const char* aEntryName,
                                       bool aDontPersist)
{
  if (NS_WARN_IF(!aCategoryName) ||
      NS_WARN_IF(!aEntryName)) {
    return NS_ERROR_INVALID_ARG;
  }

  /*
    Note: no errors are reported since failure to delete
    probably won't hurt you, and returning errors seriously
    inconveniences JS clients
  */

  CategoryNode* category;
  {
    MutexAutoLock lock(mLock);
    category = get_category(aCategoryName);
  }

  if (category) {
    category->DeleteLeaf(aEntryName);

    NotifyObservers(NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID,
                    aCategoryName, aEntryName);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCategoryManager::DeleteCategory(const char* aCategoryName)
{
  if (NS_WARN_IF(!aCategoryName)) {
    return NS_ERROR_INVALID_ARG;
  }

  // the categories are arena-allocated, so we don't
  // actually delete them. We just remove all of the
  // leaf nodes.

  CategoryNode* category;
  {
    MutexAutoLock lock(mLock);
    category = get_category(aCategoryName);
  }

  if (category) {
    category->Clear();
    NotifyObservers(NS_XPCOM_CATEGORY_CLEARED_OBSERVER_ID,
                    aCategoryName, nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCategoryManager::EnumerateCategory(const char* aCategoryName,
                                     nsISimpleEnumerator** aResult)
{
  if (NS_WARN_IF(!aCategoryName) ||
      NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  CategoryNode* category;
  {
    MutexAutoLock lock(mLock);
    category = get_category(aCategoryName);
  }

  if (!category) {
    return NS_NewEmptyEnumerator(aResult);
  }

  return category->Enumerate(aResult);
}

NS_IMETHODIMP
nsCategoryManager::EnumerateCategories(nsISimpleEnumerator** aResult)
{
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  MutexAutoLock lock(mLock);
  CategoryEnumerator* enumObj = CategoryEnumerator::Create(mTable);

  if (!enumObj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aResult = enumObj;
  NS_ADDREF(*aResult);
  return NS_OK;
}

struct writecat_struct
{
  PRFileDesc* fd;
  bool        success;
};

NS_METHOD
nsCategoryManager::SuppressNotifications(bool aSuppress)
{
  mSuppressNotifications = aSuppress;
  return NS_OK;
}

/*
 * CreateServicesFromCategory()
 *
 * Given a category, this convenience functions enumerates the category and
 * creates a service of every CID or ContractID registered under the category.
 * If observerTopic is non null and the service implements nsIObserver,
 * this will attempt to notify the observer with the origin, observerTopic string
 * as parameter.
 */
void
NS_CreateServicesFromCategory(const char* aCategory,
                              nsISupports* aOrigin,
                              const char* aObserverTopic)
{
  nsresult rv;

  nsCOMPtr<nsICategoryManager> categoryManager =
    do_GetService("@mozilla.org/categorymanager;1");
  if (!categoryManager) {
    return;
  }

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = categoryManager->EnumerateCategory(aCategory,
                                          getter_AddRefs(enumerator));
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIUTF8StringEnumerator> senumerator =
    do_QueryInterface(enumerator);
  if (!senumerator) {
    NS_WARNING("Category enumerator doesn't support nsIUTF8StringEnumerator.");
    return;
  }

  bool hasMore;
  while (NS_SUCCEEDED(senumerator->HasMore(&hasMore)) && hasMore) {
    // From here on just skip any error we get.
    nsAutoCString entryString;
    if (NS_FAILED(senumerator->GetNext(entryString))) {
      continue;
    }

    nsXPIDLCString contractID;
    rv = categoryManager->GetCategoryEntry(aCategory, entryString.get(),
                                           getter_Copies(contractID));
    if (NS_FAILED(rv)) {
      continue;
    }

    nsCOMPtr<nsISupports> instance = do_GetService(contractID);
    if (!instance) {
#if !defined(MOZILLA_XPCOMRT_API)
      LogMessage("While creating services from category '%s', could not create service for entry '%s', contract ID '%s'",
                 aCategory, entryString.get(), contractID.get());
#endif // !defined(MOZILLA_XPCOMRT_API)
      continue;
    }

    if (aObserverTopic) {
      // try an observer, if it implements it.
      nsCOMPtr<nsIObserver> observer = do_QueryInterface(instance);
      if (observer) {
        observer->Observe(aOrigin, aObserverTopic, EmptyString().get());
      } else {
#if !defined(MOZILLA_XPCOMRT_API)
        LogMessage("While creating services from category '%s', service for entry '%s', contract ID '%s' does not implement nsIObserver.",
                   aCategory, entryString.get(), contractID.get());
#endif // !defined(MOZILLA_XPCOMRT_API)
      }
    }
  }
}
