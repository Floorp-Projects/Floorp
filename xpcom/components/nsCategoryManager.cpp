/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsIServiceManagerUtils.h"
#include "nsIObserver.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsEnumeratorUtils.h"

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
           nsIUTF8StringEnumerator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR
  NS_DECL_NSIUTF8STRINGENUMERATOR

protected:
  BaseStringEnumerator()
    : mArray(nsnull),
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

  const char** mArray;
  PRUint32 mCount;
  PRUint32 mSimpleCurItem;
  PRUint32 mStringCurItem;
};

NS_IMPL_ISUPPORTS2(BaseStringEnumerator, nsISimpleEnumerator, nsIUTF8StringEnumerator)

NS_IMETHODIMP
BaseStringEnumerator::HasMoreElements(PRBool *_retval)
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
BaseStringEnumerator::HasMore(PRBool *_retval)
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


//
// EntryEnumerator is the wrapper that allows nsICategoryManager::EnumerateCategory
//
class EntryEnumerator
  : public BaseStringEnumerator
{
public:
  static EntryEnumerator* Create(nsTHashtable<CategoryLeaf>& aTable);

private:
  static PLDHashOperator PR_CALLBACK
    enumfunc_createenumerator(CategoryLeaf* aLeaf, void* userArg);
};


PLDHashOperator PR_CALLBACK
EntryEnumerator::enumfunc_createenumerator(CategoryLeaf* aLeaf, void* userArg)
{
  EntryEnumerator* mythis = NS_STATIC_CAST(EntryEnumerator*, userArg);
  mythis->mArray[mythis->mCount++] = aLeaf->GetKey();

  return PL_DHASH_NEXT;
}

EntryEnumerator*
EntryEnumerator::Create(nsTHashtable<CategoryLeaf>& aTable)
{
  EntryEnumerator* enumObj = new EntryEnumerator();
  if (!enumObj)
    return nsnull;

  enumObj->mArray = new char const* [aTable.Count()];
  if (!enumObj->mArray) {
    delete enumObj;
    return nsnull;
  }

  aTable.EnumerateEntries(enumfunc_createenumerator, enumObj);

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
    return nsnull;

  if (!node->mTable.Init()) {
    delete node;
    return nsnull;
  }

  node->mLock = PR_NewLock();
  if (!node->mLock) {
    delete node;
    return nsnull;
  }

  return node;
}

CategoryNode::~CategoryNode()
{
  if (mLock)
    PR_DestroyLock(mLock);
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
  PR_Lock(mLock);
  nsresult rv = NS_ERROR_NOT_AVAILABLE;
  CategoryLeaf* ent =
    mTable.GetEntry(aEntryName);

  // we only want the non-persistent value
  if (ent && ent->nonpValue) {
    *_retval = nsCRT::strdup(ent->nonpValue);
    if (*_retval)
      rv = NS_OK;
  }
  PR_Unlock(mLock);

  return rv;
}

NS_METHOD
CategoryNode::AddLeaf(const char* aEntryName,
                      const char* aValue,
                      PRBool aPersist,
                      PRBool aReplace,
                      char** _retval,
                      PLArenaPool* aArena)
{
  PR_Lock(mLock);
  CategoryLeaf* leaf = 
    mTable.GetEntry(aEntryName);

  nsresult rv = NS_OK;
  if (leaf) {
    //if the entry was found, aReplace must be specified
    if (!aReplace && (leaf->nonpValue || (aPersist && leaf->pValue )))
      rv = NS_ERROR_INVALID_ARG;
  } else {
    const char* arenaEntryName = ArenaStrdup(aEntryName, aArena);
    if (!arenaEntryName) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
      leaf = mTable.PutEntry(arenaEntryName);
      if (!leaf)
        rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    const char* arenaValue = ArenaStrdup(aValue, aArena);
    if (!arenaValue) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
      leaf->nonpValue = arenaValue;
      if (aPersist)
        leaf->pValue = arenaValue;
    }
  }
    
  PR_Unlock(mLock);
  return rv;
}

