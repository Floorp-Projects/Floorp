/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#if defined(XP_UNIX) || defined(XP_BEOS)
#include <unistd.h>
#elif defined(XP_MAC)
#include <Files.h>
#elif defined(XP_WIN)
#include <windows.h>
#elif defined(XP_OS2)
#define INCL_DOSERRORS
#include <os2.h>
#else
// XXX add necessary include file for ftruncate (or equivalent)
#endif

#if defined(XP_MAC)
#include "pprio.h"
#else
#include "private/pprio.h"
#endif

#include "nsFileStreams.h"
#include "nsILocalFile.h"
#include "nsXPIDLString.h"
#include "prerror.h"
#include "nsCRT.h"
#include "nsInt64.h"
#include "nsIFile.h"
#include "nsDirectoryIndexStream.h"
#include "nsMimeTypes.h"
#include "nsReadLine.h"
#include "nsNetUtil.h"
//#include "nsFileTransportService.h"

#define NS_NO_INPUT_BUFFERING 1 // see http://bugzilla.mozilla.org/show_bug.cgi?id=41067

#if defined(PR_LOGGING)
//
// Log module for nsFileTransport logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsFileIO:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gFileIOLog = nsnull;

#endif /* PR_LOGGING */

////////////////////////////////////////////////////////////////////////////////
// nsFileStream

nsFileStream::nsFileStream()
    : mFD(nsnull)
    , mCloseFD(PR_TRUE)
{
}

nsFileStream::~nsFileStream()
{
    if (mCloseFD)
        Close();
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsFileStream, nsISeekableStream)

nsresult
nsFileStream::InitWithFileDescriptor(PRFileDesc* fd, nsISupports* parent)
{
    NS_ENSURE_TRUE(mFD == nsnull, NS_ERROR_ALREADY_INITIALIZED);
    //
    // this file stream is dependent on its parent to keep the
    // file descriptor valid.  an owning reference to the parent
    // prevents the file descriptor from going away prematurely.
    //
    mFD = fd;
    mCloseFD = PR_FALSE;
    mParent = parent;
    return NS_OK;
}

nsresult
nsFileStream::Close()
{
    nsresult rv = NS_OK;
    if (mFD) {
        if (mCloseFD)
            if (PR_Close(mFD) == PR_FAILURE)
                rv = NS_BASE_STREAM_OSERROR;
        mFD = nsnull;
    }
    return rv;
}

