/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsICategoryManager.h"

#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsIFactory.h"
#include "nsIRegistry.h"
#include "nsISupportsPrimitives.h"
#include "nsIObserver.h"
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
  nsCOMPtr<nsISupportsString> obj = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &status);
  if ( obj ) {
    nsCStringKey* strkey = NS_STATIC_CAST(nsCStringKey*, key);
    status = obj->SetDataWithLength(strkey->GetStringLength(), strkey->GetString());
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
          : nsObjectHashtable((nsHashtableCloneElementFunc) 0, 0,
                                (nsHashtableEnumFunc) Destroy_LeafNode, 0 )
        {
          // Nothing else to do here...
        }

      LeafNode* find_leaf( const char* );
  };

LeafNode*
CategoryNode::find_leaf( const char* aLeafName )
  {
    nsCStringKey leafNameKey(aLeafName);
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
    private:
      friend class nsCategoryManagerFactory;
      nsCategoryManager();
      nsresult initialize();
        // Warning: whoever creates an instance must call |initialize()| before any other method

    public:
        virtual ~nsCategoryManager();

      NS_DECL_ISUPPORTS
      NS_DECL_NSICATEGORYMANAGER

    private:
      CategoryNode* find_category( const char* );
      nsresult persist( const char* aCategoryName, const char* aKey, const char* aValue );
      nsresult dont_persist( const char* aCategoryName, const char* aKey );

    private:
        // |mRegistry != 0| after |initialize()|... I can't live without it
      nsCOMPtr<nsIRegistry>  mRegistry;
      nsRegistryKey          mCategoriesRegistryKey;
  };

NS_IMPL_ISUPPORTS1(nsCategoryManager, nsICategoryManager)

nsCategoryManager::nsCategoryManager()
    : nsObjectHashtable((nsHashtableCloneElementFunc) 0, 0,
                                (nsHashtableEnumFunc) Destroy_CategoryNode, 0 )

  {
    NS_INIT_REFCNT();
  }

nsresult
nsCategoryManager::initialize()
  {
    const char* kCategoriesRegistryPath = "software/mozilla/XPCOM/categories";
      // Alas, this is kind of buried down here, but you can't put constant strings in a class declaration... oh, well


      // Get a pointer to the registry, and get it open and ready for us

    nsresult status;
    if ( mRegistry = do_GetService(NS_REGISTRY_CONTRACTID, &status) )
      if ( NS_SUCCEEDED(status = mRegistry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry)) )
        if ( (status = mRegistry->GetSubtree(nsIRegistry::Common, kCategoriesRegistryPath, &mCategoriesRegistryKey)) == NS_ERROR_REG_NOT_FOUND )
          status = mRegistry->AddSubtree(nsIRegistry::Common, kCategoriesRegistryPath, &mCategoriesRegistryKey);

      // All right, we've got the registry now (or not), that's the important thing.
      //  Returning an error will cause callers to destroy me.  So not getting the registry
      //  is worth returning an error for (I can't live without it).  Now we'll load in our
      //  persistent data from the registry, but that's _not_ worth getting killed over, so
      //  don't save any errors from this process.

      // Now load the registry data

    if ( NS_SUCCEEDED(status) )
      {
        nsCOMPtr<nsIEnumerator> keys;
        mRegistry->EnumerateSubtrees(mCategoriesRegistryKey, getter_AddRefs(keys));
        for ( keys->First(); keys->IsDone() == NS_ENUMERATOR_FALSE; keys->Next() )
          {
            nsXPIDLCString categoryName;
            nsRegistryKey categoryKey;

            {
              nsCOMPtr<nsISupports> supportsNode;
              keys->CurrentItem(getter_AddRefs(supportsNode));

              nsCOMPtr<nsIRegistryNode> registryNode = do_QueryInterface(supportsNode);
              registryNode->GetNameUTF8(getter_Copies(categoryName));
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
                  registryValue->GetNameUTF8(getter_Copies(entryName));
                }
                
                nsXPIDLCString value;
                mRegistry->GetStringUTF8(categoryKey, entryName, getter_Copies(value));
                AddCategoryEntry(categoryName, entryName, value, PR_FALSE, PR_FALSE, 0);
              }
          }
      }

    return status;
  }

nsCategoryManager::~nsCategoryManager()
  {
  }

CategoryNode*
nsCategoryManager::find_category( const char* aCategoryName )
  {
    nsCStringKey categoryNameKey(aCategoryName);
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
      status = mRegistry->SetStringUTF8(categoryRegistryKey, aKey, aValue);

    return status;
  }

