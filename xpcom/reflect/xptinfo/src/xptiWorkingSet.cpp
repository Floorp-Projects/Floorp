/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

PRBool xptiWorkingSet::DirectoryAtHasPersistentDescriptor(PRUint32 i, 
                                                          const char* desc)
{
    nsCOMPtr<nsILocalFile> dir;
    GetDirectoryAt(i, getter_AddRefs(dir));
    if(!dir)
        return PR_FALSE;

    nsXPIDLCString str;
    if(NS_FAILED(dir->GetPersistentDescriptor(getter_Copies(str))))
        return PR_FALSE;
    
    return 0 == PL_strcmp(desc, str);
}