NS_IMETHODIMP
nsFileStream::Seek(PRInt32 whence, PRInt64 offset)
{
    if (mFD == nsnull)
        return NS_BASE_STREAM_CLOSED;

    nsInt64 cnt = PR_Seek64(mFD, offset, (PRSeekWhence)whence);
    if (cnt == nsInt64(-1)) {
        return NS_ErrorAccordingToNSPR();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFileStream::Tell(PRInt64 *result)
{
    if (mFD == nsnull)
        return NS_BASE_STREAM_CLOSED;

    nsInt64 cnt = PR_Seek64(mFD, 0, PR_SEEK_CUR);
    if (cnt == nsInt64(-1)) {
        return NS_ErrorAccordingToNSPR();
    }
    *result = cnt;
    return NS_OK;
}

NS_IMETHODIMP
nsFileStream::SetEOF()
{
    if (mFD == nsnull)
        return NS_BASE_STREAM_CLOSED;

#if defined(XP_UNIX) || defined(XP_MAC) || defined(XP_OS2) || defined(XP_BEOS)
    // Some system calls require an EOF offset.
    PRInt64 offset;
    nsresult rv = Tell(&offset);
    if (NS_FAILED(rv)) return rv;
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
    if (ftruncate(PR_FileDesc2NativeHandle(mFD), offset) != 0) {
        NS_ERROR("ftruncate failed");
        return NS_ERROR_FAILURE;
    }
#elif defined(XP_MAC)
    if (::SetEOF(PR_FileDesc2NativeHandle(mFD), offset) != 0) {
        NS_ERROR("SetEOF failed");
        return NS_ERROR_FAILURE;
    }
#elif defined(XP_WIN)
    if (!SetEndOfFile((HANDLE) PR_FileDesc2NativeHandle(mFD))) {
        NS_ERROR("SetEndOfFile failed");
        return NS_ERROR_FAILURE;
    }
#elif defined(XP_OS2)
    if (DosSetFileSize((HFILE) PR_FileDesc2NativeHandle(mFD), offset) != NO_ERROR) {
        NS_ERROR("DosSetFileSize failed");
        return NS_ERROR_FAILURE;
    }
#else
    // XXX not implemented
#endif

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsFileInputStream

NS_IMPL_ISUPPORTS_INHERITED3(nsFileInputStream, 
                             nsFileStream,
                             nsIInputStream,
                             nsIFileInputStream,
                             nsILineInputStream)

NS_METHOD
nsFileInputStream::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_NO_AGGREGATION(aOuter);

    nsFileInputStream* stream = new nsFileInputStream();
    if (stream == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(stream);
    nsresult rv = stream->QueryInterface(aIID, aResult);
    NS_RELEASE(stream);
    return rv;
}

nsresult
nsFileInputStream::Open(nsIFile* aFile, PRInt32 aIOFlags, PRInt32 aPerm)
{   
    nsresult rv = NS_OK;

    // If the previous file is open, close it
    if (mFD) {
        rv = Close();
        if (NS_FAILED(rv)) return rv;
    }

    // Open the file
    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(aFile, &rv);
    if (NS_FAILED(rv)) return rv;
    if (aIOFlags == -1)
        aIOFlags = PR_RDONLY;
    if (aPerm == -1)
        aPerm = 0;

    PRFileDesc* fd;
    rv = localFile->OpenNSPRFileDesc(aIOFlags, aPerm, &fd);
    if (NS_FAILED(rv)) return rv;

    mFD = fd;

    if (mBehaviorFlags & DELETE_ON_CLOSE) {
        // POSIX compatible filesystems allow a file to be unlinked while a
        // file descriptor is still referencing the file.  since we've already
        // opened the file descriptor, we'll try to remove the file.  if that
        // fails, then we'll just remember the nsIFile and remove it after we
        // close the file descriptor.
        rv = aFile->Remove(PR_FALSE);
        if (NS_FAILED(rv) && !(mBehaviorFlags & REOPEN_ON_REWIND)) {
            // If REOPEN_ON_REWIND is not happenin', we haven't saved the file yet
            mFile = aFile;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsFileInputStream::Init(nsIFile* aFile, PRInt32 aIOFlags, PRInt32 aPerm,
                        PRInt32 aBehaviorFlags)
{
    NS_ENSURE_TRUE(!mFD, NS_ERROR_ALREADY_INITIALIZED);
    NS_ENSURE_TRUE(!mParent, NS_ERROR_ALREADY_INITIALIZED);

    mBehaviorFlags = aBehaviorFlags;

    // If the file will be reopened on rewind, save the info to open the file
    if (mBehaviorFlags & REOPEN_ON_REWIND) {
        mFile = aFile;
        mIOFlags = aIOFlags;
        mPerm = aPerm;
    }

    return Open(aFile, aIOFlags, aPerm);
}

NS_IMETHODIMP
nsFileInputStream::Close()
{
    // null out mLineBuffer in case Close() is called again after failing
    PR_FREEIF(mLineBuffer);
    nsresult rv = nsFileStream::Close();
    if (NS_FAILED(rv)) return rv;
    if (mFile && (mBehaviorFlags & DELETE_ON_CLOSE)) {
        rv = mFile->Remove(PR_FALSE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to delete file");
        // If we don't need to save the file for reopening, free it up
        if (!(mBehaviorFlags & REOPEN_ON_REWIND)) {
          mFile = nsnull;
        }
    }
    return rv;
}

NS_IMETHODIMP
nsFileInputStream::Available(PRUint32* aResult)
{
    if (!mFD) {
        return NS_BASE_STREAM_CLOSED;
    }

    PRInt32 avail = PR_Available(mFD);
    if (avail == -1) {
        return NS_ErrorAccordingToNSPR();
    }
    *aResult = avail;
    return NS_OK;
}

NS_IMETHODIMP
nsFileInputStream::Read(char* aBuf, PRUint32 aCount, PRUint32* aResult)
{
    if (!mFD) {
        return NS_BASE_STREAM_CLOSED;
    }

    PRInt32 bytesRead = PR_Read(mFD, aBuf, aCount);
    if (bytesRead == -1) {
        return NS_ErrorAccordingToNSPR();
    }
    // Check if we're at the end of file and need to close
    if (mBehaviorFlags & CLOSE_ON_EOF) {
        if (bytesRead == 0) {
            Close();
        }
    }

    *aResult = bytesRead;
    return NS_OK;
}

NS_IMETHODIMP
nsFileInputStream::ReadLine(nsACString& aLine, PRBool* aResult)
{
    if (!mLineBuffer) {
        nsresult rv = NS_InitLineBuffer(&mLineBuffer);
        if (NS_FAILED(rv)) return rv;
    }
    return NS_ReadLine(this, mLineBuffer, aLine, aResult);
}

NS_IMETHODIMP
nsFileInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                PRUint32 aCount, PRUint32* aResult)
{
    // ReadSegments is not implemented because it would be inefficient when
    // the writer does not consume all data.  If you want to call ReadSegments,
    // wrap a BufferedInputStream around the file stream.  That will call
    // Read().
    NS_NOTREACHED("ReadSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileInputStream::IsNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsFileInputStream::Seek(PRInt32 aWhence, PRInt64 aOffset)
{
    PR_FREEIF(mLineBuffer); // this invalidates the line buffer
    if (!mFD) {
        if (mBehaviorFlags & REOPEN_ON_REWIND) {
            nsresult rv = Reopen();
            if (NS_FAILED(rv)) {
                return rv;
            }
        } else {
            return NS_BASE_STREAM_CLOSED;
        }
    }

    return nsFileStream::Seek(aWhence, aOffset);
}

////////////////////////////////////////////////////////////////////////////////
// nsFileOutputStream

NS_IMPL_ISUPPORTS_INHERITED2(nsFileOutputStream, 
                             nsFileStream,
                             nsIOutputStream,
                             nsIFileOutputStream)
 
NS_METHOD
nsFileOutputStream::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_NO_AGGREGATION(aOuter);

    nsFileOutputStream* stream = new nsFileOutputStream();
    if (stream == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(stream);
    nsresult rv = stream->QueryInterface(aIID, aResult);
    NS_RELEASE(stream);
    return rv;
}

NS_IMETHODIMP
nsFileOutputStream::Init(nsIFile* file, PRInt32 ioFlags, PRInt32 perm,
                         PRInt32 behaviorFlags)
{
    NS_ENSURE_TRUE(mFD == nsnull, NS_ERROR_ALREADY_INITIALIZED);

    nsresult rv;
    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(file, &rv);
    if (NS_FAILED(rv)) return rv;
    if (ioFlags == -1)
        ioFlags = PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE;
    if (perm <= 0)
        perm = 0664;

    PRFileDesc* fd;
    rv = localFile->OpenNSPRFileDesc(ioFlags, perm, &fd);
    if (NS_FAILED(rv)) return rv;

    mFD = fd;
    return NS_OK;
}

NS_IMETHODIMP
nsFileOutputStream::Close()
{
    return nsFileStream::Close();
}

NS_IMETHODIMP
nsFileOutputStream::Write(const char *buf, PRUint32 count, PRUint32 *result)
{
    if (mFD == nsnull)
        return NS_BASE_STREAM_CLOSED;

    PRInt32 cnt = PR_Write(mFD, buf, count);
    if (cnt == -1) {
        return NS_ErrorAccordingToNSPR();
    }
    *result = cnt;
    return NS_OK;
}

NS_IMETHODIMP
nsFileOutputStream::Flush(void)
{
    if (mFD == nsnull)
        return NS_BASE_STREAM_CLOSED;

    PRInt32 cnt = PR_Sync(mFD);
    if (cnt == -1) {
        return NS_ErrorAccordingToNSPR();
    }
    return NS_OK;
}
    
NS_IMETHODIMP
nsFileOutputStream::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
    NS_NOTREACHED("WriteFrom (see source comment)");
    return NS_ERROR_NOT_IMPLEMENTED;
    // File streams intentionally do not support this method.
    // If you need something like this, then you should wrap
    // the file stream using nsIBufferedOutputStream
}

NS_IMETHODIMP
nsFileOutputStream::WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval)
{
    NS_NOTREACHED("WriteSegments (see source comment)");
    return NS_ERROR_NOT_IMPLEMENTED;
    // File streams intentionally do not support this method.
    // If you need something like this, then you should wrap
    // the file stream using nsIBufferedOutputStream
}

NS_IMETHODIMP
nsFileOutputStream::IsNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = PR_FALSE;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsSafeFileOutputStream

NS_IMPL_ISUPPORTS_INHERITED3(nsSafeFileOutputStream, 
                             nsFileOutputStream,
                             nsISafeFileOutputStream,
                             nsIOutputStream,
                             nsIFileOutputStream)

NS_IMETHODIMP
nsSafeFileOutputStream::Init(nsIFile* file, PRInt32 ioFlags, PRInt32 perm,
                             PRInt32 behaviorFlags)
{
    nsresult rv = file->Exists(&mTargetFileExists);
    if (NS_FAILED(rv)) {
        NS_ERROR("Can't tell if target file exists");
        mTargetFileExists = PR_TRUE; // Safer to assume it exists - we just do more work.
    }

    nsCOMPtr<nsIFile> tempResult;
    rv = file->Clone(getter_AddRefs(tempResult));
    if (NS_SUCCEEDED(rv) && mTargetFileExists) {
        PRUint32 origPerm;
        if (NS_FAILED(file->GetPermissions(&origPerm))) {
            NS_ERROR("Can't get permissions of target file");
            origPerm = perm;
        }
        // XXX What if |perm| is more restrictive then |origPerm|?
        // This leaves the user supplied permssions as they were.
        rv = tempResult->CreateUnique(nsIFile::NORMAL_FILE_TYPE, origPerm);
    }
    if (NS_SUCCEEDED(rv)) {
        mTempFile = tempResult;
        mTargetFile = file;
        rv = nsFileOutputStream::Init(mTempFile, ioFlags, perm, behaviorFlags);
    }
    return rv;
}

NS_IMETHODIMP
nsSafeFileOutputStream::SetWriteSucceeded(PRBool aWriteSucceeded)
{
    mWriteSucceeded = aWriteSucceeded;
    return NS_OK;
}

NS_IMETHODIMP
nsSafeFileOutputStream::GetWriteSucceeded(PRBool *aWriteSucceeded)
{
    *aWriteSucceeded = mWriteSucceeded && mInternalWriteSucceeded;
    return NS_OK;
}

NS_IMETHODIMP
nsSafeFileOutputStream::Close()
{
    nsresult rv = nsFileOutputStream::Close();

    // if there is no temp file, don't try to move it over the original target.
    // It would destroy the targetfile if close() is called twice.
    if (!mTempFile)
        return NS_OK;

    // Only overwrite if everything was ok, and the temp file could be closed.
    if (mWriteSucceeded && mInternalWriteSucceeded && NS_SUCCEEDED(rv)) {
        NS_ENSURE_STATE(mTargetFile);

        if (!mTargetFileExists) {
            // If the target file did not exist when we were initialized, then the
            // temp file we gave out was actually a reference to the target file.
            // since we succeeded in writing to the temp file (and hence succeeded
            // in writing to the target file), there is nothing more to do.
#ifdef DEBUG      
            PRBool equal;
            if (NS_FAILED(mTargetFile->Equals(mTempFile, &equal)) || !equal)
                NS_ERROR("mTempFile not equal to mTargetFile");
#endif
            return NS_OK;      
        }

        nsCAutoString targetFilename;
        rv = mTargetFile->GetNativeLeafName(targetFilename);
    
        if (NS_SUCCEEDED(rv))
            rv = mTempFile->MoveToNative(nsnull, targetFilename); // This will replace target
    }
    else {
        rv = mTempFile->Remove(PR_FALSE);
    }
    mTempFile = nsnull;
    return rv;
}

NS_IMETHODIMP
nsSafeFileOutputStream::Write(const char *buf, PRUint32 count, PRUint32 *result)
{
    nsresult rv = nsFileOutputStream::Write(buf, count, result);
    if (NS_FAILED(rv) || count != *result)
        mInternalWriteSucceeded = PR_FALSE;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
