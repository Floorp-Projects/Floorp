/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpAuthCache_h__
#define nsHttpAuthCache_h__

#include "nsHttp.h"
#include "nsError.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsAString.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "plhash.h"
#include "nsCRT.h"
#include "nsIObserver.h"

struct nsHttpAuthPath {
    struct nsHttpAuthPath *mNext;
    char                   mPath[1];
};

//-----------------------------------------------------------------------------
// nsHttpAuthIdentity
//-----------------------------------------------------------------------------

class nsHttpAuthIdentity
{
public:
    nsHttpAuthIdentity()
        : mUser(nullptr)
        , mPass(nullptr)
        , mDomain(nullptr)
    {
    }
    nsHttpAuthIdentity(const PRUnichar *domain,
                       const PRUnichar *user,
                       const PRUnichar *password)
        : mUser(nullptr)
    {
        Set(domain, user, password);
    }
   ~nsHttpAuthIdentity()
    {
        Clear();
    }

    const PRUnichar *Domain()   const { return mDomain; }
    const PRUnichar *User()     const { return mUser; }
    const PRUnichar *Password() const { return mPass; }

    nsresult Set(const PRUnichar *domain,
                 const PRUnichar *user,
                 const PRUnichar *password);
    nsresult Set(const nsHttpAuthIdentity &other) { return Set(other.mDomain, other.mUser, other.mPass); }
    void Clear();

    bool Equals(const nsHttpAuthIdentity &other) const;
    bool IsEmpty() const { return !mUser; }

private:
    // allocated as one contiguous blob, starting at mUser.
    PRUnichar *mUser;
    PRUnichar *mPass;
    PRUnichar *mDomain;
};

//-----------------------------------------------------------------------------
// nsHttpAuthEntry
//-----------------------------------------------------------------------------

class nsHttpAuthEntry
{
public:
    const char *Realm()       const { return mRealm; }
    const char *Creds()       const { return mCreds; }
    const char *Challenge()   const { return mChallenge; }
    const PRUnichar *Domain() const { return mIdent.Domain(); }
    const PRUnichar *User()   const { return mIdent.User(); }
    const PRUnichar *Pass()   const { return mIdent.Password(); }
    nsHttpAuthPath *RootPath()      { return mRoot; }

    const nsHttpAuthIdentity &Identity() const { return mIdent; }

    nsresult AddPath(const char *aPath);

    nsCOMPtr<nsISupports> mMetaData;

private:
    nsHttpAuthEntry(const char *path,
                    const char *realm,
                    const char *creds,
                    const char *challenge,
                    const nsHttpAuthIdentity *ident,
                    nsISupports *metadata)
        : mRoot(nullptr)
        , mTail(nullptr)
        , mRealm(nullptr)
    {
        Set(path, realm, creds, challenge, ident, metadata);
    }
   ~nsHttpAuthEntry();

    nsresult Set(const char *path,
                 const char *realm,
                 const char *creds,
                 const char *challenge,
                 const nsHttpAuthIdentity *ident,
                 nsISupports *metadata);

    nsHttpAuthIdentity mIdent;

    nsHttpAuthPath *mRoot; //root pointer
    nsHttpAuthPath *mTail; //tail pointer

    // allocated together in one blob, starting with mRealm.
    char *mRealm;
    char *mCreds;
    char *mChallenge;

    friend class nsHttpAuthNode;
    friend class nsHttpAuthCache;
    friend class nsAutoPtr<nsHttpAuthEntry>; // needs to call the destructor
};

//-----------------------------------------------------------------------------
// nsHttpAuthNode
//-----------------------------------------------------------------------------

class nsHttpAuthNode
{
private:
    nsHttpAuthNode();
   ~nsHttpAuthNode();

    // path can be null, in which case we'll search for an entry
    // with a null path.
    nsHttpAuthEntry *LookupEntryByPath(const char *path);

    // realm must not be null
    nsHttpAuthEntry *LookupEntryByRealm(const char *realm);

    // if a matching entry is found, then credentials will be changed.
    nsresult SetAuthEntry(const char *path,
                          const char *realm,
                          const char *credentials,
                          const char *challenge,
                          const nsHttpAuthIdentity *ident,
                          nsISupports *metadata);

    void ClearAuthEntry(const char *realm);

    uint32_t EntryCount() { return mList.Length(); }

private:
    nsTArray<nsAutoPtr<nsHttpAuthEntry> > mList;

    friend class nsHttpAuthCache;
};

//-----------------------------------------------------------------------------
// nsHttpAuthCache
//  (holds a hash table from host:port to nsHttpAuthNode)
//-----------------------------------------------------------------------------

class nsHttpAuthCache
{
public:
    nsHttpAuthCache();
   ~nsHttpAuthCache();

    nsresult Init();

    // |scheme|, |host|, and |port| are required
    // |path| can be null
    // |entry| is either null or a weak reference
    nsresult GetAuthEntryForPath(const char *scheme,
                                 const char *host,
                                 int32_t     port,
                                 const char *path,
                                 uint32_t    appId,
                                 bool        inBrowserElement,
                                 nsHttpAuthEntry **entry);

    // |scheme|, |host|, and |port| are required
    // |realm| must not be null
    // |entry| is either null or a weak reference
    nsresult GetAuthEntryForDomain(const char *scheme,
                                   const char *host,
                                   int32_t     port,
                                   const char *realm,
                                   uint32_t    appId,
                                   bool        inBrowserElement,
                                   nsHttpAuthEntry **entry);

    // |scheme|, |host|, and |port| are required
    // |path| can be null
    // |realm| must not be null
    // if |credentials|, |user|, |pass|, and |challenge| are each
    // null, then the entry is deleted.
    nsresult SetAuthEntry(const char *scheme,
                          const char *host,
                          int32_t     port,
                          const char *directory,
                          const char *realm,
                          const char *credentials,
                          const char *challenge,
                          uint32_t    appId,
                          bool        inBrowserElement,
                          const nsHttpAuthIdentity *ident,
                          nsISupports *metadata);

    void ClearAuthEntry(const char *scheme,
                        const char *host,
                        int32_t     port,
                        const char *realm,
                        uint32_t    appId,
                        bool        inBrowserElement);

    // expire all existing auth list entries including proxy auths.
    nsresult ClearAll();

private:
    nsHttpAuthNode *LookupAuthNode(const char *scheme,
                                   const char *host,
                                   int32_t     port,
                                   uint32_t    appId,
                                   bool        inBrowserElement,
                                   nsCString  &key);

    // hash table allocation functions
    static void*        AllocTable(void *, size_t size);
    static void         FreeTable(void *, void *item);
    static PLHashEntry* AllocEntry(void *, const void *key);
    static void         FreeEntry(void *, PLHashEntry *he, unsigned flag);

    static PLHashAllocOps gHashAllocOps;

    class AppDataClearObserver : public nsIObserver {
    public:
      NS_DECL_ISUPPORTS
      NS_DECL_NSIOBSERVER
      AppDataClearObserver(nsHttpAuthCache* aOwner) : mOwner(aOwner) {}
      virtual ~AppDataClearObserver() {}
      nsHttpAuthCache* mOwner;
    };

    void ClearAppData(uint32_t appId, bool browserOnly);

private:
    PLHashTable *mDB; // "host:port" --> nsHttpAuthNode
    nsRefPtr<AppDataClearObserver> mObserver;
};

#endif // nsHttpAuthCache_h__
