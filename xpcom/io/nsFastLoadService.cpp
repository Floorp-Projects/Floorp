/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla FastLoad code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brendan Eich <brendan@mozilla.org> (original author)
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
#include "nsIEnumerator.h"
#include "nsIFastLoadFileControl.h"
#include "nsIFile.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsISeekableStream.h"
#include "nsISupports.h"

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
    if (NS_FAILED(rv))
        return rv;

    nsCAutoString name(aBaseName);
    MASSAGE_BASENAME(name);
    name += PLATFORM_FASL_SUFFIX;
    rv = file->Append(name);
    if (NS_FAILED(rv))
        return rv;

    *aResult = file;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::NewInputStream(nsIInputStream* aSrcStream,
                                  nsIObjectInputStream* *aResult)
{
    nsAutoLock lock(mLock);

    nsCOMPtr<nsIObjectInputStream> stream;
    nsresult rv = NS_NewFastLoadFileReader(getter_AddRefs(stream), aSrcStream);
    if (NS_FAILED(rv))
        return rv;

    *aResult = stream;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::NewOutputStream(nsIOutputStream* aDestStream,
                                   nsIObjectOutputStream* *aResult)
{
    nsAutoLock lock(mLock);

    return NS_NewFastLoadFileWriter(aResult, aDestStream, mFileIO);
}

NS_IMETHODIMP
nsFastLoadService::GetInputStream(nsIObjectInputStream* *aResult)
{
    NS_IF_ADDREF(*aResult = mInputStream);
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::SetInputStream(nsIObjectInputStream* aStream)
{
    nsAutoLock lock(mLock);
    mInputStream = aStream;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::GetOutputStream(nsIObjectOutputStream* *aResult)
{
    NS_IF_ADDREF(*aResult = mOutputStream);
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::SetOutputStream(nsIObjectOutputStream* aStream)
{
    nsAutoLock lock(mLock);
    mOutputStream = aStream;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::GetFileIO(nsIFastLoadFileIO* *aResult)
{
    NS_IF_ADDREF(*aResult = mFileIO);
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::SetFileIO(nsIFastLoadFileIO* aFileIO)
{
    nsAutoLock lock(mLock);
    mFileIO = aFileIO;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::GetDirection(PRInt32 *aResult)
{
    *aResult = mDirection;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::StartMuxedDocument(nsISupports* aURI, const char* aURISpec,
                                      PRInt32 aDirectionFlags)
{
    nsresult rv = NS_ERROR_NOT_AVAILABLE;
    nsCOMPtr<nsIFastLoadFileControl> control;
    nsAutoLock lock(mLock);

    // Try for an input stream first, in case aURISpec's data is multiplexed
    // in the current FastLoad file.
    if ((aDirectionFlags & NS_FASTLOAD_READ) && mInputStream) {
        control = do_QueryInterface(mInputStream);
        if (control) {
            // If aURISpec is not in the multiplex, control->StartMuxedDocument
            // will return NS_ERROR_NOT_AVAILABLE.
            rv = control->StartMuxedDocument(aURI, aURISpec);
            if (NS_SUCCEEDED(rv) || rv != NS_ERROR_NOT_AVAILABLE)
                return rv;

            // Ok, aURISpec is not in the existing mux.  If we have no output
            // stream yet, wrap the reader with a FastLoad file updater.
            if (!mOutputStream && mFileIO) {
                nsCOMPtr<nsIOutputStream> output;
                rv = mFileIO->GetOutputStream(getter_AddRefs(output));
                if (NS_FAILED(rv))
                    return rv;

                // NB: mInputStream must be an nsFastLoadFileReader!
                rv = NS_NewFastLoadFileUpdater(getter_AddRefs(mOutputStream),
                                               output,
                                               mInputStream);
                if (NS_FAILED(rv))
                    return rv;
            }

            if (aDirectionFlags == NS_FASTLOAD_READ) {
                // Tell our caller to re-start multiplexing, rather than attempt
                // to select and deserialize now.
                return NS_ERROR_NOT_AVAILABLE;
            }
        }
    }

    if ((aDirectionFlags & NS_FASTLOAD_WRITE) && mOutputStream) {
        control = do_QueryInterface(mOutputStream);
        if (control)
            rv = control->StartMuxedDocument(aURI, aURISpec);
    }
    return rv;
}

NS_IMETHODIMP
nsFastLoadService::SelectMuxedDocument(nsISupports* aURI)
{
    nsresult rv = NS_ERROR_NOT_AVAILABLE;
    nsCOMPtr<nsIFastLoadFileControl> control;
    nsAutoLock lock(mLock);

    // Try to select the reader, if any; then only if the URI was not in the
    // file already, select the writer/updater.
    if (mInputStream) {
        control = do_QueryInterface(mInputStream);
        if (control) {
            rv = control->SelectMuxedDocument(aURI);
            if (NS_SUCCEEDED(rv))
                mDirection = NS_FASTLOAD_READ;
        }
    }

    if (rv == NS_ERROR_NOT_AVAILABLE && mOutputStream) {
        control = do_QueryInterface(mOutputStream);
        if (control) {
            rv = control->SelectMuxedDocument(aURI);
            if (NS_SUCCEEDED(rv))
                mDirection = NS_FASTLOAD_WRITE;
        }
    }

    return rv;
}

NS_IMETHODIMP
nsFastLoadService::EndMuxedDocument(nsISupports* aURI)
{
    nsresult rv = NS_ERROR_NOT_AVAILABLE;
    nsCOMPtr<nsIFastLoadFileControl> control;
    nsAutoLock lock(mLock);

    // Try to end the document identified by aURI in the reader, if any; then
    // only if the URI was not in the file already, end the writer/updater.
    if (mInputStream) {
        control = do_QueryInterface(mInputStream);
        if (control)
            rv = control->EndMuxedDocument(aURI);
    }

    if (rv == NS_ERROR_NOT_AVAILABLE && mOutputStream) {
        control = do_QueryInterface(mOutputStream);
        if (control)
            rv = control->EndMuxedDocument(aURI);
    }

    mDirection = 0;
    return rv;
}

NS_IMETHODIMP
nsFastLoadService::AddDependency(nsIFile* aFile)
{
    nsAutoLock lock(mLock);

    nsCOMPtr<nsIFastLoadWriteControl> control(do_QueryInterface(mOutputStream));
    if (!control)
        return NS_ERROR_NOT_AVAILABLE;

    return control->AddDependency(aFile);
}

NS_IMETHODIMP
nsFastLoadService::MaxDependencyModifiedTime(nsIFastLoadReadControl* aControl,
                                             PRTime *aTime)
{
    *aTime = LL_ZERO;

    nsAutoLock lock(mLock);

    nsCOMPtr<nsISimpleEnumerator> dependencies;
    nsresult rv = aControl->GetDependencies(getter_AddRefs(dependencies));
    if (NS_FAILED(rv))
        return rv;

    PRBool more;
    while (NS_SUCCEEDED(dependencies->HasMoreElements(&more)) && more) {
        nsCOMPtr<nsIFile> file;
        dependencies->GetNext(getter_AddRefs(file));
        if (!file)
            return NS_ERROR_UNEXPECTED;

        PRTime lastModifiedTime;
        rv = file->GetLastModificationDate(&lastModifiedTime);
        if (NS_FAILED(rv))
            return rv;

        if (LL_CMP(*aTime, <, lastModifiedTime))
            *aTime = lastModifiedTime;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::ComputeChecksum(nsIFile* aFile,
                                   nsIFastLoadReadControl* aControl,
                                   PRUint32 *aChecksum)
{
    nsXPIDLCString path;
    nsresult rv = aFile->GetPath(getter_Copies(path));
    if (NS_FAILED(rv))
        return rv;

    nsCStringKey key(path);
    PRUint32 checksum = NS_PTR_TO_INT32(mChecksumTable.Get(&key));
    if (checksum) {
        *aChecksum = checksum;
        return NS_OK;
    }

    rv = aControl->ComputeChecksum(&checksum);
    if (NS_FAILED(rv))
        return rv;

    mChecksumTable.Put(&key, NS_INT32_TO_PTR(checksum));
    *aChecksum = checksum;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadService::CacheChecksum(nsIFile* aFile, nsIObjectOutputStream *aStream)
{
    nsCOMPtr<nsIFastLoadFileControl> control(do_QueryInterface(aStream));
    if (!control)
        return NS_ERROR_FAILURE;

    PRUint32 checksum;
    nsresult rv = control->GetChecksum(&checksum);
    if (NS_FAILED(rv))
        return rv;

    nsXPIDLCString path;
    rv = aFile->GetPath(getter_Copies(path));
    if (NS_FAILED(rv))
        return rv;

    nsCStringKey key(path);
    mChecksumTable.Put(&key, NS_INT32_TO_PTR(checksum));
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
    if (NS_FAILED(rv))
        return rv;

    rv = mInputStream->ReadObject(PR_TRUE, aPtrAddr);
    if (NS_FAILED(rv))
        return rv;

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
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsISeekableStream> seekable(do_QueryInterface(aInputStream));
    if (!seekable)
        return NS_ERROR_FAILURE;

    PRUint32 thisOffset;
    rv = seekable->Tell(&thisOffset);
    if (NS_FAILED(rv))
        return rv;

    rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, nextOffset);
    if (NS_FAILED(rv))
        return rv;

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
    if (NS_FAILED(rv))
        return rv;

    rv = aOutputStream->Write32(0);       // nextOffset placeholder
    if (NS_FAILED(rv))
        return rv;

    rv = aOutputStream->WriteObject(aObject, PR_TRUE);
    if (NS_FAILED(rv))
        return rv;

    PRUint32 nextOffset;
    rv = seekable->Tell(&nextOffset);
    if (NS_FAILED(rv))
        return rv;

    rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, saveOffset);
    if (NS_FAILED(rv))
        return rv;

    rv = aOutputStream->Write32(nextOffset);
    if (NS_FAILED(rv))
        return rv;

    rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, nextOffset);
    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
}
