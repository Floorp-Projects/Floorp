/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike McCabe <mccabe@netscape.com>
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* Implementation of xptiWorkingSet. */

#include "xptiprivate.h"
#include "nsString.h"

#define XPTI_STRING_ARENA_BLOCK_SIZE    (1024 * 1)
#define XPTI_STRUCT_ARENA_BLOCK_SIZE    (1024 * 1)
#define XPTI_HASHTABLE_SIZE             128

/***************************************************************************/

PR_STATIC_CALLBACK(const void*)
IIDGetKey(PLDHashTable *table, PLDHashEntryHdr *entry)
{
    return ((xptiHashEntry*)entry)->value->GetTheIID();
}

PR_STATIC_CALLBACK(PLDHashNumber)
IIDHash(PLDHashTable *table, const void *key)
{
    return (PLDHashNumber) ((const nsIID*)key)->m0;        
}

PR_STATIC_CALLBACK(PRBool)
IIDMatch(PLDHashTable *table,
         const PLDHashEntryHdr *entry,
         const void *key)
{
    const nsIID* iid1 = ((xptiHashEntry*)entry)->value->GetTheIID();
    const nsIID* iid2 = (const nsIID*)key;
    
    return iid1 == iid2 || iid1->Equals(*iid2);
}       
          
const static struct PLDHashTableOps IIDTableOps =
{
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    IIDGetKey,
    IIDHash,
    IIDMatch,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub
};

/***************************************************************************/

PR_STATIC_CALLBACK(const void*)
NameGetKey(PLDHashTable *table, PLDHashEntryHdr *entry)
{
    return ((xptiHashEntry*)entry)->value->GetTheName();
}

PR_STATIC_CALLBACK(PRBool)
NameMatch(PLDHashTable *table,
          const PLDHashEntryHdr *entry,
          const void *key)
{
    const char* str1 = ((xptiHashEntry*)entry)->value->GetTheName();
    const char* str2 = (const char*) key;
    return str1 == str2 || 0 == PL_strcmp(str1, str2);
}       

static const struct PLDHashTableOps NameTableOps =
{
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    NameGetKey,
    PL_DHashStringKey,
    NameMatch,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub
};

/***************************************************************************/

MOZ_DECL_CTOR_COUNTER(xptiWorkingSet)

xptiWorkingSet::xptiWorkingSet(nsISupportsArray* aDirectories)
    : mFileCount(0),
      mMaxFileCount(0),
      mFileArray(nsnull),
      mZipItemCount(0),
      mMaxZipItemCount(0),
      mZipItemArray(nsnull),
      mStringArena(XPT_NewArena(XPTI_STRING_ARENA_BLOCK_SIZE, sizeof(char),
                                "xptiWorkingSet strings")),
      mStructArena(XPT_NewArena(XPTI_STRUCT_ARENA_BLOCK_SIZE, sizeof(double),
                                "xptiWorkingSet structs")),
      mDirectories(aDirectories),
      mNameTable(PL_NewDHashTable(&NameTableOps, nsnull, sizeof(xptiHashEntry),
                                  XPTI_HASHTABLE_SIZE)),
      mIIDTable(PL_NewDHashTable(&IIDTableOps, nsnull, sizeof(xptiHashEntry),
                                 XPTI_HASHTABLE_SIZE)),
      mFileMergeOffsetMap(nsnull),
      mZipItemMergeOffsetMap(nsnull)
{
    MOZ_COUNT_CTOR(xptiWorkingSet);
    // do nothing else...            
}        

PRBool 
xptiWorkingSet::IsValid() const
{
    return  (mFileCount == 0 || mFileArray) &&
            (mZipItemCount == 0 || mZipItemArray) &&
            mStringArena &&
            mStructArena &&
            mNameTable &&
            mIIDTable;          
}

PR_STATIC_CALLBACK(PLDHashOperator)
xpti_Remover(PLDHashTable *table, PLDHashEntryHdr *hdr,
             PRUint32 number, void *arg)
{
    return PL_DHASH_REMOVE;
}       

