/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/* Implementation of nsIInterfaceInfoManager. */

#ifdef XP_MAC
#include <stat.h>
#else
#include <sys/stat.h>
#endif
#include "nscore.h"

#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h" 

#include "nsISupports.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIInterfaceInfo.h"
#include "nsIServiceManager.h"
#include "nsHashtableEnumerator.h"

#include "nsInterfaceInfoManager.h"
#include "nsInterfaceInfo.h"
#include "xptinfo.h"

#include "prio.h"
#include "plstr.h"
#include "prenv.h"
#include "prlog.h"
#include "prprf.h"

// this after nsISupports, to pick up IID
// so that xpt stuff doesn't try to define it itself...
#include "xpt_xdr.h"

#ifdef DEBUG_iim
#define TRACE(x) fprintf x
#else
#define TRACE(x)
#endif


NS_IMPL_THREADSAFE_ISUPPORTS1(nsInterfaceInfoManager, nsIInterfaceInfoManager)

static nsInterfaceInfoManager* gInterfaceInfoManager = NULL;

// static
nsInterfaceInfoManager*
nsInterfaceInfoManager::GetInterfaceInfoManager()
{
    if(!gInterfaceInfoManager)
    {
        gInterfaceInfoManager = new nsInterfaceInfoManager();
        if(!gInterfaceInfoManager->ctor_succeeded)
            NS_RELEASE(gInterfaceInfoManager);
    }
    if(gInterfaceInfoManager)
        NS_ADDREF(gInterfaceInfoManager);
    return gInterfaceInfoManager;
}

void
nsInterfaceInfoManager::FreeInterfaceInfoManager()
{
    NS_IF_RELEASE(gInterfaceInfoManager);
}

nsInterfaceInfoManager::nsInterfaceInfoManager()
    : typelibRecords(NULL), ctor_succeeded(PR_FALSE)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    if(NS_SUCCEEDED(this->initInterfaceTables()))
        ctor_succeeded = PR_TRUE;
}

