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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsIFileStream.h"
#include "nsFileSpec.h"
#include "nsCOMPtr.h"

#include "prerror.h"

#include "nsSegmentedBuffer.h"

#ifdef XP_MAC
#include "pprio.h" // To get PR_ImportFile
#else
#include "prio.h"
#endif

#ifdef XP_MAC
#include <Errors.h>
#include <iostream>
#endif

//========================================================================================
class FileImpl
    : public nsIRandomAccessStore
    , public nsIFileSpecOutputStream
    , public nsIFileSpecInputStream
    , public nsIOpenFile
//========================================================================================
{
    public:
                                        FileImpl(PRFileDesc* inDesc);
                                        FileImpl(const nsFileSpec& inFile, int nsprMode, PRIntn accessMode);

        virtual                         ~FileImpl();        
        // nsISupports interface
                                        NS_DECL_ISUPPORTS

        // nsIOpenFile interface
        NS_IMETHOD                      Open(const nsFileSpec& inFile, int nsprMode, PRIntn accessMode);
        NS_IMETHOD                      Close();
        NS_IMETHOD                      GetIsOpen(PRBool* outOpen);

        // nsIInputStream interface
        NS_IMETHOD                      Available(PRUint32 *aLength);
        NS_IMETHOD                      Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount);
        NS_IMETHOD                      ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval);
        NS_IMETHOD                      GetObserver(nsIInputStreamObserver * *aObserver);
        NS_IMETHOD                      SetObserver(nsIInputStreamObserver * aObserver); 

        // nsIOutputStream interface
        NS_IMETHOD                      Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount);
        NS_IMETHOD                      Flush();
        NS_IMETHOD                      WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval);
        NS_IMETHOD                      WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval);
        NS_IMETHOD                      GetNonBlocking(PRBool *aNonBlocking);
        NS_IMETHOD                      SetNonBlocking(PRBool aNonBlocking);
        NS_IMETHOD                      GetObserver(nsIOutputStreamObserver * *aObserver);
        NS_IMETHOD                      SetObserver(nsIOutputStreamObserver * aObserver);

        // nsIRandomAccessStore interface
        NS_DECL_NSISEEKABLESTREAM
        NS_IMETHOD                      GetAtEOF(PRBool* outAtEOF);
        NS_IMETHOD                      SetAtEOF(PRBool inAtEOF);

    protected:
    
        enum {
            kOuputBufferSegmentSize    = 4096,
            kOuputBufferMaxSize        = 4096
        };
        
        nsresult                        AllocateBuffers(PRUint32 segmentSize, PRUint32 maxSize);

        PRFileDesc*                     mFileDesc;
        int                             mNSPRMode;
        PRBool                          mFailed;
        PRBool                          mEOF;
        PRInt32                         mLength;

        PRBool                          mGotBuffers;
        nsSegmentedBuffer               mOutBuffer;
        char*                           mWriteCursor;
        char*                           mWriteLimit;

}; // class FileImpl

NS_IMPL_RELEASE(FileImpl)
NS_IMPL_ADDREF(FileImpl)

NS_IMPL_QUERY_HEAD(FileImpl)
  NS_IMPL_QUERY_BODY(nsIOpenFile)
  NS_IMPL_QUERY_BODY(nsISeekableStream)
  NS_IMPL_QUERY_BODY(nsIRandomAccessStore)
  NS_IMPL_QUERY_BODY(nsIOutputStream)
  NS_IMPL_QUERY_BODY(nsIInputStream)
  NS_IMPL_QUERY_BODY(nsIFileSpecInputStream)
  NS_IMPL_QUERY_BODY(nsIFileSpecOutputStream)
NS_IMPL_QUERY_TAIL(nsIOutputStream)


//----------------------------------------------------------------------------------------
FileImpl::FileImpl(PRFileDesc* inDesc)
//----------------------------------------------------------------------------------------
: mFileDesc(inDesc)
, mNSPRMode(0)
, mFailed(PR_FALSE)
, mEOF(PR_FALSE)
, mLength(-1)
, mGotBuffers(PR_FALSE)
{
    NS_INIT_REFCNT();
    
    mWriteCursor = nsnull;
    mWriteLimit  = nsnull;
}


