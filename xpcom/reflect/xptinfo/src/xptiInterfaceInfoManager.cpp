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

/* Implementation of xptiInterfaceInfoManager. */

#include "xptiprivate.h"
#include "nsDependentString.h"
#include "nsString.h"

#define NS_ZIPLOADER_CONTRACTID NS_XPTLOADER_CONTRACTID_PREFIX "zip"

NS_IMPL_THREADSAFE_ISUPPORTS2(xptiInterfaceInfoManager, 
                              nsIInterfaceInfoManager,
                              nsIInterfaceInfoSuperManager)

static xptiInterfaceInfoManager* gInterfaceInfoManager = nsnull;
#ifdef DEBUG
static int gCallCount = 0;
#endif

// static
xptiInterfaceInfoManager*
xptiInterfaceInfoManager::GetInterfaceInfoManagerNoAddRef()
{
    if(!gInterfaceInfoManager)
    {
        nsCOMPtr<nsISupportsArray> searchPath;
        BuildFileSearchPath(getter_AddRefs(searchPath));
        if(!searchPath)
        {
            NS_ERROR("can't get xpt search path!");    
            return nsnull;
        }

        gInterfaceInfoManager = new xptiInterfaceInfoManager(searchPath);
        if(gInterfaceInfoManager)
            NS_ADDREF(gInterfaceInfoManager);
        if(!gInterfaceInfoManager->IsValid())
        {
            NS_RELEASE(gInterfaceInfoManager);
        }
        else
        {
            PRBool mustAutoReg = 
                    !xptiManifest::Read(gInterfaceInfoManager, 
                                        &gInterfaceInfoManager->mWorkingSet);
#ifdef DEBUG
            {
            // This sets what will be returned by GetOpenLogFile().
            xptiAutoLog autoLog(gInterfaceInfoManager, 
                                gInterfaceInfoManager->mAutoRegLogFile, PR_TRUE);
            LOG_AUTOREG(("debug build forced autoreg after %s load of manifest\n", mustAutoReg ? "FAILED" : "successful"));
        
            mustAutoReg = PR_TRUE;
            }
#endif // DEBUG
            if(mustAutoReg)
                gInterfaceInfoManager->AutoRegisterInterfaces();
        }
    }
    return gInterfaceInfoManager;
}

void
xptiInterfaceInfoManager::FreeInterfaceInfoManager()
{
    if(gInterfaceInfoManager)
        gInterfaceInfoManager->LogStats();

    NS_IF_RELEASE(gInterfaceInfoManager);
}

PRBool 
xptiInterfaceInfoManager::IsValid()
{
    return mWorkingSet.IsValid() &&
           mResolveLock &&
           mAutoRegLock &&
           mInfoMonitor &&
           mAdditionalManagersLock;
}        

xptiInterfaceInfoManager::xptiInterfaceInfoManager(nsISupportsArray* aSearchPath)
    :   mWorkingSet(aSearchPath),
        mOpenLogFile(nsnull),
        mResolveLock(PR_NewLock()),
        mAutoRegLock(PR_NewLock()),
        mInfoMonitor(nsAutoMonitor::NewMonitor("xptiInfoMonitor")),
        mAdditionalManagersLock(PR_NewLock()),
        mSearchPath(aSearchPath)
{
    const char* statsFilename = PR_GetEnv("MOZILLA_XPTI_STATS");
    if(statsFilename)
    {
        mStatsLogFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);         
        if(mStatsLogFile && 
           NS_SUCCEEDED(mStatsLogFile->InitWithNativePath(nsDependentCString(statsFilename))))
        {
            printf("* Logging xptinfo stats to: %s\n", statsFilename);
        }
        else
        {
            printf("* Failed to create xptinfo stats file: %s\n", statsFilename);
            mStatsLogFile = nsnull;
        }
    }

    const char* autoRegFilename = PR_GetEnv("MOZILLA_XPTI_REGLOG");
    if(autoRegFilename)
    {
        mAutoRegLogFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);         
        if(mAutoRegLogFile && 
           NS_SUCCEEDED(mAutoRegLogFile->InitWithNativePath(nsDependentCString(autoRegFilename))))
        {
            printf("* Logging xptinfo autoreg to: %s\n", autoRegFilename);
        }
        else
        {
            printf("* Failed to create xptinfo autoreg file: %s\n", autoRegFilename);
            mAutoRegLogFile = nsnull;
        }
    }
}

xptiInterfaceInfoManager::~xptiInterfaceInfoManager()
{
    // We only do this on shutdown of the service.
    mWorkingSet.InvalidateInterfaceInfos();

    if(mResolveLock)
        PR_DestroyLock(mResolveLock);
    if(mAutoRegLock)
        PR_DestroyLock(mAutoRegLock);
    if(mInfoMonitor)
        nsAutoMonitor::DestroyMonitor(mInfoMonitor);
    if(mAdditionalManagersLock)
        PR_DestroyLock(mAdditionalManagersLock);

    gInterfaceInfoManager = nsnull;
#ifdef DEBUG
    xptiInterfaceInfo::DEBUG_ShutdownNotification();
    gCallCount = 0;
#endif
}

static nsresult
GetDirectoryFromDirService(const char* codename, nsILocalFile** aDir)
{
    NS_ASSERTION(codename,"loser!");
    NS_ASSERTION(aDir,"loser!");
    
    nsresult rv;
    nsCOMPtr<nsIProperties> dirService =
        do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    return dirService->Get(codename, NS_GET_IID(nsILocalFile), (void**) aDir);
}

static PRBool
AppendFromDirServiceList(const char* codename, nsISupportsArray* aPath)
{
    NS_ASSERTION(codename,"loser!");
    NS_ASSERTION(aPath,"loser!");
    
    nsCOMPtr<nsIProperties> dirService = 
        do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
    if(!dirService)
        return PR_FALSE;

    nsCOMPtr<nsISimpleEnumerator> fileList;
    dirService->Get(codename, NS_GET_IID(nsISimpleEnumerator), 
                    getter_AddRefs(fileList));
    if(!fileList)
        return PR_FALSE;
    
    PRBool more;
    while(NS_SUCCEEDED(fileList->HasMoreElements(&more)) && more)
    {
        nsCOMPtr<nsILocalFile> dir;
        fileList->GetNext(getter_AddRefs(dir));
        if(!dir || !aPath->AppendElement(dir))
            return PR_FALSE;
    }

    return PR_TRUE;
}        

// static 
PRBool xptiInterfaceInfoManager::BuildFileSearchPath(nsISupportsArray** aPath)
{
#ifdef DEBUG
    NS_ASSERTION(!gCallCount++, "Expected only one call!");
#endif

    nsCOMPtr<nsISupportsArray> searchPath;
    NS_NewISupportsArray(getter_AddRefs(searchPath));
    if(!searchPath)
        return PR_FALSE;
    
    nsCOMPtr<nsILocalFile> compDir;

    // Always put components directory first

    if(NS_FAILED(GetDirectoryFromDirService(NS_XPCOM_COMPONENT_DIR, 
                                            getter_AddRefs(compDir))) ||
       !searchPath->AppendElement(compDir))
    {
        return PR_FALSE;
    }

    // Add additional plugins dirs
    // No error checking here since this is optional in some embeddings
    
    // Add the GRE's component directory to searchPath if the 
    // application is using an GRE.
    // An application indicates that it's using an GRE by returning
    // a valid nsIFile via it's directory service provider interface.
    //
    // Please see http://www.mozilla.org/projects/embedding/MRE.html
    // for more info. on GREs
    //
    nsCOMPtr<nsILocalFile> greComponentDirectory;
    nsresult rv = GetDirectoryFromDirService(NS_GRE_COMPONENT_DIR, 
                                    getter_AddRefs(greComponentDirectory));
    if(NS_SUCCEEDED(rv) && greComponentDirectory)
    {
        // make sure we only append a directory if its a different one
        PRBool equalsCompDir = PR_FALSE;
        greComponentDirectory->Equals(compDir, &equalsCompDir);

        if(!equalsCompDir)
            searchPath->AppendElement(greComponentDirectory);
    }

    (void)AppendFromDirServiceList(NS_XPCOM_COMPONENT_DIR_LIST, searchPath);
    (void)AppendFromDirServiceList(NS_APP_PLUGINS_DIR_LIST, searchPath);

    NS_ADDREF(*aPath = searchPath);
    return PR_TRUE;
}

PRBool 
xptiInterfaceInfoManager::GetCloneOfManifestLocation(nsILocalFile** aFile)
{
    // We *trust* that this will not change!
    nsCOMPtr<nsILocalFile> lf;
    nsresult rv = GetDirectoryFromDirService(NS_XPCOM_XPTI_REGISTRY_FILE, 
                                             getter_AddRefs(lf));

    if (NS_FAILED(rv)) return PR_FALSE;

    rv = xptiCloneLocalFile(lf, aFile);
    if (NS_FAILED(rv)) return PR_FALSE;
    return PR_TRUE;
}

