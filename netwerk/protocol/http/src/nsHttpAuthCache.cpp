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
#include "plstr.h"
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
nsHttpAuthCache::GetCredentialsForPath(const char *host,
                                       PRInt32     port,
                                       const char *path,
                                       nsACString &realm,
                                       nsACString &creds)
{
    LOG(("nsHttpAuthCache::GetCredentialsForPath [host=%s:%d path=%s]\n",
        host, port, path));

    nsCAutoString key;
    nsEntryList *list = LookupEntryList(host, port, key);
    if (!list)
        return NS_ERROR_NOT_AVAILABLE;

    return list->GetCredentialsForPath(path, realm, creds);
}

nsresult
nsHttpAuthCache::GetCredentialsForDomain(const char *host,
                                         PRInt32     port,
                                         const char *realm,
                                         nsACString &creds)
{
    LOG(("nsHttpAuthCache::GetCredentialsForDomain [host=%s:%d realm=%s]\n",
        host, port, realm));

    nsCAutoString key;
    nsEntryList *list = LookupEntryList(host, port, key);
    if (!list)
        return NS_ERROR_NOT_AVAILABLE;

    return list->GetCredentialsForRealm(realm, creds);
}

nsresult
nsHttpAuthCache::SetCredentials(const char *host,
                                PRInt32     port,
                                const char *path,
                                const char *realm,
                                const char *creds)
{
    nsresult rv;

    LOG(("nsHttpAuthCache::SetCredentials\n"));

    if (!mDB) {
        rv = Init();
        if (NS_FAILED(rv)) return rv;
    }

    nsCAutoString key;
    nsEntryList *list = LookupEntryList(host, port, key);

    if (!list) {
        // only create a new list if we have a real entry
        if (!creds)
            return NS_OK;

        // create a new entry list and set the given entry
        list = new nsEntryList();
        if (!list)
            return NS_ERROR_OUT_OF_MEMORY;
        rv = list->SetCredentials(path, realm, creds);
        if (NS_FAILED(rv))
            delete list;
        else
            PL_HashTableAdd(mDB, PL_strdup(key.get()), list);
        return rv;
    }

    rv = list->SetCredentials(path, realm, creds);
    if (NS_SUCCEEDED(rv) && (list->Count() == 0)) {
        // the list has no longer has any entries
        // XXX leak.. need to free key
        PL_HashTableRemove(mDB, key.get());
        delete list;
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

nsHttpAuthCache::nsEntryList *
nsHttpAuthCache::LookupEntryList(const char *host, PRInt32 port, nsAFlatCString &key)
{
    char buf[32];

    if (!mDB)
        return nsnull;

    PR_snprintf(buf, sizeof(buf), "%d", port);

    key.Assign(host);
    key.Append(':');
    key.Append(buf);

    return (nsEntryList *) PL_HashTableLookup(mDB, key.get());
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
        delete (nsEntryList *) he->value;
        PL_strfree((char *) he->key);
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
// nsHttpAuthCache::nsEntry
//-----------------------------------------------------------------------------

nsHttpAuthCache::
nsEntry::nsEntry(const char *path, const char *realm, const char *creds)
    : mPath(PL_strdup(path))
    , mRealm(PL_strdup(realm))
    , mCreds(PL_strdup(creds))
{
    LOG(("Creating nsHttpAuthCache::nsEntry @%x\n", this));
}

nsHttpAuthCache::
nsEntry::~nsEntry()
{
    LOG(("Destroying nsHttpAuthCache::nsEntry @%x\n", this));

    PL_strfree(mPath);
    PL_strfree(mRealm);
    PL_strfree(mCreds);
}

void nsHttpAuthCache::
nsEntry::SetPath(const char *path)
{
    PL_strfree(mPath);
    mPath = PL_strdup(path);
}

void nsHttpAuthCache::
nsEntry::SetCreds(const char *creds)
{
    PL_strfree(mCreds);
    mCreds = PL_strdup(creds);
}

//-----------------------------------------------------------------------------
// nsHttpAuthCache::nsEntryList
//-----------------------------------------------------------------------------

nsHttpAuthCache::
nsEntryList::nsEntryList()
{
    LOG(("Creating nsHttpAuthCache::nsEntryList @%x\n", this));
}

nsHttpAuthCache::
nsEntryList::~nsEntryList()
{
    LOG(("Destroying nsHttpAuthCache::nsEntryList @%x\n", this));

    PRInt32 i;
    for (i=0; i<mList.Count(); ++i)
        delete (nsEntry *) mList[i];
    mList.Clear();
}

nsresult nsHttpAuthCache::
nsEntryList::GetCredentialsForPath(const char *path,
                                   nsACString &realm,
                                   nsACString &creds)
{
    nsEntry *entry = nsnull;

    // it's permissible to specify a null path, in which case we just treat
    // this as an empty string.
    if (!path)
        path = "";

    // look for an entry that either matches or contains this directory.
    // ie. we'll give out credentials if the given directory is a sub-
    // directory of an existing entry.
    PRInt32 i;
    for (i=0; i<mList.Count(); ++i) {
        entry = (nsEntry *) mList[i];
        if (PL_strncmp(path, entry->Path(), PL_strlen(entry->Path())))
            break;
        entry = nsnull;
    }

    if (!entry)
        return NS_ERROR_NOT_AVAILABLE;

    realm.Assign(entry->Realm());
    creds.Assign(entry->Creds());
    return NS_OK;
}

nsresult nsHttpAuthCache::
nsEntryList::GetCredentialsForRealm(const char *realm,
                                    nsACString &creds)
{
    NS_ENSURE_ARG_POINTER(realm);

    nsEntry *entry = nsnull;

    // look for an entry that matches this realm
    PRInt32 i;
    for (i=0; i<mList.Count(); ++i) {
        entry = (nsEntry *) mList[i];
        if (!PL_strcmp(realm, entry->Realm()))
            break;
        entry = nsnull;
    }

    if (!entry)
        return NS_ERROR_NOT_AVAILABLE;

    creds.Assign(entry->Creds());
    return NS_OK;
}

nsresult nsHttpAuthCache::
nsEntryList::SetCredentials(const char *path,
                            const char *realm,
                            const char *creds)
{
    NS_ENSURE_ARG_POINTER(realm);

    nsEntry *entry = nsnull;

    // look for an entry with a matching realm
    PRInt32 i;
    for (i=0; i<mList.Count(); ++i) {
        entry = (nsEntry *) mList[i];
        if (!PL_strcmp(realm, entry->Realm()))
            break;
        entry = nsnull;
    }

    if (!entry) {
        if (creds) {
            entry = new nsEntry(path, realm, creds);
            if (!entry)
                return NS_ERROR_OUT_OF_MEMORY;
            mList.AppendElement(entry);
        }
        // else, nothing to do
    }
    else if (!creds) {
        mList.RemoveElementAt(i);
        delete entry;
    }
    else {
        // update the entry...
        if (path) {
            // we should hold onto the top-most of the two path
            PRUint32 len1 = PL_strlen(path);
            PRUint32 len2 = PL_strlen(entry->Path());
            if (len1 < len2)
                entry->SetPath(path);
        }
        entry->SetCreds(creds);
    }

    return NS_OK;
}