//----------------------------------------------------------------------------------------
FileImpl::FileImpl(const nsFileSpec& inFile, int nsprMode, PRIntn accessMode)
//----------------------------------------------------------------------------------------
: mFileDesc(nsnull)
, mNSPRMode(-1)
, mEOF(PR_FALSE)
, mLength(-1)
, mGotBuffers(PR_FALSE)
{
    NS_INIT_REFCNT();

    mWriteCursor = nsnull;
    mWriteLimit  = nsnull;

    nsresult rv = Open(inFile, nsprMode, accessMode);		// this sets nsprMode
    
    if (NS_FAILED(rv))
    {
        mFailed = PR_TRUE;
#if DEBUG
        char *fileName = inFile.GetLeafName();
        printf("Opening file %s failed\n", fileName);
        nsCRT::free(fileName);
#endif
    }
    else
    {
        mFailed = PR_FALSE;
    }
}

//----------------------------------------------------------------------------------------
FileImpl::~FileImpl()
//----------------------------------------------------------------------------------------
{
    nsresult  rv = Close();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Close failed");
}


//----------------------------------------------------------------------------------------
NS_IMETHODIMP FileImpl::Open(
    const nsFileSpec& inFile,
    int nsprMode,
    PRIntn accessMode)
//----------------------------------------------------------------------------------------
{
    if (mFileDesc)
        if ((nsprMode & mNSPRMode) == nsprMode)
            return NS_OK;
        else
            return NS_FILE_RESULT(PR_ILLEGAL_ACCESS_ERROR);
        
    const int nspr_modes[]={
        PR_WRONLY | PR_CREATE_FILE,
        PR_WRONLY | PR_CREATE_FILE | PR_APPEND,
        PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
        PR_RDONLY,
        PR_RDONLY | PR_APPEND,
        PR_RDWR | PR_CREATE_FILE,
        PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE,
//      "wb",
//      "ab", 
//      "wb",
//      "rb",
//      "r+b",
//      "w+b",
        0 };
    const int* currentLegalMode = nspr_modes;
    while (*currentLegalMode && nsprMode != *currentLegalMode)
        ++currentLegalMode;
    if (!*currentLegalMode) 
        return NS_FILE_RESULT(PR_ILLEGAL_ACCESS_ERROR);

#ifdef XP_MAC
     // Use the file spec to open the file, because one path can be common to
     // several files on the Macintosh (you can have several volumes with the
     // same name, see).
    mFileDesc = 0;
    OSErr err = inFile.Error();
    if (err != noErr)
      if (err != fnfErr || !(nsprMode & PR_CREATE_FILE))
          return NS_FILE_RESULT(inFile.Error());
    err = noErr;
#if DEBUG
    const OSType kCreator = 'CWIE';
#else
    const OSType kCreator = 'MOSS';
#endif
    // Resolve the alias to the original file.
    nsFileSpec original = inFile;
    PRBool ignoredResult;
    original.ResolveSymlink(ignoredResult);
    const FSSpec& spec = original.operator const FSSpec&();
    if (nsprMode & PR_CREATE_FILE) {
        // In order to get the right file type/creator, do it with an nsILocalFileMac
        // Don't propagate any errors in doing this. If any error, just use FSpCreate.
        FSSpec nonConstSpec = spec;
        nsCOMPtr<nsILocalFileMac> macFile;
        nsresult res = NS_NewLocalFileWithFSSpec(&nonConstSpec, PR_FALSE, getter_AddRefs(macFile));
        if (NS_SUCCEEDED(res)) {
            nsCOMPtr<nsIFile> asFile(do_QueryInterface(macFile, &res));
            if (NS_SUCCEEDED(res)) {
                res = asFile->Create(nsIFile::NORMAL_FILE_TYPE, 0);
                if (res == NS_ERROR_FILE_ALREADY_EXISTS)
                    res = NS_OK;
            }
        }
        if (NS_FAILED(res))
            err = FSpCreate(&spec, kCreator, 'TEXT', 0);
    }

    if (err == dupFNErr)
        err = noErr;
    if (err != noErr)
        return NS_FILE_RESULT(err);
    
    SInt8 perm;
    if (nsprMode & PR_RDWR)
       perm = fsRdWrPerm;
    else if (nsprMode & PR_WRONLY)
       perm = fsWrPerm;
    else
       perm = fsRdPerm;

    short refnum;
    err = FSpOpenDF(&spec, perm, &refnum);

    if (err == noErr && (nsprMode & PR_TRUNCATE))
        err = SetEOF(refnum, 0);
    if (err == noErr && (nsprMode & PR_APPEND))
        err = SetFPos(refnum, fsFromLEOF, 0);
    if (err != noErr)
        return NS_FILE_RESULT(err);

    if ((mFileDesc = PR_ImportFile(refnum)) == 0)
        return NS_FILE_RESULT(PR_GetError());
#else
    //    Platforms other than Macintosh...
    //  Another bug in NSPR: Mac PR_Open assumes a unix style path, but Win PR_Open assumes
    //  a windows path.
    if ((mFileDesc = PR_Open((const char*)nsFileSpec(inFile), nsprMode, accessMode)) == 0)
        return NS_FILE_RESULT(PR_GetError());
#endif
     mNSPRMode = nsprMode;
     mLength = PR_Available(mFileDesc);
     return NS_OK;
} // FileImpl::Open