PRBool 
xptiInterfaceInfoManager::GetApplicationDir(nsILocalFile** aDir)
{
    // We *trust* that this will not change!
    return NS_SUCCEEDED(GetDirectoryFromDirService(NS_XPCOM_CURRENT_PROCESS_DIR, aDir));
}

PRBool 
xptiInterfaceInfoManager::BuildFileList(nsISupportsArray* aSearchPath,
                                        nsISupportsArray** aFileList)
{
    NS_ASSERTION(aFileList, "loser!");
    
    nsresult rv;

    nsCOMPtr<nsISupportsArray> fileList = 
        do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID);
    if(!fileList)
        return PR_FALSE;

    PRUint32 pathCount;
    if(NS_FAILED(aSearchPath->Count(&pathCount)))
        return PR_FALSE;

    for(PRUint32 i = 0; i < pathCount; i++)
    {
        nsCOMPtr<nsILocalFile> dir;
        rv = xptiCloneElementAsLocalFile(aSearchPath, i, getter_AddRefs(dir));
        if(NS_FAILED(rv) || !dir)
            return PR_FALSE;

        nsCOMPtr<nsISimpleEnumerator> entries;
        rv = dir->GetDirectoryEntries(getter_AddRefs(entries));
        if(NS_FAILED(rv) || !entries)
            continue;

        PRUint32 count = 0;
        PRBool hasMore;
        while(NS_SUCCEEDED(entries->HasMoreElements(&hasMore)) && hasMore)
        {
            nsCOMPtr<nsISupports> sup;
            entries->GetNext(getter_AddRefs(sup));
            if(!sup)
                return PR_FALSE;
            nsCOMPtr<nsILocalFile> file = do_QueryInterface(sup);
            if(!file)
                return PR_FALSE;

            PRBool isFile;
            if(NS_FAILED(file->IsFile(&isFile)) || !isFile)
            {
                continue;
            }
     
            nsCAutoString name;
            if(NS_FAILED(file->GetNativeLeafName(name)))
                return PR_FALSE;

            if(xptiFileType::IsUnknown(name.get()))
                continue;

            LOG_AUTOREG(("found file: %s\n", name.get()));

            if(!fileList->InsertElementAt(file, count))
                return PR_FALSE;
            ++count;
        }
    }

    NS_ADDREF(*aFileList = fileList); 
    return PR_TRUE;
}

XPTHeader* 
xptiInterfaceInfoManager::ReadXPTFile(nsILocalFile* aFile, 
                                      xptiWorkingSet* aWorkingSet)
{
    NS_ASSERTION(aFile, "loser!");

    XPTHeader *header = nsnull;
    char *whole = nsnull;
    PRFileDesc*   fd = nsnull;
    XPTState *state = nsnull;
    XPTCursor cursor;
    PRInt32 flen;
    PRInt64 fileSize;
    
    PRBool saveFollowLinks;
    aFile->GetFollowLinks(&saveFollowLinks);
    aFile->SetFollowLinks(PR_TRUE);

    if(NS_FAILED(aFile->GetFileSize(&fileSize)) || !(flen = nsInt64(fileSize)))
    {
        aFile->SetFollowLinks(saveFollowLinks);
        return nsnull;
    }

    whole = new char[flen];
    if (!whole)
    {
        aFile->SetFollowLinks(saveFollowLinks);
        return nsnull;
    }

    // all exits from on here should be via 'goto out' 

    if(NS_FAILED(aFile->OpenNSPRFileDesc(PR_RDONLY, 0444, &fd)) || !fd)
    {
        goto out;
    }

    if(flen > PR_Read(fd, whole, flen))
    {
        goto out;
    }

    if(!(state = XPT_NewXDRState(XPT_DECODE, whole, flen)))
    {
        goto out;
    }
    
    if(!XPT_MakeCursor(state, XPT_HEADER, 0, &cursor))
    {
        goto out;
    }
    
    if (!XPT_DoHeader(aWorkingSet->GetStructArena(), &cursor, &header))
    {
        header = nsnull;
        goto out;
    }

 out:
    if(fd)
        PR_Close(fd);
    if(state)
        XPT_DestroyXDRState(state);
    if(whole)
        delete [] whole;
    aFile->SetFollowLinks(saveFollowLinks);
    return header;
}