nsresult
nsCategoryManager::dont_persist( const char* aCategoryName, const char* aKey )
  {
    NS_ASSERTION(mRegistry, "mRegistry is NULL!");

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
    CategoryNode* category = find_category(aCategoryName);
    if (category) 
      {
        nsCStringKey entryKey(aEntryName);
        LeafNode* entry = NS_STATIC_CAST(LeafNode*, category->Get(&entryKey));
        if (entry)
          status = (*_retval = nsCRT::strdup(*entry)) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
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


      // Before we can insert a new entry, we'll need to
      //  find the |CategoryNode| to put it in...
    CategoryNode* category;
    if ( !(category = find_category(aCategoryName)) )
      {
          // That category doesn't exist yet; let's make it.
        category = new CategoryNode;
        nsCStringKey categoryNameKey(aCategoryName);
        Put(&categoryNameKey, category);
      }

      // See if this entry is already in this category
    LeafNode* entry = category->find_leaf(aEntryName);

    nsresult status = NS_OK;
    if ( entry )
      {
          // If this entry is in the category already,
          //  then you better have said 'replace'!

        if ( aReplace )
          {
              // return the value that we're replacing
            if ( _retval )
              *_retval = nsCRT::strdup(*entry);
          }
        else
          status = NS_ERROR_INVALID_ARG; // ...stops us from putting the value in
      }

      // If you didn't say 'replace', and there was already an entry there,
      //  then we can't put your value in (that's why we set |status|, above),
      //  or make it persistent (see below)

    if ( NS_SUCCEEDED(status) )
      { // it's OK to put a value in

          // don't leak the entry we're replacing (if any)
        delete entry;

          // now put in the new vaentrylue
        entry = new LeafNode(aValue);
        nsCStringKey entryNameKey(aEntryName);
        category->Put(&entryNameKey, entry);

          // If you said 'persist' but not 'replace', and an entry
          //  got in the way ... sorry, you won't go into the persistent store.
          //  This was probably an error on your part.  If this was _really_
          //  what you wanted to do, i.e., have a different value in the backing
          //  store than in the live database, then do it by setting the value
          //  with 'persist' and 'replace' and saving the returned old value, then
          //  restoring the old value with 'replace' but _not_ 'persist'.

        if ( aPersist )
          status = persist(aCategoryName, aEntryName, aValue);
      }

    return status;
  }



NS_IMETHODIMP
nsCategoryManager::DeleteCategoryEntry( const char *aCategoryName,
                                        const char *aEntryName,
                                        PRBool aDontPersist)
  {

    NS_ASSERTION(aCategoryName, "aCategoryName is NULL!");
    NS_ASSERTION(aEntryName,    "aEntryName is NULL!");

      /*
        Note: no errors are reported since failure to delete
        probably won't hurt you, and returning errors seriously
        inconveniences JS clients
      */

    CategoryNode* category = find_category(aCategoryName);
    if (category)
      {
        nsCStringKey entryKey(aEntryName);
        category->RemoveAndDelete(&entryKey);
      }

    nsresult status = NS_OK;
    if ( aDontPersist )
      status = dont_persist(aCategoryName, aEntryName);

    return status;
  }



NS_IMETHODIMP
nsCategoryManager::DeleteCategory( const char *aCategoryName )
  {
    NS_ASSERTION(aCategoryName, "aCategoryName is NULL!");

      // QUESTION: consider whether this should be an error
    nsCStringKey categoryKey(aCategoryName);
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
    CategoryNode* category = find_category(aCategoryName);
    if (category)
      {
        nsCOMPtr<nsIEnumerator> innerEnumerator;
        if ( NS_SUCCEEDED(status = NS_NewHashtableEnumerator(category, ExtractKeyString, 0, getter_AddRefs(innerEnumerator))) )
          status = NS_NewAdapterEnumerator(_retval, innerEnumerator);
      }

      // If you couldn't find the category, or had trouble creating an enumerator...
    if ( !NS_SUCCEEDED(status) )
      {
        NS_IF_RELEASE(*_retval);
        status = NS_NewEmptyEnumerator(_retval);
      }

    return status;
  }



NS_IMETHODIMP
nsCategoryManager::GetCategoryContents( const char *category,
                                        char ***entries,
                                        char ***values,
                                        PRUint32 *count )
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
        nsCategoryManager* raw_category_manager;
        nsCOMPtr<nsICategoryManager> new_category_manager = (raw_category_manager = new nsCategoryManager);
        if ( new_category_manager )
          {
            if ( NS_SUCCEEDED(status = raw_category_manager->initialize()) )
              status = new_category_manager->QueryInterface(aIID, aResult);
          }
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
nsresult
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
        nsCOMPtr<nsISupportsString> catEntry = do_QueryInterface(entry, &rv);
        if (NS_FAILED(rv)) {
            nFailed++;
            continue;
        }
        nsXPIDLCString entryString;
        rv = catEntry->GetData(getter_Copies(entryString));
        if (NS_FAILED(rv)) {
            nFailed++;
            continue;
        }
        nsXPIDLCString contractID;
        rv = categoryManager->GetCategoryEntry(category,(const char *)entryString, getter_Copies(contractID));
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
                observer->Observe(origin, observerTopic, NS_LITERAL_STRING("").get());
        }
    }
    return (nFailed ? NS_ERROR_FAILURE : NS_OK);
}
