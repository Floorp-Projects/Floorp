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
 *   mcmullen@netscape.com (original author)
 *   warren@netscape.com
 *   alecf@netscape.com
 *   scc@mozilla.org
 *   david.gardiner@unisa.edu.au
 *   fur@netscape.com
 *   norris@netscape.com
 *   pinkerton@netscape.com
 *   davidm@netscape.com
 *   sfraser@netscape.com
 *   darin@netscape.com
 *   bzbarsky@mit.edu
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

/**
 * Based on original code from nsIStringStream.cpp
 */

#include "nsStringStream.h"

#include "prerror.h"
#include "plstr.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsISeekableStream.h"
#include "nsInt64.h"

#define NS_FILE_RESULT(x) ns_file_convert_result((PRInt32)x)
#define NS_FILE_FAILURE NS_FILE_RESULT(-1)

static nsresult ns_file_convert_result(PRInt32 nativeErr)
{
    return nativeErr ?
        NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES,((nativeErr)&0xFFFF))
        : NS_OK;
}
 
//-----------------------------------------------------------------------------
// nsIStringInputStream implementation
//-----------------------------------------------------------------------------

class nsStringInputStream : public nsIStringInputStream
                          , public nsIRandomAccessStore
                            
{
public:
    nsStringInputStream()
        : mOffset(0)
        , mLastResult(NS_OK)
        , mEOF(PR_FALSE)
        , mOwned(PR_FALSE)
        , mConstString(nsnull)
        , mLength(0)
    {}
    
private:
    ~nsStringInputStream()
    {
        if (mOwned)
            nsMemory::Free((char*)mConstString);
    }

public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSISTRINGINPUTSTREAM
    NS_DECL_NSIINPUTSTREAM

    // nsIRandomAccessStore interface
    NS_IMETHOD GetAtEOF(PRBool* outAtEOF);
    NS_IMETHOD SetAtEOF(PRBool inAtEOF);

    NS_DECL_NSISEEKABLESTREAM
    
protected:
    PRInt32 LengthRemaining() const
    {
        return mLength - mOffset;
    }

    void Clear()
    {
        NS_ASSERTION(mConstString || !mOwned,
                     "Can't have mOwned set and have a null string!");
        if (mOwned)
            nsMemory::Free((char*)mConstString);

        // We're about to get a new string; clear the members that
        // would no longer have valid values.
        mOffset = 0;
        mLastResult = NS_OK;
        mEOF = PR_FALSE;
    }

    PRInt32                        mOffset;
    nsresult                       mLastResult;
    PRPackedBool                   mEOF;
    PRPackedBool                   mOwned;
    const char*                    mConstString;
    PRInt32                        mLength;
};

NS_IMPL_THREADSAFE_ISUPPORTS4(nsStringInputStream,
                              nsIStringInputStream,
                              nsIInputStream,
                              nsIRandomAccessStore,
                              nsISeekableStream)

/////////
// nsIStringInputStream implementation
/////////
NS_IMETHODIMP
nsStringInputStream::SetData(const char *data, PRInt32 dataLen)
{
    if (dataLen < 0)
        dataLen = strlen(data);

    return AdoptData(nsCRT::strndup(data, dataLen), dataLen);
}