PRBool 
xptiInterfaceInfoManager::LoadFile(const xptiTypelib& aTypelibRecord,
                                 xptiWorkingSet* aWorkingSet)
{
    if(!aWorkingSet)
        aWorkingSet = &mWorkingSet;

    if(!aWorkingSet->IsValid())
        return PR_FALSE;

    xptiFile* fileRecord = &aWorkingSet->GetFileAt(aTypelibRecord.GetFileIndex());
    xptiZipItem* zipItem = nsnull;

    nsCOMPtr<nsILocalFile> file;
    if(NS_FAILED(aWorkingSet->GetCloneOfDirectoryAt(fileRecord->GetDirectory(),
                                    getter_AddRefs(file))) || !file)
        return PR_FALSE;

    if(NS_FAILED(file->AppendNative(nsDependentCString(fileRecord->GetName()))))
        return PR_FALSE;

    XPTHeader* header;

    if(aTypelibRecord.IsZip())
    {
        zipItem = &aWorkingSet->GetZipItemAt(aTypelibRecord.GetZipItemIndex());
        
        // See the big comment below in the 'non-zip' case...
        if(zipItem->GetGuts())
        {
            NS_ERROR("Trying to load an xpt file from a zip twice");    
            
            // Force an autoreg on next run
            (void) xptiManifest::Delete(this);

            return PR_FALSE;
        }
        
        LOG_LOAD(("# loading zip item %s::%s\n", fileRecord->GetName(), zipItem->GetName()));
        
        nsCOMPtr<nsIXPTLoader> loader =
            do_GetService(NS_ZIPLOADER_CONTRACTID);

        if (loader) {
            nsresult rv;
            
            nsCOMPtr<nsIInputStream> stream;
            rv = loader->LoadEntry(file, zipItem->GetName(),
                                   getter_AddRefs(stream));

            if (NS_FAILED(rv))
                return PR_FALSE;
            
            header =
                xptiZipLoader::ReadXPTFileFromInputStream(stream, aWorkingSet);
        } else {
            header = nsnull;
            NS_WARNING("Could not load XPT Zip loader");
        }
    } 
    else
    {
        // The file would only have guts already if we previously failed to 
        // find an interface info in a file where the manifest claimed it was 
        // going to be. 
        //
        // Normally, when the file gets loaded (and the guts set) then all 
        // interfaces would also be resolved. So, if we are here again for
        // the same file then there must have been some interface that was 
        // expected but not present. Now we are explicitly trying to find it
        // and it isn't going to be there this time either.
        //
        // This is an assertion style error in a DEBUG build because it shows 
        // that we failed to detect this in autoreg. For release builds (where
        // autoreg is not run on every startup) it is just bad. But by returning
        // PR_FALSE we mark this interface as RESOLVE_FAILED and get on with 
        // things without crashing or anything.
        //
        // We don't want to do an autoreg here because this is too much of an 
        // edge case (and in that odd case it might autoreg multiple times if
        // many interfaces had been removed). But, by deleting the manifest we 
        // force the system to get it right on the next run.
        
        if(fileRecord->GetGuts())
        {
            NS_ERROR("Trying to load an xpt file twice");    
            
            // Force an autoreg on next run
            (void) xptiManifest::Delete(this);

            return PR_FALSE;
        }

        LOG_LOAD(("^ loading file %s\n", fileRecord->GetName()));
        header = ReadXPTFile(file, aWorkingSet);
    } 

    if(!header)
        return PR_FALSE;


    if(aTypelibRecord.IsZip())
    {
        // This also allocs zipItem.GetGuts() used below.
        if(!zipItem->SetHeader(header, aWorkingSet))
            return PR_FALSE;
    }
    else
    {
        // This also allocs fileRecord.GetGuts() used below.
        if(!fileRecord->SetHeader(header, aWorkingSet))
            return PR_FALSE;
    }

    // For each interface in the header we want to find the xptiInterfaceInfo
    // object and set its resolution info.

    for(PRUint16 i = 0; i < header->num_interfaces; i++)
    {
        static const nsID zeroIID =
            { 0x0, 0x0, 0x0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

        XPTInterfaceDirectoryEntry* iface = header->interface_directory + i;
        
        xptiHashEntry* hashEntry;

        if(!iface->iid.Equals(zeroIID))
        {
            hashEntry = (xptiHashEntry*)
                PL_DHashTableOperate(aWorkingSet->mIIDTable, 
                                     &iface->iid, PL_DHASH_LOOKUP);
        }
        else
        {
            hashEntry = (xptiHashEntry*)
                PL_DHashTableOperate(aWorkingSet->mNameTable, 
                                     iface->name, PL_DHASH_LOOKUP);
        }

        xptiInterfaceEntry* entry = 
            PL_DHASH_ENTRY_IS_FREE(hashEntry) ? nsnull : hashEntry->value;

        if(!entry)
        {
            // This one is just not resolved anywhere!
            continue;    
        }

        if(aTypelibRecord.IsZip())
            zipItem->GetGuts()->SetEntryAt(i, entry);
        else
            fileRecord->GetGuts()->SetEntryAt(i, entry);

        XPTInterfaceDescriptor* descriptor = iface->interface_descriptor;

        if(descriptor && aTypelibRecord.Equals(entry->GetTypelibRecord()))
            entry->PartiallyResolveLocked(descriptor, aWorkingSet);
    }
    return PR_TRUE;
}

static int
IndexOfFileWithName(const char* aName, const xptiWorkingSet* aWorkingSet)
{
    NS_ASSERTION(aName, "loser!");

    for(PRUint32 i = 0; i < aWorkingSet->GetFileCount(); ++i)
    {
        if(0 == PL_strcmp(aName, aWorkingSet->GetFileAt(i).GetName()))
            return i;     
    }
    return -1;        
}        

static int
IndexOfDirectoryOfFile(nsISupportsArray* aSearchPath, nsILocalFile* aFile)
{
    nsCOMPtr<nsIFile> parent;
    aFile->GetParent(getter_AddRefs(parent));
    if(parent)
    {
        PRUint32 count = 0;
        aSearchPath->Count(&count);
        NS_ASSERTION(count, "broken search path! bad count");
        for(PRUint32 i = 0; i < count; i++)
        {
            nsCOMPtr<nsIFile> current;
            aSearchPath->QueryElementAt(i, NS_GET_IID(nsIFile), 
                                        getter_AddRefs(current));
            NS_ASSERTION(current, "broken search path! bad element");
            PRBool same;
            if(NS_SUCCEEDED(parent->Equals(current, &same)) && same)
                return (int) i;
        }
    }
    NS_ERROR("file not in search directory!");
    return -1;
}        

struct SortData
{
    nsISupportsArray* mSearchPath;
    xptiWorkingSet*   mWorkingSet;
};

PR_STATIC_CALLBACK(int)
xptiSortFileList(const void * p1, const void *p2, void * closure)
{
    nsILocalFile* pFile1 = *((nsILocalFile**) p1);
    nsILocalFile* pFile2 = *((nsILocalFile**) p2);
    SortData* data = (SortData*) closure;
    
    nsCAutoString name1;
    nsCAutoString name2;
        
    if(NS_FAILED(pFile1->GetNativeLeafName(name1)))
    {
        NS_ERROR("way bad, with no happy out!");
        return 0;    
    }    
    if(NS_FAILED(pFile2->GetNativeLeafName(name2)))
    {
        NS_ERROR("way bad, with no happy out!");
        return 0;    
    }    

    int index1 = IndexOfFileWithName(name1.get(), data->mWorkingSet); 
    int index2 = IndexOfFileWithName(name2.get(), data->mWorkingSet); 
   
    // Get these now in case we need them later.
    PRBool isXPT1 = xptiFileType::IsXPT(name1.get());
    PRBool isXPT2 = xptiFileType::IsXPT(name2.get());
    int nameOrder = Compare(name1, name2);
    
    // both in workingSet, preserve old order
    if(index1 != -1 && index2 != -1)
        return index1 - index2;

    if(index1 != -1)
        return 1;

    if(index2 != -1)
        return -1;

    // neither is in workingset

    // check how they compare in search path order
    
    int dirIndex1 = IndexOfDirectoryOfFile(data->mSearchPath, pFile1);
    int dirIndex2 = IndexOfDirectoryOfFile(data->mSearchPath, pFile2);

    if(dirIndex1 != dirIndex2)
        return dirIndex1 - dirIndex2;

    // .xpt files come before archives (.zip, .jar, etc)
    if(isXPT1 &&!isXPT2)
        return -1;
        
    if(!isXPT1 && isXPT2)
        return 1;
    
    // neither element is in the workingSet and both are same type and in 
    // the same directory, sort by size

    PRInt64 size1;
    PRInt64 size2;

    if(NS_FAILED(pFile1->GetFileSize(&size1)))
    {
        NS_ERROR("way bad, with no happy out!");
        return 0;    
    }    
    if(NS_FAILED(pFile2->GetFileSize(&size2)))
    {
        NS_ERROR("way bad, with no happy out!");
        return 0;    
    }    

    // by size with largest first, or by name if size is the same
    int sizeDiff = int(PRInt32(nsInt64(size2) - nsInt64(size1)));
    return sizeDiff != 0  ? sizeDiff : nameOrder;
}        

nsILocalFile** 
xptiInterfaceInfoManager::BuildOrderedFileArray(nsISupportsArray* aSearchPath,
                                                nsISupportsArray* aFileList,
                                                xptiWorkingSet* aWorkingSet)
{
    // We want to end up with a file list that starts with the files from
    // aWorkingSet (but only those that are in aFileList) in the order in 
    // which they appeared in aWorkingSet-> Following those files will be those
    // files in aFileList which are not in aWorkingSet-> These additional
    // files will be ordered by file size (larger first) but all .xpt files
    // will preceed all zipfile of those files not already in the working set.
    // To do this we will do a fancy sort on a copy of aFileList.

    nsILocalFile** orderedFileList = nsnull;
    PRUint32 countOfFilesInFileList;
    PRUint32 i;

    NS_ASSERTION(aFileList, "loser!");
    NS_ASSERTION(aWorkingSet, "loser!");
    NS_ASSERTION(aWorkingSet->IsValid(), "loser!");

    if(NS_FAILED(aFileList->Count(&countOfFilesInFileList)) || 
       0 == countOfFilesInFileList)
        return nsnull;

    orderedFileList = (nsILocalFile**) 
        XPT_MALLOC(aWorkingSet->GetStructArena(),
                   sizeof(nsILocalFile*) * countOfFilesInFileList);
    
    if(!orderedFileList)
        return nsnull;

    // fill our list for sorting
    for(i = 0; i < countOfFilesInFileList; ++i)
    {
        nsCOMPtr<nsILocalFile> file;
        aFileList->QueryElementAt(i, NS_GET_IID(nsILocalFile), getter_AddRefs(file));
        NS_ASSERTION(file, "loser!");

        // Intentionally NOT addref'd cuz we know these are pinned in aFileList.
        orderedFileList[i] = file.get();
    }

    // sort the filelist

    SortData sortData = {aSearchPath, aWorkingSet};
    NS_QuickSort(orderedFileList, countOfFilesInFileList, sizeof(nsILocalFile*),
                 xptiSortFileList, &sortData);
     
    return orderedFileList;
}

xptiInterfaceInfoManager::AutoRegMode 
xptiInterfaceInfoManager::DetermineAutoRegStrategy(nsISupportsArray* aSearchPath,
                                                   nsISupportsArray* aFileList,
                                                   xptiWorkingSet* aWorkingSet)
{
    NS_ASSERTION(aFileList, "loser!");
    NS_ASSERTION(aWorkingSet, "loser!");
    NS_ASSERTION(aWorkingSet->IsValid(), "loser!");

    PRUint32 countOfFilesInWorkingSet = aWorkingSet->GetFileCount();
    PRUint32 countOfFilesInFileList;
    PRUint32 i;
    PRUint32 k;

    if(0 == countOfFilesInWorkingSet)
    {
        // Loading manifest might have failed. Better safe...     
        return FULL_VALIDATION_REQUIRED;
    }

    if(NS_FAILED(aFileList->Count(&countOfFilesInFileList)))
    {
        NS_ERROR("unexpected!");
        return FULL_VALIDATION_REQUIRED;
    }       

    if(countOfFilesInFileList == countOfFilesInWorkingSet)
    {
        // try to determine if *no* files are new or changed.
     
        PRBool same = PR_TRUE;
        for(i = 0; i < countOfFilesInFileList && same; ++i)
        {
            nsCOMPtr<nsILocalFile> file;
            aFileList->QueryElementAt(i, NS_GET_IID(nsILocalFile), getter_AddRefs(file));
            NS_ASSERTION(file, "loser!");

            PRInt64 size;
            PRInt64 date;
            nsCAutoString name;
            PRUint32 directory;

            if(NS_FAILED(file->GetFileSize(&size)) ||
               NS_FAILED(file->GetLastModifiedTime(&date)) ||
               NS_FAILED(file->GetNativeLeafName(name)) ||
               !aWorkingSet->FindDirectoryOfFile(file, &directory))
            {
                NS_ERROR("unexpected!");
                return FULL_VALIDATION_REQUIRED;
            }    

            for(k = 0; k < countOfFilesInWorkingSet; ++k)
            {
                xptiFile& target = aWorkingSet->GetFileAt(k);
                
                if(directory == target.GetDirectory() &&
                   name.Equals(target.GetName()))
                {
                    if(nsInt64(size) != target.GetSize() ||
                       nsInt64(date) != target.GetDate())
                        same = PR_FALSE;
                    break;        
                }
            }
            // failed to find our file in the workingset?
            if(k == countOfFilesInWorkingSet)
                same = PR_FALSE;
        }
        if(same)
            return NO_FILES_CHANGED;
    }
    else if(countOfFilesInFileList > countOfFilesInWorkingSet)
    {
        // try to determine if the only changes are additional new files
        // XXX Wimping out and doing this as a separate walk through the lists.

        PRBool same = PR_TRUE;

        for(i = 0; i < countOfFilesInWorkingSet && same; ++i)
        {
            xptiFile& target = aWorkingSet->GetFileAt(i);
            
            for(k = 0; k < countOfFilesInFileList; ++k)
            {
                nsCOMPtr<nsILocalFile> file;
                aFileList->QueryElementAt(k, NS_GET_IID(nsILocalFile), getter_AddRefs(file));
                NS_ASSERTION(file, "loser!");
                
                nsCAutoString name;
                PRInt64 size;
                PRInt64 date;
                if(NS_FAILED(file->GetFileSize(&size)) ||
                   NS_FAILED(file->GetLastModifiedTime(&date)) ||
                   NS_FAILED(file->GetNativeLeafName(name)))
                {
                    NS_ERROR("unexpected!");
                    return FULL_VALIDATION_REQUIRED;
                }    
            
                PRBool sameName = name.Equals(target.GetName());
                if(sameName)
                {
                    if(nsInt64(size) != target.GetSize() ||
                       nsInt64(date) != target.GetDate())
                        same = PR_FALSE;
                    break;        
                }
            }
            // failed to find our file in the file list?
            if(k == countOfFilesInFileList)
                same = PR_FALSE;
        }
        if(same)
            return FILES_ADDED_ONLY;
    }

    return FULL_VALIDATION_REQUIRED; 
}

PRBool 
xptiInterfaceInfoManager::AddOnlyNewFilesFromFileList(nsISupportsArray* aSearchPath,
                                                      nsISupportsArray* aFileList,
                                                      xptiWorkingSet* aWorkingSet)
{
    nsILocalFile** orderedFileArray;
    PRUint32 countOfFilesInFileList;
    PRUint32 i;

    NS_ASSERTION(aFileList, "loser!");
    NS_ASSERTION(aWorkingSet, "loser!");
    NS_ASSERTION(aWorkingSet->IsValid(), "loser!");

    if(NS_FAILED(aFileList->Count(&countOfFilesInFileList)))
        return PR_FALSE;
    NS_ASSERTION(countOfFilesInFileList, "loser!");
    NS_ASSERTION(countOfFilesInFileList > aWorkingSet->GetFileCount(), "loser!");

    orderedFileArray = BuildOrderedFileArray(aSearchPath, aFileList, aWorkingSet);

    if(!orderedFileArray)
        return PR_FALSE;

    // Make enough space in aWorkingset for additions to xptiFile array.

    if(!aWorkingSet->ExtendFileArray(countOfFilesInFileList))   
        return PR_FALSE;

    // For each file that is not already in our working set, add any valid 
    // interfaces that don't conflict with previous interfaces added.
    for(i = 0; i < countOfFilesInFileList; i++)
    {
        nsILocalFile* file = orderedFileArray[i];

        nsCAutoString name;
        PRInt64 size;
        PRInt64 date;
        PRUint32 dir;
        if(NS_FAILED(file->GetFileSize(&size)) ||
           NS_FAILED(file->GetLastModifiedTime(&date)) ||
           NS_FAILED(file->GetNativeLeafName(name)) ||
           !aWorkingSet->FindDirectoryOfFile(file, &dir))
        {
            return PR_FALSE;
        }    
    

        if(xptiWorkingSet::NOT_FOUND != aWorkingSet->FindFile(dir, name.get()))
        {
            // This file was found in the working set, so skip it.       
            continue;
        }

        LOG_AUTOREG(("  finding interfaces in new file: %s\n", name.get()));

        xptiFile fileRecord;
        fileRecord = xptiFile(nsInt64(size), nsInt64(date), dir,
                              name.get(), aWorkingSet);

        if(xptiFileType::IsXPT(fileRecord.GetName()))
        {
            XPTHeader* header = ReadXPTFile(file, aWorkingSet);
            if(!header)
            {
                // XXX do something!
                NS_ERROR("");    
                continue;
            }

    
            xptiTypelib typelibRecord;
            typelibRecord.Init(aWorkingSet->GetFileCount());
    
            PRBool AddedFile = PR_FALSE;

            if(header->major_version >= XPT_MAJOR_INCOMPATIBLE_VERSION)
            {
                NS_ASSERTION(!header->num_interfaces,"bad libxpt");
                LOG_AUTOREG(("      file is version %d.%d  Type file of version %d.0 or higher can not be read.\n", (int)header->major_version, (int)header->minor_version, (int)XPT_MAJOR_INCOMPATIBLE_VERSION));
            }

            for(PRUint16 k = 0; k < header->num_interfaces; k++)
            {
                xptiInterfaceEntry* entry = nsnull;
    
                if(!VerifyAndAddEntryIfNew(aWorkingSet,
                                           header->interface_directory + k,
                                           typelibRecord,
                                           &entry))
                    return PR_FALSE;    
    
                if(!entry)
                    continue;
                
                // If this is the first interface we found for this file then
                // setup the fileRecord for the header and infos.
                if(!AddedFile)
                {
                    if(!fileRecord.SetHeader(header, aWorkingSet))
                    {
                        // XXX that would be bad.
                        return PR_FALSE;    
                    }
                    AddedFile = PR_TRUE;
                }
                fileRecord.GetGuts()->SetEntryAt(k, entry);
            }
            
            // This will correspond to typelibRecord above.
            aWorkingSet->AppendFile(fileRecord);
        }
        else // its another kind of archive
        {
            nsCOMPtr<nsIXPTLoader> loader =
                do_GetService(NS_ZIPLOADER_CONTRACTID);
            
            if (loader) {
                nsresult rv;
                
                nsCOMPtr<nsIXPTLoaderSink> sink =
                    new xptiZipLoaderSink(this, aWorkingSet);
                if (!sink)
                    return PR_FALSE;
                
                rv = loader->EnumerateEntries(file, sink);
                if (NS_FAILED(rv))
                    return PR_FALSE;
                // This will correspond to typelibRecord used in
                // xptiInterfaceInfoManager::FoundEntry.
                aWorkingSet->AppendFile(fileRecord);
            } else {
                NS_WARNING("Could not load XPT Zip loader");
            }
        }
    }

    return PR_TRUE;
}        

PRBool 
xptiInterfaceInfoManager::DoFullValidationMergeFromFileList(nsISupportsArray* aSearchPath,
                                                            nsISupportsArray* aFileList,
                                                            xptiWorkingSet* aWorkingSet)
{
    nsILocalFile** orderedFileArray;
    PRUint32 countOfFilesInFileList;
    PRUint32 i;

    NS_ASSERTION(aFileList, "loser!");

    if(!aWorkingSet->IsValid())
        return PR_FALSE;

    if(NS_FAILED(aFileList->Count(&countOfFilesInFileList)))
        return PR_FALSE;

    if(!countOfFilesInFileList)
    {
        // maybe there are no xpt files to register.  
        // a minimal install would have this case.
        return PR_TRUE;
    }

    orderedFileArray = BuildOrderedFileArray(aSearchPath, aFileList, aWorkingSet);

    if(!orderedFileArray)
        return PR_FALSE;

    // DEBUG_DumpFileArray(orderedFileArray, countOfFilesInFileList);

    // Make space in aWorkingset for a new xptiFile array.

    if(!aWorkingSet->NewFileArray(countOfFilesInFileList))   
        return PR_FALSE;

    aWorkingSet->ClearZipItems();
    aWorkingSet->ClearHashTables();

    // For each file, add any valid interfaces that don't conflict with 
    // previous interfaces added.
    for(i = 0; i < countOfFilesInFileList; i++)
    {
        nsILocalFile* file = orderedFileArray[i];

        nsCAutoString name;
        PRInt64 size;
        PRInt64 date;
        PRUint32 dir;
        if(NS_FAILED(file->GetFileSize(&size)) ||
           NS_FAILED(file->GetLastModifiedTime(&date)) ||
           NS_FAILED(file->GetNativeLeafName(name)) ||
           !aWorkingSet->FindDirectoryOfFile(file, &dir))
        {
            return PR_FALSE;
        }    

        LOG_AUTOREG(("  finding interfaces in file: %s\n", name.get()));
    
        xptiFile fileRecord;
        fileRecord = xptiFile(nsInt64(size), nsInt64(date), dir,
                              name.get(), aWorkingSet);

//        printf("* found %s\n", fileRecord.GetName());


        if(xptiFileType::IsXPT(fileRecord.GetName()))
        {
            XPTHeader* header = ReadXPTFile(file, aWorkingSet);
            if(!header)
            {
                // XXX do something!
                NS_ERROR("Unable to read an XPT file, turn logging on to see which file");    
                LOG_AUTOREG(("      unable to read file\n"));
                continue;
            }
    
            xptiTypelib typelibRecord;
            typelibRecord.Init(aWorkingSet->GetFileCount());
    
            PRBool AddedFile = PR_FALSE;

            if(header->major_version >= XPT_MAJOR_INCOMPATIBLE_VERSION)
            {
                NS_ASSERTION(!header->num_interfaces,"bad libxpt");
                LOG_AUTOREG(("      file is version %d.%d  Type file of version %d.0 or higher can not be read.\n", (int)header->major_version, (int)header->minor_version, (int)XPT_MAJOR_INCOMPATIBLE_VERSION));
            }

            for(PRUint16 k = 0; k < header->num_interfaces; k++)
            {
                xptiInterfaceEntry* entry = nsnull;
    
                if(!VerifyAndAddEntryIfNew(aWorkingSet,
                                           header->interface_directory + k,
                                           typelibRecord,
                                           &entry))
                    return PR_FALSE;    
    
                if(!entry)
                    continue;
                
                // If this is the first interface we found for this file then
                // setup the fileRecord for the header and infos.
                if(!AddedFile)
                {
                    if(!fileRecord.SetHeader(header, aWorkingSet))
                    {
                        // XXX that would be bad.
                        return PR_FALSE;    
                    }
                    AddedFile = PR_TRUE;
                }
                fileRecord.GetGuts()->SetEntryAt(k, entry);
            }
            
            // This will correspond to typelibRecord above.
            aWorkingSet->AppendFile(fileRecord);
        }

        else
        {
            nsCOMPtr<nsIXPTLoader> loader =
                do_GetService(NS_ZIPLOADER_CONTRACTID);
            
            if (loader) {
                nsresult rv;
                
                nsCOMPtr<nsIXPTLoaderSink> sink =
                    new xptiZipLoaderSink(this, aWorkingSet);
                if (!sink)
                    return PR_FALSE;
                
                rv = loader->EnumerateEntries(file, sink);
                if (NS_FAILED(rv))
                    return PR_FALSE;
                // This will correspond to typelibRecord used in
                // xptiInterfaceInfoManager::FoundEntry.
                aWorkingSet->AppendFile(fileRecord);
            } else {
                NS_WARNING("Could not load XPT Zip loader");
            }
        }
    }
    return PR_TRUE;
}        

NS_IMPL_ISUPPORTS1(xptiZipLoaderSink, nsIXPTLoaderSink)

// implement nsIXPTLoader
NS_IMETHODIMP
xptiZipLoaderSink::FoundEntry(const char* entryName,
                              PRInt32 index,
                              nsIInputStream *aStream)
{
    XPTHeader *header =
        xptiZipLoader::ReadXPTFileFromInputStream(aStream, mWorkingSet);
    if (!header)
        return NS_ERROR_OUT_OF_MEMORY;
    
    if (!mManager->FoundZipEntry(entryName, index, header, mWorkingSet))
        return NS_ERROR_FAILURE;
    
    return NS_OK;
}

// implement xptiEntrySink
PRBool 
xptiInterfaceInfoManager::FoundZipEntry(const char* entryName,
                                        int index,
                                        XPTHeader* header,
                                        xptiWorkingSet* aWorkingSet)
{

    NS_ASSERTION(entryName, "loser!");
    NS_ASSERTION(header, "loser!");
    NS_ASSERTION(aWorkingSet, "loser!");

    int countOfInterfacesAddedForItem = 0;
    xptiZipItem zipItemRecord(entryName, aWorkingSet);
    
    LOG_AUTOREG(("    finding interfaces in file: %s\n", entryName));

    if(header->major_version >= XPT_MAJOR_INCOMPATIBLE_VERSION)
    {
        NS_ASSERTION(!header->num_interfaces,"bad libxpt");
        LOG_AUTOREG(("      file is version %d.%d. Type file of version %d.0 or higher can not be read.\n", (int)header->major_version, (int)header->minor_version, (int)XPT_MAJOR_INCOMPATIBLE_VERSION));
    }

    if(!header->num_interfaces)
    {
        // We are not interested in files without interfaces.
        return PR_TRUE;
    }
    
    xptiTypelib typelibRecord;
    typelibRecord.Init(aWorkingSet->GetFileCount(),
                       aWorkingSet->GetZipItemCount());

    for(PRUint16 k = 0; k < header->num_interfaces; k++)
    {
        xptiInterfaceEntry* entry = nsnull;
    
        if(!VerifyAndAddEntryIfNew(aWorkingSet,
                                   header->interface_directory + k,
                                   typelibRecord,
                                   &entry))
            return PR_FALSE;    
    
        if(!entry)
            continue;

        // If this is the first interface we found for this item
        // then setup the zipItemRecord for the header and infos.
        if(!countOfInterfacesAddedForItem)
        {
            // XXX fix this!
            if(!zipItemRecord.SetHeader(header, aWorkingSet))
            {
                // XXX that would be bad.
                return PR_FALSE;    
            }
        }

        // zipItemRecord.GetGuts()->SetEntryAt(k, entry);
        ++countOfInterfacesAddedForItem;
    }   

    if(countOfInterfacesAddedForItem)
    {
        if(!aWorkingSet->GetZipItemFreeSpace())
        {
            if(!aWorkingSet->ExtendZipItemArray(
                aWorkingSet->GetZipItemCount() + 20))
            {        
                // out of space!
                return PR_FALSE;    
            }
        }
        aWorkingSet->AppendZipItem(zipItemRecord);
    } 
    return PR_TRUE;
}

PRBool 
xptiInterfaceInfoManager::VerifyAndAddEntryIfNew(xptiWorkingSet* aWorkingSet,
                                                 XPTInterfaceDirectoryEntry* iface,
                                                 const xptiTypelib& typelibRecord,
                                                 xptiInterfaceEntry** entryAdded)
{
    NS_ASSERTION(iface, "loser!");
    NS_ASSERTION(entryAdded, "loser!");

    *entryAdded = nsnull;

    if(!iface->interface_descriptor)
    {
        // Not resolved, ignore this one.
        // XXX full logging might note this...
        return PR_TRUE;
    }
    
    xptiHashEntry* hashEntry = (xptiHashEntry*)
        PL_DHashTableOperate(aWorkingSet->mIIDTable, &iface->iid, PL_DHASH_LOOKUP);

    xptiInterfaceEntry* entry = 
        PL_DHASH_ENTRY_IS_FREE(hashEntry) ? nsnull : hashEntry->value;
    
    if(entry)
    {
        // XXX validate this info to find possible inconsistencies
        LOG_AUTOREG(("      ignoring repeated interface: %s\n", iface->name));
        return PR_TRUE;
    }
    
    // Build a new xptiInterfaceEntry object and hook it up. 

    entry = xptiInterfaceEntry::NewEntry(iface->name, strlen(iface->name),
                                         iface->iid,
                                         typelibRecord, aWorkingSet);
    if(!entry)
    {
        // XXX bad!
        return PR_FALSE;    
    }

    //XXX  We should SetHeader too as part of the validation, no?
    entry->SetScriptableFlag(XPT_ID_IS_SCRIPTABLE(iface->interface_descriptor->flags));

    // Add our entry to the iid hashtable.

    hashEntry = (xptiHashEntry*)
        PL_DHashTableOperate(aWorkingSet->mNameTable, 
                             entry->GetTheName(), PL_DHASH_ADD);
    if(hashEntry)
        hashEntry->value = entry;
    
    // Add our entry to the name hashtable.

    hashEntry = (xptiHashEntry*)
        PL_DHashTableOperate(aWorkingSet->mIIDTable, 
                             entry->GetTheIID(), PL_DHASH_ADD);
    if(hashEntry)
        hashEntry->value = entry;
    
    *entryAdded = entry;

    LOG_AUTOREG(("      added interface: %s\n", iface->name));

    return PR_TRUE;
}

// local struct used to pass two pointers as one pointer
struct TwoWorkingSets
{
    TwoWorkingSets(xptiWorkingSet* src, xptiWorkingSet* dest)
        : aSrcWorkingSet(src), aDestWorkingSet(dest) {}

    xptiWorkingSet* aSrcWorkingSet;
    xptiWorkingSet* aDestWorkingSet;
};        

PR_STATIC_CALLBACK(PLDHashOperator)
xpti_Merger(PLDHashTable *table, PLDHashEntryHdr *hdr,
            PRUint32 number, void *arg)
{
    xptiInterfaceEntry* srcEntry = ((xptiHashEntry*)hdr)->value;
    xptiWorkingSet* aSrcWorkingSet = ((TwoWorkingSets*)arg)->aSrcWorkingSet;
    xptiWorkingSet* aDestWorkingSet = ((TwoWorkingSets*)arg)->aDestWorkingSet;

    xptiHashEntry* hashEntry = (xptiHashEntry*)
        PL_DHashTableOperate(aDestWorkingSet->mIIDTable, 
                             srcEntry->GetTheIID(), PL_DHASH_LOOKUP);

    xptiInterfaceEntry* destEntry = 
        PL_DHASH_ENTRY_IS_FREE(hashEntry) ? nsnull : hashEntry->value;
    
    if(destEntry)
    {
        // Let's see if this is referring to the same exact typelib
         
        const char* destFilename = 
            aDestWorkingSet->GetTypelibFileName(destEntry->GetTypelibRecord());
        
        const char* srcFilename = 
            aSrcWorkingSet->GetTypelibFileName(srcEntry->GetTypelibRecord());
    
        if(0 == PL_strcmp(destFilename, srcFilename) && 
           (destEntry->GetTypelibRecord().GetZipItemIndex() ==
            srcEntry->GetTypelibRecord().GetZipItemIndex()))
        {
            // This is the same item.
            // But... Let's make sure they didn't change the interface name.
            // There are wacky developers that do stuff like that!
            if(0 == PL_strcmp(destEntry->GetTheName(), srcEntry->GetTheName()))
                return PL_DHASH_NEXT;
        }
    }
    
    // Clone the xptiInterfaceEntry into our destination WorkingSet.

    xptiTypelib typelibRecord;

    uint16 fileIndex = srcEntry->GetTypelibRecord().GetFileIndex();
    uint16 zipItemIndex = srcEntry->GetTypelibRecord().GetZipItemIndex();
    
    fileIndex += aDestWorkingSet->mFileMergeOffsetMap[fileIndex];
    
    // If it is not a zipItem, then the original index is fine.
    if(srcEntry->GetTypelibRecord().IsZip())
        zipItemIndex += aDestWorkingSet->mZipItemMergeOffsetMap[zipItemIndex];

    typelibRecord.Init(fileIndex, zipItemIndex);
                
    destEntry = xptiInterfaceEntry::NewEntry(*srcEntry, typelibRecord, 
                                             aDestWorkingSet);
    if(!destEntry)
    {
        // XXX bad! should log
        return PL_DHASH_NEXT;
    }


    // Add our entry to the iid hashtable.

    hashEntry = (xptiHashEntry*)
        PL_DHashTableOperate(aDestWorkingSet->mNameTable, 
                             destEntry->GetTheName(), PL_DHASH_ADD);
    if(hashEntry)
        hashEntry->value = destEntry;
    
    // Add our entry to the name hashtable.

    hashEntry = (xptiHashEntry*)
        PL_DHashTableOperate(aDestWorkingSet->mIIDTable, 
                             destEntry->GetTheIID(), PL_DHASH_ADD);
    if(hashEntry)
        hashEntry->value = destEntry;

    return PL_DHASH_NEXT;
}       

PRBool 
xptiInterfaceInfoManager::MergeWorkingSets(xptiWorkingSet* aDestWorkingSet,
                                           xptiWorkingSet* aSrcWorkingSet)
{

    PRUint32 i;

    // Combine file lists.

    PRUint32 originalFileCount = aDestWorkingSet->GetFileCount();
    PRUint32 additionalFileCount = aSrcWorkingSet->GetFileCount();

    // Create a new array big enough to hold both lists and copy existing files

    if(additionalFileCount)
    {
        if(!aDestWorkingSet->ExtendFileArray(originalFileCount +
                                             additionalFileCount))
            return PR_FALSE;
    
        // Now we are where we started, but we know we have enough space.
    
        // Prepare offset array for later fixups. 
        // NOTE: Storing with dest, but alloc'ing from src. This is intentional.
        aDestWorkingSet->mFileMergeOffsetMap = (PRUint32*)
            XPT_CALLOC(aSrcWorkingSet->GetStructArena(),
                       additionalFileCount * sizeof(PRUint32)); 
        if(!aDestWorkingSet->mFileMergeOffsetMap)
            return PR_FALSE;
    }

    for(i = 0; i < additionalFileCount; ++i)
    {
        xptiFile& srcFile = aSrcWorkingSet->GetFileAt(i);
        PRUint32 k;
        for(k = 0; k < originalFileCount; ++k)
        {
            // If file (with same name, date, and time) is in both lists
            // then reuse that record.
            xptiFile& destFile = aDestWorkingSet->GetFileAt(k);
            if(srcFile.Equals(destFile))
            {
                aDestWorkingSet->mFileMergeOffsetMap[i] = k - i;
                break;    
            }
        }
        if(k == originalFileCount)
        {
            // No match found, tack it on the end.

            PRUint32 newIndex = aDestWorkingSet->GetFileCount();

            aDestWorkingSet->AppendFile(xptiFile(srcFile, aDestWorkingSet));

            // Fixup the merge offset map.
            aDestWorkingSet->mFileMergeOffsetMap[i] = newIndex - i;
        }
    }

    // Combine ZipItem lists.

    PRUint32 originalZipItemCount = aDestWorkingSet->GetZipItemCount();
    PRUint32 additionalZipItemCount = aSrcWorkingSet->GetZipItemCount();

    // Create a new array big enough to hold both lists and copy existing ZipItems

    if(additionalZipItemCount)
    {
        if(!aDestWorkingSet->ExtendZipItemArray(originalZipItemCount +
                                                additionalZipItemCount))
            return PR_FALSE;
    
        // Now we are where we started, but we know we have enough space.
    
        // Prepare offset array for later fixups. 
        // NOTE: Storing with dest, but alloc'ing from src. This is intentional.
        aDestWorkingSet->mZipItemMergeOffsetMap = (PRUint32*)
            XPT_CALLOC(aSrcWorkingSet->GetStructArena(),
                       additionalZipItemCount * sizeof(PRUint32)); 
        if(!aDestWorkingSet->mZipItemMergeOffsetMap)
            return PR_FALSE;
    }

    for(i = 0; i < additionalZipItemCount; ++i)
    {
        xptiZipItem& srcZipItem = aSrcWorkingSet->GetZipItemAt(i);
        PRUint32 k;
        for(k = 0; k < originalZipItemCount; ++k)
        {
            // If ZipItem (with same name) is in both lists
            // then reuse that record.
            xptiZipItem& destZipItem = aDestWorkingSet->GetZipItemAt(k);
            if(srcZipItem.Equals(destZipItem))
            {
                aDestWorkingSet->mZipItemMergeOffsetMap[i] = k - i;
                break;    
            }
        }
        if(k == originalZipItemCount)
        {
            // No match found, tack it on the end.

            PRUint32 newIndex = aDestWorkingSet->GetZipItemCount();

            aDestWorkingSet->AppendZipItem(
                    xptiZipItem(srcZipItem, aDestWorkingSet));

            // Fixup the merge offset map.
            aDestWorkingSet->mZipItemMergeOffsetMap[i] = newIndex - i;
        }
    }

    // Migrate xptiInterfaceInfos

    TwoWorkingSets sets(aSrcWorkingSet, aDestWorkingSet);

    PL_DHashTableEnumerate(aSrcWorkingSet->mNameTable, xpti_Merger, &sets);

    return PR_TRUE;
}        

PRBool 
xptiInterfaceInfoManager::DEBUG_DumpFileList(nsISupportsArray* aFileList)
{
    PRUint32 count;

    if(NS_FAILED(aFileList->Count(&count)))
        return PR_FALSE;
    
    for(PRUint32 i = 0; i < count; i++)
    {
        nsCOMPtr<nsIFile> file;
        aFileList->QueryElementAt(i, NS_GET_IID(nsILocalFile), getter_AddRefs(file));
        if(!file)
            return PR_FALSE;

        nsCAutoString name;
        if(NS_FAILED(file->GetNativeLeafName(name)))
            return PR_FALSE;

        printf("* found %s\n", name.get());
    }
    return PR_TRUE;
}

PRBool 
xptiInterfaceInfoManager::DEBUG_DumpFileListInWorkingSet(xptiWorkingSet* aWorkingSet)
{
    for(PRUint16 i = 0; i < aWorkingSet->GetFileCount(); ++i)
    {
        xptiFile& record = aWorkingSet->GetFileAt(i);
    
        printf("! has %s\n", record.GetName());
    }        
    return PR_TRUE;
}        

PRBool 
xptiInterfaceInfoManager::DEBUG_DumpFileArray(nsILocalFile** aFileArray, 
                                              PRUint32 count)
{
    // dump the sorted list
    for(PRUint32 i = 0; i < count; ++i)
    {
        nsILocalFile* file = aFileArray[i];
    
        nsCAutoString name;
        if(NS_FAILED(file->GetNativeLeafName(name)))
            return PR_FALSE;

        printf("found file: %s\n", name.get());
    }        
    return PR_TRUE;        
}        

/***************************************************************************/

// static 
void 
xptiInterfaceInfoManager::WriteToLog(const char *fmt, ...)
{
    if(!gInterfaceInfoManager)
        return;

    PRFileDesc* fd = gInterfaceInfoManager->GetOpenLogFile();
    if(fd)
    {
        va_list ap;
        va_start(ap, fmt);
        PR_vfprintf(fd, fmt, ap);
        va_end(ap);
    }
}        

PR_STATIC_CALLBACK(PLDHashOperator)
xpti_ResolvedFileNameLogger(PLDHashTable *table, PLDHashEntryHdr *hdr,
                            PRUint32 number, void *arg)
{
    xptiInterfaceEntry* entry = ((xptiHashEntry*)hdr)->value;
    xptiInterfaceInfoManager* mgr = (xptiInterfaceInfoManager*) arg;

    if(entry->IsFullyResolved())
    {
        xptiWorkingSet*  aWorkingSet = mgr->GetWorkingSet();
        PRFileDesc* fd = mgr->GetOpenLogFile();

        const xptiTypelib& typelib = entry->GetTypelibRecord();
        const char* filename = 
                aWorkingSet->GetFileAt(typelib.GetFileIndex()).GetName();
        
        if(typelib.IsZip())
        {
            const char* zipItemName = 
                aWorkingSet->GetZipItemAt(typelib.GetZipItemIndex()).GetName();
            PR_fprintf(fd, "xpti used interface: %s from %s::%s\n", 
                       entry->GetTheName(), filename, zipItemName);
        }    
        else
        {
            PR_fprintf(fd, "xpti used interface: %s from %s\n", 
                       entry->GetTheName(), filename);
        }
    }
    return PL_DHASH_NEXT;
}

void   
xptiInterfaceInfoManager::LogStats()
{
    PRUint32 i;
    
    // This sets what will be returned by GetOpenLogFile().
    xptiAutoLog autoLog(this, mStatsLogFile, PR_FALSE);

    PRFileDesc* fd = GetOpenLogFile();
    if(!fd)
        return;

    // Show names of xpt (only) files from which at least one interface 
    // was resolved.

    PRUint32 fileCount = mWorkingSet.GetFileCount();
    for(i = 0; i < fileCount; ++i)
    {
        xptiFile& f = mWorkingSet.GetFileAt(i);
        if(f.GetGuts())
            PR_fprintf(fd, "xpti used file: %s\n", f.GetName());
    }

    PR_fprintf(fd, "\n");

    // Show names of xptfiles loaded from zips from which at least 
    // one interface was resolved.

    PRUint32 zipItemCount = mWorkingSet.GetZipItemCount();
    for(i = 0; i < zipItemCount; ++i)
    {
        xptiZipItem& zi = mWorkingSet.GetZipItemAt(i);
        if(zi.GetGuts())                           
            PR_fprintf(fd, "xpti used file from zip: %s\n", zi.GetName());
    }

    PR_fprintf(fd, "\n");

    // Show name of each interface that was fully resolved and the name
    // of the file and (perhaps) zip from which it was loaded.

    PL_DHashTableEnumerate(mWorkingSet.mNameTable, 
                           xpti_ResolvedFileNameLogger, this);

} 

/***************************************************************************/

// this is a private helper
static nsresult 
EntryToInfo(xptiInterfaceEntry* entry, nsIInterfaceInfo **_retval)
{
    xptiInterfaceInfo* info;
    nsresult rv;

    if(!entry)
    {
        *_retval = nsnull;
        return NS_ERROR_FAILURE;    
    }

    rv = entry->GetInterfaceInfo(&info);
    if(NS_FAILED(rv))
        return rv;

    // Transfer the AddRef done by GetInterfaceInfo.
    *_retval = NS_STATIC_CAST(nsIInterfaceInfo*, info);
    return NS_OK;    
}

/* nsIInterfaceInfo getInfoForIID (in nsIIDPtr iid); */
NS_IMETHODIMP xptiInterfaceInfoManager::GetInfoForIID(const nsIID * iid, nsIInterfaceInfo **_retval)
{
    NS_ASSERTION(iid, "bad param");
    NS_ASSERTION(_retval, "bad param");

    xptiHashEntry* hashEntry = (xptiHashEntry*)
        PL_DHashTableOperate(mWorkingSet.mIIDTable, iid, PL_DHASH_LOOKUP);

    xptiInterfaceEntry* entry = 
        PL_DHASH_ENTRY_IS_FREE(hashEntry) ? nsnull : hashEntry->value;

    return EntryToInfo(entry, _retval);
}

/* nsIInterfaceInfo getInfoForName (in string name); */
NS_IMETHODIMP xptiInterfaceInfoManager::GetInfoForName(const char *name, nsIInterfaceInfo **_retval)
{
    NS_ASSERTION(name, "bad param");
    NS_ASSERTION(_retval, "bad param");

    xptiHashEntry* hashEntry = (xptiHashEntry*)
        PL_DHashTableOperate(mWorkingSet.mNameTable, name, PL_DHASH_LOOKUP);

    xptiInterfaceEntry* entry = 
        PL_DHASH_ENTRY_IS_FREE(hashEntry) ? nsnull : hashEntry->value;

    return EntryToInfo(entry, _retval);
}

/* nsIIDPtr getIIDForName (in string name); */
NS_IMETHODIMP xptiInterfaceInfoManager::GetIIDForName(const char *name, nsIID * *_retval)
{
    NS_ASSERTION(name, "bad param");
    NS_ASSERTION(_retval, "bad param");

    xptiHashEntry* hashEntry = (xptiHashEntry*)
        PL_DHashTableOperate(mWorkingSet.mNameTable, name, PL_DHASH_LOOKUP);

    xptiInterfaceEntry* entry = 
        PL_DHASH_ENTRY_IS_FREE(hashEntry) ? nsnull : hashEntry->value;

    if(!entry)
    {
        *_retval = nsnull;
        return NS_ERROR_FAILURE;    
    }

    return entry->GetIID(_retval);
}

/* string getNameForIID (in nsIIDPtr iid); */
NS_IMETHODIMP xptiInterfaceInfoManager::GetNameForIID(const nsIID * iid, char **_retval)
{
    NS_ASSERTION(iid, "bad param");
    NS_ASSERTION(_retval, "bad param");

    xptiHashEntry* hashEntry = (xptiHashEntry*)
        PL_DHashTableOperate(mWorkingSet.mIIDTable, iid, PL_DHASH_LOOKUP);

    xptiInterfaceEntry* entry = 
        PL_DHASH_ENTRY_IS_FREE(hashEntry) ? nsnull : hashEntry->value;

    if(!entry)
    {
        *_retval = nsnull;
        return NS_ERROR_FAILURE;    
    }

    return entry->GetName(_retval);
}

PR_STATIC_CALLBACK(PLDHashOperator)
xpti_ArrayAppender(PLDHashTable *table, PLDHashEntryHdr *hdr,
                   PRUint32 number, void *arg)
{
    xptiInterfaceEntry* entry = ((xptiHashEntry*)hdr)->value;
    nsISupportsArray* array = (nsISupportsArray*) arg;

    nsCOMPtr<nsIInterfaceInfo> ii;
    if(NS_SUCCEEDED(EntryToInfo(entry, getter_AddRefs(ii))))
        array->AppendElement(ii);
    return PL_DHASH_NEXT;
}

/* nsIEnumerator enumerateInterfaces (); */
NS_IMETHODIMP xptiInterfaceInfoManager::EnumerateInterfaces(nsIEnumerator **_retval)
{
    // I didn't want to incur the size overhead of using nsHashtable just to
    // make building an enumerator easier. So, this code makes a snapshot of 
    // the table using an nsISupportsArray and builds an enumerator for that.
    // We can afford this transient cost.

    nsCOMPtr<nsISupportsArray> array;
    NS_NewISupportsArray(getter_AddRefs(array));
    if(!array)
        return NS_ERROR_UNEXPECTED;

    PL_DHashTableEnumerate(mWorkingSet.mNameTable, xpti_ArrayAppender, array);
    
    return array->Enumerate(_retval);
}

struct ArrayAndPrefix
{
    nsISupportsArray* array;
    const char*       prefix;
    PRUint32          length;
};

PR_STATIC_CALLBACK(PLDHashOperator)
xpti_ArrayPrefixAppender(PLDHashTable *table, PLDHashEntryHdr *hdr,
                         PRUint32 number, void *arg)
{
    xptiInterfaceEntry* entry = ((xptiHashEntry*)hdr)->value;
    ArrayAndPrefix* args = (ArrayAndPrefix*) arg;

    const char* name = entry->GetTheName();
    if(name != PL_strnstr(name, args->prefix, args->length))
        return PL_DHASH_NEXT;

    nsCOMPtr<nsIInterfaceInfo> ii;
    if(NS_SUCCEEDED(EntryToInfo(entry, getter_AddRefs(ii))))
        args->array->AppendElement(ii);
    return PL_DHASH_NEXT;
}

/* nsIEnumerator enumerateInterfacesWhoseNamesStartWith (in string prefix); */
NS_IMETHODIMP xptiInterfaceInfoManager::EnumerateInterfacesWhoseNamesStartWith(const char *prefix, nsIEnumerator **_retval)
{
    nsCOMPtr<nsISupportsArray> array;
    NS_NewISupportsArray(getter_AddRefs(array));
    if(!array)
        return NS_ERROR_UNEXPECTED;

    ArrayAndPrefix args = {array, prefix, PL_strlen(prefix)};
    PL_DHashTableEnumerate(mWorkingSet.mNameTable, xpti_ArrayPrefixAppender, &args);
    
    return array->Enumerate(_retval);
}

/* void autoRegisterInterfaces (); */
NS_IMETHODIMP xptiInterfaceInfoManager::AutoRegisterInterfaces()
{
    nsCOMPtr<nsISupportsArray> fileList;
    AutoRegMode mode;
    PRBool ok;

    nsAutoLock lock(xptiInterfaceInfoManager::GetAutoRegLock(this));

    xptiWorkingSet workingSet(mSearchPath);
    if(!workingSet.IsValid())
        return NS_ERROR_UNEXPECTED;

    // This sets what will be returned by GetOpenLogFile().
    xptiAutoLog autoLog(this, mAutoRegLogFile, PR_TRUE);

    LOG_AUTOREG(("start AutoRegister\n"));

    // We re-read the manifest rather than muck with the 'live' one.
    // It is OK if this fails.
    // XXX But we should track failure as a warning.
    ok = xptiManifest::Read(this, &workingSet);

    LOG_AUTOREG(("read of manifest %s\n", ok ? "successful" : "FAILED"));

    // Grovel for all the typelibs we can find (in .xpt or .zip, .jar,...).
    if(!BuildFileList(mSearchPath, getter_AddRefs(fileList)) || !fileList)
        return NS_ERROR_UNEXPECTED;
    
    // DEBUG_DumpFileList(fileList);

    // Check to see how much work we need to do.
    mode = DetermineAutoRegStrategy(mSearchPath, fileList, &workingSet);

    switch(mode)
    {
    case NO_FILES_CHANGED:
        LOG_AUTOREG(("autoreg strategy: no files changed\n"));
        LOG_AUTOREG(("successful end of AutoRegister\n"));
        return NS_OK;
    case FILES_ADDED_ONLY:
        LOG_AUTOREG(("autoreg strategy: files added only\n"));
        if(!AddOnlyNewFilesFromFileList(mSearchPath, fileList, &workingSet))
        {
            LOG_AUTOREG(("FAILED to add new files\n"));
            return NS_ERROR_UNEXPECTED;
        }
        break;
    case FULL_VALIDATION_REQUIRED:
        LOG_AUTOREG(("autoreg strategy: doing full validation merge\n"));
        if(!DoFullValidationMergeFromFileList(mSearchPath, fileList, &workingSet))
        {
            LOG_AUTOREG(("FAILED to do full validation\n"));
            return NS_ERROR_UNEXPECTED;
        }
        break;
    default:
        NS_ERROR("switch missing a case");
        return NS_ERROR_UNEXPECTED;
    }

    // Failure to write the manifest is not fatal in production builds.
    // However, this would require the next startup to find and read all the
    // xpt files. This will make that startup slower. If this ever becomes a 
    // chronic problem for anyone, then we'll want to figure out why!
    
    if(!xptiManifest::Write(this, &workingSet))
    {
        LOG_AUTOREG(("FAILED to write manifest\n"));
        NS_ERROR("Failed to write xpti manifest!");
    }
    
    if(!MergeWorkingSets(&mWorkingSet, &workingSet))
    {
        LOG_AUTOREG(("FAILED to merge into live workingset\n"));
        return NS_ERROR_UNEXPECTED;
    }

//    DEBUG_DumpFileListInWorkingSet(mWorkingSet);

    LOG_AUTOREG(("successful end of AutoRegister\n"));

    return NS_OK;
}

/***************************************************************************/

class xptiAdditionalManagersEnumerator : public nsISimpleEnumerator 
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR

    xptiAdditionalManagersEnumerator();

    PRBool SizeTo(PRUint32 likelyCount) {return mArray.SizeTo(likelyCount);}
    PRBool AppendElement(nsIInterfaceInfoManager* element);

private:
    ~xptiAdditionalManagersEnumerator() {}

    nsSupportsArray mArray;
    PRUint32        mIndex;
    PRUint32        mCount;
};

