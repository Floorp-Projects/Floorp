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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

/**
 * This file contains implementations of the nsIBinaryInputStream and
 * nsIBinaryOutputStream interfaces.  Together, these interfaces allows reading
 * and writing of primitive data types (integers, floating-point values,
 * booleans, etc.) to a stream in a binary, untagged, fixed-endianness format.
 * This might be used, for example, to implement network protocols or to
 * produce architecture-neutral binary disk files, i.e. ones that can be read
 * and written by both big-endian and little-endian platforms.  Output is
 * written in big-endian order (high-order byte first), as this is traditional
 * network order.
 *
 * @See nsIBinaryInputStream
 * @See nsIBinaryOutputStream
 */
#include <string.h>
#include "nsBinaryStream.h"
#include "nsCRT.h"
#include "nsIStreamBufferAccess.h"
#include "nsMemory.h"
#include "prlong.h"

nsBinaryOutputStream::nsBinaryOutputStream(nsIOutputStream* aStream)
  : mOutputStream(aStream),
    mBufferAccess(do_QueryInterface(aStream))
{
    NS_INIT_ISUPPORTS();
}

NS_IMPL_ISUPPORTS1(nsBinaryOutputStream, nsIBinaryOutputStream)

NS_IMETHODIMP
nsBinaryOutputStream::Flush() { return mOutputStream->Flush(); }

NS_IMETHODIMP
nsBinaryOutputStream::Close() { return mOutputStream->Close(); }