NS_IMETHODIMP
nsStringInputStream::AdoptData(char *data, PRInt32 dataLen)
{
    NS_ENSURE_ARG_POINTER(data);

    if (dataLen < 0)
        dataLen = strlen(data);

    Clear();
    
    mConstString = (const char *) data;
    mLength = dataLen;
    mOwned = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::ShareData(const char *data, PRInt32 dataLen)
{
    NS_ENSURE_ARG_POINTER(data);

    if (dataLen < 0)
        dataLen = strlen(data);

    Clear();
    
    mConstString = data;
    mLength = dataLen;
    mOwned = PR_FALSE;
    return NS_OK;
}

/////////
// nsIInputStream implementation
/////////
NS_IMETHODIMP nsStringInputStream::Close()
{
    return NS_OK;
}
    
NS_IMETHODIMP nsStringInputStream::Available(PRUint32 *aLength)
{
    NS_PRECONDITION(aLength != nsnull, "null ptr");
    if (!aLength)
        return NS_ERROR_NULL_POINTER;
    *aLength = LengthRemaining();
    return NS_OK;
}

NS_IMETHODIMP nsStringInputStream::Read(char* aBuf, PRUint32 aCount,
                                        PRUint32 *aReadCount)
{
    NS_PRECONDITION(aBuf != nsnull, "null ptr");
    if (!aBuf)
        return NS_ERROR_NULL_POINTER;
    NS_PRECONDITION(aReadCount != nsnull, "null ptr");
    if (!aReadCount)
        return NS_ERROR_NULL_POINTER;
    if (NS_FAILED(mLastResult))
        return mLastResult;

    PRInt32 bytesRead;
    PRInt32 maxCount = mLength - mOffset;
    if ((PRInt32)aCount > maxCount)
        bytesRead = maxCount;
    else
        bytesRead = aCount;
  
    memcpy(aBuf, mConstString + mOffset, bytesRead);
    mOffset += bytesRead;

    *aReadCount = bytesRead;
    if (bytesRead < (PRInt32)aCount)
        SetAtEOF(PR_TRUE);
    return NS_OK;
}


NS_IMETHODIMP
nsStringInputStream::ReadSegments(nsWriteSegmentFun writer, void * closure,
                                  PRUint32 aCount, PRUint32 * result)
{
    nsresult rv;
    PRInt32 maxCount = mLength - mOffset;
    if (maxCount == 0) {
        *result = 0;
        return NS_OK;
    }
    if ((PRInt32)aCount > maxCount)
        aCount = maxCount;
    rv = writer(this, closure, mConstString + mOffset, 
                0, aCount, result);
    if (NS_SUCCEEDED(rv))
        mOffset += *result;
    // errors returned from the writer end here!
    return NS_OK;
}
    
NS_IMETHODIMP
nsStringInputStream::IsNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = PR_TRUE;
    return NS_OK;
}


/////////
// nsISeekableStream implementation
/////////
NS_IMETHODIMP nsStringInputStream::Seek(PRInt32 whence, PRInt64 offset)
{
    mLastResult = NS_OK; // reset on a seek.
    const nsInt64 maxUint32 = PR_UINT32_MAX;
    nsInt64 offset64(offset);
    PRInt32 offset32;
    LL_L2I(offset32, offset);

    NS_ASSERTION(maxUint32 > offset64, "string streams only support 32 bit offsets");
    mEOF = PR_FALSE; // reset on a seek.
    PRInt32 fileSize = LengthRemaining();
    PRInt32 newPosition=-1;
    switch (whence)
    {
        case NS_SEEK_CUR: newPosition = mOffset + offset32; break;
        case NS_SEEK_SET: newPosition = offset32; break;
        case NS_SEEK_END: newPosition = fileSize + offset32; break;
    }
    if (newPosition < 0)
    {
        newPosition = 0;
        mLastResult = NS_FILE_RESULT(PR_FILE_SEEK_ERROR);
    }
    if (newPosition >= fileSize)
    {
        newPosition = fileSize;
        mEOF = PR_TRUE;
    }
    mOffset = newPosition;
    return NS_OK;
}


NS_IMETHODIMP nsStringInputStream::Tell(PRInt64* outWhere)
{
    *outWhere = mOffset;
    return NS_OK;
}

