/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla FastLoad code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Brendan Eich <brendan@mozilla.org> (original author)
 */

#include "prtypes.h"
#include "prio.h"
#include "prtime.h"
#include "pldhash.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsAutoLock.h"
#include "nsCOMPtr.h"
#include "nsFastLoadFile.h"
#include "nsFastLoadPtr.h"
#include "nsFastLoadService.h"
#include "nsString.h"

#include "nsIComponentManager.h"
#include "nsIFile.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsISeekableStream.h"
#include "nsISupports.h"

// XXXbe we *know* that nsIObject*Stream is implemented by nsFastLoadFile.cpp
inline nsFastLoadFileReader* GetReader(nsIObjectInputStream* aStream) {
    return NS_STATIC_CAST(nsFastLoadFileReader*, aStream);
}

inline nsFastLoadFileWriter* GetWriter(nsIObjectOutputStream* aStream) {
    return NS_STATIC_CAST(nsFastLoadFileWriter*, aStream);
}

PR_IMPLEMENT_DATA(nsIFastLoadService*) gFastLoadService_ = nsnull;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsFastLoadService, nsIFastLoadService)

nsFastLoadService::nsFastLoadService()
  : mLock(nsnull),
    mFastLoadPtrMap(nsnull),
    mDirection(0)
{
    NS_INIT_REFCNT();

    NS_ASSERTION(gFastLoadService_ == nsnull, "double FastLoadService init?");
    gFastLoadService_ = this;
}

nsFastLoadService::~nsFastLoadService()
{
    gFastLoadService_ = nsnull;

    if (mInputStream)
        mInputStream->Close();
    if (mOutputStream)
        mOutputStream->Close();

    if (mFastLoadPtrMap)
        PL_DHashTableDestroy(mFastLoadPtrMap);
    if (mLock)
        PR_DestroyLock(mLock);
}

NS_IMETHODIMP
nsFastLoadService::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    *aResult = nsnull;
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsFastLoadService* fastLoadService = new nsFastLoadService();
    if (!fastLoadService)
        return NS_ERROR_OUT_OF_MEMORY;

    fastLoadService->mLock = PR_NewLock();
    if (!fastLoadService->mLock) {
        delete fastLoadService;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(fastLoadService);
    nsresult rv = fastLoadService->QueryInterface(aIID, aResult);
    NS_RELEASE(fastLoadService);
    return rv;
}

#if defined XP_MAC

// Mac format: "<Basename> FastLoad File" with <basename> capitalized.
# include "nsCRT.h"

# define MASSAGE_BASENAME(bn)   (bn.SetCharAt(nsCRT::ToUpper(bn.CharAt(0)), 0))
# define PLATFORM_FASL_SUFFIX   " FastLoad File"

#elif defined(XP_UNIX) || defined(XP_BEOS)

// Unix format: "<basename>.mfasl".
# define MASSAGE_BASENAME(bn)   /* nothing */
# define PLATFORM_FASL_SUFFIX   ".mfasl"

#elif defined(XP_WIN) || defined(XP_OS2)

// Windows format: "<basename>.mfl".
# define MASSAGE_BASENAME(bn)   /* nothing */
# define PLATFORM_FASL_SUFFIX   ".mfl"

#endif

