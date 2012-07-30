/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "mozilla/Services.h"

#include "ManifestParser.h"
#include "mozilla/FunctionTimer.h"

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
char* ArenaStrdup(const char* s, PLArenaPool* aArena);

//
// BaseStringEnumerator is subclassed by EntryEnumerator and
// CategoryEnumerator
//
class BaseStringEnumerator
  : public nsISimpleEnumerator,
    private nsIUTF8StringEnumerator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR
  NS_DECL_NSIUTF8STRINGENUMERATOR

protected:
  // Callback function for NS_QuickSort to sort mArray
  static int SortCallback(const void *, const void *, void *);

  BaseStringEnumerator()
    : mArray(nullptr),
      mCount(0),
      mSimpleCurItem(0),
      mStringCurItem(0) { }

  // A virtual destructor is needed here because subclasses of
  // BaseStringEnumerator do not implement their own Release() method.

  virtual ~BaseStringEnumerator()
  {
    if (mArray)
      delete[] mArray;
  }

  void Sort();

  const char** mArray;
  PRUint32 mCount;
  PRUint32 mSimpleCurItem;
  PRUint32 mStringCurItem;
};

NS_IMPL_ISUPPORTS2(BaseStringEnumerator, nsISimpleEnumerator, nsIUTF8StringEnumerator)

NS_IMETHODIMP
BaseStringEnumerator::HasMoreElements(bool *_retval)
{
  *_retval = (mSimpleCurItem < mCount);

  return NS_OK;
}

