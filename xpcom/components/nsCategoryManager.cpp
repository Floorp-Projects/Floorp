/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@netscape.com>
 */

#include "nsICategoryManager.h"

#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsIRegistry.h"
#include "nsISupportsPrimitives.h"
#include "nsComponentManager.h"

#include "nsHashtableEnumerator.h"
#include "nsEnumeratorUtils.h"


  /*
    CategoryDatabase
      contains 0 or more 1-1 mappings of string to Category
        each Category contains 0 or more 1-1 mappings of string keys to string values

    In other words, the CategoryDatabase is a tree, whose root is a hashtable.
    Internal nodes (or Categories) are hashtables.
    Leaf nodes are strings.
  */








static
NS_IMETHODIMP
ExtractKeyString( nsHashKey* key, void*, void*, nsISupports** _retval )
  {
    nsresult status;
    nsCOMPtr<nsISupportsString> obj = do_CreateInstance(NS_SUPPORTS_STRING_PROGID, &status);
    if ( obj )
      {
        const nsString& s = NS_STATIC_CAST(nsStringKey*, key)->GetString();
          // BULLSHIT ALERT: deflate to single byte until I can add, e.g., |nsCStringKey| to hashtable
        status = obj->SetDataWithLength(s.Length(), nsCAutoString(s));
      }

    *_retval = obj;
    NS_IF_ADDREF(*_retval);
    return status;
  }



typedef nsCString LeafNode;

  /*
    Our interior nodes are hashtables whose elements are |LeafNode|s,
    and we need a suitable destruction function to register with a
    given (interior node) hashtable for destroying its (|LeafNode|) elements.
  */
static
PRBool
Destroy_LeafNode( nsHashKey*, void* aElement, void* )
  {
    delete NS_STATIC_CAST(LeafNode*, aElement);
    return PR_TRUE;
  }


class CategoryNode
    : public nsObjectHashtable
  {
    public:
      CategoryNode()
          : nsObjectHashtable(0, 0, Destroy_LeafNode, 0)
        {
          // Nothing else to do here...
        }

      LeafNode* find_leaf( const char* );
  };

LeafNode*
CategoryNode::find_leaf( const char* aLeafName )
  {
    nsStringKey leafNameKey(aLeafName);
    return NS_STATIC_CAST(LeafNode*, Get(&leafNameKey));
  }

  /*
    We keep a hashtable of hashtables, therefore, we need a suitable
    destruction function to register with the outer table for destroying
    elements which are the inner tables.
  */
static
PRBool
Destroy_CategoryNode( nsHashKey*, void* aElement, void* )
  {
    delete NS_STATIC_CAST(CategoryNode*, aElement);
    return PR_TRUE;
  }







class nsCategoryManager
    : public nsICategoryManager,
      public nsObjectHashtable
  {
    public:
      nsCategoryManager();
      ~nsCategoryManager();

      NS_DECL_ISUPPORTS
      NS_DECL_NSICATEGORYMANAGER

    private:
      nsresult initialize();
      CategoryNode* find_category( const char* );
      nsresult persist( const char* aCategoryName, const char* aKey, const char* aValue );
      nsresult dont_persist( const char* aCategoryName, const char* aKey );

    private:
      nsCOMPtr<nsIRegistry>  mRegistry;
      nsRegistryKey          mCategoriesRegistryKey;
  };

NS_IMPL_ISUPPORTS1(nsCategoryManager, nsICategoryManager)

nsCategoryManager::nsCategoryManager()
    : nsObjectHashtable(0, 0, Destroy_CategoryNode, 0),
      mRegistry( do_GetService(NS_REGISTRY_PROGID) )
  {
    // assert(mRegistry);

  }

nsresult
nsCategoryManager::initialize()
  {
    const char* kCategoriesRegistryPath = "Software/Mozilla/XPCOM/Categories";
      // Alas, this is kind of buried down here, but you can't put constant strings in a class declaration... oh, well

    nsresult rv;
    if ( mRegistry = do_GetService(NS_REGISTRY_PROGID, &rv) )
      if ( NS_SUCCEEDED(rv = mRegistry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry)) )
        if ( (rv = mRegistry->GetSubtree(nsIRegistry::Common, kCategoriesRegistryPath, &mCategoriesRegistryKey)) == NS_ERROR_REG_NOT_FOUND )
          rv = mRegistry->AddSubtree(nsIRegistry::Common, kCategoriesRegistryPath, &mCategoriesRegistryKey);

    return rv;
  }

nsCategoryManager::~nsCategoryManager()
  {
    if ( mRegistry )
      mRegistry->Close();
  }

CategoryNode*
nsCategoryManager::find_category( const char* aCategoryName )
  {
    nsStringKey categoryNameKey(aCategoryName);
    return NS_STATIC_CAST(CategoryNode*, Get(&categoryNameKey));
  }