PR_STATIC_CALLBACK(PLDHashOperator)
xpti_Invalidator(PLDHashTable *table, PLDHashEntryHdr *hdr,
                 PRUint32 number, void *arg)
{
    xptiInterfaceEntry* entry = ((xptiHashEntry*)hdr)->value;
    entry->LockedInvalidateInterfaceInfo();
    return PL_DHASH_NEXT;
}

void 
xptiWorkingSet::InvalidateInterfaceInfos()
{
    if(mNameTable)
    {
        nsAutoMonitor lock(xptiInterfaceInfoManager::GetInfoMonitor());
        PL_DHashTableEnumerate(mNameTable, xpti_Invalidator, nsnull);
    }
}        

void 
xptiWorkingSet::ClearHashTables()
{
    if(mNameTable)
        PL_DHashTableEnumerate(mNameTable, xpti_Remover, nsnull);
        
    if(mIIDTable)
        PL_DHashTableEnumerate(mIIDTable, xpti_Remover, nsnull);
}

void 
xptiWorkingSet::ClearFiles()
{
    if(mFileArray)
        delete [] mFileArray;
    mFileArray = nsnull;
    mMaxFileCount = 0;
    mFileCount = 0;
}

void 
xptiWorkingSet::ClearZipItems()
{
    if(mZipItemArray)
        delete [] mZipItemArray;
    mZipItemArray = nsnull;
    mMaxZipItemCount = 0;
    mZipItemCount = 0;
}

xptiWorkingSet::~xptiWorkingSet()
{
    MOZ_COUNT_DTOR(xptiWorkingSet);

    ClearFiles();
    ClearZipItems();
    ClearHashTables();

    if(mNameTable)
        PL_DHashTableDestroy(mNameTable);
        
    if(mIIDTable)
        PL_DHashTableDestroy(mIIDTable);

    if(mFileArray)
        delete [] mFileArray;

    if(mZipItemArray)
        delete [] mZipItemArray;

    // Destroy arenas last in case they are referenced in other members' dtors.

    if(mStringArena)
    {
#ifdef DEBUG
        XPT_DumpStats(mStringArena);
#endif        
        XPT_DestroyArena(mStringArena);
    }
    
    if(mStructArena)
    {
#ifdef DEBUG
        XPT_DumpStats(mStructArena);
#endif        
        XPT_DestroyArena(mStructArena);
    }
}        

PRUint32 
xptiWorkingSet::FindFile(PRUint32 dir, const char* name)
{
    if(mFileArray)
    {
        for(PRUint32 i = 0; i < mFileCount;++i)
        {
            xptiFile& file = mFileArray[i];
            if(file.GetDirectory() == dir && 
               0 == PL_strcmp(name, file.GetName()))
            {
                return i;
            }    
        }
    }
    return NOT_FOUND;
}

PRBool
xptiWorkingSet::NewFileArray(PRUint32 count)
{
    if(mFileArray)
        delete [] mFileArray;
    mFileCount = 0;
    mFileArray = new xptiFile[count];
    if(!mFileArray)
    {
        mMaxFileCount = 0;
        return PR_FALSE;
    }
    mMaxFileCount = count;
    return PR_TRUE;
}

PRBool 
xptiWorkingSet::ExtendFileArray(PRUint32 count)
{
    if(mFileArray && count < mMaxFileCount)
        return PR_TRUE;

    xptiFile* newArray = new xptiFile[count];
    if(!newArray)
        return PR_FALSE;

    if(mFileArray)
    {
        for(PRUint32 i = 0; i < mFileCount; ++i)
            newArray[i] = mFileArray[i];
        delete [] mFileArray;
    }
    mFileArray = newArray;
    mMaxFileCount = count;
    return PR_TRUE;
}

/***************************************************************************/

