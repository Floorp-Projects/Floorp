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

/* Implementation of xptiInterfaceInfoManager. */

#include "xptiprivate.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(xptiInterfaceInfoManager, nsIInterfaceInfoManager)

static xptiInterfaceInfoManager* gInterfaceInfoManager = nsnull;

// static
xptiInterfaceInfoManager*
xptiInterfaceInfoManager::GetInterfaceInfoManager()
{
    if(!gInterfaceInfoManager)
    {
        gInterfaceInfoManager = new xptiInterfaceInfoManager();
        if(gInterfaceInfoManager)
            NS_ADDREF(gInterfaceInfoManager);
        if(!gInterfaceInfoManager->mWorkingSet.IsValid())
            NS_RELEASE(gInterfaceInfoManager);
    }
    if(gInterfaceInfoManager)
        NS_ADDREF(gInterfaceInfoManager);
    return gInterfaceInfoManager;
}

void
xptiInterfaceInfoManager::FreeInterfaceInfoManager()
{
    NS_IF_RELEASE(gInterfaceInfoManager);
}

xptiInterfaceInfoManager::xptiInterfaceInfoManager()
    :   mWorkingSet()
{
    NS_INIT_ISUPPORTS();
    if(!xptiManifest::Read(this, mWorkingSet))
        AutoRegisterInterfaces();
}

xptiInterfaceInfoManager::~xptiInterfaceInfoManager()
{
    // We only do this on shutdown of the service.
    mWorkingSet.InvalidateInterfaceInfos();
}

PRBool 
xptiInterfaceInfoManager::GetComponentsDir(nsILocalFile** aDir)
{
    NS_ASSERTION(aDir,"loser!");
    
    // Make a new one each time because caller *will* modify it.

    nsCOMPtr<nsILocalFile> dir = do_CreateInstance(NS_LOCAL_FILE_PROGID);
    if(!dir)
        return PR_FALSE;

    nsresult rv = dir->InitWithPath(
        nsSpecialSystemDirectory(
            nsSpecialSystemDirectory::XPCOM_CurrentProcessComponentDirectory));
    if(NS_FAILED(rv))
        return PR_FALSE;
    
    NS_ADDREF(*aDir = dir);
    return PR_TRUE;
}

PRBool 
xptiInterfaceInfoManager::BuildFileList(nsISupportsArray** aFileList)
{
    NS_ASSERTION(aFileList, "loser!");
    
    nsresult rv;

    nsCOMPtr<nsILocalFile> dir;
    
    rv = GetComponentsDir(getter_AddRefs(dir));
    if(NS_FAILED(rv) || !dir)
        return PR_FALSE;

    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = dir->GetDirectoryEntries(getter_AddRefs(entries));
    if(NS_FAILED(rv) || !entries)
        return PR_FALSE;

    nsCOMPtr<nsISupportsArray> fileList = 
        do_CreateInstance(NS_SUPPORTSARRAY_PROGID);
    if(!fileList)
        return PR_FALSE;

    PRUint32 count = 0;
    PRBool hasMore;
    while(NS_SUCCEEDED(entries->HasMoreElements(&hasMore)) && hasMore)
    {
        nsCOMPtr<nsISupports> sup;
        entries->GetNext(getter_AddRefs(sup));
        if(!sup)
            return PR_FALSE;
        nsCOMPtr<nsIFile> file = do_QueryInterface(sup);
        if(!file)
            return PR_FALSE;

        char* name;
        if(NS_FAILED(file->GetLeafName(&name)))
            return PR_FALSE;

        int flen = PL_strlen(name);
        if (flen < 4 || 
            (PL_strcasecmp(&(name[flen - 4]), ".xpt")) &&
            (PL_strcasecmp(&(name[flen - 4]), ".zip")) &&
            (PL_strcasecmp(&(name[flen - 4]), ".jar"))) {
            nsAllocator::Free(name);
            continue;
        }
        nsAllocator::Free(name);

        PRBool isFile;
        if(NS_FAILED(file->IsFile(&isFile)) || !isFile)
        {
            nsAllocator::Free(name);
            continue;
        }
     
        if(!fileList->InsertElementAt(file, count))
            return PR_FALSE;
        ++count;
    }

    NS_ADDREF(*aFileList = fileList); 
    return PR_TRUE;
}


xptiInterfaceInfoManager::AutoRegMode 
xptiInterfaceInfoManager::DetermineAutoRegStrategy(nsISupportsArray* aFileList,
                                                 xptiWorkingSet& aWorkingSet)
{
    // XXX implement!
    return FULL_VALIDATION_REQUIRED; 
}