NS_METHOD
CategoryNode::DeleteLeaf(const char* aEntryName,
                         PRBool aDontPersist)
{
  // we don't throw any errors, because it normally doesn't matter
  // and it makes JS a lot cleaner
  PR_Lock(mLock);

  if (aDontPersist) {
    // we can just remove the entire hash entry without introspection
    mTable.RemoveEntry(aEntryName);
  } else {
    // if we are keeping the persistent value, we need to look at
    // the contents of the current entry
    CategoryLeaf* leaf = mTable.GetEntry(aEntryName);
    if (leaf) {
      if (leaf->pValue) {
        leaf->nonpValue = nsnull;
      } else {
        // if there is no persistent value, just remove the entry
        mTable.RawRemoveEntry(leaf);
      }
    }
  }
  PR_Unlock(mLock);

  return NS_OK;
}

NS_METHOD 
CategoryNode::Enumerate(nsISimpleEnumerator **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  PR_Lock(mLock);
  EntryEnumerator* enumObj = EntryEnumerator::Create(mTable);
  PR_Unlock(mLock);

  if (!enumObj)
    return NS_ERROR_OUT_OF_MEMORY;

  *_retval = enumObj;
  NS_ADDREF(*_retval);
  return NS_OK;
}

struct persistent_userstruct {
  PRFileDesc* fd;
  const char* categoryName;
  PRBool      success;
};

PLDHashOperator PR_CALLBACK
enumfunc_pentries(CategoryLeaf* aLeaf, void* userArg)
{
  persistent_userstruct* args =
    NS_STATIC_CAST(persistent_userstruct*, userArg);

  PLDHashOperator status = PL_DHASH_NEXT;

  if (aLeaf->pValue) {
    if (PR_fprintf(args->fd,
                   "%s,%s,%s\n",
                   args->categoryName,
                   aLeaf->GetKey(),
                   aLeaf->pValue) == (PRUint32) -1) {
      args->success = PR_FALSE;
      status = PL_DHASH_STOP;
    }
  }

  return status;
}

PRBool
CategoryNode::WritePersistentEntries(PRFileDesc* fd, const char* aCategoryName)
{
  persistent_userstruct args = {
    fd,
    aCategoryName,
    PR_TRUE
  };

  PR_Lock(mLock);
  mTable.EnumerateEntries(enumfunc_pentries, &args);
  PR_Unlock(mLock);

  return args.success;
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
  static PLDHashOperator PR_CALLBACK
  enumfunc_createenumerator(const char* aStr,
                            CategoryNode* aNode,
                            void* userArg);
};

CategoryEnumerator*
CategoryEnumerator::Create(nsClassHashtable<nsDepCharHashKey, CategoryNode>& aTable)
{
  CategoryEnumerator* enumObj = new CategoryEnumerator();
  if (!enumObj)
    return nsnull;

  enumObj->mArray = new const char* [aTable.Count()];
  if (!enumObj->mArray) {
    delete enumObj;
    return nsnull;
  }

  aTable.EnumerateRead(enumfunc_createenumerator, enumObj);

  return enumObj;
}

PLDHashOperator PR_CALLBACK
CategoryEnumerator::enumfunc_createenumerator(const char* aStr, CategoryNode* aNode, void* userArg)
{
  CategoryEnumerator* mythis = NS_STATIC_CAST(CategoryEnumerator*, userArg);

  // if a category has no entries, we pretend it doesn't exist
  if (aNode->Count())
    mythis->mArray[mythis->mCount++] = aStr;

  return PL_DHASH_NEXT;
}


//
// nsCategoryManager implementations
//

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCategoryManager, nsICategoryManager)

nsCategoryManager*
nsCategoryManager::Create()
{
  nsCategoryManager* manager = new nsCategoryManager();
  
  if (!manager)
    return nsnull;

  PL_INIT_ARENA_POOL(&(manager->mArena), "CategoryManagerArena",
                     NS_CATEGORYMANAGER_ARENA_SIZE); // this never fails

  if (!manager->mTable.Init()) {
    delete manager;
    return nsnull;
  }

  manager->mLock = PR_NewLock();

  if (!manager->mLock) {
    delete manager;
    return nsnull;
  }

  return manager;
}