NS_IMETHODIMP nsStringInputStream::SetEOF()
{
    NS_NOTYETIMPLEMENTED("nsStringInputStream::SetEOF");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/////////
// nsIRandomAccessStore implementation
/////////
NS_IMETHODIMP nsStringInputStream::GetAtEOF(PRBool* outAtEOF)
{
    *outAtEOF = mEOF;
    return NS_OK;
}

NS_IMETHODIMP nsStringInputStream::SetAtEOF(PRBool inAtEOF)
{
    mEOF = inAtEOF;
    return NS_OK;
}

// Factory method to get an nsInputStream from an nsAString.  Result will
// implement nsIStringInputStream and nsIRandomAccessStore
extern "C" NS_COM nsresult
NS_NewStringInputStream(nsIInputStream** aStreamResult,
                        const nsAString& aStringToRead)
{
    NS_PRECONDITION(aStreamResult, "null out ptr");

    char* data = ToNewCString(aStringToRead);
    if (!data)
        return NS_ERROR_OUT_OF_MEMORY;

    nsStringInputStream* stream = new nsStringInputStream();
    if (! stream) {
        nsMemory::Free(data);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(stream);

    nsresult rv = stream->AdoptData(data, aStringToRead.Length());
    if (NS_FAILED(rv)) {
        nsMemory::Free(data);
        NS_RELEASE(stream);
        return rv;
    }
    
    *aStreamResult = stream;
    return NS_OK;
}

// Factory method to get an nsInputStream from an nsACString.  Result will
// implement nsIStringInputStream and nsIRandomAccessStore
extern "C" NS_COM nsresult
NS_NewCStringInputStream(nsIInputStream** aStreamResult,
                         const nsACString& aStringToRead)
{
    NS_PRECONDITION(aStreamResult, "null out ptr");

    char* data = ToNewCString(aStringToRead);
    if (!data)
        return NS_ERROR_OUT_OF_MEMORY;

    nsStringInputStream* stream = new nsStringInputStream();
    if (! stream) {
        nsMemory::Free(data);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(stream);

    nsresult rv = stream->AdoptData(data, aStringToRead.Length());
    if (NS_FAILED(rv)) {
        nsMemory::Free(data);
        NS_RELEASE(stream);
        return rv;
    }
    
    *aStreamResult = stream;
    return NS_OK;
}

// Factory method to get an nsInputStream from a C string.  Result will
// implement nsIStringInputStream and nsIRandomAccessStore
extern "C" NS_COM nsresult
NS_NewCharInputStream(nsIInputStream** aStreamResult,
                      const char* aStringToRead)
{
    NS_PRECONDITION(aStreamResult, "null out ptr");

    nsStringInputStream* stream = new nsStringInputStream();
    if (! stream)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(stream);

    nsresult rv = stream->ShareData(aStringToRead, -1);
    
    if (NS_FAILED(rv)) {
        NS_RELEASE(stream);
        return rv;
    }
    
    *aStreamResult = stream;
    return NS_OK;
}

// Factory method to get an nsInputStream from a byte array.  Result will
// implement nsIStringInputStream and nsIRandomAccessStore
extern "C" NS_COM nsresult
NS_NewByteInputStream(nsIInputStream** aStreamResult,
                      const char* aStringToRead,
                      PRInt32 aLength)
{
    NS_PRECONDITION(aStreamResult, "null out ptr");

    nsStringInputStream* stream = new nsStringInputStream();
    if (! stream)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(stream);

    nsresult rv = stream->ShareData(aStringToRead, aLength);
    
    if (NS_FAILED(rv)) {
        NS_RELEASE(stream);
        return rv;
    }
    
    *aStreamResult = stream;
    return NS_OK;
}

// factory method for constructing a nsStringInputStream object
NS_METHOD
nsStringInputStreamConstructor(nsISupports *outer, REFNSIID iid, void **result)
{
    *result = nsnull;

    if (outer)
        return NS_ERROR_NO_AGGREGATION;

    nsStringInputStream *inst;
    NS_NEWXPCOM(inst, nsStringInputStream);
    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(inst);
    nsresult rv = inst->QueryInterface(iid, result);
    NS_RELEASE(inst);

    return rv;
}