PRUint32 
xptiWorkingSet::FindZipItemWithName(const char* name)
{
    if(mZipItemArray)
    {
        for(PRUint32 i = 0; i < mZipItemCount;++i)
            if(0 == PL_strcmp(name, mZipItemArray[i].GetName()))
                return i;
    }
    return NOT_FOUND;
}

PRBool
xptiWorkingSet::NewZipItemArray(PRUint32 count)
{
    if(mZipItemArray)
        delete [] mZipItemArray;
    mZipItemCount = 0;
    mZipItemArray = new xptiZipItem[count];
    if(!mZipItemArray)
    {
        mMaxZipItemCount = 0;
        return PR_FALSE;
    }
    mMaxZipItemCount = count;
    return PR_TRUE;
}

PRBool 
xptiWorkingSet::ExtendZipItemArray(PRUint32 count)
{
    if(mZipItemArray && count < mMaxZipItemCount)
        return PR_TRUE;

    xptiZipItem* newArray = new xptiZipItem[count];
    if(!newArray)
        return PR_FALSE;

    if(mZipItemArray)
    {
        for(PRUint32 i = 0; i < mZipItemCount; ++i)
            newArray[i] = mZipItemArray[i];
        delete [] mZipItemArray;
    }
    mZipItemArray = newArray;
    mMaxZipItemCount = count;
    return PR_TRUE;
}

/***************************************************************************/
// Directory stuff...

PRUint32 xptiWorkingSet::GetDirectoryCount()
{
    PRUint32 count = 0;
    mDirectories->Count(&count);
    return count;
}

nsresult xptiWorkingSet::GetCloneOfDirectoryAt(PRUint32 i, nsILocalFile** dir)
{
    return xptiCloneElementAsLocalFile(mDirectories, i, dir);
}

nsresult xptiWorkingSet::GetDirectoryAt(PRUint32 i, nsILocalFile** dir)
{
    return mDirectories->QueryElementAt(i, NS_GET_IID(nsILocalFile), (void**)dir);
}       

PRBool xptiWorkingSet::FindDirectory(nsILocalFile* dir, PRUint32* index)
{
    PRUint32 count;
    nsresult rv = mDirectories->Count(&count);
    if(NS_FAILED(rv))
        return PR_FALSE;

    for(PRUint32 i = 0; i < count; i++)
    {
        PRBool same;
        nsCOMPtr<nsILocalFile> current;
        mDirectories->QueryElementAt(i, NS_GET_IID(nsILocalFile), 
                                     getter_AddRefs(current));
        if(!current || NS_FAILED(current->Equals(dir, &same)))
            break;
        if(same)
        {
            *index = i;    
            return PR_TRUE;       
        }
    }
    return PR_FALSE;
}

PRBool xptiWorkingSet::FindDirectoryOfFile(nsILocalFile* file, PRUint32* index)
{
    nsCOMPtr<nsIFile> dirAbstract;
    file->GetParent(getter_AddRefs(dirAbstract));
    if(!dirAbstract)
        return PR_FALSE;
    nsCOMPtr<nsILocalFile> dir = do_QueryInterface(dirAbstract);
    if(!dir)
        return PR_FALSE;
    return FindDirectory(dir, index);
}

PRBool xptiWorkingSet::DirectoryAtMatchesPersistentDescriptor(PRUint32 i, 
                                                          const char* inDesc)
{
    nsCOMPtr<nsILocalFile> dir;
    GetDirectoryAt(i, getter_AddRefs(dir));
    if(!dir)
        return PR_FALSE;

    nsCOMPtr<nsILocalFile> descDir;
    nsresult rv = NS_NewNativeLocalFile(EmptyCString(), PR_FALSE, getter_AddRefs(descDir));
    if(NS_FAILED(rv))
        return PR_FALSE;

    rv = descDir->SetPersistentDescriptor(nsDependentCString(inDesc));
    if(NS_FAILED(rv))
        return PR_FALSE;
    
    PRBool matches;
    rv = dir->Equals(descDir, &matches);
    return NS_SUCCEEDED(rv) && matches;
}