#if 0
PRBool 
xptiInterfaceInfoManager::PopulateFileRecordsInWorkingSet(nsISupportsArray* aFileList,
                                                        xptiWorkingSet& aWorkingSet)
{
    // XXX implement
    return PR_FALSE;
}        
#endif

PRBool 
xptiInterfaceInfoManager::AttemptAddOnlyFromFileList(nsISupportsArray* aFileList,
                                                   xptiWorkingSet& aWorkingSet,
                                                   AutoRegMode* pmode)
{
    // XXX implement

    return PR_TRUE;
}        

XPTHeader* 
xptiInterfaceInfoManager::ReadXPTFile(nsILocalFile* aFile, 
                                    xptiWorkingSet& aWorkingSet)
{
    NS_ASSERTION(aFile, "loser!");

    XPTHeader *header = nsnull;
    char *whole = nsnull;
    FILE*   file = nsnull;
    XPTState *state = nsnull;
    XPTCursor cursor;
    PRUint32 flen;
    PRInt64 fileSize;

    if(NS_FAILED(aFile->GetFileSize(&fileSize)) || !(flen = nsInt64(fileSize)))
    {
        return nsnull;
    }

    whole = new char[flen];
    if (!whole)
    {
        return nsnull;
    }

    // all exits from on here should be via 'goto out' 

    if(NS_FAILED(aFile->OpenANSIFileDesc("rb", &file)) || !file)
    {
        goto out;
    }

    if(flen > fread(whole, 1, flen, file))
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
    
    if (!XPT_DoHeader(aWorkingSet.GetStructArena(), &cursor, &header))
    {
        goto out;
    }

 out:
    if(file)
        fclose(file);
    if(state)
        XPT_DestroyXDRState(state);
    if(whole)
        delete [] whole;
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
    if(!GetComponentsDir(getter_AddRefs(file)) || !file)
        return PR_FALSE;

    if(NS_FAILED(file->Append(fileRecord->GetName())))
        return PR_FALSE;

    XPTHeader* header;

    if(aTypelibRecord.IsZip())
    {
        zipItem = &aWorkingSet->GetZipItemAt(aTypelibRecord.GetZipItemIndex());
        printf("# loading zip item %s::%s\n", fileRecord->GetName(), zipItem->GetName());
        header = ReadXPTFileFromZip(file, zipItem->GetName(), *aWorkingSet);
    } 
    else
    {
        printf("^ loading file %s\n", fileRecord->GetName());
        header = ReadXPTFile(file, *aWorkingSet);
    } 

    if(!header)
        return PR_FALSE;


    if(aTypelibRecord.IsZip())
    {
        // This also allocs zipItem.GetGuts() used below.
        if(!zipItem->SetHeader(header))
            return PR_FALSE;
    }
    else
    {
        // This also allocs fileRecord.GetGuts() used below.
        if(!fileRecord->SetHeader(header))
            return PR_FALSE;
    }

    // For each interface in the header we want to find the xptiInterfaceInfo
    // object and set its resolution info.

    for(PRUint16 i = 0; i < header->num_interfaces; i++)
    {
        static const nsID zeroIID =
            { 0x0, 0x0, 0x0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

        XPTInterfaceDirectoryEntry* iface = header->interface_directory + i;
        xptiInterfaceInfo* info;
        
        if(!iface->iid.Equals(zeroIID))
        {
            info = (xptiInterfaceInfo*)
                PL_HashTableLookup(aWorkingSet->mIIDTable, &iface->iid);
        }
        else
        {
            info = (xptiInterfaceInfo*)
                PL_HashTableLookup(aWorkingSet->mNameTable, iface->name);
        }

        if(!info)
        {
            // This one is just not resolved anywhere!
            continue;    
        }

        if(aTypelibRecord.IsZip())
            zipItem->GetGuts()->SetInfoAt(i, info);
        else
            fileRecord->GetGuts()->SetInfoAt(i, info);

        XPTInterfaceDescriptor* descriptor = iface->interface_descriptor;

        if(descriptor && aTypelibRecord.Equals(info->GetTypelibRecord()))
            info->PartiallyResolve(descriptor, aWorkingSet);
    }
    return PR_TRUE;
}

XPTHeader* 
xptiInterfaceInfoManager::ReadXPTFileFromZip(nsILocalFile* file,
                                           const char* itemName,
                                           xptiWorkingSet& aWorkingSet)
{
    nsCOMPtr<nsIZipReader> zip = 
                do_CreateInstance("component://netscape/libjar");
    if(!zip)
    {
        return nsnull;
    }

    if(NS_FAILED(zip->Init(file)) || NS_FAILED(zip->Open()))
    {
        return nsnull;
    }
    
    // This is required before getting streams for some reason.
    if(NS_FAILED(zip->ParseManifest()))
    {
        return nsnull;
    }

    nsCOMPtr<nsIZipEntry> entry;
    if(NS_FAILED(zip->GetEntry(itemName, getter_AddRefs(entry))) || !entry)
    {
        return nsnull;
    }

    return ReadXPTFileFromOpenZip(zip, entry, itemName, aWorkingSet);
}

XPTHeader* 
xptiInterfaceInfoManager::ReadXPTFileFromOpenZip(nsIZipReader* zip,
                                               nsIZipEntry* entry,
                                               const char* itemName,
                                               xptiWorkingSet& aWorkingSet)
{
    NS_ASSERTION(zip, "loser!");
    NS_ASSERTION(entry, "loser!");
    NS_ASSERTION(itemName, "loser!");


    XPTHeader *header = nsnull;
    char *whole = nsnull;
    XPTState *state = nsnull;
    XPTCursor cursor;
    PRUint32 flen;
    PRUint32 totalRead = 0;

    if(NS_FAILED(entry->GetRealSize(&flen)) || !flen)
    {
        return nsnull;
    }

    nsCOMPtr<nsIInputStream> stream;
    if(NS_FAILED(zip->GetInputStream(itemName, getter_AddRefs(stream))) || !stream)
    {
        return nsnull;
    }

    whole = new char[flen];
    if (!whole)
    {
        return nsnull;
    }

    // all exits from on here should be via 'goto out' 

    while(flen - totalRead)
    {
        PRUint32 avail;
        PRUint32 read;

        if(NS_FAILED(stream->Available(&avail)))
        {
            goto out;
        }

        if(avail > flen)
        {
            goto out;
        }

        if(NS_FAILED(stream->Read(whole+totalRead, avail, &read)))
        {
            goto out;
        }

        totalRead += read;
    }
    
    // Go ahead and close the stream now.
    stream = nsnull;

    if(!(state = XPT_NewXDRState(XPT_DECODE, whole, flen)))
    {
        goto out;
    }
    
    if(!XPT_MakeCursor(state, XPT_HEADER, 0, &cursor))
    {
        goto out;
    }
    
    if (!XPT_DoHeader(aWorkingSet.GetStructArena(), &cursor, &header))
    {
        goto out;
    }

 out:
    if(state)
        XPT_DestroyXDRState(state);
    if(whole)
        delete [] whole;
    return header;
}

static PRBool
IsXPTFile(const char* name)
{
    NS_ASSERTION(name, "bad name");
    NS_ASSERTION(PL_strlen(name) >= 4, "bad name");
    return 0 == PL_strcasecmp(&(name[PL_strlen(name) - 4]), ".xpt");
}

static int
IndexOfFileWithName(const char* aName, const xptiWorkingSet& aWorkingSet)
{
    NS_ASSERTION(aName, "loser!");

    for(PRUint32 i = 0; i < aWorkingSet.GetFileCount(); ++i)
    {
        if(!PL_strcmp(aName, aWorkingSet.GetFileAt(i).GetName()))
            return i;     
    }
    return -1;        
}        

static PR_CALLBACK int
xptiSortFileList(const void * p1, const void *p2, void * closure)
{
    nsILocalFile* pFile1 = *((nsILocalFile**) p1);
    nsILocalFile* pFile2 = *((nsILocalFile**) p2);
    xptiWorkingSet* pWorkingSet = (xptiWorkingSet*) closure;
        
    char* name1;
    char* name2;
    
    if(NS_FAILED(pFile1->GetLeafName(&name1)))
    {
        NS_ASSERTION(0, "way bad, with no happy out!");
        return 0;    
    }    
    if(NS_FAILED(pFile2->GetLeafName(&name2)))
    {
        NS_ASSERTION(0, "way bad, with no happy out!");
        return 0;    
    }    

    int index1 = IndexOfFileWithName(name1, *pWorkingSet); 
    int index2 = IndexOfFileWithName(name2, *pWorkingSet); 
   
    // Get thses now in case we need them later.
    PRBool isXPT1 = IsXPTFile(name1);
    PRBool isXPT2 = IsXPTFile(name2);
    int nameOrder = PL_strcmp(name1, name2);
    
    nsAllocator::Free(name1);
    nsAllocator::Free(name2);

    // XXX need to test with non-empty working sets to be sure this right

    // both in workingSet, preserve old order
    if(index1 != -1 && index2 != -1)
        return index1 - index2;

    if(index1 != -1)
        return 1;

    if(index2 != -1)
        return -1;

    // .xpt files come before archives (.zip, .jar, etc)
    if(isXPT1 &&!isXPT2)
        return -1;
        
    if(!isXPT1 && isXPT2)
        return 1;
    
    // neither element is in the workingSet and both are same type, sort by size

    PRInt64 size1;
    PRInt64 size2;

    if(NS_FAILED(pFile1->GetFileSize(&size1)))
    {
        NS_ASSERTION(0, "way bad, with no happy out!");
        return 0;    
    }    
    if(NS_FAILED(pFile2->GetFileSize(&size2)))
    {
        NS_ASSERTION(0, "way bad, with no happy out!");
        return 0;    
    }    

    // by size with largest first, or by name if size is the same
    int sizeDiff = int(PRInt32(nsInt64(size2) - nsInt64(size1)));
    return sizeDiff != 0  ? sizeDiff : nameOrder;
}        

PRBool 
xptiInterfaceInfoManager::DoFullValidationMergeFromFileList(nsISupportsArray* aFileList,
                                                          xptiWorkingSet& aWorkingSet)
{
    // nsresult rv;
    PRUint32 countOfFilesInFileList;
    PRUint32 i;

    NS_ASSERTION(aFileList, "loser!");

    if(!aWorkingSet.IsValid())
        return PR_FALSE;

    if(NS_FAILED(aFileList->Count(&countOfFilesInFileList)))
        return PR_FALSE;

    if(!countOfFilesInFileList)
    {
        // XXX do the right thing here!    
        return PR_FALSE;
    }

    // We want to end up with a file list that starts with the files from
    // aWorkingSet (but only those that are in aFileList) in the order in 
    // which they appeared in aWorkingSet. Following those files will be those
    // files in aFileList which are not in aWorkingSet. These additional
    // files will be ordered by file size (larger first) but all .xpt files
    // will preceed all zipfile of those files not already in the working set.
    // To do this we will do a fancy sort on a copy of aFileList.


    nsILocalFile** orderedFileList = (nsILocalFile**) 
        XPT_MALLOC(aWorkingSet.GetStructArena(),
                   sizeof(nsILocalFile*) * countOfFilesInFileList);
    
    // fill our list for sorting
    for(i = 0; i < countOfFilesInFileList; ++i)
    {
        nsCOMPtr<nsISupports> sup;
        aFileList->GetElementAt(i, getter_AddRefs(sup));
        if(!sup)
            return PR_FALSE;
        nsCOMPtr<nsILocalFile> file = do_QueryInterface(sup);
        if(!file)
            return PR_FALSE;

        // Intentionally NOT addref'd cuz we know these are pinned in aFileList.
        orderedFileList[i] = file.get();
    }

    // sort the filelist
    NS_QuickSort(orderedFileList, countOfFilesInFileList, sizeof(nsILocalFile*),
                 xptiSortFileList, &aWorkingSet);
    
     
    // dump the sorted list
    for(i = 0; i < countOfFilesInFileList; ++i)
    {
        nsILocalFile* file = orderedFileList[i];
    
        char* name;
        if(NS_FAILED(file->GetLeafName(&name)))
            return PR_FALSE;

        printf("* found %s\n", name);
        nsAllocator::Free(name);
    }        


    // Make space in aWorkingset for a new xptiFile array.

    if(!aWorkingSet.NewFileArray(countOfFilesInFileList))   
        return PR_FALSE;

    aWorkingSet.ClearHashTables();

    // For each file, add any valid interfaces that don't conflict with 
    // previous interfaces added.
    for(i = 0; i < countOfFilesInFileList; i++)
    {
        nsILocalFile* file = orderedFileList[i];

        char*   name;
        PRInt64 size;
        PRInt64 date;
        if(NS_FAILED(file->GetFileSize(&size)) ||
           NS_FAILED(file->GetLastModificationDate(&date)) ||
           NS_FAILED(file->GetLeafName(&name)))
        {
            return PR_FALSE;
        }    
    
        xptiFile fileRecord(nsInt64(size), nsInt64(date), name, &aWorkingSet);
        nsAllocator::Free(name);

//        printf("* found %s\n", fileRecord.GetName());


        if(IsXPTFile(fileRecord.GetName()))
        {
            XPTHeader* header = ReadXPTFile(file, aWorkingSet);
            if(!header)
            {
                // XXX do something!
                NS_ASSERTION(0,"");    
                continue;
            }
    
            if(!header->num_interfaces)
            {
                // We are not interested in files without interfaces.
                continue;
            }
    
            xptiTypelib typelibRecord;
            typelibRecord.Init(aWorkingSet.GetFileCount());
    
            int countOfInterfacesAddedForFile = 0;
            for(PRUint16 k = 0; k < header->num_interfaces; k++)
            {
                xptiInterfaceInfo* info = nsnull;
    
                if(!VerifyAndAddInterfaceIfNew(aWorkingSet,
                                               header->interface_directory + k,
                                               typelibRecord,
                                               &info))
                    return PR_FALSE;    
    
                if(!info)
                    continue;
                
                // If this is the first interface we found for this file then
                // setup the fileRecord for the header and infos.
                if(!countOfInterfacesAddedForFile)
                {
                    if(!fileRecord.SetHeader(header))
                    {
                        // XXX that would be bad.
                        return PR_FALSE;    
                    }
                }
                fileRecord.GetGuts()->SetInfoAt(k, info);
                ++countOfInterfacesAddedForFile;
            }
            
            if(countOfInterfacesAddedForFile)
            {
                // This will correspond to typelibRecord above.
                aWorkingSet.AppendFile(fileRecord);
            }
        }
        else // It is a zip file, Oh boy!
        {
            nsCOMPtr<nsIZipReader> zip = 
                do_CreateInstance("component://netscape/libjar");
            if(!zip)
                return PR_FALSE;
            if(NS_FAILED(zip->Init(file)) || NS_FAILED(zip->Open()))
                return PR_FALSE;
            
            // This is required before getting streams for some reason.
            if(NS_FAILED(zip->ParseManifest()))
                return PR_FALSE;

            nsCOMPtr<nsISimpleEnumerator> entries;
            if(NS_FAILED(zip->FindEntries("*.xpt", getter_AddRefs(entries))) ||
               !entries)
            {
                // XXX We TRUST that this means there are no .xpt files.    
                continue;
            }

            int countOfInterfacesAddedForFile = 0;

            do
            {
                // For each item...
                int countOfInterfacesAddedForItem = 0;
                
                PRBool hasMore;
                if(NS_FAILED(entries->HasMoreElements(&hasMore)))
                    return PR_FALSE;
                if(!hasMore)
                    break;

                nsCOMPtr<nsISupports> sup;
                if(NS_FAILED(entries->GetNext(getter_AddRefs(sup))) ||!sup)
                    return PR_FALSE;
                
                nsCOMPtr<nsIZipEntry> entry = do_QueryInterface(sup);
                if(!entry)
                    return PR_FALSE;

                // we have a zip entry!

                char* itemName = nsnull;
                
                if(NS_FAILED(entry->GetName(&itemName)) || !itemName)
                    return PR_FALSE;


                xptiZipItem zipItemRecord(itemName, &aWorkingSet);
                XPTHeader* header = 
                    ReadXPTFileFromOpenZip(zip, entry, itemName, aWorkingSet);
                nsAllocator::Free(itemName);

                if(!header)
                {
                    // XXX do something!
                    NS_ASSERTION(0,"");    
                    continue;
                }
                
                if(!header->num_interfaces)
                {
                    // We are not interested in files without interfaces.
                    continue;
                }
                
                xptiTypelib typelibRecord;
                typelibRecord.Init(aWorkingSet.GetFileCount(),
                                   aWorkingSet.GetZipItemCount());

                for(PRUint16 k = 0; k < header->num_interfaces; k++)
                {
                    xptiInterfaceInfo* info = nsnull;
    
                    if(!VerifyAndAddInterfaceIfNew(aWorkingSet,
                                                   header->interface_directory + k,
                                                   typelibRecord,
                                                   &info))
                        return PR_FALSE;    
    
                    if(!info)
                        continue;

                    // If this is the first interface we found for this item
                    // then setup the zipItemRecord for the header and infos.
                    if(!countOfInterfacesAddedForItem)
                    {
                        // XXX fix this!
                        if(!zipItemRecord.SetHeader(header))
                        {
                            // XXX that would be bad.
                            return PR_FALSE;    
                        }
                    }

                    // zipItemRecord.GetGuts()->SetInfoAt(k, info);
                    ++countOfInterfacesAddedForItem;
                    ++countOfInterfacesAddedForFile;
                }   

                if(countOfInterfacesAddedForItem)
                {
                    if(!aWorkingSet.GetZipItemFreeSpace())
                    {
                        if(!aWorkingSet.ExtendZipItemArray(
                            aWorkingSet.GetZipItemCount() + 20))
                        {        
                            // out of space!
                            return PR_FALSE;
                        }
                    }
                    aWorkingSet.AppendZipItem(zipItemRecord);
                } 
            } while(1);

            if(countOfInterfacesAddedForFile)
            {
                // This will correspond to typelibRecord above.
                aWorkingSet.AppendFile(fileRecord);
            }
        }
    }
    return PR_TRUE;
}        
PRBool 
xptiInterfaceInfoManager::VerifyAndAddInterfaceIfNew(xptiWorkingSet& aWorkingSet,
                                                   XPTInterfaceDirectoryEntry* iface,
                                                   const xptiTypelib& typelibRecord,
                                                   xptiInterfaceInfo** infoAdded)
{
    NS_ASSERTION(iface, "loser!");
    NS_ASSERTION(infoAdded, "loser!");

    *infoAdded = nsnull;

    if(!iface->interface_descriptor)
    {
        // Not resolved, ignore this one.
        // XXX full logging might note this...
        return PR_TRUE;
    }
    
    xptiInterfaceInfo* info = (xptiInterfaceInfo*)
        PL_HashTableLookup(aWorkingSet.mIIDTable, &iface->iid);
    
    if(info)
    {
        // XXX validate this info to find possible inconsistencies
        return PR_TRUE;
    }
    
    // Build a new xptiInterfaceInfo object and hook it up. 

    info = new xptiInterfaceInfo(iface->name, iface->iid,
                               typelibRecord, aWorkingSet);
    if(!info)
    {
        // XXX bad!
        return PR_FALSE;    
    }

    NS_ADDREF(info);
    if(!info->IsValid())
    {
        // XXX bad!
        NS_RELEASE(info);
        return PR_FALSE;    
    }

    // The name table now owns the reference we AddRef'd above
    PL_HashTableAdd(aWorkingSet.mNameTable, iface->name, info);
    PL_HashTableAdd(aWorkingSet.mIIDTable, &iface->iid, info);
    
    *infoAdded = info;
    return PR_TRUE;
}

static PR_CALLBACK PRIntn 
xpti_Merger(PLHashEntry *he, PRIntn i, void *arg)
{
    xptiInterfaceInfo* srcInfo = (xptiInterfaceInfo*) he->value;
    xptiWorkingSet* aWorkingSet = (xptiWorkingSet*) arg;

    xptiInterfaceInfo* destInfo = (xptiInterfaceInfo*)
            PL_HashTableLookup(aWorkingSet->mIIDTable, srcInfo->GetTheIID());
    
    if(destInfo)
    {
        // XXX we could do some serious validation here!
    }
    else
    {
        // Clone the xptiInterfaceInfo into our WorkingSet.

        uint16 srcIndex = srcInfo->GetTypelibRecord().GetFileIndex();

        xptiTypelib typelibRecord;
        typelibRecord.Init(srcIndex + aWorkingSet->mMergeOffsetMap[srcIndex]);
                    
        destInfo = new xptiInterfaceInfo(srcInfo->GetTheName(), 
                                       *srcInfo->GetTheIID(),
                                       typelibRecord, *aWorkingSet);

        if(!destInfo)
        {
            // XXX bad! should log
            return HT_ENUMERATE_NEXT;    
        }

        NS_ADDREF(destInfo);
        if(!destInfo->IsValid())
        {
            // XXX bad! should log
            NS_RELEASE(destInfo);
            return HT_ENUMERATE_NEXT;    
        }

        // The name table now owns the reference we AddRef'd above
        PL_HashTableAdd(aWorkingSet->mNameTable, destInfo->GetTheName(), destInfo);
        PL_HashTableAdd(aWorkingSet->mIIDTable, destInfo->GetTheIID(), destInfo);
    }

    return HT_ENUMERATE_NEXT;
}       


PRBool 
xptiInterfaceInfoManager::MergeWorkingSets(xptiWorkingSet& aDestWorkingSet,
                                         xptiWorkingSet& aSrcWorkingSet)
{
    // Combine file lists.

    PRUint32 originalTypelibFileCount = aDestWorkingSet.GetFileCount();
    PRUint32 additionalTypelibFileCount = aSrcWorkingSet.GetFileCount();
    PRUint32 i;

    if(!additionalTypelibFileCount)
        return PR_TRUE;

    // Create a new array big enough to hold both lists and copy existing files

    if(!aDestWorkingSet.ExtendFileArray(originalTypelibFileCount +
                                        additionalTypelibFileCount))
        return PR_FALSE;

    // Now we are where we started, but we know we have enough space.

    // Prepare offset array for later fixups. 
    // NOTE: Storing with dest, but alloc'ing from src. This is intentional.
    aDestWorkingSet.mMergeOffsetMap = (PRUint32*)
        XPT_CALLOC(aSrcWorkingSet.GetStructArena(),
                   additionalTypelibFileCount * sizeof(PRUint32)); 

    for(i = 0; i < additionalTypelibFileCount; ++i)
    {
        xptiFile& srcFile = aSrcWorkingSet.GetFileAt(i);
        PRUint32 k;
        for(k = 0; k < originalTypelibFileCount; ++k)
        {
            // If file (with same name, date, and time) is in both lists
            // then reuse that record.
            xptiFile& destFile = aDestWorkingSet.GetFileAt(k);
            if(srcFile.Equals(destFile))
            {
                aDestWorkingSet.mMergeOffsetMap[i] = k - i;
                break;    
            }
        }
        if(k == originalTypelibFileCount)
        {
            // No match found, tack it on the end.

            PRUint32 newIndex = aDestWorkingSet.GetFileCount();

            aDestWorkingSet.AppendFile(
                    xptiFile(srcFile, &aDestWorkingSet, PR_FALSE));

            // Fixup the merge offset map.
            aDestWorkingSet.mMergeOffsetMap[i] = newIndex - i;
        }
    }
    
    // Now the dest file array is set up, migrate xptiInterfaceInfos

    PL_HashTableEnumerateEntries(aSrcWorkingSet.mNameTable, xpti_Merger, 
                                 &aDestWorkingSet);

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
        nsCOMPtr<nsISupports> sup;
        aFileList->GetElementAt(i, getter_AddRefs(sup));
        if(!sup)
            return PR_FALSE;
        nsCOMPtr<nsIFile> file = do_QueryInterface(sup);
        if(!file)
            return PR_FALSE;

        char* name;
        if(NS_FAILED(file->GetLeafName(&name)))
            return PR_FALSE;

        printf("* found %s\n", name);
        nsAllocator::Free(name);
    }
    return PR_TRUE;
}

