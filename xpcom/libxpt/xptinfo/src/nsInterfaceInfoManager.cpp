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

#include <sys/stat.h>
#include "nscore.h"

#include "nsISupports.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIInterfaceInfo.h"
#include "nsIServiceManager.h"
#include "nsIAllocator.h"

#include "nsInterfaceInfoManager.h"
#include "nsInterfaceInfo.h"

// this after nsISupports, to pick up IID
// so that xpt stuff doesn't try to define it itself...
//  #include "xpt_struct.h"
#include "xpt_xdr.h"

// should get multiple xpt files from some well-known dir.
#define XPTFILE "simple.xpt"

// Stolen from xpt_dump.c
// todo - lazy loading of file, etc.
XPTHeader *getheader() {
    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    XPTHeader *header;
    struct stat file_stat;
    int flen;
    char *whole;
    FILE *in;

    if (stat(XPTFILE, &file_stat) != 0) {
        perror("FAILED: fstat");
        return NULL;
    }
    flen = file_stat.st_size;
    in = fopen(XPTFILE, "r");

    if (!in) {
        perror("FAILED: fopen");
        return NULL;
    }

    whole = (char *)malloc(flen);
    if (!whole) {
        perror("FAILED: malloc for whole");
        return NULL;
    }

    if (flen > 0) {
        fread(whole, flen, 1, in);
        state = XPT_NewXDRState(XPT_DECODE, whole, flen);
        if (!XPT_MakeCursor(state, XPT_HEADER, 0, cursor)) {
            fprintf(stdout, "MakeCursor failed\n");
            return NULL;
        }
        if (!XPT_DoHeader(cursor, &header)) {
            fprintf(stdout, "DoHeader failed\n");
            return NULL;
        }

       free(header);
   
       XPT_DestroyXDRState(state);
       // assum'd to be OK
       free(whole);
       fclose(in);
       return header;
    }

    free(whole);
    fclose(in);
    return NULL;
}



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


#define HACK_CACHE_SIZE 200
nsInterfaceInfoManager::nsInterfaceInfoManager()
    : mAllocator(NULL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    mInfoArray = (nsInterfaceInfo**) calloc(HACK_CACHE_SIZE, sizeof(void*));

    mHeader = getheader();
    PR_ASSERT((mHeader != NULL));

    nsServiceManager::GetService(kAllocatorCID,
                                 kIAllocatorIID,
                                 (nsISupports **)&mAllocator);

    // just added this baby.
    // allocator isn't currently registered, so just use malloc instead.
//      PR_ASSERT((mAllocator != NULL));
}

nsInterfaceInfo *
nsInterfaceInfoManager::buildII(XPTInterfaceDirectoryEntry *entry) {
    int i;
    for (i = 0; i < HACK_CACHE_SIZE; i++) {
        if (mInfoArray[i] == NULL)
            break;
        if (mInfoArray[i]->mEntry == entry)
            return mInfoArray[i];
    }

    // ok, no dice.  Does it have a parent?
    nsInterfaceInfo *parent = NULL;
    if (entry->interface_descriptor->parent_interface != NULL) {
        for (i = 0; i < HACK_CACHE_SIZE; i++) {
            if (mInfoArray[i] == NULL)
                break;
            if (mInfoArray[i]->mEntry ==
                entry->interface_descriptor->parent_interface)
                parent = mInfoArray[i];
        }
        if (parent == NULL)
            parent = buildII(entry->interface_descriptor->parent_interface);
        PR_ASSERT(parent);
    }
    
    nsInterfaceInfo *result = new nsInterfaceInfo(entry, parent);

    while (mInfoArray[i] == NULL)
        i++;
    PR_ASSERT(i < HACK_CACHE_SIZE);
    mInfoArray[i] = result;

    return result;
}

nsInterfaceInfoManager::~nsInterfaceInfoManager()
{
    // let the singleton leak
}

NS_IMETHODIMP
nsInterfaceInfoManager::GetInfoForIID(const nsIID* iid,
                                        nsIInterfaceInfo** info)
{
    for(int i = 0; i < mHeader->num_interfaces; i++) {
        XPTInterfaceDirectoryEntry *entry = &mHeader->interface_directory[i];
        if (iid->Equals(entry->iid)) {
            *info = buildII(entry);
            NS_ADDREF(*info);
            return NS_OK;
        }
    }
    *info = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsInterfaceInfoManager::GetInfoForName(const char* name,
                                         nsIInterfaceInfo** info)
{
    for(int i = 0; i < mHeader->num_interfaces; i++) {
        XPTInterfaceDirectoryEntry *entry = &mHeader->interface_directory[i];
        if (!strcmp(name, entry->name)) {
            *info = buildII(entry);
            NS_ADDREF(*info);
            return NS_OK;
        }
    }
    *info = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsInterfaceInfoManager::GetIIDForName(const char* name, nsIID** iid)
{
    for(int i = 0; i < mHeader->num_interfaces; i++) {
        XPTInterfaceDirectoryEntry *entry = &mHeader->interface_directory[i];
        if (!strcmp(name, entry->name)) {
            nsIID* p;

            // Allocator isn't registered, so just use malloc for now...
//              if(!(p = (nsIID*)mAllocator->Alloc(sizeof(nsIID))))
//                  break;
            if (!(p = (nsIID*)malloc(sizeof(nsIID))))
                break;

            // XXX I'm confused here about the lifetime of IID pointers.
            memcpy(p, &entry->iid, sizeof(nsIID));
            *iid = p;
            return NS_OK;
        }
    }
    *iid = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsInterfaceInfoManager::GetNameForIID(const nsIID* iid, char** name)
{
    for(int i = 0; i < mHeader->num_interfaces; i++) {
        XPTInterfaceDirectoryEntry *entry = &mHeader->interface_directory[i];
        if (iid->Equals(entry->iid)) {
            char* p;
            int len = strlen(entry->name)+1;
            if(!(p = (char*)mAllocator->Alloc(len)))
                break;
            memcpy(p, &entry->name, len);
            *name = p;
            return NS_OK;
        }
    }
    *name = NULL;
    return NS_ERROR_FAILURE;
}

// XXX this goes away; IIM should be a service.
// ... where does decl for this go?
XPT_PUBLIC_API(nsIInterfaceInfoManager*)
XPT_GetInterfaceInfoManager()
{
    return nsInterfaceInfoManager::GetInterfaceInfoManager();
}
