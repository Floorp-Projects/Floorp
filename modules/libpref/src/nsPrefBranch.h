/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPrefLocalizedString.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIRelativeFilePref.h"
#include "nsILocalFile.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsTArray.h"
#include "nsWeakReference.h"
#include "nsClassHashtable.h"
#include "nsCRT.h"
#include "prbit.h"
#include "nsTraceRefcnt.h"
#include "mozilla/HashFunctions.h"

class nsPrefBranch;

class PrefCallback : public PLDHashEntryHdr {

  public:
    typedef PrefCallback* KeyType;
    typedef const PrefCallback* KeyTypePointer;

    static const PrefCallback* KeyToPointer(PrefCallback *aKey)
    {
      return aKey;
    }

    static PLDHashNumber HashKey(const PrefCallback *aKey)
    {
      PRUint32 hash = mozilla::HashString(aKey->mDomain);
      return mozilla::AddToHash(hash, aKey->mCanonical);
    }


  public:
    // Create a PrefCallback with a strong reference to its observer.
    PrefCallback(const char *aDomain, nsIObserver *aObserver,
                 nsPrefBranch *aBranch)
      : mDomain(aDomain),
        mBranch(aBranch),
        mWeakRef(nsnull),
        mStrongRef(aObserver)
    {
      MOZ_COUNT_CTOR(PrefCallback);
      nsCOMPtr<nsISupports> canonical = do_QueryInterface(aObserver);
      mCanonical = canonical;
    }

    // Create a PrefCallback with a weak reference to its observer.
    PrefCallback(const char *aDomain,
                 nsISupportsWeakReference *aObserver,
                 nsPrefBranch *aBranch)
      : mDomain(aDomain),
        mBranch(aBranch),
        mWeakRef(do_GetWeakReference(aObserver)),
        mStrongRef(nsnull)
    {
      MOZ_COUNT_CTOR(PrefCallback);
      nsCOMPtr<nsISupports> canonical = do_QueryInterface(aObserver);
      mCanonical = canonical;
    }

    // Copy constructor needs to be explicit or the linker complains.
    PrefCallback(const PrefCallback *&aCopy)
      : mDomain(aCopy->mDomain),
        mBranch(aCopy->mBranch),
        mWeakRef(aCopy->mWeakRef),
        mStrongRef(aCopy->mStrongRef),
        mCanonical(aCopy->mCanonical)
    {
      MOZ_COUNT_CTOR(PrefCallback);
    }

    ~PrefCallback()
    {
      MOZ_COUNT_DTOR(PrefCallback);
    }

    bool KeyEquals(const PrefCallback *aKey) const
    {
      // We want to be able to look up a weakly-referencing PrefCallback after
      // its observer has died so we can remove it from the table.  Once the
      // callback's observer dies, its canonical pointer is stale -- in
      // particular, we may have allocated a new observer in the same spot in
      // memory!  So we can't just compare canonical pointers to determine
      // whether aKey refers to the same observer as this.
      //
      // Our workaround is based on the way we use this hashtable: When we ask
      // the hashtable to remove a PrefCallback whose weak reference has
      // expired, we use as the key for removal the same object as was inserted
      // into the hashtable.  Thus we can say that if one of the keys' weak
      // references has expired, the two keys are equal iff they're the same
      // object.

      if (IsExpired() || aKey->IsExpired())
        return this == aKey;

      if (mCanonical != aKey->mCanonical)
        return false;

      return mDomain.Equals(aKey->mDomain);
    }

    PrefCallback *GetKey() const
    {
      return const_cast<PrefCallback*>(this);
    }

    // Get a reference to the callback's observer, or null if the observer was
    // weakly referenced and has been destroyed.
    already_AddRefed<nsIObserver> GetObserver() const
    {
      if (!IsWeak()) {
        nsCOMPtr<nsIObserver> copy = mStrongRef;
        return copy.forget();
      }

      nsCOMPtr<nsIObserver> observer = do_QueryReferent(mWeakRef);
      return observer.forget();
    }

    const nsCString& GetDomain() const
    {
      return mDomain;
    }

    nsPrefBranch* GetPrefBranch() const
    {
      return mBranch;
    }

    // Has this callback's weak reference died?
    bool IsExpired() const
    {
      if (!IsWeak())
        return false;

      nsCOMPtr<nsIObserver> observer(do_QueryReferent(mWeakRef));
      return !observer;
    }

    enum { ALLOW_MEMMOVE = true };

  private:
    nsCString             mDomain;
    nsPrefBranch         *mBranch;

    // Exactly one of mWeakRef and mStrongRef should be non-null.
    nsWeakPtr             mWeakRef;
    nsCOMPtr<nsIObserver> mStrongRef;

    // We need a canonical nsISupports pointer, per bug 578392.
    nsISupports          *mCanonical;

    bool IsWeak() const
    {
      return !!mWeakRef;
    }
};

class nsPrefBranch : public nsIPrefBranchInternal,
                     public nsIObserver,
                     public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPREFBRANCH
  NS_DECL_NSIPREFBRANCH2
  NS_DECL_NSIOBSERVER

  nsPrefBranch(const char *aPrefRoot, bool aDefaultBranch);
  virtual ~nsPrefBranch();

  PRInt32 GetRootLength() { return mPrefRootLength; }

  nsresult RemoveObserverFromMap(const char *aDomain, nsISupports *aObserver);

  static nsresult NotifyObserver(const char *newpref, void *data);

protected:
  nsPrefBranch()    /* disallow use of this constructer */
    { }

  nsresult   GetDefaultFromPropertiesFile(const char *aPrefName, PRUnichar **return_buf);
  void RemoveExpiredCallback(PrefCallback *aCallback);
  const char *getPrefName(const char *aPrefName);
  void       freeObserverList(void);

  friend PLDHashOperator
    FreeObserverFunc(PrefCallback *aKey,
                     nsAutoPtr<PrefCallback> &aCallback,
                     void *aArgs);

private:
  PRInt32               mPrefRootLength;
  nsCString             mPrefRoot;
  bool                  mIsDefault;

  bool                  mFreeingObserverList;
  nsClassHashtable<PrefCallback, PrefCallback> mObservers;
};


class nsPrefLocalizedString : public nsIPrefLocalizedString,
                              public nsISupportsString
{
public:
  nsPrefLocalizedString();
  virtual ~nsPrefLocalizedString();

  NS_DECL_ISUPPORTS
  NS_FORWARD_NSISUPPORTSSTRING(mUnicodeString->)
  NS_FORWARD_NSISUPPORTSPRIMITIVE(mUnicodeString->)

  nsresult Init();

private:
  NS_IMETHOD GetData(PRUnichar**);
  NS_IMETHOD SetData(const PRUnichar* aData);
  NS_IMETHOD SetDataWithLength(PRUint32 aLength, const PRUnichar *aData);

  nsCOMPtr<nsISupportsString> mUnicodeString;
};


class nsRelativeFilePref : public nsIRelativeFilePref
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRELATIVEFILEPREF
  
                nsRelativeFilePref();
  virtual       ~nsRelativeFilePref();
  
private:
  nsCOMPtr<nsILocalFile> mFile;
  nsCString mRelativeToKey;
};
