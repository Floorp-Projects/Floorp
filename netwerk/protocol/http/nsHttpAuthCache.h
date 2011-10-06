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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gagan Saksena <gagan@netscape.com> (original author)
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
        : mUser(nsnull)
        , mPass(nsnull)
        , mDomain(nsnull)
    {
    }
    nsHttpAuthIdentity(const PRUnichar *domain,
                       const PRUnichar *user,
                       const PRUnichar *password)
        : mUser(nsnull)
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
        : mRoot(nsnull)
        , mTail(nsnull)
        , mRealm(nsnull)
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

    PRUint32 EntryCount() { return mList.Length(); }

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
                                 PRInt32     port,
                                 const char *path,
                                 nsHttpAuthEntry **entry);

    // |scheme|, |host|, and |port| are required
    // |realm| must not be null
    // |entry| is either null or a weak reference
    nsresult GetAuthEntryForDomain(const char *scheme,
                                   const char *host,
                                   PRInt32     port,
                                   const char *realm,
                                   nsHttpAuthEntry **entry);

    // |scheme|, |host|, and |port| are required
    // |path| can be null
    // |realm| must not be null
    // if |credentials|, |user|, |pass|, and |challenge| are each
    // null, then the entry is deleted.
    nsresult SetAuthEntry(const char *scheme,
                          const char *host,
                          PRInt32     port,
                          const char *directory,
                          const char *realm,
                          const char *credentials,
                          const char *challenge,
                          const nsHttpAuthIdentity *ident,
                          nsISupports *metadata);

    void ClearAuthEntry(const char *scheme,
                        const char *host,
                        PRInt32     port,
                        const char *realm);

    // expire all existing auth list entries including proxy auths. 
    nsresult ClearAll();

private:
    nsHttpAuthNode *LookupAuthNode(const char *scheme,
                                   const char *host,
                                   PRInt32     port,
                                   nsCString  &key);

    // hash table allocation functions
    static void*        AllocTable(void *, PRSize size);
    static void         FreeTable(void *, void *item);
    static PLHashEntry* AllocEntry(void *, const void *key);
    static void         FreeEntry(void *, PLHashEntry *he, PRUintn flag);

    static PLHashAllocOps gHashAllocOps;
    
private:
    PLHashTable *mDB; // "host:port" --> nsHttpAuthNode
};

#endif // nsHttpAuthCache_h__