static
XPTHeader *getHeader(const nsFileSpec *fileSpec)
{
    XPTState *state = NULL;
    XPTCursor curs;
    XPTHeader *header = NULL;
    PRUint32 flen;
    char *whole = NULL;
    XPTCursor *cursor = &curs;

    flen = fileSpec->GetFileSize();
    if (!fileSpec->Valid())
        return NULL;

    whole = (char *)nsAllocator::Alloc(flen);

    if (!whole) {
        NS_ERROR("FAILED: allocation for whole");
        return NULL;
    }

    nsInputFileStream inputStream(*fileSpec); // defaults to read only

    if (flen > 0) {
       	PRInt32 howMany = inputStream.read(whole, flen);
       	if (howMany < 0) {
           	NS_ERROR("FAILED: reading typelib file");
           	goto out;
       	}

//#ifdef XP_MAC
        // Mac can lie about the filesize, because it includes the resource fork
        // (where our CVS information lives). So we'll just believe it if it read
        // less than flen bytes.
        
        // sfraser :I believe this comment is inaccurate; the file size we
        // get will only ever be the data fork, so it should be accurate.
//            flen = howMany;
//#else
        if (PRUint32(howMany) < flen) {
            NS_ERROR("typelib short read");
            goto out;
        }
//#endif

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
    // ~nsInputFileStream closes the file
    if (state != NULL)
        XPT_DestroyXDRState(state);
    if (whole != NULL)
        nsAllocator::Free(whole);
    return header;
}

nsresult
nsInterfaceInfoManager::indexify_file(const nsFileSpec *fileSpec)
{
    XPTHeader *header = getHeader(fileSpec);
    if (header == NULL) {
        // XXX glean something more meaningful from getHeader?
        return NS_ERROR_FAILURE;
    }

    int limit = header->num_interfaces;
    nsTypelibRecord *tlrecord = new nsTypelibRecord(limit, this->typelibRecords,
                                                    header);
    if (tlrecord == NULL) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // add it to the list of typelibs; this->typelibRecords record supplied
    // to constructor becomes the 'next' field in the new nsTypelibRecord.
    this->typelibRecords = tlrecord; 


    for (int i = 0; i < limit; i++) {
        XPTInterfaceDirectoryEntry *current = header->interface_directory + i;

        // find or create an interface record, and set the appropriate
        // slot in the nsTypelibRecord array.
        static const nsID zeroIID =
        { 0x0, 0x0, 0x0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };
        nsInterfaceRecord *record = NULL;

        PRBool iidIsZero = current->iid.Equals(zeroIID);

        // XXX fix bogus repetitive logic.
        PRBool shouldAddToIDDTable = PR_TRUE;

        if (iidIsZero == PR_FALSE) {
            // prefer the iid.
            nsIDKey idKey(current->iid);
            record = (nsInterfaceRecord *)this->IIDTable->Get(&idKey);
            // if it wasn't in the iid table then maybe it was already entered
            // in the nametable. If so then we should reuse that record.
            if (record == NULL) {
                record = (nsInterfaceRecord *)
                    PL_HashTableLookup(this->nameTable, current->name);
            }
        } else {
            shouldAddToIDDTable = PR_FALSE;
            // resort to using the name.  Warn?
            record = (nsInterfaceRecord *)PL_HashTableLookup(this->nameTable,
                                                             current->name);
        }

        // if none was found, create one and tuck it into the appropriate places.
        if (record == NULL) {
            record = new nsInterfaceRecord();
            record->typelibRecord = NULL;
            record->interfaceDescriptor = NULL;
            record->info = NULL;

            // XXX copy these values?
            record->name = current->name;
            record->name_space = current->name_space;

            // add it to the name->interfaceRecord table
            // XXX check that result (PLHashEntry *) isn't NULL
            PL_HashTableAdd(this->nameTable, current->name, record);

            if (iidIsZero == PR_FALSE) {
                // Add it to the iid table too, if we have an iid.
                // don't check against old value, b/c we shouldn't have one.
                shouldAddToIDDTable = PR_FALSE;
                nsIDKey idKey(current->iid);
                this->IIDTable->Put(&idKey, record);
            }
        }
                
        // Is the entry we're looking at resolved?
        if (current->interface_descriptor != NULL) {
            if (record->interfaceDescriptor != NULL) {
                char *fileName = fileSpec->GetLeafName();
                char *warnstr = PR_smprintf
                    ("interface %s in typelib %s overrides previous definition",
                     current->name, fileName);
                NS_WARNING(warnstr);
                PR_smprintf_free(warnstr);
                nsCRT::free(fileName);
            }
            record->interfaceDescriptor = current->interface_descriptor;
            record->typelibRecord = tlrecord;
            record->iid = current->iid;

            if (shouldAddToIDDTable == PR_TRUE) {
                nsIDKey idKey(current->iid);
                this->IIDTable->Put(&idKey, record);
            }
        }

        // all fixed up?  Put a pointer to the interfaceRecord we
        // found/made into the appropriate place.
        *(tlrecord->interfaceRecords + i) = record;
    }
    return NS_OK;
}

// as many InterfaceDirectoryEntries as we expect to see.
#define XPT_HASHSIZE 512

#ifdef DEBUG_iim
static PRIntn
check_nametable_enumerator(PLHashEntry *he, PRIntn index, void *arg)
{
    char *key = (char *)he->key;
    nsInterfaceRecord *value = (nsInterfaceRecord *)he->value;
    nsHashtable *iidtable = (nsHashtable *)arg;

    TRACE((stderr, "name table has %s\n", key));

    if (value->interfaceDescriptor == NULL) {
        TRACE((stderr, "unresolved interface %s\n", key));
    } else {
        nsIDKey idKey(value->iid);
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

static PRBool
check_iidtable_enumerator(nsHashKey *aKey, void *aData, void *closure)
{
//      PLHashTable *nameTable = (PLHashTable *)closure;
    nsInterfaceRecord *record = (nsInterfaceRecord *)aData;
    // can I do anything with the key?
    TRACE((stderr, "record has name %s, iid %s\n",
            record->name, record->iid.ToString()));
    return PR_TRUE;
}

#endif // DEBUG_iim

nsresult
nsInterfaceInfoManager::initInterfaceTables()
{
    // make a hashtable to map names to interface records.
    this->nameTable = PL_NewHashTable(XPT_HASHSIZE,
                                       PL_HashString,  // hash keys
                                       PL_CompareStrings, // compare keys
                                       PL_CompareValues, // comp values
                                       NULL, NULL);
    if (this->nameTable == NULL)
        return NS_ERROR_FAILURE;

    // make a hashtable to map iids to interface records.
    this->IIDTable = new nsHashtable(XPT_HASHSIZE);
    if (this->IIDTable == NULL) {
        PL_HashTableDestroy(this->nameTable);
        return NS_ERROR_FAILURE;
    }

    // this seems to be the One True Way to get the components directory
    // (frankm@eng.sun.com, 9.9.99)
    nsSpecialSystemDirectory 
        sysdir(nsSpecialSystemDirectory::XPCOM_CurrentProcessComponentDirectory);
    
#ifdef XP_MAC
    PRBool wasAlias;
    sysdir.ResolveSymlink(wasAlias);
#endif
    
#ifdef DEBUG_iim
    int which = 0;
#endif
    
    for (nsDirectoryIterator i(sysdir, PR_FALSE); i.Exists(); i++) {
        // XXX need to copy?
        nsFileSpec spec = i.Spec();
        
#ifdef XP_MAC
        spec.ResolveSymlink(wasAlias);
#endif
        
        // See if this is a .xpt file by looking at the filename                
        char*   fileName = spec.GetLeafName();
        int flen = PL_strlen(fileName);
        if (flen < 4 || PL_strcasecmp(&(fileName[flen - 4]), ".xpt")) {
            nsCRT::free(fileName);
            continue;
        }
        
        if (! spec.IsFile())
            continue;
        
        // it's a valid file, read it in.
#ifdef DEBUG_iim
        which++;
        TRACE((stderr, "%d %s\n", which, fileName));
#endif
        
        nsresult nsr = this->indexify_file(&spec);
        if (NS_FAILED(nsr)) {
            char *warnstr = PR_smprintf("failed to process typelib file %s",
                                        fileName);
            NS_WARNING(warnstr);
            PR_smprintf_free(warnstr);
        }
        nsCRT::free(fileName);
    }
    
#ifdef DEBUG_iim
    TRACE((stderr, "\nchecking name table for unresolved entries...\n"));
    // scan here to confirm that all interfaces are resolved.
    PL_HashTableEnumerateEntries(this->nameTable,
                                 check_nametable_enumerator,
                                 this->IIDTable);

    TRACE((stderr, "\nchecking iid table for unresolved entries...\n"));
    IIDTable->Enumerate(check_iidtable_enumerator, this->nameTable);

#endif
    return NS_OK;
}

static PRIntn
free_nametable_records(PLHashEntry *he, PRIntn index, void *arg)
{
    nsInterfaceRecord *value = (nsInterfaceRecord *)he->value;
    if (value) {
        delete value;
    }
    return HT_ENUMERATE_NEXT;
}

nsInterfaceInfoManager::~nsInterfaceInfoManager()
{
    nsTypelibRecord::DestroyList(typelibRecords);
    if (nameTable) {
        PL_HashTableEnumerateEntries(nameTable,
                                     free_nametable_records,
                                     0);
        PL_HashTableDestroy(nameTable);
    }
    delete IIDTable;
}

NS_IMETHODIMP
nsInterfaceInfoManager::GetInfoForIID(const nsIID* iid,
                                      nsIInterfaceInfo **info)
{
    nsIDKey idKey(*iid);
    nsInterfaceRecord *record =
        (nsInterfaceRecord *)this->IIDTable->Get(&idKey);
    if (record == NULL) {
        *info = NULL;
        return NS_ERROR_FAILURE;
    }
    
    return record->GetInfo((nsInterfaceInfo **)info);
}

NS_IMETHODIMP
nsInterfaceInfoManager::GetInfoForName(const char* name,
                                       nsIInterfaceInfo **info)
{
    nsInterfaceRecord *record =
        (nsInterfaceRecord *)PL_HashTableLookup(this->nameTable, name);
    if (record == NULL) {
        *info = NULL;
        return NS_ERROR_FAILURE;
    }

    return record->GetInfo((nsInterfaceInfo **)info);
}

NS_IMETHODIMP
nsInterfaceInfoManager::GetIIDForName(const char* name, nsIID** iid)
{
    nsInterfaceRecord *record =
        (nsInterfaceRecord *)PL_HashTableLookup(this->nameTable, name);
    if (record == NULL) {
        *iid = NULL;
        return NS_ERROR_FAILURE;
    }
    
    return record->GetIID(iid);
}

NS_IMETHODIMP
nsInterfaceInfoManager::GetNameForIID(const nsIID* iid, char** name)
{
    nsIDKey idKey(*iid);
    nsInterfaceRecord *record =
        (nsInterfaceRecord *)this->IIDTable->Get(&idKey);
    if (record == NULL) {
        *name = NULL;
        return NS_ERROR_FAILURE;
    }

#ifdef DEBUG
    // Note that this might fail for same-name, different-iid interfaces!
    nsIID *newid;
    nsresult isok = GetIIDForName(record->name, &newid);
    PR_ASSERT(!(NS_FAILED(isok)));
    PR_ASSERT(newid->Equals(*newid));
#endif
    PR_ASSERT(record->name != NULL);
    
    *name = (char *)nsAllocator::Clone(record->name, strlen(record->name) + 1);
    if (*name == NULL)
        return NS_ERROR_FAILURE;
    return NS_OK;
}    

static
NS_IMETHODIMP convert_interface_record(nsHashKey *key, void *data,
                                       void *convert_data, nsISupports **retval)
{
    nsInterfaceRecord *rec = (nsInterfaceRecord *) data;
    nsInterfaceInfo *iinfo;
    nsresult rv;
    
    if(NS_FAILED(rv = rec->GetInfo(&iinfo)))
    {
#ifdef DEBUG
        char *name;
        rec->GetName(&name);
        TRACE((stderr, "GetInfo failed for InterfaceRecord '%s'\n", name));
        nsAllocator::Free(name);
#endif
        return NS_ERROR_FAILURE;
    }
    
    rv = iinfo->QueryInterface(NS_GET_IID(nsISupports),
                               (void **)retval);

#ifdef DEBUG
    if(NS_FAILED(rv))
    {
        char *name;
        rec->GetName(&name);
        TRACE((stderr, "QI[nsISupports] Failed for InterfaceInfo '%s'\n",
               name));
        nsAllocator::Free(name);
    }
#endif
        
    return rv;
    
}

NS_IMETHODIMP
nsInterfaceInfoManager::EnumerateInterfaces(nsIEnumerator** emumerator)
{
    if(!emumerator)
    {
        NS_ASSERTION(0, "null ptr");
        return NS_ERROR_NULL_POINTER;
    }

    return NS_NewHashtableEnumerator(this->IIDTable, convert_interface_record,
                                     nsnull, emumerator);
}        

XPTI_PUBLIC_API(nsIInterfaceInfoManager*)
XPTI_GetInterfaceInfoManager()
{
    return nsInterfaceInfoManager::GetInterfaceInfoManager();
}

XPTI_PUBLIC_API(void)
XPTI_FreeInterfaceInfoManager()
{
    nsInterfaceInfoManager::FreeInterfaceInfoManager();
}