nsresult
nsCategoryManager::persist( const char* aCategoryName, const char* aKey, const char* aValue )
  {
    // assert(mRegistry);

    nsRegistryKey categoryRegistryKey;
    nsresult status = mRegistry->GetSubtreeRaw(mCategoriesRegistryKey, aCategoryName, &categoryRegistryKey);

    if ( status == NS_ERROR_REG_NOT_FOUND )
      status = mRegistry->AddSubtreeRaw(mCategoriesRegistryKey, aCategoryName, &categoryRegistryKey);

    if ( NS_SUCCEEDED(status) )
      status = mRegistry->SetString(categoryRegistryKey, aKey, aValue);

    return status;
  }

nsresult
nsCategoryManager::dont_persist( const char* aCategoryName, const char* aKey )
  {
    // assert(mRegistry);

    nsRegistryKey categoryRegistryKey;
    nsresult status = mRegistry->GetSubtreeRaw(mCategoriesRegistryKey, aCategoryName, &categoryRegistryKey);

    if ( NS_SUCCEEDED(status) )
      status = mRegistry->DeleteValue(categoryRegistryKey, aKey);

    return status;
  }



NS_IMETHODIMP
nsCategoryManager::GetCategoryEntry( const char *aCategoryName,
                                     const char *aEntryName,
                                     char **_retval )
  {
      // Category `handler's currently not implemented, so just call through
    return GetCategoryEntryRaw(aCategoryName, aEntryName, _retval);
  }



NS_IMETHODIMP
nsCategoryManager::GetCategoryEntryRaw( const char *aCategoryName,
                                        const char *aEntryName,
                                        char **_retval )
  {
    // assert(_retval);

    nsresult status = NS_ERROR_NOT_AVAILABLE;
    if ( CategoryNode* category = find_category(aCategoryName) )
      {
        nsStringKey entryKey(aEntryName);
        if ( LeafNode* entry = NS_STATIC_CAST(LeafNode*, category->Get(&entryKey)) )
          status = (*_retval = nsXPIDLCString::Copy(*entry)) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
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
    // assert(_retval);

    nsresult status;

    CategoryNode* category;
    if ( !(category = find_category(aCategoryName)) )
      {
        // Create and add a category
      }

    LeafNode* entry = category->find_leaf(aEntryName);

    // BULLSHIT ALERT: more stuff here

    if ( aPersist )
      persist(aCategoryName, aEntryName, aValue);

    return status;
  }



NS_IMETHODIMP
nsCategoryManager::DeleteCategoryEntry( const char *aCategoryName,
                                        const char *aEntryName,
                                        PRBool aDontPersist,
                                        char **_retval )
  {
      // BULLSHIT ALERT: consider whether this should be an error
    nsresult status = NS_ERROR_NOT_AVAILABLE;

    if ( CategoryNode* category = find_category(aCategoryName) )
      {
        nsStringKey entryKey(aEntryName);
        if ( category->RemoveAndDelete(&entryKey) )
          status = NS_OK;
      }

    if ( aDontPersist )
      dont_persist(aCategoryName, aEntryName);

    return status;
  }



NS_IMETHODIMP
nsCategoryManager::DeleteCategory( const char *aCategoryName )
  {
      // BULLSHIT ALERT: consider whether this should be an error
    nsStringKey categoryKey(aCategoryName);
    return RemoveAndDelete(&categoryKey) ? NS_OK : NS_ERROR_NOT_AVAILABLE;
  }





NS_IMETHODIMP
nsCategoryManager::EnumerateCategory( const char *aCategoryName,
                                      nsISimpleEnumerator **_retval )
  {
      // BULLSHIT ALERT: consider whether this should be an error
      // NS_NewEmptyEnumerator()?

    // assert(_retval);
    *_retval = 0;

    nsresult status = NS_ERROR_NOT_AVAILABLE;
    if ( CategoryNode* category = find_category(aCategoryName) )
      {
        nsCOMPtr<nsIEnumerator> innerEnumerator;
        if ( NS_SUCCEEDED(status = NS_NewHashtableEnumerator(category, ExtractKeyString, 0, getter_AddRefs(innerEnumerator))) )
          status = NS_NewAdapterEnumerator(_retval, innerEnumerator);
      }

    return status;
  }



NS_IMETHODIMP
nsCategoryManager::GetCategoryContents( const char *category,
                                        char ***entries,
                                        char ***values,
                                        PRInt32 *count )
  {
      // Wasn't implemented in JS either.  Will people use this?
      //  If not, let's get rid of it
    return NS_ERROR_NOT_IMPLEMENTED;
  }



NS_IMETHODIMP
nsCategoryManager::RegisterCategoryHandler( const char*          /* aCategoryName */,
                                            nsICategoryHandler*  /* aHandler */,
                                            PRInt32              /* aMode */,
                                            nsICategoryHandler** /* _retval */ )
  {
      // Category `handler's currently not implemented
    return NS_ERROR_NOT_IMPLEMENTED;
  }



NS_IMETHODIMP
nsCategoryManager::UnregisterCategoryHandler( const char *category,
                                              nsICategoryHandler *handler,
                                              nsICategoryHandler *previous )
  {
      // Category `handler's currently not implemented.
      //  Wasn't implemented in the JS version either.
    return NS_ERROR_NOT_IMPLEMENTED;
  }
 