nsresult
nsFastLoadService::NewFastLoadFile(const char* aBaseName, nsIFile* *aResult)
{
    nsresult rv;
    nsCOMPtr<nsIFile> file;

    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString name(aBaseName);
    MASSAGE_BASENAME(name);
    name += PLATFORM_FASL_SUFFIX;
    rv = file->Append(name);
    if (NS_FAILED(rv)) return rv;

    *aResult = file;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::NewInputStream(nsIInputStream* aSrcStream,
                                  PRUint32 *aChecksum,
                                  nsIObjectInputStream* *aResult)
{
    nsresult rv;
    nsAutoLock lock(mLock);

    nsCOMPtr<nsIObjectInputStream> stream;
    rv = NS_NewFastLoadFileReader(getter_AddRefs(stream), aSrcStream);
    if (NS_FAILED(rv)) return rv;

    nsFastLoadFileReader* reader = GetReader(stream);

    rv = reader->GetChecksum(aChecksum);
    if (NS_FAILED(rv)) return rv;

    *aResult = stream;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::NewOutputStream(nsIOutputStream* aDestStream,
                                   nsIObjectOutputStream* *aResult)
{
    nsAutoLock lock(mLock);

    return NS_NewFastLoadFileWriter(aResult, aDestStream);
}

NS_IMETHODIMP
nsFastLoadService::GetCurrentInputStream(nsIObjectInputStream* *aResult)
{
    NS_IF_ADDREF(*aResult = mInputStream);
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::SetCurrentInputStream(nsIObjectInputStream* aStream)
{
    nsAutoLock lock(mLock);
    mInputStream = aStream;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::GetCurrentOutputStream(nsIObjectOutputStream* *aResult)
{
    NS_IF_ADDREF(*aResult = mOutputStream);
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::SetCurrentOutputStream(nsIObjectOutputStream* aStream)
{
    nsAutoLock lock(mLock);
    mOutputStream = aStream;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::GetCurrentFileIO(nsIFastLoadFileIO* *aResult)
{
    NS_IF_ADDREF(*aResult = mFileIO);
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::SetCurrentFileIO(nsIFastLoadFileIO* aFileIO)
{
    nsAutoLock lock(mLock);
    mFileIO = aFileIO;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::GetCurrentDirection(PRInt32 *aResult)
{
    *aResult = mDirection;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::StartMuxedDocument(nsISupports* aURI, const char* aURISpec,
                                      PRInt32 aDirectionFlags)
{
    nsresult rv = NS_ERROR_NOT_AVAILABLE;
    nsAutoLock lock(mLock);

    // Try for an input stream first, in case aURISpec's data is multiplexed
    // in the current FastLoad file.
    if ((aDirectionFlags & NS_FASTLOAD_READ) && mInputStream) {
        nsFastLoadFileReader* reader = GetReader(mInputStream);
        rv = reader->StartMuxedDocument(aURI, aURISpec);
        if (NS_SUCCEEDED(rv) || rv != NS_ERROR_NOT_AVAILABLE)
            return rv;

        // If aURISpec is not found in the reader, wrap it with an updater
        // and store the updater at mOutputStream.
        if (!mOutputStream && mFileIO) {
            nsCOMPtr<nsIOutputStream> output;
            rv = mFileIO->GetOutputStream(getter_AddRefs(output));
            if (NS_FAILED(rv)) return rv;

            rv = NS_NewFastLoadFileUpdater(getter_AddRefs(mOutputStream),
                                           output,
                                           reader);
            if (NS_FAILED(rv)) return rv;
        }

        if (aDirectionFlags == NS_FASTLOAD_READ) {
            // Tell our caller to re-start multiplexing, rather than attempt
            // to select and deserialize now.
            return NS_ERROR_NOT_AVAILABLE;
        }
    }

    if ((aDirectionFlags & NS_FASTLOAD_WRITE) && mOutputStream)
        rv = GetWriter(mOutputStream)->StartMuxedDocument(aURI, aURISpec);
    return rv;
}

NS_IMETHODIMP
nsFastLoadService::SelectMuxedDocument(nsISupports* aURI)
{
    nsresult rv = NS_ERROR_NOT_AVAILABLE;
    nsAutoLock lock(mLock);

    // Try to select the reader, if any; then only if the URI was not in the
    // file already, select the writer/updater.
    if (mInputStream) {
        rv = GetReader(mInputStream)->SelectMuxedDocument(aURI);
        if (NS_SUCCEEDED(rv))
            mDirection = NS_FASTLOAD_READ;
    }

    if (rv == NS_ERROR_NOT_AVAILABLE && mOutputStream) {
        rv = GetWriter(mOutputStream)->SelectMuxedDocument(aURI);
        if (NS_SUCCEEDED(rv))
            mDirection = NS_FASTLOAD_WRITE;
    }

    return rv;
}

NS_IMETHODIMP
nsFastLoadService::EndMuxedDocument(nsISupports* aURI)
{
    nsresult rv = NS_ERROR_NOT_AVAILABLE;
    nsAutoLock lock(mLock);

    // Try to end the document identified by aURI in the reader, if any; then
    // only if the URI was not in the file already, end the writer/updater.
    if (mInputStream)
        rv = GetReader(mInputStream)->EndMuxedDocument(aURI);

    if (rv == NS_ERROR_NOT_AVAILABLE && mOutputStream)
        rv = GetWriter(mOutputStream)->EndMuxedDocument(aURI);

    mDirection = 0;
    return rv;
}

NS_IMETHODIMP
nsFastLoadService::AppendDependency(const char* aFileName)
{
    nsAutoLock lock(mLock);
    if (!mOutputStream)
        return NS_OK;

    if (!GetWriter(mOutputStream)->AppendDependency(aFileName))
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::MaxDependencyModifiedTime(PRTime *aTime)
{
    *aTime = LL_ZERO;

    nsAutoLock lock(mLock);
    if (!mOutputStream)
        return NS_OK;

    nsFastLoadFileWriter* writer = GetWriter(mOutputStream);

    for (PRUint32 i = 0, n = writer->GetDependencyCount(); i < n; i++) {
        PRFileInfo info;
        if (PR_GetFileInfo(writer->GetDependency(i), &info) == PR_SUCCESS &&
            LL_CMP(*aTime, <, info.modifyTime)) {
            *aTime = info.modifyTime;
        }
    }

    return NS_OK;
}

struct nsFastLoadPtrEntry : public PLDHashEntryHdr {
    nsISupports** mPtrAddr;     // key, must come first for PL_DHashGetStubOps
    PRUint32      mOffset;
};

NS_IMETHODIMP
nsFastLoadService::GetFastLoadReferent(nsISupports* *aPtrAddr)
{
    NS_ASSERTION(*aPtrAddr == nsnull,
                 "aPtrAddr doesn't point to null nsFastLoadPtr<T>::mRawAddr?");

    nsAutoLock lock(mLock);
    if (!mFastLoadPtrMap || !mInputStream)
        return NS_OK;

    nsFastLoadPtrEntry* entry =
        NS_STATIC_CAST(nsFastLoadPtrEntry*,
                       PL_DHashTableOperate(mFastLoadPtrMap, aPtrAddr,
                                            PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_FREE(entry))
        return NS_OK;

    nsresult rv;
    nsCOMPtr<nsISeekableStream> seekable(do_QueryInterface(mInputStream));

    rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, entry->mOffset);
    if (NS_FAILED(rv)) return rv;

    rv = mInputStream->ReadObject(PR_TRUE, aPtrAddr);
    if (NS_FAILED(rv)) return rv;

    // Shrink the table if half the entries are removed sentinels.
    PRUint32 size = PR_BIT(mFastLoadPtrMap->sizeLog2);
    if (mFastLoadPtrMap->removedCount >= (size >> 2))
        PL_DHashTableOperate(mFastLoadPtrMap, entry, PL_DHASH_REMOVE);
    else
        PL_DHashTableRawRemove(mFastLoadPtrMap, entry);

    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::ReadFastLoadPtr(nsIObjectInputStream* aInputStream,
                                   nsISupports* *aPtrAddr)
{
    // nsFastLoadPtrs self-construct to null, so if we have a non-null value
    // in our inout parameter, we must have been read already, alright!
    if (*aPtrAddr)
        return NS_OK;

    nsresult rv;
    PRUint32 nextOffset;
    nsAutoLock lock(mLock);

    rv = aInputStream->Read32(&nextOffset);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISeekableStream> seekable(do_QueryInterface(aInputStream));
    if (!seekable)
        return NS_ERROR_FAILURE;

    PRUint32 thisOffset;
    rv = seekable->Tell(&thisOffset);
    if (NS_FAILED(rv)) return rv;

    rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, nextOffset);
    if (NS_FAILED(rv)) return rv;

    if (!mFastLoadPtrMap) {
        mFastLoadPtrMap = PL_NewDHashTable(PL_DHashGetStubOps(), this,
                                           sizeof(nsFastLoadPtrEntry),
                                           PL_DHASH_MIN_SIZE);
        if (!mFastLoadPtrMap)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    nsFastLoadPtrEntry* entry =
        NS_STATIC_CAST(nsFastLoadPtrEntry*,
                       PL_DHashTableOperate(mFastLoadPtrMap, aPtrAddr,
                                            PL_DHASH_ADD));
    NS_ASSERTION(entry->mPtrAddr == nsnull, "duplicate nsFastLoadPtr?!");

    entry->mPtrAddr = aPtrAddr;
    entry->mOffset = thisOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::WriteFastLoadPtr(nsIObjectOutputStream* aOutputStream,
                                    nsISupports* aObject)
{
    NS_ASSERTION(aObject != nsnull, "writing an unread nsFastLoadPtr?!");
    if (!aObject)
        return NS_ERROR_UNEXPECTED;

    nsresult rv;
    nsAutoLock lock(mLock);     // serialize writes to aOutputStream

    nsCOMPtr<nsISeekableStream> seekable(do_QueryInterface(aOutputStream));
    if (!seekable)
        return NS_ERROR_FAILURE;

    PRUint32 saveOffset;
    rv = seekable->Tell(&saveOffset);
    if (NS_FAILED(rv)) return rv;

    rv = aOutputStream->Write32(0);       // nextOffset placeholder
    if (NS_FAILED(rv)) return rv;

    rv = aOutputStream->WriteObject(aObject, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    PRUint32 nextOffset;
    rv = seekable->Tell(&nextOffset);
    if (NS_FAILED(rv)) return rv;

    rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, saveOffset);
    if (NS_FAILED(rv)) return rv;

    rv = aOutputStream->Write32(nextOffset);
    if (NS_FAILED(rv)) return rv;

    rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, nextOffset);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}
