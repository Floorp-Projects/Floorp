/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Implementation of nsIInterfaceInfo. */

#include <sys/stat.h>
#include "nscore.h"

#include "nsISupports.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIInterfaceInfo.h"
#include "nsIServiceManager.h"
#include "nsIAllocator.h"

#include "nsInterfaceInfoManager.h"
#include "nsInterfaceInfo.h"
#include "xptinfo.h"

#include "prio.h"
#include "plstr.h"
#include "prenv.h"

// this after nsISupports, to pick up IID
// so that xpt stuff doesn't try to define it itself...
#include "xpt_xdr.h"

static NS_DEFINE_IID(kIIIManagerIID, NS_IINTERFACEINFO_MANAGER_IID);
NS_IMPL_ISUPPORTS(nsInterfaceInfoManager, kIIIManagerIID);

// static
nsInterfaceInfoManager*
nsInterfaceInfoManager::GetInterfaceInfoManager()
{
    static nsInterfaceInfoManager* impl = NULL;
    if(!impl)
    {
        impl = new nsInterfaceInfoManager();
        // XXX ought to check for properly formed impl here..
    }
    if(impl)
        NS_ADDREF(impl);
    return impl;
}

// static
nsIAllocator*
nsInterfaceInfoManager::GetAllocator(nsInterfaceInfoManager* iim /*= NULL*/)
{
    nsIAllocator* al;
    nsInterfaceInfoManager* iiml = iim;

    if(!iiml && !(iiml = GetInterfaceInfoManager()))
        return NULL;
    if(NULL != (al = iiml->mAllocator))
        NS_ADDREF(al);
    if(!iim)
        NS_RELEASE(iiml);
    return al;
}

static NS_DEFINE_IID(kAllocatorCID, NS_ALLOCATOR_CID);
static NS_DEFINE_IID(kIAllocatorIID, NS_IALLOCATOR_IID);

nsInterfaceInfoManager::nsInterfaceInfoManager()
    : mAllocator(NULL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    nsServiceManager::GetService(kAllocatorCID,
                                 kIAllocatorIID,
                                 (nsISupports **)&mAllocator);

    PR_ASSERT((mAllocator != NULL));

    initInterfaceTables();
}

// Stolen and modified from xpt_dump.c
XPTHeader *getHeader(const char *filename) {
    XPTState *state = NULL;
    XPTCursor curs, *cursor = &curs;
    XPTHeader *header = NULL;
    PRFileInfo fileinfo;
    PRUint32 flen;
    char *whole = NULL;
    PRFileDesc *in = NULL;

    if (PR_GetFileInfo(filename, &fileinfo) != PR_SUCCESS) {
        NS_ERROR("PR_GetFileInfo failed");
        return NULL;
    }
    flen = fileinfo.size;

    whole = (char *)malloc(flen);
    if (!whole) {
        NS_ERROR("FAILED: malloc for whole");
        return NULL;
    }

    // XXX changed this to PR_OPEN; does this do binary for windows? ("b")
//      in = fopen(filename, "rb");
    in = PR_Open(filename, PR_RDONLY, 0);
    if (!in) {
        NS_ERROR("FAILED: fopen");
        goto out;
    }

    if (flen > 0) {
        PRInt32 howmany = PR_Read(in, whole, flen);
        if (howmany < 0) {
            NS_ERROR("FAILED: reading typelib file");
            goto out;
        }

        // XXX lengths are PRUInt32, reads are PRInt32?
        if (howmany < flen) {
            NS_ERROR("short read of typelib file");
            goto out;
        }
        state = XPT_NewXDRState(XPT_DECODE, whole, flen);
        if (!XPT_MakeCursor(state, XPT_HEADER, 0, cursor)) {
            NS_ERROR("MakeCursor failed\n");
            goto out;
        }
        if (!XPT_DoHeader(cursor, &header)) {
            NS_ERROR("DoHeader failed\n");
            goto out;
        }
    }

 out:
    if (state != NULL)
        XPT_DestroyXDRState(state);
    if (whole != NULL)
        free(whole);
    if (in != NULL)
        PR_Close(in);
    return header;
}

