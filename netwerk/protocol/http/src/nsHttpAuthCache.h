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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsHttpAuthCache_h__
#define nsHttpAuthCache_h__

#include "nsHttp.h"
#include "nsError.h"
#include "nsVoidArray.h"
#include "nsAString.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "plhash.h"
#include "nsCRT.h"

//-----------------------------------------------------------------------------
// nsHttpAuthEntry
//-----------------------------------------------------------------------------

class nsHttpAuthEntry
{
public:
    const char *Path()      { return mPath; }
    const char *Realm()     { return mRealm; }
    const char *Creds()     { return mCreds; }
    const PRUnichar *User() { return mUser; }
    const PRUnichar *Pass() { return mPass; }
    const char *Challenge() { return mChallenge; }
    nsISupports *MetaData() { return mMetaData; }

private:
    nsHttpAuthEntry(const char *path,
                    const char *realm,
                    const char *creds,
                    const PRUnichar *user,
                    const PRUnichar *pass,
                    const char *challenge,
                    nsISupports *metadata);
   ~nsHttpAuthEntry();

    void SetPath(const char *v)      { CRTFREEIF(mPath); mPath = strdup_if(v); }
    void SetCreds(const char *v)     { CRTFREEIF(mCreds); mCreds = strdup_if(v); }
    void SetUser(const PRUnichar *v) { CRTFREEIF(mUser); mUser = strdup_if(v); }
    void SetPass(const PRUnichar *v) { CRTFREEIF(mPass); mPass = strdup_if(v); }
    void SetChallenge(const char *v) { CRTFREEIF(mChallenge); mChallenge = strdup_if(v); }
    void SetMetaData(nsISupports *v) { mMetaData = v; }
            
private:
    char                 *mPath;
    char                 *mRealm;
    char                 *mCreds;
    PRUnichar            *mUser;
    PRUnichar            *mPass;
    char                 *mChallenge;
    nsCOMPtr<nsISupports> mMetaData;

    friend class nsHttpAuthCache;
    friend class nsHttpAuthNode;
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
    nsresult GetAuthEntryForPath(const char *path,
                                 nsHttpAuthEntry **entry);

    // realm must not be null
    nsresult GetAuthEntryForRealm(const char *realm,
                                  nsHttpAuthEntry **entry);

    // if a matching entry is found, then credentials will be changed.
    nsresult SetAuthEntry(const char *path,
                          const char *realm,
                          const char *credentials,
                          const PRUnichar *user,
                          const PRUnichar *pass,
                          const char *challenge,
                          nsISupports *metadata);

    PRUint32 EntryCount() { return (PRUint32) mList.Count(); }

private:
    nsVoidArray mList; // list of nsHttpAuthEntry objects

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

    // |host| and |port| are required
    // |path| can be null
    // |entry| is either null or a weak reference
    nsresult GetAuthEntryForPath(const char *host,
                                 PRInt32     port,
                                 const char *path,
                                 nsHttpAuthEntry **entry);

    // |host| and |port| are required
    // |realm| must not be null
    // |entry| is either null or a weak reference
    nsresult GetAuthEntryForDomain(const char *host,
                                   PRInt32     port,
                                   const char *realm,
                                   nsHttpAuthEntry **entry);

    // |host| and |port| are required
    // |path| can be null
    // |realm| must not be null
    // if |credentials|, |user|, |pass|, and |challenge| are each
    // null, then the entry is deleted.
    nsresult SetAuthEntry(const char *host,
                          PRInt32     port,
                          const char *directory,
                          const char *realm,
                          const char *credentials,
                          const PRUnichar *user,
                          const PRUnichar *pass,
                          const char *challenge,
                          nsISupports *metadata);

    // expire all existing auth list entries including proxy auths. 
    nsresult ClearAll();

private:
    nsHttpAuthNode *LookupAuthNode(const char *host, PRInt32 port, nsAFlatCString &key);

    // hash table allocation functions
    static void*        PR_CALLBACK AllocTable(void *, PRSize size);
    static void         PR_CALLBACK FreeTable(void *, void *item);
    static PLHashEntry* PR_CALLBACK AllocEntry(void *, const void *key);
    static void         PR_CALLBACK FreeEntry(void *, PLHashEntry *he, PRUintn flag);

    static PLHashAllocOps gHashAllocOps;
    
private:
    PLHashTable *mDB; // "host:port" --> nsHttpAuthNode
};

#endif // nsHttpAuthCache_h__