PRBool 
xptiInterfaceInfoManager::DEBUG_DumpFileListInWorkingSet(xptiWorkingSet& aWorkingSet)
{
    for(PRUint16 i = 0; i < aWorkingSet.GetFileCount(); ++i)
    {
        xptiFile& record = aWorkingSet.GetFileAt(i);
    
        printf("! has %s\n", record.GetName());
    }        
    return PR_TRUE;
}        


/***************************************************************************/

/* nsIInterfaceInfo getInfoForIID (in nsIIDPtr iid); */
NS_IMETHODIMP xptiInterfaceInfoManager::GetInfoForIID(const nsIID * iid, nsIInterfaceInfo **_retval)
{
    NS_ASSERTION(iid, "bad param");
    NS_ASSERTION(_retval, "bad param");

    xptiInterfaceInfo* info = (xptiInterfaceInfo*)
        PL_HashTableLookup(mWorkingSet.mIIDTable, iid);

    if(!info)
    {
        *_retval = nsnull;
        return NS_ERROR_FAILURE;    
    }

    NS_ADDREF(*_retval = NS_STATIC_CAST(nsIInterfaceInfo*, info));
    return NS_OK;    
}

/* nsIInterfaceInfo getInfoForName (in string name); */
NS_IMETHODIMP xptiInterfaceInfoManager::GetInfoForName(const char *name, nsIInterfaceInfo **_retval)
{
    NS_ASSERTION(name, "bad param");
    NS_ASSERTION(_retval, "bad param");

    xptiInterfaceInfo* info = (xptiInterfaceInfo*)
        PL_HashTableLookup(mWorkingSet.mNameTable, name);

    if(!info)
    {
        *_retval = nsnull;
        return NS_ERROR_FAILURE;    
    }

    NS_ADDREF(*_retval = NS_STATIC_CAST(nsIInterfaceInfo*, info));
    return NS_OK;    
}