//----------------------------------------------------------------------------------------
NS_IMETHODIMP FileImpl::Available(PRUint32 *aLength)
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(aLength != nsnull, "null ptr");
    if (!aLength)
        return NS_ERROR_NULL_POINTER;
    if (mLength < 0)
        return NS_ERROR_UNEXPECTED;
    *aLength = mLength;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP FileImpl::GetIsOpen(PRBool* outOpen)
//----------------------------------------------------------------------------------------
{
    *outOpen = (mFileDesc != nsnull && !mFailed);
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP FileImpl::Seek(PRInt32 whence, PRInt32 offset)
//----------------------------------------------------------------------------------------
{
    if (mFileDesc==PR_STDIN || mFileDesc==PR_STDOUT || mFileDesc==PR_STDERR || !mFileDesc) 
       return NS_FILE_RESULT(PR_BAD_DESCRIPTOR_ERROR);
    mFailed = PR_FALSE; // reset on a seek.
    mEOF = PR_FALSE; // reset on a seek.
    
    // To avoid corruption, we flush during a seek. see bug number 18949
    Flush();

    PRInt32 position = PR_Seek(mFileDesc, 0, PR_SEEK_CUR);
    PRInt32 available = PR_Available(mFileDesc);
    PRInt32 fileSize = position + available;
    PRInt32 newPosition = 0;
    switch (whence)
    {
        case NS_SEEK_CUR: newPosition = position + offset; break;
        case NS_SEEK_SET: newPosition = offset; break;
        case NS_SEEK_END: newPosition = fileSize + offset; break;
    }
    if (newPosition < 0)
    {
        newPosition = 0;
        mFailed = PR_TRUE;
    }
    if (newPosition >= fileSize) // nb: not "else if".
    {
        newPosition = fileSize;
        mEOF = PR_TRUE;
    }
    if (PR_Seek(mFileDesc, newPosition, PR_SEEK_SET) < 0)
        mFailed = PR_TRUE;
    return NS_OK;
} // FileImpl::Seek


//----------------------------------------------------------------------------------------
NS_IMETHODIMP FileImpl::Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount)
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(aBuf != nsnull, "null ptr");
    if (!aBuf)
        return NS_ERROR_NULL_POINTER;
    NS_PRECONDITION(aReadCount != nsnull, "null ptr");
    if (!aReadCount)
        return NS_ERROR_NULL_POINTER;
    if (!mFileDesc)
        return NS_FILE_RESULT(PR_BAD_DESCRIPTOR_ERROR);
    if (mFailed)
        return NS_ERROR_FAILURE;
    PRInt32 bytesRead = PR_Read(mFileDesc, aBuf, aCount);
    if (bytesRead < 0)
    {
        *aReadCount = 0;
        mFailed = PR_TRUE;
        return NS_FILE_RESULT(PR_GetError());
    }
    else if (bytesRead == 0)
    {
        mEOF = PR_TRUE;
    }
    *aReadCount = bytesRead;
    return NS_OK;
}

