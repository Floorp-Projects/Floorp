/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#include <stdlib.h>
#include "nsHttp.h"
#include "nsHttpAuthCache.h"
#include "nsString.h"
#include "nsCRT.h"
#include "prprf.h"

//-----------------------------------------------------------------------------
// nsHttpAuthCache <public>
//-----------------------------------------------------------------------------

nsHttpAuthCache::nsHttpAuthCache()
    : mDB(nsnull)
{
}

nsHttpAuthCache::~nsHttpAuthCache()
{
    if (mDB)
        ClearAll();
}

nsresult
nsHttpAuthCache::Init()
{
    NS_ENSURE_TRUE(!mDB, NS_ERROR_ALREADY_INITIALIZED);

    LOG(("nsHttpAuthCache::Init\n"));

    mDB = PL_NewHashTable(128, (PLHashFunction) PL_HashString,
                               (PLHashComparator) PL_CompareStrings,
                               (PLHashComparator) 0, &gHashAllocOps, this);
    if (!mDB)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

nsresult
nsHttpAuthCache::GetAuthEntryForPath(const char *host,
                                     PRInt32     port,
                                     const char *path,
                                     nsHttpAuthEntry **entry)
{
    LOG(("nsHttpAuthCache::GetAuthEntryForPath [host=%s:%d path=%s]\n",
        host, port, path));

    nsCAutoString key;
    nsHttpAuthNode *node = LookupAuthNode(host, port, key);
    if (!node)
        return NS_ERROR_NOT_AVAILABLE;

    return node->GetAuthEntryForPath(path, entry);
}

nsresult
nsHttpAuthCache::GetAuthEntryForDomain(const char *host,
                                       PRInt32     port,
                                       const char *realm,
                                       nsHttpAuthEntry **entry)

{
    LOG(("nsHttpAuthCache::GetAuthEntryForDomain [host=%s:%d realm=%s]\n",
        host, port, realm));

    nsCAutoString key;
    nsHttpAuthNode *node = LookupAuthNode(host, port, key);
    if (!node)
        return NS_ERROR_NOT_AVAILABLE;

    return node->GetAuthEntryForRealm(realm, entry);
}

nsresult
nsHttpAuthCache::SetAuthEntry(const char *host,
                              PRInt32     port,
                              const char *path,
                              const char *realm,
                              const char *creds,
                              const PRUnichar *user,
                              const PRUnichar *pass,
                              const char *challenge,
                              nsISupports *metadata)
{
    nsresult rv;

    LOG(("nsHttpAuthCache::SetAuthEntry [host=%s:%d realm=%s path=%s metadata=%x]\n",
        host, port, realm, path, metadata));

    if (!mDB) {
        rv = Init();
        if (NS_FAILED(rv)) return rv;
    }

    nsCAutoString key;
    nsHttpAuthNode *node = LookupAuthNode(host, port, key);

    if (!node) {
        // only create a new node if we have a real entry
        if (!creds && !user && !pass && !challenge)
            return NS_OK;

        // create a new entry node and set the given entry
        node = new nsHttpAuthNode();
        if (!node)
            return NS_ERROR_OUT_OF_MEMORY;
        rv = node->SetAuthEntry(path, realm, creds, user, pass, challenge, metadata);
        if (NS_FAILED(rv))
            delete node;
        else
            PL_HashTableAdd(mDB, nsCRT::strdup(key.get()), node);
        return rv;
    }

    rv = node->SetAuthEntry(path, realm, creds, user, pass, challenge, metadata);
    if (NS_SUCCEEDED(rv) && (node->EntryCount() == 0)) {
        // the node has no longer has any entries
        PL_HashTableRemove(mDB, key.get());
    }

    return rv;
}

nsresult
nsHttpAuthCache::ClearAll()
{
    LOG(("nsHttpAuthCache::ClearAll\n"));

    if (mDB) {
        PL_HashTableDestroy(mDB);
        mDB = 0;
    }
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpAuthCache <private>
//-----------------------------------------------------------------------------

nsHttpAuthNode *
nsHttpAuthCache::LookupAuthNode(const char *host, PRInt32 port, nsAFlatCString &key)
{
    char buf[32];

    if (!mDB)
        return nsnull;

    PR_snprintf(buf, sizeof(buf), "%d", port);

    key.Assign(host);
    key.Append(':');
    key.Append(buf);

    return (nsHttpAuthNode *) PL_HashTableLookup(mDB, key.get());
}

void *
nsHttpAuthCache::AllocTable(void *self, PRSize size)
{
    return malloc(size);
}

void
nsHttpAuthCache::FreeTable(void *self, void *item)
{
    free(item);
}

PLHashEntry *
nsHttpAuthCache::AllocEntry(void *self, const void *key)
{
    return (PLHashEntry *) malloc(sizeof(PLHashEntry));
}

void
nsHttpAuthCache::FreeEntry(void *self, PLHashEntry *he, PRUintn flag)
{
    if (flag == HT_FREE_VALUE) {
        // this would only happen if PL_HashTableAdd were to replace an
        // existing entry in the hash table, but we _always_ do a lookup
        // before adding a new entry to avoid this case.
        NS_NOTREACHED("should never happen");
    }
    else if (flag == HT_FREE_ENTRY) {
        // three wonderful flavors of freeing memory ;-)
        delete (nsHttpAuthNode *) he->value;
        nsCRT::free((char *) he->key);
        free(he);
    }
}

PLHashAllocOps nsHttpAuthCache::gHashAllocOps =
{
    nsHttpAuthCache::AllocTable,
    nsHttpAuthCache::FreeTable,
    nsHttpAuthCache::AllocEntry,
    nsHttpAuthCache::FreeEntry
};

//-----------------------------------------------------------------------------
// nsHttpAuthEntry
//-----------------------------------------------------------------------------

nsHttpAuthEntry::nsHttpAuthEntry(const char *path, const char *realm,
                                 const char *creds, const PRUnichar *user,
                                 const PRUnichar *pass, const char *challenge,
                                 nsISupports *metadata)
    : mPath(strdup_if(path))
    , mRealm(strdup_if(realm))
    , mCreds(strdup_if(creds))
    , mUser(strdup_if(user))
    , mPass(strdup_if(pass))
    , mChallenge(strdup_if(challenge))
    , mMetaData(metadata)
{
    LOG(("Creating nsHttpAuthCache::nsEntry @%x\n", this));
}

nsHttpAuthEntry::~nsHttpAuthEntry()
{
    LOG(("Destroying nsHttpAuthCache::nsEntry @%x\n", this));

    CRTFREEIF(mPath);
    CRTFREEIF(mRealm);
    CRTFREEIF(mCreds);
    CRTFREEIF(mUser);
    CRTFREEIF(mPass);
    CRTFREEIF(mChallenge);
}

//-----------------------------------------------------------------------------
// nsHttpAuthNode
//-----------------------------------------------------------------------------

nsHttpAuthNode::nsHttpAuthNode()
{
    LOG(("Creating nsHttpAuthNode @%x\n", this));
}

nsHttpAuthNode::~nsHttpAuthNode()
{
    LOG(("Destroying nsHttpAuthNode @%x\n", this));

    PRInt32 i;
    for (i=0; i<mList.Count(); ++i)
        delete (nsHttpAuthEntry *) mList[i];
    mList.Clear();
}

nsresult
nsHttpAuthNode::GetAuthEntryForPath(const char *path,
                                    nsHttpAuthEntry **result)
{
    *result = nsnull;

    // look for an entry that either matches or contains this directory.
    // ie. we'll give out credentials if the given directory is a sub-
    // directory of an existing entry.
    for (PRInt32 i=0; i<mList.Count(); ++i) {
        nsHttpAuthEntry *entry = (nsHttpAuthEntry *) mList[i];
        const char *entryPath = entry->Path();
        // path's can be empty (even NULL)
        if (!path || !*path) {
            if (!entryPath || !*entryPath) {
                *result = entry;
                break;
            }
        }
        else if (!entryPath || !*entryPath)
            continue;
        else if (!nsCRT::strncmp(path, entryPath, (unsigned int)strlen(entryPath))) {
            *result = entry;
            break;
        }
    }

    return *result ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

nsresult
nsHttpAuthNode::GetAuthEntryForRealm(const char *realm,
                                     nsHttpAuthEntry **entry)
{
    NS_ENSURE_ARG_POINTER(realm);

    *entry = nsnull;

    // look for an entry that matches this realm
    PRInt32 i;
    for (i=0; i<mList.Count(); ++i) {
        *entry = (nsHttpAuthEntry *) mList[i];
        if (!nsCRT::strcmp(realm, (*entry)->Realm()))
            break;
        *entry = nsnull;
    }

    return *entry ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

nsresult
nsHttpAuthNode::SetAuthEntry(const char *path,
                             const char *realm,
                             const char *creds,
                             const PRUnichar *user,
                             const PRUnichar *pass,
                             const char *challenge,
                             nsISupports *metadata)
{
    NS_ENSURE_ARG_POINTER(realm);

    nsHttpAuthEntry *entry = nsnull;

    // look for an entry with a matching realm
    PRInt32 i;
    for (i=0; i<mList.Count(); ++i) {
        entry = (nsHttpAuthEntry *) mList[i];
        if (!nsCRT::strcmp(realm, entry->Realm()))
            break;
        entry = nsnull;
    }

    if (!entry) {
        if (creds || user || pass || challenge) {
            entry = new nsHttpAuthEntry(path, realm, creds, user, pass, challenge, metadata);
            if (!entry)
                return NS_ERROR_OUT_OF_MEMORY;
            mList.AppendElement(entry);
        }
        // else, nothing to do
    }
    else if (!creds && !user && !pass && !challenge) {
        mList.RemoveElementAt(i);
        delete entry;
    }
    else {
        // update the entry...
        if (path) {
            // we should hold onto the top-most of the two path
            PRUint32 len1 = strlen(path);
            PRUint32 len2 = strlen(entry->Path());
            if (len1 < len2)
                entry->SetPath(path);
        }
        entry->SetCreds(creds);
        entry->SetUser(user);
        entry->SetPass(pass);
        entry->SetChallenge(challenge);
        entry->SetMetaData(metadata);
    }

    return NS_OK;
}
