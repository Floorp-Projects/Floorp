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
#include "nsIFactory.h"
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
    /*
      ...works with |nsHashtableEnumerator| to make the hash keys enumerable.
    */
  {
    nsresult status;
    nsCOMPtr<nsISupportsString> obj = do_CreateInstance(NS_SUPPORTS_STRING_PROGID, &status);
    if ( obj )
      {
          // BULLSHIT ALERT: use |nsCAutoString| to deflate to single-byte until I can add, e.g.,
          //  |nsCStringKey| to hashtable
        const nsString& s = NS_STATIC_CAST(nsStringKey*, key)->GetString();
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
      virtual ~nsCategoryManager();

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
    : nsObjectHashtable(0, 0, Destroy_CategoryNode, 0)
  {
    NS_INIT_REFCNT();
  }

nsresult
nsCategoryManager::initialize()
  {
    // BULLSHIT ALERT: need more consistent error handling in this routine
 
 
    const char* kCategoriesRegistryPath = "Software/Mozilla/XPCOM/Categories";
      // Alas, this is kind of buried down here, but you can't put constant strings in a class declaration... oh, well


      // Get a pointer to the registry, and get it open and ready for us

    nsresult rv;
    if ( mRegistry = do_GetService(NS_REGISTRY_PROGID, &rv) )
      if ( NS_SUCCEEDED(rv = mRegistry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry)) )
        if ( (rv = mRegistry->GetSubtree(nsIRegistry::Common, kCategoriesRegistryPath, &mCategoriesRegistryKey)) == NS_ERROR_REG_NOT_FOUND )
          rv = mRegistry->AddSubtree(nsIRegistry::Common, kCategoriesRegistryPath, &mCategoriesRegistryKey);


      // Now load the registry data

    if ( NS_SUCCEEDED(rv) )
      {
        nsCOMPtr<nsIEnumerator> keys;
        rv = mRegistry->EnumerateSubtrees(mCategoriesRegistryKey, getter_AddRefs(keys));
        for ( keys->First(); keys->IsDone() == NS_ENUMERATOR_FALSE; keys->Next() )
          {
            nsXPIDLCString categoryName;
            nsRegistryKey categoryKey;

            {
              nsCOMPtr<nsISupports> supportsNode;
              keys->CurrentItem(getter_AddRefs(supportsNode));

              nsCOMPtr<nsIRegistryNode> registryNode = do_QueryInterface(supportsNode);
              registryNode->GetName(getter_Copies(categoryName));
              registryNode->GetKey(&categoryKey);
            }

            nsCOMPtr<nsIEnumerator> values;
            mRegistry->EnumerateValues(categoryKey, getter_AddRefs(values));
            for ( values->First(); values->IsDone() == NS_ENUMERATOR_FALSE; values->Next() )
              {
                nsXPIDLCString entryName;

                {
                  nsCOMPtr<nsISupports> supportsValue;
                  values->CurrentItem(getter_AddRefs(supportsValue));

                  nsCOMPtr<nsIRegistryValue> registryValue = do_QueryInterface(supportsValue);
                  registryValue->GetName(getter_Copies(entryName));
                }
                
                nsXPIDLCString value;
                mRegistry->GetString(categoryKey, entryName, getter_Copies(value));
                AddCategoryEntry(categoryName, entryName, value, PR_FALSE, PR_FALSE, 0);
              }
          }
      }

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
    NS_ASSERTION(mRegistry, "mRegistry is NULL!");

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
    NS_ASSERTION(aCategoryName, "aCategoryName is NULL!");
    NS_ASSERTION(aKey,          "aKey is NULL!");
    NS_ASSERTION(mRegistry,     "mRegistry is NULL!");

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
      // BULLSHIT ALERT: Category `handler's currently not implemented, so just call through
    return GetCategoryEntryRaw(aCategoryName, aEntryName, _retval);
  }



NS_IMETHODIMP
nsCategoryManager::GetCategoryEntryRaw( const char *aCategoryName,
                                        const char *aEntryName,
                                        char **_retval )
  {
    NS_ASSERTION(aCategoryName, "aCategoryName is NULL!");
    NS_ASSERTION(aEntryName,    "aEntryName is NULL!");
    NS_ASSERTION(_retval,       "_retval is NULL!");

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
    NS_ASSERTION(aCategoryName, "aCategoryName is NULL!");
    NS_ASSERTION(aEntryName,    "aEntryName is NULL!");
    NS_ASSERTION(aValue,        "aValue is NULL!");


      /*
        Note: if |_retval| is |NULL|, I won't bother returning a copy
        of the replaced value.
      */

    if ( _retval )
      *_retval = 0;



    nsresult status = NS_OK;

    CategoryNode* category;
    if ( !(category = find_category(aCategoryName)) )
      {
        category = new CategoryNode;
        nsStringKey categoryNameKey(aCategoryName);
        Put(&categoryNameKey, category);
      }

    LeafNode* entry = category->find_leaf(aEntryName);

    if ( entry )
      {
        if ( aReplace )
          {
            if ( _retval )
              *_retval = nsXPIDLCString::Copy(*entry);
          }
        else
          status = NS_ERROR_INVALID_ARG;
      }

    entry = new LeafNode(aValue);
    nsStringKey entryNameKey(aEntryName);
    category->Put(&entryNameKey, entry);

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
    NS_ASSERTION(aCategoryName, "aCategoryName is NULL!");
    NS_ASSERTION(aEntryName,    "aEntryName is NULL!");
    NS_ASSERTION(_retval,       "_retval is NULL!");


      /*
        Note: no errors are reported since failure to delete
        probably won't hurt you, and returning errors seriously
        inconveniences JS clients
      */

    if ( CategoryNode* category = find_category(aCategoryName) )
      {
        nsStringKey entryKey(aEntryName);
        category->RemoveAndDelete(&entryKey);
      }

    if ( aDontPersist )
      dont_persist(aCategoryName, aEntryName);

    return NS_OK;
  }



NS_IMETHODIMP
nsCategoryManager::DeleteCategory( const char *aCategoryName )
  {
    NS_ASSERTION(aCategoryName, "aCategoryName is NULL!");

      // QUESTION: consider whether this should be an error
    nsStringKey categoryKey(aCategoryName);
    return RemoveAndDelete(&categoryKey) ? NS_OK : NS_ERROR_NOT_AVAILABLE;
  }





NS_IMETHODIMP
nsCategoryManager::EnumerateCategory( const char *aCategoryName,
                                      nsISimpleEnumerator **_retval )
  {
    NS_ASSERTION(aCategoryName, "aCategoryName is NULL!");
    NS_ASSERTION(_retval,       "_retval is NULL!");

    *_retval = 0;

    nsresult status = NS_ERROR_NOT_AVAILABLE;
    if ( CategoryNode* category = find_category(aCategoryName) )
      {
        nsCOMPtr<nsIEnumerator> innerEnumerator;
        if ( NS_SUCCEEDED(status = NS_NewHashtableEnumerator(category, ExtractKeyString, 0, getter_AddRefs(innerEnumerator))) )
          status = NS_NewAdapterEnumerator(_retval, innerEnumerator);
      }

    if ( !NS_SUCCEEDED(status) )
      status = NS_NewEmptyEnumerator(_retval);

    return status;
  }



NS_IMETHODIMP
nsCategoryManager::GetCategoryContents( const char *category,
                                        char ***entries,
                                        char ***values,
                                        PRInt32 *count )
  {
      // BULLSHIT ALERT: Wasn't implemented in JS either.
      //  Will people use this?  If not, let's get rid of it
    return NS_ERROR_NOT_IMPLEMENTED;
  }



NS_IMETHODIMP
nsCategoryManager::RegisterCategoryHandler( const char*          /* aCategoryName */,
                                            nsICategoryHandler*  /* aHandler */,
                                            PRInt32              /* aMode */,
                                            nsICategoryHandler** /* _retval */ )
  {
      // BULLSHIT ALERT: Category `handler's currently not implemented
    return NS_ERROR_NOT_IMPLEMENTED;
  }



NS_IMETHODIMP
nsCategoryManager::UnregisterCategoryHandler( const char *category,
                                              nsICategoryHandler *handler,
                                              nsICategoryHandler *previous )
  {
      // BULLSHIT ALERT: Category `handler's currently not implemented.
      //  Wasn't implemented in the JS version either.
    return NS_ERROR_NOT_IMPLEMENTED;
  }
 
 
class nsCategoryManagerFactory : public nsIFactory
   {
     public:
       nsCategoryManagerFactory();

       NS_DECL_ISUPPORTS
      NS_DECL_NSIFACTORY
   };

NS_IMPL_ISUPPORTS1(nsCategoryManagerFactory, nsIFactory)

nsCategoryManagerFactory::nsCategoryManagerFactory()
  {
    NS_INIT_REFCNT();
  }

NS_IMETHODIMP
nsCategoryManagerFactory::CreateInstance( nsISupports* aOuter, const nsIID& aIID, void** aResult )
  {
    // assert(aResult);

    *aResult = 0;

    nsresult status = NS_OK;
    if ( aOuter )
      status = NS_ERROR_NO_AGGREGATION;
    else
      {
         nsCOMPtr<nsICategoryManager> new_category_manager = new nsCategoryManager;
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

extern "C"
NS_EXPORT
nsresult
NS_CategoryManagerGetFactory( nsIFactory** aFactory )
  {
    // assert(aFactory);

    nsresult status;

    *aFactory = 0;
    if ( nsIFactory* new_factory = NS_STATIC_CAST(nsIFactory*, new nsCategoryManagerFactory) )
      {
        *aFactory = new_factory;
        NS_ADDREF(*aFactory);
        status = NS_OK;
      }
    else
      status = NS_ERROR_OUT_OF_MEMORY;

    return status;
  }