nsCategoryManager::~nsCategoryManager()
{
  if (mLock)
    PR_DestroyLock(mLock);

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
    return nsnull;
  }
  return node;
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

  PR_Lock(mLock);
  CategoryNode* category = get_category(aCategoryName);
  PR_Unlock(mLock);

  if (category) {
    status = category->GetLeaf(aEntryName, _retval);
  }

  return status;
}

NS_IMETHODIMP
nsCategoryManager::AddCategoryEntry( const char *aCategoryName,
                                     const char *aEntryName,
                                     const char *aValue,
                                     PRBool aPersist,
                                     PRBool aReplace,
                                     char **_retval )
{
  NS_ENSURE_ARG_POINTER(aCategoryName);
  NS_ENSURE_ARG_POINTER(aEntryName);
  NS_ENSURE_ARG_POINTER(aValue);

  // Before we can insert a new entry, we'll need to
  //  find the |CategoryNode| to put it in...
  PR_Lock(mLock);
  CategoryNode* category = get_category(aCategoryName);

  if (!category) {
    // That category doesn't exist yet; let's make it.
    category = CategoryNode::Create(&mArena);
        
    char* categoryName = ArenaStrdup(aCategoryName, &mArena);
    mTable.Put(categoryName, category);
  }
  PR_Unlock(mLock);

  if (!category)
    return NS_ERROR_OUT_OF_MEMORY;

  return category->AddLeaf(aEntryName,
                           aValue,
                           aPersist,
                           aReplace,
                           _retval,
                           &mArena);
}

NS_IMETHODIMP
nsCategoryManager::DeleteCategoryEntry( const char *aCategoryName,
                                        const char *aEntryName,
                                        PRBool aDontPersist)
{
  NS_ENSURE_ARG_POINTER(aCategoryName);
  NS_ENSURE_ARG_POINTER(aEntryName);

  /*
    Note: no errors are reported since failure to delete
    probably won't hurt you, and returning errors seriously
    inconveniences JS clients
  */

  PR_Lock(mLock);
  CategoryNode* category = get_category(aCategoryName);
  PR_Unlock(mLock);

  if (!category)
    return NS_OK;

  return category->DeleteLeaf(aEntryName,
                              aDontPersist);
}

NS_IMETHODIMP
nsCategoryManager::DeleteCategory( const char *aCategoryName )
{
  NS_ENSURE_ARG_POINTER(aCategoryName);

  // the categories are arena-allocated, so we don't
  // actually delete them. We just remove all of the
  // leaf nodes.

  PR_Lock(mLock);
  CategoryNode* category = get_category(aCategoryName);
  PR_Unlock(mLock);

  if (category)
    category->Clear();

  return NS_OK;
}

NS_IMETHODIMP
nsCategoryManager::EnumerateCategory( const char *aCategoryName,
                                      nsISimpleEnumerator **_retval )
{
  NS_ENSURE_ARG_POINTER(aCategoryName);
  NS_ENSURE_ARG_POINTER(_retval);

  PR_Lock(mLock);
  CategoryNode* category = get_category(aCategoryName);
  PR_Unlock(mLock);
  
  if (!category) {
    return NS_NewEmptyEnumerator(_retval);
  }

  return category->Enumerate(_retval);
}

NS_IMETHODIMP 
nsCategoryManager::EnumerateCategories(nsISimpleEnumerator **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  PR_Lock(mLock);
  CategoryEnumerator* enumObj = CategoryEnumerator::Create(mTable);
  PR_Unlock(mLock);

  if (!enumObj)
    return NS_ERROR_OUT_OF_MEMORY;

  *_retval = enumObj;
  NS_ADDREF(*_retval);
  return NS_OK;
}

struct writecat_struct {
  PRFileDesc* fd;
  PRBool      success;
};