/* nsIIDPtr getIIDForName (in string name); */
NS_IMETHODIMP xptiInterfaceInfoManager::GetIIDForName(const char *name, nsIID * *_retval)
{
    NS_ASSERTION(name, "bad param");
    NS_ASSERTION(_retval, "bad param");

    xptiInterfaceInfo* info = (xptiInterfaceInfo*)
        PL_HashTableLookup(mWorkingSet.mNameTable, name);

    if(!info)
    {
        *_retval = nsnull;
        return NS_ERROR_FAILURE;    
    }

    return info->GetIID(_retval);
}

/* string getNameForIID (in nsIIDPtr iid); */
NS_IMETHODIMP xptiInterfaceInfoManager::GetNameForIID(const nsIID * iid, char **_retval)
{
    NS_ASSERTION(iid, "bad param");
    NS_ASSERTION(_retval, "bad param");

    xptiInterfaceInfo* info = (xptiInterfaceInfo*)
        PL_HashTableLookup(mWorkingSet.mIIDTable, iid);

    if(!info)
    {
        *_retval = nsnull;
        return NS_ERROR_FAILURE;    
    }

    return info->GetName(_retval);
}

static PR_CALLBACK PRIntn 
xpti_ArrayAppender(PLHashEntry *he, PRIntn i, void *arg)
{
    nsIInterfaceInfo* ii = (nsIInterfaceInfo*) he->value;
    nsISupportsArray* array = (nsISupportsArray*) arg;

    // XXX nsSupportsArray.h shows that this method returns the wrong value!
    array->AppendElement(ii);
    return HT_ENUMERATE_NEXT;
}


