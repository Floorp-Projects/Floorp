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

#include "nsError.h"
#include "nsVoidArray.h"
#include "nsAString.h"
#include "plhash.h"

//-----------------------------------------------------------------------------
// nsHttpAuthCache
//-----------------------------------------------------------------------------

class nsHttpAuthCache
{
public:
    nsHttpAuthCache();
   ~nsHttpAuthCache();

    nsresult Init();

    // host and port are required
    // path can be null
    nsresult GetCredentialsForPath(const char *host,
                                   PRInt32     port,
                                   const char *path,
                                   nsACString &realm,
                                   nsACString &credentials);

    // host and port are required
    // realm must not be null
    nsresult GetCredentialsForDomain(const char *host,
                                     PRInt32     port,
                                     const char *realm,
                                     nsACString &credentials);

    // host and port are required
    // path can be null
    // realm must not be null
    // credentials, if null, clears the entry
    nsresult SetCredentials(const char *host,
                            PRInt32     port,
                            const char *directory,
                            const char *realm,
                            const char *credentials);

    // Expire all existing auth list entries including proxy auths. 
    nsresult ClearAll();

public: /* internal */

    class nsEntry
    {
    public:
        nsEntry(const char *path,
                const char *realm,
                const char *creds);
       ~nsEntry();

        const char *Path()  { return mPath; }
        const char *Realm() { return mRealm; }
        const char *Creds() { return mCreds; }

        void SetPath(const char *);
        void SetCreds(const char *);
                
    private:
        char *mPath;
        char *mRealm;
        char *mCreds;
    };

    class nsEntryList
    {
    public:
        nsEntryList();
       ~nsEntryList();

        // path can be null, in which case we'll search for an entry
        // with a null path.
        nsresult GetCredentialsForPath(const char *path,
                                       nsACString &realm,
                                       nsACString &creds);

        // realm must not be null
        nsresult GetCredentialsForRealm(const char *realm,
                                        nsACString &creds);

        // if a matching entry is found, then credentials will be changed.
        nsresult SetCredentials(const char *path,
                                const char *realm,
                                const char *credentials);

        PRUint32 Count() { return (PRUint32) mList.Count(); }

    private:
        nsVoidArray mList;
    };

private:
    nsEntryList *LookupEntryList(const char *host, PRInt32 port, nsAFlatCString &key);

    // hash table allocation functions
    static void*        PR_CALLBACK AllocTable(void *, PRSize size);
    static void         PR_CALLBACK FreeTable(void *, void *item);
    static PLHashEntry* PR_CALLBACK AllocEntry(void *, const void *key);
    static void         PR_CALLBACK FreeEntry(void *, PLHashEntry *he, PRUintn flag);

    static PLHashAllocOps gHashAllocOps;
    
private:
    PLHashTable *mDB; // "host:port" --> nsEntryList
};

#endif // nsHttpAuthCache_h__
