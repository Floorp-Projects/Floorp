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
 */

/* Implementation of xptiWorkingSet. */

#include "xptiprivate.h"

#define XPTI_STRING_ARENA_BLOCK_SIZE    (1024 * 1)
#define XPTI_STRUCT_ARENA_BLOCK_SIZE    (1024 * 1)
#define XPTI_HASHTABLE_SIZE             128

PR_STATIC_CALLBACK(PLHashNumber)
xpti_HashIID(const void *key)
{
    return ((nsID*)key)->m0;        
}

PR_STATIC_CALLBACK(PRIntn)
xpti_CompareIIDs(const void *v1, const void *v2)
{
    return (PRIntn) ((const nsID*)v1)->Equals(*((const nsID*)v2));        
}         

MOZ_DECL_CTOR_COUNTER(xptiWorkingSet)

xptiWorkingSet::xptiWorkingSet()
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
      mNameTable(PL_NewHashTable(XPTI_HASHTABLE_SIZE, PL_HashString,
                                 PL_CompareStrings, PL_CompareValues,
                                 nsnull, nsnull)),
      mIIDTable(PL_NewHashTable(XPTI_HASHTABLE_SIZE, xpti_HashIID,
                                xpti_CompareIIDs, PL_CompareValues,
                                nsnull, nsnull)),
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

PR_STATIC_CALLBACK(PRIntn)
xpti_Remover(PLHashEntry *he, PRIntn i, void *arg)
{
  return HT_ENUMERATE_REMOVE;
}       

PR_STATIC_CALLBACK(PRIntn)
xpti_ReleaseAndRemover(PLHashEntry *he, PRIntn i, void *arg)
{
  nsIInterfaceInfo* ii = (nsIInterfaceInfo*) he->value;
  NS_RELEASE(ii);
  return HT_ENUMERATE_REMOVE;
}
 

PR_STATIC_CALLBACK(PRIntn)
xpti_Invalidator(PLHashEntry *he, PRIntn i, void *arg)
{
  ((xptiInterfaceInfo*)he->value)->Invalidate();
  return HT_ENUMERATE_NEXT;
}

void 
xptiWorkingSet::InvalidateInterfaceInfos()
{
    if(mNameTable)
        PL_HashTableEnumerateEntries(mNameTable, xpti_Invalidator, nsnull);
}        

void 
xptiWorkingSet::ClearHashTables()
{
    if(mNameTable)
        PL_HashTableEnumerateEntries(mNameTable, xpti_Remover, nsnull);
        
    if(mIIDTable)
        PL_HashTableEnumerateEntries(mIIDTable, xpti_ReleaseAndRemover, nsnull);
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
        PL_HashTableDestroy(mNameTable);
        
    if(mIIDTable)
        PL_HashTableDestroy(mIIDTable);

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
xptiWorkingSet::FindFileWithName(const char* name)
{
    if(mFileArray)
    {
        for(PRUint32 i = 0; i < mFileCount;++i)
            if(0 == PL_strcasecmp(name, mFileArray[i].GetName()))
                return i;
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
            if(0 == PL_strcasecmp(name, mZipItemArray[i].GetName()))
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