NS_IMETHODIMP
FileImpl::ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval)
{
    NS_NOTREACHED("ReadSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileImpl::GetObserver(nsIInputStreamObserver * *aObserver)
{
    NS_NOTREACHED("GetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileImpl::SetObserver(nsIInputStreamObserver * aObserver)
{
    NS_NOTREACHED("SetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP FileImpl::Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount)
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(aBuf != nsnull, "null ptr");
    NS_PRECONDITION(aWriteCount != nsnull, "null ptr");
                    
    *aWriteCount = 0;

#ifdef XP_MAC
    // Calling PR_Write on stdout is sure suicide.
    if (mFileDesc == PR_STDOUT || mFileDesc == PR_STDERR)
    {
        std::cout.write(aBuf, aCount);
        *aWriteCount = aCount;
        return NS_OK;
    }
#endif
    if (!mFileDesc)
        return NS_FILE_RESULT(PR_BAD_DESCRIPTOR_ERROR);
    if (mFailed)
       return NS_ERROR_FAILURE;

    if (!mGotBuffers)
    {
        nsresult rv = AllocateBuffers(kOuputBufferSegmentSize, kOuputBufferMaxSize);
        if (NS_FAILED(rv))
          return rv;        // try to write non-buffered?
    }
    
    PRUint32 bufOffset = 0;
    PRUint32 currentWrite = 0;
    while (aCount > 0) 
    {
        if (mWriteCursor == nsnull || mWriteCursor == mWriteLimit)
        {
            char* seg = mOutBuffer.AppendNewSegment();
            if (seg == nsnull) 
            {
                // buffer is full, try again
                Flush();
                seg = mOutBuffer.AppendNewSegment();
                if (seg == nsnull)
                    return NS_ERROR_OUT_OF_MEMORY;
            }
            mWriteCursor = seg;
            mWriteLimit  = seg + mOutBuffer.GetSegmentSize();
        }
        
        // move
        currentWrite = mWriteLimit - mWriteCursor;
        
        if (aCount < currentWrite)
            currentWrite = aCount;

        memcpy(mWriteCursor, (aBuf + bufOffset), currentWrite);
        
        mWriteCursor += currentWrite;  
        
        aCount    -= currentWrite;
        bufOffset += currentWrite;
        *aWriteCount += currentWrite;
    }
    
    return NS_OK;
}

static NS_METHOD
nsWriteSegmentToFile(nsIInputStream* in,
                     void* closure,
                     const char* fromRawSegment,
                     PRUint32 toOffset,
                     PRUint32 count,
                     PRUint32 *writeCount)
{
    NS_NOTREACHED("nsWriteSegmentToFile");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
FileImpl::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *result)
{
    return inStr->ReadSegments(nsWriteSegmentToFile, nsnull, count, result);
}

NS_IMETHODIMP 
FileImpl::WriteSegments(nsReadSegmentFun reader, void * closure, 
                        PRUint32 count, PRUint32 *result)
{
    NS_NOTREACHED("WriteSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileImpl::GetNonBlocking(PRBool *aNonBlocking)
{
    NS_NOTREACHED("GetNonBlocking");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileImpl::SetNonBlocking(PRBool aNonBlocking)
{
    NS_NOTREACHED("SetNonBlocking");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileImpl::GetObserver(nsIOutputStreamObserver * *aObserver)
{
    NS_NOTREACHED("GetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileImpl::SetObserver(nsIOutputStreamObserver * aObserver)
{
    NS_NOTREACHED("SetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP FileImpl::Tell(PRUint32* outWhere)
//----------------------------------------------------------------------------------------
{
    if (mFileDesc==PR_STDIN || mFileDesc==PR_STDOUT || mFileDesc==PR_STDERR || !mFileDesc) 
       return NS_FILE_RESULT(PR_BAD_DESCRIPTOR_ERROR);
    *outWhere = PR_Seek(mFileDesc, 0, PR_SEEK_CUR);
    return NS_OK;
} // FileImpl::Tell

//----------------------------------------------------------------------------------------
NS_IMETHODIMP FileImpl::Close()
//----------------------------------------------------------------------------------------
{
    if ((mNSPRMode & PR_RDONLY) == 0)
        Flush();

    if (mFileDesc==PR_STDIN || mFileDesc==PR_STDOUT || mFileDesc==PR_STDERR || !mFileDesc) 
       return NS_OK;
    if (PR_Close(mFileDesc) == PR_SUCCESS)
        mFileDesc = 0;
    else
        return NS_FILE_RESULT(PR_GetError());
    return NS_OK;
} // FileImpl::close

//----------------------------------------------------------------------------------------
NS_IMETHODIMP FileImpl::Flush()
//----------------------------------------------------------------------------------------
{
#ifdef XP_MAC
    if (mFileDesc == PR_STDOUT || mFileDesc == PR_STDERR)
    {
        std::cout.flush();
        return NS_OK;
    }
#endif
    if (!mFileDesc) 
        return NS_FILE_RESULT(PR_BAD_DESCRIPTOR_ERROR);
    
    PRInt32 segCount = mOutBuffer.GetSegmentCount();
    PRUint32 segSize = mOutBuffer.GetSegmentSize();

    for (PRInt32 i = 0; i < segCount; i++) 
    {
        char* seg = mOutBuffer.GetSegment(i);

        // if it is the last buffer, it may not be completely full.  
        if(i == (segCount-1))
            segSize = (mWriteCursor - seg);

        PRInt32 bytesWrit = PR_Write(mFileDesc, seg, segSize);
        if (bytesWrit != (PRInt32)segSize)
        {
          mFailed = PR_TRUE;
          return NS_FILE_RESULT(PR_GetError());
        }
    }

    if (mGotBuffers)
        mOutBuffer.Empty();
    mWriteCursor = nsnull;
    mWriteLimit  = nsnull;

#ifdef XP_MAC
    // On unix, it seems to fail always.
    if (PR_Sync(mFileDesc) != PR_SUCCESS)
        mFailed = PR_TRUE;
#endif
                                                
    return NS_OK;
} // FileImpl::flush


//----------------------------------------------------------------------------------------
NS_IMETHODIMP FileImpl::GetAtEOF(PRBool* outAtEOF)
//----------------------------------------------------------------------------------------
{
  *outAtEOF = mEOF;
  return NS_OK;
}


//----------------------------------------------------------------------------------------
NS_IMETHODIMP FileImpl::SetAtEOF(PRBool inAtEOF)
//----------------------------------------------------------------------------------------
{
    mEOF = inAtEOF;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP FileImpl::SetEOF()
//----------------------------------------------------------------------------------------
{
    NS_NOTYETIMPLEMENTED("FileImpl::SetEOF");
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------------------------
nsresult FileImpl::AllocateBuffers(PRUint32 segmentSize, PRUint32 maxBufSize)
//----------------------------------------------------------------------------------------
{
    nsresult rv = mOutBuffer.Init(segmentSize, maxBufSize);
    if (NS_SUCCEEDED(rv))
      mGotBuffers = PR_TRUE;

    return rv;
}


//----------------------------------------------------------------------------------------
NS_COM nsresult NS_NewTypicalInputFileStream(
    nsISupports** aResult,
    const nsFileSpec& inFile
    /*Default nsprMode == PR_RDONLY*/
    /*Default accessmode = 0666 (octal)*/)
// Factory method to get an nsInputStream from a file, using most common options
//----------------------------------------------------------------------------------------
{
  // This QueryInterface was needed because NS_NewIOFileStream
  // does a cast from (void *) to (nsISupports *) thus causing a 
  // vtable problem on Windows, where we really didn't have the proper pointer
  // to an nsIInputStream, this ensures that we do 
#if 1
    nsISupports    * supports;
    nsIInputStream * inStr;

    nsresult rv = NS_NewIOFileStream(&supports, inFile, PR_RDONLY, 0666);

    *aResult = nsnull;
    if (NS_SUCCEEDED(rv)) {
      if (NS_SUCCEEDED(supports->QueryInterface(NS_GET_IID(nsIInputStream), (void**)&inStr))) {
        *aResult = inStr;
      }
      NS_RELEASE(supports);
    }
    return rv;
#else
    return NS_NewIOFileStream(aResult, inFile, PR_RDONLY, 0666);
#endif
}

//----------------------------------------------------------------------------------------
NS_COM nsresult NS_NewOutputConsoleStream(
    nsISupports** aResult)
// Factory method to get an nsOutputStream to the console.
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    FileImpl* stream = new FileImpl(PR_STDOUT);
    if (! stream)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(stream);
    *aResult = (nsISupports*)(void*)stream;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_COM nsresult NS_NewTypicalOutputFileStream(
    nsISupports** aResult,
    const nsFileSpec& inFile
    /*default nsprMode= (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE)*/
    /*Default accessMode= 0666 (octal)*/)
// Factory method to get an nsOutputStream to a file - most common case.
//----------------------------------------------------------------------------------------
{
  // This QueryInterface was needed because NS_NewIOFileStream
  // does a cast from (void *) to (nsISupports *) thus causing a 
  // vtable problem on Windows, where we really didn't have the proper pointer
  // to an nsIOutputStream, this ensures that we do 
#if 1
/*    nsISupports     * supports;
    nsIOutputStream * outStr;

    nsresult rv = NS_NewIOFileStream(
        &supports,
        inFile,
        (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE),
        0666);

    *aResult = nsnull;
    if (NS_SUCCEEDED(rv)) { 
      if (NS_SUCCEEDED(supports->QueryInterface(NS_GET_IID(nsIOutputStream), (void**)&outStr))) {
        *aResult = outStr;
      }
      NS_RELEASE(supports);
    }
    return rv;
    */

    nsCOMPtr<nsISupports> supports;
    nsIOutputStream * outStr;

    nsresult rv = NS_NewIOFileStream(
        getter_AddRefs(supports),
        inFile,
        (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE),
        0666);

    *aResult = nsnull;
    if (NS_SUCCEEDED(rv)) { 
      if (NS_SUCCEEDED(supports->QueryInterface(NS_GET_IID(nsIOutputStream), (void**)&outStr))) {
        *aResult = outStr;
      }
    }
    return rv;
#else
    return NS_NewIOFileStream(
        aResult,
        inFile,
        (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE),
        0666);
#endif
}

//----------------------------------------------------------------------------------------
NS_COM nsresult NS_NewIOFileStream(
    nsISupports** aResult,
    const nsFileSpec& inFile,
    PRInt32 nsprMode /*default = (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE)*/,
    PRInt32 accessMode /*Default = 0666 (octal)*/)
    // Factory method to get an object that implements both nsIInputStream
    // and nsIOutputStream, associated with a file.
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (!aResult)
        return NS_ERROR_NULL_POINTER;

    FileImpl* stream = new FileImpl(inFile, nsprMode, accessMode);
    if (! stream)
        return NS_ERROR_OUT_OF_MEMORY;
    else 
    {
        PRBool isOpened = PR_FALSE;
        stream->GetIsOpen(&isOpened);
        if (!isOpened)
        {
            delete stream;
            return NS_ERROR_FAILURE;
        }
    }

    NS_ADDREF(stream);
    *aResult = (nsISupports*)(void*)stream;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_COM nsresult NS_NewTypicalIOFileStream(
    nsISupports** aResult,
    const nsFileSpec& inFile
    /*default nsprMode= (PR_RDWR | PR_CREATE_FILE)*/
    /*Default accessMode= 0666 (octal)*/)
    // Factory method to get an object that implements both nsIInputStream
    // and nsIOutputStream, associated with a single file.
//----------------------------------------------------------------------------------------
{
    return NS_NewIOFileStream(
        aResult,
        inFile,
        (PR_RDWR | PR_CREATE_FILE),
        0666);
}