static void
indexify_file(const char *filename,
              PLHashTable *interfaceTable,
              nsHashtable *IIDTable,
              nsIAllocator *al)
{
    XPTHeader *header = getHeader(filename);

    int limit = header->num_interfaces;

    nsInterfaceRecord *value;
#ifdef DEBUG_mccabe
    static int which = 0;
    which++;
#endif

    for (int i = 0; i < limit; i++) {
        XPTInterfaceDirectoryEntry *current = header->interface_directory + i;

#ifdef DEBUG_mccabe
        fprintf(stderr, "%s", current->name);
#endif
        // first try to look it up...
        value = (nsInterfaceRecord *)PL_HashTableLookup(interfaceTable,
                                                  current->name);
        // if none found, make a dummy record.
        if (value == NULL) {
            value = new nsInterfaceRecord();
            value->which_header = NULL;
            value->resolved = PR_FALSE;
            value->which = -1;
            value->entry = NULL;
            value->info = NULL;
            void *hashEntry =
                PL_HashTableAdd(interfaceTable, current->name, value);
#ifdef DEBUG_mccabe
            fprintf(stderr, "... added, %d\n", which);
#endif
            NS_ASSERTION(hashEntry != NULL, "PL_HashTableAdd failed?");
        }
#ifdef DEBUG_mccabe
        else {
            fprintf(stderr, "... found, %d\n", value->which);
        }
#endif

        // save info from the interface in the global table.  if it's resolved.
        if (current->interface_descriptor != NULL) {
            // we claim it should only be defined once.  XXX ?
            NS_ASSERTION(value->which_header == NULL,
                         "some interface def'd in multiple typelibs.");
            value->which_header = header;
            value->resolved = PR_TRUE;
#ifdef DEBUG_mccabe
            value->which = which;
#endif
            value->entry = current;

            // XXX is this a leak?
            nsIDKey idKey(current->iid);
#ifdef DEBUG
            char * found_name;
            found_name = (char *)IIDTable->Get(&idKey);
            NS_ASSERTION(found_name == NULL,
                         "iid already associated with a name?");
#endif
            IIDTable->Put(&idKey, current->name);
#ifdef DEBUG_mccabe            
            fprintf(stderr, "\t... resolved, %d\n", value->which);
#endif
        }
    }
}

// as many InterfaceDirectoryEntries as we expect to see.
#define XPT_HASHSIZE 64

#ifdef DEBUG
static PRIntn
check_enumerator(PLHashEntry *he, PRIntn index, void *arg); 
#endif

void nsInterfaceInfoManager::initInterfaceTables()
{
    // make a hashtable to associate names with arbitrary info
    this->mInterfaceTable = PL_NewHashTable(XPT_HASHSIZE,
                                            PL_HashString,  // hash keys
                                            PL_CompareStrings, // compare keys
                                            PL_CompareValues, // comp values
                                            NULL, NULL);
                                          
    // make a hashtable to map iids to names
    this->mIIDTable = new nsHashtable(XPT_HASHSIZE);

    // First, find the xpt directory from the env.
    // XXX don't free this?
    char *xptdirname = PR_GetEnv("XPTDIR");
    NS_ASSERTION(xptdirname != NULL,
                 "set env var XPTDIR to a directory containg .xpt files.");

    // now loop thru the xpt files in the directory.

    // XXX This code stolen with few modifications from nsRepository; any
    // point in doing it through them instead?)

    PRDir *xptdir = PR_OpenDir(xptdirname);
    if (xptdir == NULL) {
        NS_ERROR("Couldn't open XPT directory");
        return;  // XXX fail gigantically.
    }

    // Create a buffer that has dir/ in it so we can append
    // the filename each time in the loop
    char fullname[1024]; // NS_MAX_FILENAME_LEN
    PL_strncpyz(fullname, xptdirname, sizeof(fullname));
    unsigned int n = strlen(fullname);
    if (n+1 < sizeof(fullname)) {
        fullname[n] = '/';
        n++;
    }
    char *filepart = fullname + n;
	
    PRDirEntry *dirent = NULL;
#ifdef DEBUG_mccabe
    int which = 0;
#endif
    while ((dirent = PR_ReadDir(xptdir, PR_SKIP_BOTH)) != NULL) {
        PL_strncpyz(filepart, dirent->name, sizeof(fullname)-n);
	PRFileInfo statbuf;
        // stattable?
	if (PR_GetFileInfo(fullname,&statbuf) != PR_SUCCESS)
            continue;
        // plain file?
	else if (statbuf.type != PR_FILE_FILE)
            continue;
        // .xpt suffix?
	int flen = PL_strlen(fullname);
        if (flen >= 4 && !PL_strcasecmp(&(fullname[flen - 4]), ".xpt")) {
            // it's a valid file, read it in.
#ifdef DEBUG_mccabe
            which++;
            fprintf(stderr, "%d %s\n", which, fullname);
#endif
            indexify_file(fullname,
                          this->mInterfaceTable,
                          this->mIIDTable,
                          this->mAllocator);
        } else {
            continue;
        }
    }
    PR_CloseDir(xptdir);

#ifdef DEBUG
    // scan here to confirm that all interfaces are resolved.
   PL_HashTableEnumerateEntries(this->mInterfaceTable,
                                check_enumerator,
                                this->mIIDTable);
#endif
}