NS_IMETHODIMP
BaseStringEnumerator::GetNext(nsISupports **_retval)
{
  if (mSimpleCurItem >= mCount)
    return NS_ERROR_FAILURE;

  nsSupportsDependentCString* str =
    new nsSupportsDependentCString(mArray[mSimpleCurItem++]);
  if (!str)
    return NS_ERROR_OUT_OF_MEMORY;

  *_retval = str;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
BaseStringEnumerator::HasMore(bool *_retval)
{
  *_retval = (mStringCurItem < mCount);

  return NS_OK;
}

NS_IMETHODIMP
BaseStringEnumerator::GetNext(nsACString& _retval)
{
  if (mStringCurItem >= mCount)
    return NS_ERROR_FAILURE;

  _retval = nsDependentCString(mArray[mStringCurItem++]);
  return NS_OK;
}

int
BaseStringEnumerator::SortCallback(const void *e1, const void *e2,
                                   void * /*unused*/)
{
  char const *const *s1 = reinterpret_cast<char const *const *>(e1);
  char const *const *s2 = reinterpret_cast<char const *const *>(e2);

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

private:
  static PLDHashOperator
    enumfunc_createenumerator(CategoryLeaf* aLeaf, void* userArg);
};


PLDHashOperator
EntryEnumerator::enumfunc_createenumerator(CategoryLeaf* aLeaf, void* userArg)
{
  EntryEnumerator* mythis = static_cast<EntryEnumerator*>(userArg);
  if (aLeaf->value)
    mythis->mArray[mythis->mCount++] = aLeaf->GetKey();

  return PL_DHASH_NEXT;
}

EntryEnumerator*
EntryEnumerator::Create(nsTHashtable<CategoryLeaf>& aTable)
{
  EntryEnumerator* enumObj = new EntryEnumerator();
  if (!enumObj)
    return nullptr;

  enumObj->mArray = new char const* [aTable.Count()];
  if (!enumObj->mArray) {
    delete enumObj;
    return nullptr;
  }

  aTable.EnumerateEntries(enumfunc_createenumerator, enumObj);

  enumObj->Sort();

  return enumObj;
}


//
// CategoryNode implementations
//

CategoryNode*
CategoryNode::Create(PLArenaPool* aArena)
{
  CategoryNode* node = new(aArena) CategoryNode();
  if (!node)
    return nullptr;

  node->mTable.Init();
  return node;
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
                      char** _retval)
{
  MutexAutoLock lock(mLock);
  nsresult rv = NS_ERROR_NOT_AVAILABLE;
  CategoryLeaf* ent =
    mTable.GetEntry(aEntryName);

  if (ent && ent->value) {
    *_retval = NS_strdup(ent->value);
    if (*_retval)
      rv = NS_OK;
  }

  return rv;
}

NS_METHOD
CategoryNode::AddLeaf(const char* aEntryName,
                      const char* aValue,
                      bool aReplace,
                      char** _retval,
                      PLArenaPool* aArena)
{
  if (_retval)
    *_retval = NULL;

  MutexAutoLock lock(mLock);
  CategoryLeaf* leaf = 
    mTable.GetEntry(aEntryName);

  if (!leaf) {
    const char* arenaEntryName = ArenaStrdup(aEntryName, aArena);
    if (!arenaEntryName)
      return NS_ERROR_OUT_OF_MEMORY;

    leaf = mTable.PutEntry(arenaEntryName);
    if (!leaf)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (leaf->value && !aReplace)
    return NS_ERROR_INVALID_ARG;

  const char* arenaValue = ArenaStrdup(aValue, aArena);
  if (!arenaValue)
    return NS_ERROR_OUT_OF_MEMORY;

  if (_retval && leaf->value) {
    *_retval = ToNewCString(nsDependentCString(leaf->value));
    if (!*_retval)
      return NS_ERROR_OUT_OF_MEMORY;
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
CategoryNode::Enumerate(nsISimpleEnumerator **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  MutexAutoLock lock(mLock);
  EntryEnumerator* enumObj = EntryEnumerator::Create(mTable);

  if (!enumObj)
    return NS_ERROR_OUT_OF_MEMORY;

  *_retval = enumObj;
  NS_ADDREF(*_retval);
  return NS_OK;
}

struct persistent_userstruct {
  PRFileDesc* fd;
  const char* categoryName;
  bool        success;
};

PLDHashOperator
enumfunc_pentries(CategoryLeaf* aLeaf, void* userArg)
{
  persistent_userstruct* args =
    static_cast<persistent_userstruct*>(userArg);

  PLDHashOperator status = PL_DHASH_NEXT;

  if (aLeaf->value) {
    if (PR_fprintf(args->fd,
                   "%s,%s,%s\n",
                   args->categoryName,
                   aLeaf->GetKey(),
                   aLeaf->value) == (PRUint32) -1) {
      args->success = false;
      status = PL_DHASH_STOP;
    }
  }

  return status;
}

//
// CategoryEnumerator class
//

class CategoryEnumerator
  : public BaseStringEnumerator
{
public:
  static CategoryEnumerator* Create(nsClassHashtable<nsDepCharHashKey, CategoryNode>& aTable);

private:
  static PLDHashOperator
  enumfunc_createenumerator(const char* aStr,
                            CategoryNode* aNode,
                            void* userArg);
};

CategoryEnumerator*
CategoryEnumerator::Create(nsClassHashtable<nsDepCharHashKey, CategoryNode>& aTable)
{
  CategoryEnumerator* enumObj = new CategoryEnumerator();
  if (!enumObj)
    return nullptr;

  enumObj->mArray = new const char* [aTable.Count()];
  if (!enumObj->mArray) {
    delete enumObj;
    return nullptr;
  }

  aTable.EnumerateRead(enumfunc_createenumerator, enumObj);

  return enumObj;
}

PLDHashOperator
CategoryEnumerator::enumfunc_createenumerator(const char* aStr, CategoryNode* aNode, void* userArg)
{
  CategoryEnumerator* mythis = static_cast<CategoryEnumerator*>(userArg);

  // if a category has no entries, we pretend it doesn't exist
  if (aNode->Count())
    mythis->mArray[mythis->mCount++] = aStr;

  return PL_DHASH_NEXT;
}


//
// nsCategoryManager implementations
//

NS_IMPL_QUERY_INTERFACE1(nsCategoryManager, nsICategoryManager)

NS_IMETHODIMP_(nsrefcnt)
nsCategoryManager::AddRef()
{
  return 2;
}

NS_IMETHODIMP_(nsrefcnt)
nsCategoryManager::Release()
{
  return 1;
}

nsCategoryManager* nsCategoryManager::gCategoryManager;

/* static */ nsCategoryManager*
nsCategoryManager::GetSingleton()
{
  if (!gCategoryManager)
    gCategoryManager = new nsCategoryManager();
  return gCategoryManager;
}

/* static */ void
nsCategoryManager::Destroy()
{
  delete gCategoryManager;
}

nsresult
nsCategoryManager::Create(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  return GetSingleton()->QueryInterface(aIID, aResult);
}

nsCategoryManager::nsCategoryManager()
  : mLock("nsCategoryManager")
  , mSuppressNotifications(false)
{
  PL_INIT_ARENA_POOL(&mArena, "CategoryManagerArena",
                     NS_CATEGORYMANAGER_ARENA_SIZE);

  mTable.Init();
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
nsCategoryManager::get_category(const char* aName) {
  CategoryNode* node;
  if (!mTable.Get(aName, &node)) {
    return nullptr;
  }
  return node;
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
  { }

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
  if (observerService)
    observerService->NotifyObservers(mSubject, mTopic, mData.get());

  return NS_OK;
}
  
} // anonymous namespace


void
nsCategoryManager::NotifyObservers( const char *aTopic,
                                    const char *aCategoryName,
                                    const char *aEntryName )
{
  if (mSuppressNotifications)
    return;

  nsRefPtr<CategoryNotificationRunnable> r;

  if (aEntryName) {
    nsCOMPtr<nsISupportsCString> entry
      (do_CreateInstance (NS_SUPPORTS_CSTRING_CONTRACTID));
    if (!entry)
      return;

    nsresult rv = entry->SetData(nsDependentCString(aEntryName));
    if (NS_FAILED(rv))
      return;

    r = new CategoryNotificationRunnable(entry, aTopic, aCategoryName);
  } else {
    r = new CategoryNotificationRunnable(this, aTopic, aCategoryName);
  }

  NS_DispatchToMainThread(r);
}

NS_IMETHODIMP
nsCategoryManager::GetCategoryEntry( const char *aCategoryName,
                                     const char *aEntryName,
                                     char **_retval )
{
  NS_ENSURE_ARG_POINTER(aCategoryName);
  NS_ENSURE_ARG_POINTER(aEntryName);
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult status = NS_ERROR_NOT_AVAILABLE;

  CategoryNode* category;
  {
    MutexAutoLock lock(mLock);
    category = get_category(aCategoryName);
  }

  if (category) {
    status = category->GetLeaf(aEntryName, _retval);
  }

  return status;
}

NS_IMETHODIMP
nsCategoryManager::AddCategoryEntry( const char *aCategoryName,
                                     const char *aEntryName,
                                     const char *aValue,
                                     bool aPersist,
                                     bool aReplace,
                                     char **_retval )
{
  if (aPersist) {
    NS_ERROR("Category manager doesn't support persistence.");
    return NS_ERROR_INVALID_ARG;
  }

  AddCategoryEntry(aCategoryName, aEntryName, aValue, aReplace, _retval);
  return NS_OK;
}

void
nsCategoryManager::AddCategoryEntry(const char *aCategoryName,
                                    const char *aEntryName,
                                    const char *aValue,
                                    bool aReplace,
                                    char** aOldValue)
{
  if (aOldValue)
    *aOldValue = NULL;

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

  if (!category)
    return;

  // We will need the return value of AddLeaf even if the called doesn't want it
  char *oldEntry = nullptr;

  nsresult rv = category->AddLeaf(aEntryName,
                                  aValue,
                                  aReplace,
                                  &oldEntry,
                                  &mArena);

  if (NS_SUCCEEDED(rv)) {
    if (oldEntry) {
      NotifyObservers(NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID,
                      aCategoryName, oldEntry);
    }
    NotifyObservers(NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID,
                    aCategoryName, aEntryName);

    if (aOldValue)
      *aOldValue = oldEntry;
    else
      NS_Free(oldEntry);
  }
}

NS_IMETHODIMP
nsCategoryManager::DeleteCategoryEntry( const char *aCategoryName,
                                        const char *aEntryName,
                                        bool aDontPersist)
{
  NS_ENSURE_ARG_POINTER(aCategoryName);
  NS_ENSURE_ARG_POINTER(aEntryName);

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
nsCategoryManager::DeleteCategory( const char *aCategoryName )
{
  NS_ENSURE_ARG_POINTER(aCategoryName);

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
nsCategoryManager::EnumerateCategory( const char *aCategoryName,
                                      nsISimpleEnumerator **_retval )
{
  NS_ENSURE_ARG_POINTER(aCategoryName);
  NS_ENSURE_ARG_POINTER(_retval);

  CategoryNode* category;
  {
    MutexAutoLock lock(mLock);
    category = get_category(aCategoryName);
  }
  
  if (!category) {
    return NS_NewEmptyEnumerator(_retval);
  }

  return category->Enumerate(_retval);
}

NS_IMETHODIMP 
nsCategoryManager::EnumerateCategories(nsISimpleEnumerator **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  MutexAutoLock lock(mLock);
  CategoryEnumerator* enumObj = CategoryEnumerator::Create(mTable);

  if (!enumObj)
    return NS_ERROR_OUT_OF_MEMORY;

  *_retval = enumObj;
  NS_ADDREF(*_retval);
  return NS_OK;
}

struct writecat_struct {
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
NS_CreateServicesFromCategory(const char *category,
                              nsISupports *origin,
                              const char *observerTopic)
{
  NS_TIME_FUNCTION_FMT("NS_CreateServicesFromCategory: %s (%s)",
                       category, observerTopic ? observerTopic : "(no topic)");

  nsresult rv;

  nsCOMPtr<nsICategoryManager> categoryManager = 
    do_GetService("@mozilla.org/categorymanager;1");
  if (!categoryManager)
    return;

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = categoryManager->EnumerateCategory(category, 
                                          getter_AddRefs(enumerator));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIUTF8StringEnumerator> senumerator =
    do_QueryInterface(enumerator);
  if (!senumerator) {
    NS_WARNING("Category enumerator doesn't support nsIUTF8StringEnumerator.");
    return;
  }

  bool hasMore;
  while (NS_SUCCEEDED(senumerator->HasMore(&hasMore)) && hasMore) {
    // From here on just skip any error we get.
    nsCAutoString entryString;
    if (NS_FAILED(senumerator->GetNext(entryString)))
      continue;
      
    nsXPIDLCString contractID;
    rv = categoryManager->GetCategoryEntry(category,entryString.get(),
                                           getter_Copies(contractID));
    if (NS_FAILED(rv))
      continue;
        
    NS_TIME_FUNCTION_MARK("getservice: %s", contractID.get());

    nsCOMPtr<nsISupports> instance = do_GetService(contractID);
    if (!instance) {
      LogMessage("While creating services from category '%s', could not create service for entry '%s', contract ID '%s'",
                 category, entryString.get(), contractID.get());
      continue;
    }

    if (observerTopic) {
      NS_TIME_FUNCTION_MARK("observe: %s", contractID.get());

      // try an observer, if it implements it.
      nsCOMPtr<nsIObserver> observer = do_QueryInterface(instance);
      if (observer)
        observer->Observe(origin, observerTopic, EmptyString().get());
      else
        LogMessage("While creating services from category '%s', service for entry '%s', contract ID '%s' does not implement nsIObserver.",
                   category, entryString.get(), contractID.get());
    }
  }
}
