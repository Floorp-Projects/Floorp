/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCategoryCache_h_
#define nsCategoryCache_h_

#include "mozilla/Attributes.h"

#include "nsICategoryManager.h"
#include "nsIObserver.h"
#include "nsISimpleEnumerator.h"
#include "nsISupportsPrimitives.h"

#include "nsServiceManagerUtils.h"

#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsDataHashtable.h"

#include "nsXPCOM.h"

class NS_NO_VTABLE nsCategoryListener {
  protected:
    // no virtual destructor (people shouldn't delete through an
    // nsCategoryListener pointer)
    ~nsCategoryListener() {}

  public:
    virtual void EntryAdded(const nsCString& aValue) = 0;
    virtual void EntryRemoved(const nsCString& aValue) = 0;
    virtual void CategoryCleared() = 0;
};

class NS_COM_GLUE nsCategoryObserver MOZ_FINAL : public nsIObserver {
  public:
    nsCategoryObserver(const char* aCategory,
                       nsCategoryListener* aCategoryListener);
    ~nsCategoryObserver();

    void ListenerDied();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
  private:
    NS_HIDDEN_(void) RemoveObservers();

    nsDataHashtable<nsCStringHashKey, nsCString> mHash;
    nsCategoryListener*                          mListener;
    nsCString                                    mCategory;
    bool                                         mObserversRemoved;
};

/**
 * This is a helper class that caches services that are registered in a certain
 * category. The intended usage is that a service stores a variable of type
 * nsCategoryCache<nsIFoo> in a member variable, where nsIFoo is the interface
 * that these services should implement. The constructor of this class should
 * then get the name of the category.
 */
template<class T>
class nsCategoryCache MOZ_FINAL : protected nsCategoryListener {
  public:
    explicit nsCategoryCache(const char* aCategory);
    ~nsCategoryCache() { if (mObserver) mObserver->ListenerDied(); }

    const nsCOMArray<T>& GetEntries() {
      // Lazy initialization, so that services in this category can't
      // cause reentrant getService (bug 386376)
      if (!mObserver)
        mObserver = new nsCategoryObserver(mCategoryName.get(), this);
      return mEntries;
    }
  protected:
    virtual void EntryAdded(const nsCString& aValue);
    virtual void EntryRemoved(const nsCString& aValue);
    virtual void CategoryCleared();
  private:
    friend class CategoryObserver;

    // Not to be implemented
    nsCategoryCache(const nsCategoryCache<T>&);

    nsCString mCategoryName;
    nsCOMArray<T> mEntries;
    nsRefPtr<nsCategoryObserver> mObserver;
};

// -----------------------------------
// Implementation

template<class T>
nsCategoryCache<T>::nsCategoryCache(const char* aCategory)
: mCategoryName(aCategory)
{
}

template<class T>
void nsCategoryCache<T>::EntryAdded(const nsCString& aValue) {
  nsCOMPtr<T> catEntry = do_GetService(aValue.get());
  if (catEntry)
    mEntries.AppendObject(catEntry);
}

template<class T>
void nsCategoryCache<T>::EntryRemoved(const nsCString& aValue) {
  nsCOMPtr<T> catEntry = do_GetService(aValue.get());
  if (catEntry)
    mEntries.RemoveObject(catEntry);
}

template<class T>
void nsCategoryCache<T>::CategoryCleared() {
  mEntries.Clear();
}

#endif