#ifdef DEBUG
PRIntn check_enumerator(PLHashEntry *he, PRIntn index, void *arg) {
    char *key = (char *)he->key;
    nsInterfaceRecord *value = (nsInterfaceRecord *)he->value;
    nsHashtable *iidtable = (nsHashtable *)arg;


    if (value->resolved == PR_FALSE) {
        fprintf(stderr, "unresolved interface %s\n", key);
    } else {
        NS_ASSERTION(value->entry, "resolved, but no entry?");
        nsIDKey idKey(value->entry->iid);
        char *name_from_iid = (char *)iidtable->Get(&idKey);
        NS_ASSERTION(name_from_iid != NULL,
                     "no name assoc'd with iid for entry for name?");

        // XXX note that below is only ncc'ly the case if xdr doesn't give us
        // duplicated strings.
//          NS_ASSERTION(name_from_iid == key,
//                       "key and iid name xpected to be same");
    }
    return HT_ENUMERATE_NEXT;
}
#endif

nsInterfaceInfoManager::~nsInterfaceInfoManager()
{
    // let the singleton leak
}

NS_IMETHODIMP
nsInterfaceInfoManager::GetInfoForIID(const nsIID* iid,
                                      nsIInterfaceInfo** info)
{
    nsIDKey idKey(*iid);
    char *result_name = (char *)this->mIIDTable->Get(&idKey);

    return this->GetInfoForName(result_name, info);
}

NS_IMETHODIMP
nsInterfaceInfoManager::GetInfoForName(const char* name,
                                       nsIInterfaceInfo** info)
{
    nsInterfaceRecord *record =
        (nsInterfaceRecord *)PL_HashTableLookup(this->mInterfaceTable, name);
    if (record == NULL || record->resolved == PR_FALSE) {
        *info = NULL;
        return NS_ERROR_FAILURE;
    }
    PR_ASSERT(record->entry != NULL);

    // Is there already an II obj associated with the nsInterfaceRecord?
    if (record->info != NULL) {
        // yay!
        *info = record->info;
        NS_ADDREF(*info);
        return NS_OK;
    }

    // nope, better make one.  first, find a parent for it.
    nsIInterfaceInfo *parent;
    uint16 parent_index = record->entry->interface_descriptor->parent_interface;
    // Does it _get_ a parent? (is it nsISupports?)
    if (parent_index == 0) {
        // presumably this is only the case for nsISupports.
        parent = NULL;
    } else {
        // there's a parent index that points to an entry in the same table
        // that this one was defined in.  Accounting for magic offset.
        XPTInterfaceDirectoryEntry *parent_entry =
            record->which_header->interface_directory + parent_index - 1;
        // get a name from it (which should never be null) and build
        // that.  XXX OPT Hm, could have a helper function to avoid
        // second lookup if this entry happens to be resolved.
        nsresult nsr = GetInfoForName(parent_entry->name, &parent);
        if (NS_IS_ERROR(nsr)) {
            *info = NULL;
            return NS_ERROR_FAILURE;
        }
    }

    // got a parent for it, now build the object itself
    nsInterfaceInfo *result =
        new nsInterfaceInfo(record, (nsInterfaceInfo *)parent);
    *info = result;
    NS_ADDREF(*info);
    return NS_OK;
}

NS_IMETHODIMP
nsInterfaceInfoManager::GetIIDForName(const char* name, nsIID** iid)
{
    nsInterfaceRecord *record =
        (nsInterfaceRecord *)PL_HashTableLookup(this->mInterfaceTable, name);
    if (record == NULL || record->resolved == PR_FALSE) {
        *iid = NULL;
        return NS_ERROR_FAILURE;
    }
    PR_ASSERT(record->entry != NULL);

    nsIID* p;
    if(!(p = (nsIID *)mAllocator->Alloc(sizeof(nsIID))))
        return NS_ERROR_FAILURE;
    
    // XXX I'm confused here about the lifetime of IID pointers.
    memcpy(p, &record->entry->iid, sizeof(nsIID));
    *iid = p;
    return NS_OK;
}

NS_IMETHODIMP
nsInterfaceInfoManager::GetNameForIID(const nsIID* iid, char** name)
{
    nsIDKey idKey(*iid);
    char *result_name = (char *)this->mIIDTable->Get(&idKey);

#ifdef DEBUG
    // XXX assert here that lookup in table matches iid?
    nsIID *newid;
    nsresult isok = GetIIDForName(result_name, &newid);
    PR_ASSERT(newid->Equals(*newid));
    PR_ASSERT(isok == NS_OK);
#endif

    if (result_name == NULL) {
        *name = NULL;
        return NS_ERROR_FAILURE;
    }
    
    char *p;
    int len = strlen(result_name) + 1;
    if(!(p = (char *)mAllocator->Alloc(len))) {
        *name = NULL;
        return NS_ERROR_FAILURE;
    }
    memcpy(p, result_name, len);
    *name = p;
    return NS_OK;
}    

XPTI_PUBLIC_API(nsIInterfaceInfoManager*)
XPTI_GetInterfaceInfoManager()
{
    return nsInterfaceInfoManager::GetInterfaceInfoManager();
}