NS_IMPL_ISUPPORTS1(xptiAdditionalManagersEnumerator, nsISimpleEnumerator)

xptiAdditionalManagersEnumerator::xptiAdditionalManagersEnumerator()
    : mIndex(0), mCount(0)
{
}

PRBool xptiAdditionalManagersEnumerator::AppendElement(nsIInterfaceInfoManager* element)
{
    if(!mArray.AppendElement(NS_STATIC_CAST(nsISupports*, element)))
        return PR_FALSE;
    mCount++;
    return PR_TRUE;
}

/* boolean hasMoreElements (); */
NS_IMETHODIMP xptiAdditionalManagersEnumerator::HasMoreElements(PRBool *_retval)
{
    *_retval = mIndex < mCount;
    return NS_OK;
}

/* nsISupports getNext (); */
NS_IMETHODIMP xptiAdditionalManagersEnumerator::GetNext(nsISupports **_retval)
{
    if(!(mIndex < mCount))
    {
        NS_ERROR("Bad nsISimpleEnumerator caller!");
        return NS_ERROR_FAILURE;    
    }

    *_retval = mArray.ElementAt(mIndex++);
    return *_retval ? NS_OK : NS_ERROR_FAILURE;
}

/***************************************************************************/

/* void addAdditionalManager (in nsIInterfaceInfoManager manager); */
NS_IMETHODIMP xptiInterfaceInfoManager::AddAdditionalManager(nsIInterfaceInfoManager *manager)
{
    nsCOMPtr<nsIWeakReference> weakRef = do_GetWeakReference(manager);
    nsISupports* ptrToAdd = weakRef ? 
                    NS_STATIC_CAST(nsISupports*, weakRef) :
                    NS_STATIC_CAST(nsISupports*, manager);
    { // scoped lock...
        nsAutoLock lock(mAdditionalManagersLock);
        PRInt32 index;
        nsresult rv = mAdditionalManagers.GetIndexOf(ptrToAdd, &index);
        if(NS_FAILED(rv) || -1 != index)
            return NS_ERROR_FAILURE;
        if(!mAdditionalManagers.AppendElement(ptrToAdd))
            return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

/* void removeAdditionalManager (in nsIInterfaceInfoManager manager); */
NS_IMETHODIMP xptiInterfaceInfoManager::RemoveAdditionalManager(nsIInterfaceInfoManager *manager)
{
    nsCOMPtr<nsIWeakReference> weakRef = do_GetWeakReference(manager);
    nsISupports* ptrToRemove = weakRef ? 
                    NS_STATIC_CAST(nsISupports*, weakRef) :
                    NS_STATIC_CAST(nsISupports*, manager);
    { // scoped lock...
        nsAutoLock lock(mAdditionalManagersLock);
        if(!mAdditionalManagers.RemoveElement(ptrToRemove))
            return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

/* PRBool hasAdditionalManagers (); */
NS_IMETHODIMP xptiInterfaceInfoManager::HasAdditionalManagers(PRBool *_retval)
{
    PRUint32 count;
    nsresult rv = mAdditionalManagers.Count(&count);
    *_retval = count != 0;
    return rv;
}

/* nsISimpleEnumerator enumerateAdditionalManagers (); */
NS_IMETHODIMP xptiInterfaceInfoManager::EnumerateAdditionalManagers(nsISimpleEnumerator **_retval)
{
    nsAutoLock lock(mAdditionalManagersLock);

    PRUint32 count;
    nsresult rv = mAdditionalManagers.Count(&count);
    if(NS_FAILED(rv))
        return rv;

    nsCOMPtr<xptiAdditionalManagersEnumerator> enumerator = 
        new xptiAdditionalManagersEnumerator();
    if(!enumerator)
        return NS_ERROR_OUT_OF_MEMORY;

    enumerator->SizeTo(count);

    for(PRUint32 i = 0; i < count; /* i incremented in the loop body */)
    {
        nsCOMPtr<nsISupports> raw = 
            dont_AddRef(mAdditionalManagers.ElementAt(i++));
        if(!raw)
            return NS_ERROR_FAILURE;
        nsCOMPtr<nsIWeakReference> weakRef = do_QueryInterface(raw);
        if(weakRef)
        {
            nsCOMPtr<nsIInterfaceInfoManager> manager = 
                do_QueryReferent(weakRef);
            if(manager)
            {
                if(!enumerator->AppendElement(manager))
                    return NS_ERROR_FAILURE;
            }
            else
            {
                // The manager is no more. Remove the element.
                if(!mAdditionalManagers.RemoveElementAt(--i))
                    return NS_ERROR_FAILURE;
                count--;
            }
        }
        else
        {
            // We *know* we put a pointer to either a nsIWeakReference or
            // an nsIInterfaceInfoManager into the array, so we can avoid an
            // extra QI here and just do a cast.
            if(!enumerator->AppendElement(
                    NS_REINTERPRET_CAST(nsIInterfaceInfoManager*, raw.get())))
                return NS_ERROR_FAILURE;
        }
    }
    
    NS_ADDREF(*_retval = enumerator);
    return NS_OK;
}

/***************************************************************************/

XPTI_PUBLIC_API(nsIInterfaceInfoManager*)
XPTI_GetInterfaceInfoManager()
{
    nsIInterfaceInfoManager* iim =
        xptiInterfaceInfoManager::GetInterfaceInfoManagerNoAddRef();
    NS_IF_ADDREF(iim);
    return iim;
}

XPTI_PUBLIC_API(void)
XPTI_FreeInterfaceInfoManager()
{
    xptiInterfaceInfoManager::FreeInterfaceInfoManager();
}