NS_IMETHODIMP
nsBinaryOutputStream::Write(const char *aBuf, PRUint32 aCount, PRUint32 *aActualBytes)
{
    return mOutputStream->Write(aBuf, aCount, aActualBytes);
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
    NS_NOTREACHED("WriteFrom");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval)
{
    NS_NOTREACHED("WriteSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBinaryOutputStream::IsNonBlocking(PRBool *aNonBlocking)
{
    return mOutputStream->IsNonBlocking(aNonBlocking);
}

nsresult
nsBinaryOutputStream::WriteFully(const char *aBuf, PRUint32 aCount)
{
    nsresult rv;
    PRUint32 bytesWritten;

    rv = mOutputStream->Write(aBuf, aCount, &bytesWritten);
    if (NS_FAILED(rv)) return rv;
    if (bytesWritten != aCount)
        return NS_ERROR_FAILURE;
    return NS_OK;
}

NS_IMETHODIMP
nsBinaryOutputStream::SetOutputStream(nsIOutputStream *aOutputStream)
{
    NS_ENSURE_ARG_POINTER(aOutputStream);
    mOutputStream = aOutputStream;
    return NS_OK;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteBoolean(PRBool aBoolean)
{
    return Write8(aBoolean);
}

NS_IMETHODIMP
nsBinaryOutputStream::Write8(PRUint8 aByte)
{
    return WriteFully((const char*)&aByte, sizeof aByte);
}

NS_IMETHODIMP
nsBinaryOutputStream::Write16(PRUint16 a16)
{
    a16 = NS_SWAP16(a16);
    return WriteFully((const char*)&a16, sizeof a16);
}

NS_IMETHODIMP
nsBinaryOutputStream::Write32(PRUint32 a32)
{
    a32 = NS_SWAP32(a32);
    return WriteFully((const char*)&a32, sizeof a32);
}

NS_IMETHODIMP
nsBinaryOutputStream::Write64(PRUint64 a64)
{
    nsresult rv;
    PRUint32 bytesWritten;

    a64 = NS_SWAP64(a64);
    rv = Write(NS_REINTERPRET_CAST(char*, &a64), sizeof a64, &bytesWritten);
    if (NS_FAILED(rv)) return rv;
    if (bytesWritten != sizeof a64)
        return NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteFloat(float aFloat)
{
    NS_ASSERTION(sizeof(float) == sizeof (PRUint32),
                 "False assumption about sizeof(float)");
    return Write32(*NS_REINTERPRET_CAST(PRUint32*, &aFloat));
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteDouble(double aDouble)
{
    NS_ASSERTION(sizeof(double) == sizeof(PRUint64),
                 "False assumption about sizeof(double)");
    return Write64(*NS_REINTERPRET_CAST(PRUint64*, &aDouble));
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteStringZ(const char *aString)
{
    PRUint32 length;
    nsresult rv;

    length = strlen(aString);
    rv = Write32(length);
    if (NS_FAILED(rv)) return rv;
    return WriteFully(aString, length);
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteWStringZ(const PRUnichar* aString)
{
    PRUint32 length, byteCount;
    nsresult rv;

    length = nsCRT::strlen(aString);
    rv = Write32(length);
    if (NS_FAILED(rv)) return rv;

    if (length == 0)
        return NS_OK;
    byteCount = length * sizeof(PRUnichar);

#ifdef IS_BIG_ENDIAN
    rv = WriteBytes(NS_REINTERPRET_CAST(const char*, aString), byteCount);
#else
    PRUnichar *copy, temp[64];
    if (length <= 64) {
        copy = temp;
    } else {
        copy = NS_REINTERPRET_CAST(PRUnichar*, nsMemory::Alloc(byteCount));
        if (!copy)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    for (PRUint32 i = 0; i < length; i++)
        copy[i] = NS_SWAP16(aString[i]);
    rv = WriteBytes(NS_REINTERPRET_CAST(const char*, copy), byteCount);
    if (copy != temp)
        nsMemory::Free(copy);
#endif

    return rv;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteUtf8Z(const PRUnichar* aString)
{
    NS_NOTREACHED("WriteUtf8Z");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteBytes(const char *aString, PRUint32 aLength)
{
    nsresult rv;
    PRUint32 bytesWritten;

    rv = Write(aString, aLength, &bytesWritten);
    if (NS_FAILED(rv)) return rv;
    if (bytesWritten != aLength)
        return NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteObject(nsISupports* aObject, PRBool aIsStrongRef)
{
    NS_NOTREACHED("WriteObject");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteSingleRefObject(nsISupports* aObject)
{
    NS_NOTREACHED("WriteSingleRefObject");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteCompoundObject(nsISupports* aObject,
                                          const nsIID& aIID,
                                          PRBool aIsStrongRef)
{
    NS_NOTREACHED("WriteCompoundObject");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteID(const nsIID& aIID)
{
    NS_NOTREACHED("WriteID");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(char*)
nsBinaryOutputStream::GetBuffer(PRUint32 aLength, PRUint32 aAlignMask)
{
    if (mBufferAccess)
        return mBufferAccess->GetBuffer(aLength, aAlignMask);
    return nsnull;
}

NS_IMETHODIMP_(void)
nsBinaryOutputStream::PutBuffer(char* aBuffer, PRUint32 aLength)
{
    if (mBufferAccess)
        mBufferAccess->PutBuffer(aBuffer, aLength);
}

nsBinaryInputStream::nsBinaryInputStream(nsIInputStream* aStream)
  : mInputStream(aStream),
    mBufferAccess(do_QueryInterface(aStream))
{
    NS_INIT_ISUPPORTS();
}

NS_IMPL_ISUPPORTS1(nsBinaryInputStream, nsIBinaryInputStream)

NS_IMETHODIMP
nsBinaryInputStream::Available(PRUint32* aResult)
{
    return mInputStream->Available(aResult);
}

NS_IMETHODIMP
nsBinaryInputStream::Read(char* aBuffer, PRUint32 aCount, PRUint32 *aNumRead)
{
    return mInputStream->Read(aBuffer, aCount, aNumRead);
}


// when forwarding ReadSegments to mInputStream, we need to make sure
// 'this' is being passed to the writer each time. To do this, we need
// a thunking function which keeps the real input stream around.

// the closure wrapper
struct ReadSegmentsClosure {
    nsIInputStream* mRealInputStream;
    void* mRealClosure;
    nsWriteSegmentFun mRealWriter;
};

// the thunking function
static NS_METHOD
ReadSegmentForwardingThunk(nsIInputStream* aStream,
                           void *aClosure,
                           const char* aFromSegment,
                           PRUint32 aToOffset,
                           PRUint32 aCount,
                           PRUint32 *aWriteCount)
{
    ReadSegmentsClosure* thunkClosure =
        NS_REINTERPRET_CAST(ReadSegmentsClosure*, aClosure);

    return thunkClosure->mRealWriter(thunkClosure->mRealInputStream,
                                     thunkClosure->mRealClosure,
                                     aFromSegment, aToOffset,
                                     aCount, aWriteCount);
}


NS_IMETHODIMP
nsBinaryInputStream::ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval)
{
    ReadSegmentsClosure thunkClosure = { this, closure, writer };
    
    return mInputStream->ReadSegments(ReadSegmentForwardingThunk, &thunkClosure, count, _retval);
}

NS_IMETHODIMP
nsBinaryInputStream::IsNonBlocking(PRBool *aNonBlocking)
{
    return mInputStream->IsNonBlocking(aNonBlocking);
}

NS_IMETHODIMP
nsBinaryInputStream::Close() { return mInputStream->Close(); }

NS_IMETHODIMP
nsBinaryInputStream::SetInputStream(nsIInputStream *aInputStream)
{
    NS_ENSURE_ARG_POINTER(aInputStream);
    mInputStream = aInputStream;
    return NS_OK;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadBoolean(PRBool* aBoolean)
{
    PRUint8 byteResult;
    nsresult rv = Read8(&byteResult);
    *aBoolean = byteResult;
    return rv;
}

NS_IMETHODIMP
nsBinaryInputStream::Read8(PRUint8* aByte)
{
    nsresult rv;
    PRUint32 bytesRead;

    rv = Read(NS_REINTERPRET_CAST(char*, aByte), sizeof(*aByte), &bytesRead);
    if (NS_FAILED(rv)) return rv;
    if (bytesRead != 1)
        return NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP
nsBinaryInputStream::Read16(PRUint16* a16)
{
    nsresult rv;
    PRUint32 bytesRead;

    rv = Read(NS_REINTERPRET_CAST(char*, a16), sizeof *a16, &bytesRead);
    if (NS_FAILED(rv)) return rv;
    if (bytesRead != sizeof *a16)
        return NS_ERROR_FAILURE;
    *a16 = NS_SWAP16(*a16);
    return rv;
}

NS_IMETHODIMP
nsBinaryInputStream::Read32(PRUint32* a32)
{
    nsresult rv;
    PRUint32 bytesRead;

    rv = Read(NS_REINTERPRET_CAST(char*, a32), sizeof *a32, &bytesRead);
    if (NS_FAILED(rv)) return rv;
    if (bytesRead != sizeof *a32)
        return NS_ERROR_FAILURE;
    *a32 = NS_SWAP32(*a32);
    return rv;
}

NS_IMETHODIMP
nsBinaryInputStream::Read64(PRUint64* a64)
{
    nsresult rv;
    PRUint32 bytesRead;

    rv = Read(NS_REINTERPRET_CAST(char*, a64), sizeof *a64, &bytesRead);
    if (NS_FAILED(rv)) return rv;
    if (bytesRead != sizeof *a64)
        return NS_ERROR_FAILURE;
    *a64 = NS_SWAP64(*a64);
    return rv;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadFloat(float* aFloat)
{
    NS_ASSERTION(sizeof(float) == sizeof (PRUint32),
                 "False assumption about sizeof(float)");
    return Read32(NS_REINTERPRET_CAST(PRUint32*, aFloat));
}

NS_IMETHODIMP
nsBinaryInputStream::ReadDouble(double* aDouble)
{
    NS_ASSERTION(sizeof(double) == sizeof(PRUint64),
                 "False assumption about sizeof(double)");
    return Read64(NS_REINTERPRET_CAST(PRUint64*, aDouble));
}

static NS_METHOD
WriteSegmentToCString(nsIInputStream* aStream,
                      void *aClosure,
                      const char* aFromSegment,
                      PRUint32 aToOffset,
                      PRUint32 aCount,
                      PRUint32 *aWriteCount)
{
    nsACString* outString = NS_STATIC_CAST(nsACString*,aClosure);

    outString->Append(aFromSegment, aCount);

    *aWriteCount = aCount;
    
    return NS_OK;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadCString(nsACString& aString)
{
    nsresult rv;
    PRUint32 length, bytesRead;

    rv = Read32(&length);
    if (NS_FAILED(rv)) return rv;

    aString.Truncate();
    rv = ReadSegments(WriteSegmentToCString, &aString, length, &bytesRead);
    if (NS_FAILED(rv)) return rv;
    
    if (bytesRead != length)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

// same version of the above, but with correct casting and endian swapping
static NS_METHOD
WriteSegmentToString(nsIInputStream* aStream,
                     void *aClosure,
                     const char* aFromSegment,
                     PRUint32 aToOffset,
                     PRUint32 aCount,
                     PRUint32 *aWriteCount)
{
    nsAString* outString = NS_STATIC_CAST(nsAString*,aClosure);
    const PRUnichar *unicodeSegment =
        NS_REINTERPRET_CAST(const PRUnichar*, aFromSegment);
    PRUint32 segmentLength = aCount / sizeof(PRUnichar);

    // this sucks. we have to swap every 2 bytes on some machines
#ifdef IS_LITTLE_ENDIAN
    for (PRUint32 i = 0; i < segmentLength; i++)
        outString->Append(PRUnichar(NS_SWAP16(unicodeSegment[i])));
#else    
    outString->Append(unicodeSegment, segmentLength);
#endif

    *aWriteCount = aCount;
    return NS_OK;
}


NS_IMETHODIMP
nsBinaryInputStream::ReadString(nsAString& aString)
{
    nsresult rv;
    PRUint32 length, bytesRead;

    rv = Read32(&length);
    if (NS_FAILED(rv)) return rv;

    aString.Truncate();
    rv = ReadSegments(WriteSegmentToString, &aString, length*sizeof(PRUnichar), &bytesRead);
    if (NS_FAILED(rv)) return rv;
    
    if (bytesRead != length*sizeof(PRUnichar))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadBytes(char* *aString, PRUint32 aLength)
{
    nsresult rv;
    PRUint32 bytesRead;
    char* s;

    s = NS_REINTERPRET_CAST(char*, nsMemory::Alloc(aLength));
    if (!s)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = Read(s, aLength, &bytesRead);
    if (NS_FAILED(rv)) return rv;
    if (bytesRead != aLength) {
        nsMemory::Free(s);
        return NS_ERROR_FAILURE;
    }

    *aString = s;
    return NS_OK;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadObject(PRBool aIsStrongRef, nsISupports* *aObject)
{
    NS_NOTREACHED("ReadObject");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadID(nsID *aResult)
{
    NS_NOTREACHED("ReadID");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(char*)
nsBinaryInputStream::GetBuffer(PRUint32 aLength, PRUint32 aAlignMask)
{
    if (mBufferAccess)
        return mBufferAccess->GetBuffer(aLength, aAlignMask);
    return nsnull;
}

NS_IMETHODIMP_(void)
nsBinaryInputStream::PutBuffer(char* aBuffer, PRUint32 aLength)
{
    if (mBufferAccess)
        mBufferAccess->PutBuffer(aBuffer, aLength);
}