PLDHashOperator PR_CALLBACK
enumfunc_categories(const char* aKey, CategoryNode* aCategory, void* userArg)
{
  writecat_struct* args = NS_STATIC_CAST(writecat_struct*, userArg);

  PLDHashOperator result = PL_DHASH_NEXT;

  if (!aCategory->WritePersistentEntries(args->fd, aKey)) {
    args->success = PR_FALSE;
    result = PL_DHASH_STOP;
  }

  return result;
}

NS_METHOD
nsCategoryManager::WriteCategoryManagerToRegistry(PRFileDesc* fd)
{
  writecat_struct args = {
    fd,
    PR_TRUE
  };

  PR_Lock(mLock);
  mTable.EnumerateRead(enumfunc_categories, &args);
  PR_Unlock(mLock);

  if (!args.success) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

class nsCategoryManagerFactory : public nsIFactory
   {
     public:
       nsCategoryManagerFactory() { }

       NS_DECL_ISUPPORTS
       NS_DECL_NSIFACTORY
   };

NS_IMPL_ISUPPORTS1(nsCategoryManagerFactory, nsIFactory)

NS_IMETHODIMP
nsCategoryManagerFactory::CreateInstance( nsISupports* aOuter, const nsIID& aIID, void** aResult )
  {
    NS_ENSURE_ARG_POINTER(aResult);

    *aResult = 0;

    nsresult status = NS_OK;
    if ( aOuter )
      status = NS_ERROR_NO_AGGREGATION;
    else
      {
        nsCategoryManager* raw_category_manager = nsCategoryManager::Create();
        nsCOMPtr<nsICategoryManager> new_category_manager = raw_category_manager;
        if ( new_category_manager )
              status = new_category_manager->QueryInterface(aIID, aResult);
        else
          status = NS_ERROR_OUT_OF_MEMORY;
      }

    return status;
  }

NS_IMETHODIMP
nsCategoryManagerFactory::LockFactory( PRBool )
  {
      // Not implemented...
    return NS_OK;
  }

nsresult
NS_CategoryManagerGetFactory( nsIFactory** aFactory )
  {
    // assert(aFactory);

    nsresult status;

    *aFactory = 0;
    nsIFactory* new_factory = NS_STATIC_CAST(nsIFactory*, new nsCategoryManagerFactory);
    if (new_factory)
      {
        *aFactory = new_factory;
        NS_ADDREF(*aFactory);
        status = NS_OK;
      }
    else
      status = NS_ERROR_OUT_OF_MEMORY;

    return status;
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
NS_COM nsresult
NS_CreateServicesFromCategory(const char *category,
                              nsISupports *origin,
                              const char *observerTopic)
{
    nsresult rv = NS_OK;
    
    int nFailed = 0; 
    nsCOMPtr<nsICategoryManager> categoryManager = 
        do_GetService("@mozilla.org/categorymanager;1", &rv);
    if (!categoryManager) return rv;

    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = categoryManager->EnumerateCategory(category, 
            getter_AddRefs(enumerator));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISupports> entry;
    while (NS_SUCCEEDED(enumerator->GetNext(getter_AddRefs(entry)))) {
        // From here on just skip any error we get.
        nsCOMPtr<nsISupportsCString> catEntry = do_QueryInterface(entry, &rv);
        if (NS_FAILED(rv)) {
            nFailed++;
            continue;
        }
        nsCAutoString entryString;
        rv = catEntry->GetData(entryString);
        if (NS_FAILED(rv)) {
            nFailed++;
            continue;
        }
        nsXPIDLCString contractID;
        rv = categoryManager->GetCategoryEntry(category,entryString.get(), getter_Copies(contractID));
        if (NS_FAILED(rv)) {
            nFailed++;
            continue;
        }
        
        nsCOMPtr<nsISupports> instance = do_GetService(contractID, &rv);
        if (NS_FAILED(rv)) {
            nFailed++;
            continue;
        }

        if (observerTopic) {
            // try an observer, if it implements it.
            nsCOMPtr<nsIObserver> observer = do_QueryInterface(instance, &rv);
            if (NS_SUCCEEDED(rv) && observer)
                observer->Observe(origin, observerTopic, EmptyString().get());
        }
    }
    return (nFailed ? NS_ERROR_FAILURE : NS_OK);
}