/* nsIEnumerator enumerateInterfaces (); */
NS_IMETHODIMP xptiInterfaceInfoManager::EnumerateInterfaces(nsIEnumerator **_retval)
{
    // I didn't want to incur the size overhead of using nsHashtable just to
    // make building an enumerator easier. So, this code makes a snapshot of 
    // the table using an nsISupportsArray and builds an enumerator for that.
    // We can afford this transient cost.

    nsCOMPtr<nsISupportsArray> array = 
        do_CreateInstance(NS_SUPPORTSARRAY_PROGID);
    if(!array)
        return NS_ERROR_UNEXPECTED;

    PL_HashTableEnumerateEntries(mWorkingSet.mNameTable, xpti_ArrayAppender, 
                                 array);
    
    return array->Enumerate(_retval);
}

/* void autoRegisterInterfaces (); */
NS_IMETHODIMP xptiInterfaceInfoManager::AutoRegisterInterfaces()
{
    nsCOMPtr<nsISupportsArray> fileList;
    xptiWorkingSet workingSet;
    AutoRegMode mode;

    if(!workingSet.IsValid())
        return NS_ERROR_UNEXPECTED;

    // We re-read the manifest rather than muck with the 'live' one.
    // It is OK if this fails.
    // XXX But we should track failure as a warning.
    xptiManifest::Read(this, workingSet);

    // Grovel for all the typelibs we can find (in .xpt or .zip, .jar,...).
    if(!BuildFileList(getter_AddRefs(fileList)) || !fileList)
        return NS_ERROR_UNEXPECTED;
    
    // DEBUG_DumpFileList(fileList);

    // Check to see how much work we need to do.
    mode = DetermineAutoRegStrategy(fileList, workingSet);

    switch(mode)
    {
    case NO_FILES_CHANGED:
        return NS_OK;
    case MAYBE_ONLY_ADDITIONS:
        // This might fallback and do a full validation
        // (by calling DoFullValidationMergeFromFileList) so we let
        // it possibly modify 'mode'.
        if(!AttemptAddOnlyFromFileList(fileList, workingSet, &mode))
            return NS_ERROR_UNEXPECTED;
        break;
    case FULL_VALIDATION_REQUIRED:
        if(!DoFullValidationMergeFromFileList(fileList, workingSet))
            return NS_ERROR_UNEXPECTED;
        break;
    default:
        NS_ASSERTION(0,"switch missing a case");
        return NS_ERROR_UNEXPECTED;
    }

    // XXX Is failure to write the file a good reason to not merge?
    if(!xptiManifest::Write(this, workingSet))
        return NS_ERROR_UNEXPECTED;
    
    if(!MergeWorkingSets(mWorkingSet, workingSet))
        return NS_ERROR_UNEXPECTED;

//    DEBUG_DumpFileListInWorkingSet(mWorkingSet);

    return NS_OK;
}

/***************************************************************************/

XPTI_PUBLIC_API(nsIInterfaceInfoManager*)
XPTI_GetInterfaceInfoManager()
{
    return xptiInterfaceInfoManager::GetInterfaceInfoManager();
}

XPTI_PUBLIC_API(void)
XPTI_FreeInterfaceInfoManager()
{
    xptiInterfaceInfoManager::FreeInterfaceInfoManager();
}

