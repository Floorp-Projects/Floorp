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
#include "nsBinaryStream.h"
#include "nsMemory.h"
#include <string.h>

// Swap macros, used to convert to/from canonical (big-endian) format
#ifdef IS_LITTLE_ENDIAN
#    define SWAP16(x) ((((x) & 0xff) << 8) | (((x) >> 8) & 0xff))
#    define SWAP32(x) ((SWAP16((x) & 0xffff) << 16) | (SWAP16((x) >> 16)))
#else
#    ifndef IS_BIG_ENDIAN
#        error "Unknown endianness"
#    endif
#    define SWAP16(x) (x)
#    define SWAP32(x) (x)
#endif

nsBinaryOutputStream::nsBinaryOutputStream(nsIOutputStream* aStream): mOutputStream(aStream)
{
    NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS(nsBinaryOutputStream, NS_GET_IID(nsIBinaryOutputStream))

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
nsBinaryOutputStream::GetNonBlocking(PRBool *aNonBlocking)
{
    NS_NOTREACHED("GetNonBlocking");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsBinaryOutputStream::SetNonBlocking(PRBool aNonBlocking)
{
    NS_NOTREACHED("SetNonBlocking");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsBinaryOutputStream::GetObserver(nsIOutputStreamObserver * *aObserver)
{
    NS_NOTREACHED("GetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsBinaryOutputStream::SetObserver(nsIOutputStreamObserver * aObserver)
{
    NS_NOTREACHED("SetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsBinaryOutputStream::WriteFully(const char *aBuf, PRUint32 aCount)
{
    nsresult rv;
    PRUint32 actualBytesWritten;
    rv = mOutputStream->Write(aBuf, aCount, &actualBytesWritten);
    if (NS_FAILED(rv)) return rv;
    if (actualBytesWritten != aCount)
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
    a16 = SWAP16(a16);
    return WriteFully((const char*)&a16, sizeof a16);
}

NS_IMETHODIMP
nsBinaryOutputStream::Write32(PRUint32 a32)
{
    a32 = SWAP32(a32);
    return WriteFully((const char*)&a32, sizeof a32);
}

NS_IMETHODIMP
nsBinaryOutputStream::Write64(PRUint64 a64)
{
    nsresult rv;
    PRUint32* raw32 = (PRUint32*)&a64;

#ifdef IS_BIG_ENDIAN
    rv = Write32(raw32[0]);
    if (NS_FAILED(rv)) return rv;
    return Write32(raw32[1]);
#else
    rv = Write32(raw32[1]);
    if (NS_FAILED(rv)) return rv;
    return Write32(raw32[0]);
#endif
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
    return Write64(*NS_REINTERPRET_CAST(PRUint64*,&aDouble));
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteStringZ(const char *aString)
{
    return WriteFully(aString, strlen(aString) + 1);
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteWStringZ(const PRUnichar* aString)
{
    NS_NOTREACHED("WriteWStringZ");
    return NS_ERROR_NOT_IMPLEMENTED;
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
    if (bytesWritten != aLength)
        return NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteString(nsString* aString)
{
    NS_NOTREACHED("WriteString");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsBinaryInputStream::nsBinaryInputStream(nsIInputStream* aStream): mInputStream(aStream) { NS_INIT_REFCNT(); }

NS_IMPL_ISUPPORTS(nsBinaryInputStream, NS_GET_IID(nsIBinaryInputStream))

NS_IMETHODIMP
nsBinaryInputStream::Available(PRUint32* aResult) { return mInputStream->Available(aResult); }

NS_IMETHODIMP
nsBinaryInputStream::Read(char* aBuffer, PRUint32 aCount, PRUint32 *aNumRead)
{
    return mInputStream->Read(aBuffer, aCount, aNumRead);
}

NS_IMETHODIMP 
nsBinaryInputStream::ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval)
{
    NS_NOTREACHED("ReadSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsBinaryInputStream::GetNonBlocking(PRBool *aNonBlocking)
{
    NS_NOTREACHED("GetNonBlocking");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsBinaryInputStream::GetObserver(nsIInputStreamObserver * *aObserver)
{
    NS_NOTREACHED("GetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsBinaryInputStream::SetObserver(nsIInputStreamObserver * aObserver)
{
    NS_NOTREACHED("SetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
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
    
    rv = Read((char*)aByte, sizeof(*aByte), &bytesRead);
    if (NS_FAILED(rv)) return rv;
    if (bytesRead != sizeof (*aByte))
        return NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP
nsBinaryInputStream::Read16(PRUint16* a16)
{
    nsresult rv;
    PRUint32 bytesRead;

    rv = Read((char*)a16, sizeof *a16, &bytesRead);
    if (NS_FAILED(rv)) return rv;
    if (bytesRead != sizeof *a16)
        return NS_ERROR_FAILURE;
    *a16 = SWAP16(*a16);
    return rv;
}

NS_IMETHODIMP
nsBinaryInputStream::Read32(PRUint32* a32)
{
    nsresult rv;
    PRUint32 bytesRead;

    rv = Read((char*)a32, sizeof *a32, &bytesRead);
    if (NS_FAILED(rv)) return rv;
    if (bytesRead != sizeof *a32)
        return NS_ERROR_FAILURE;
    *a32 = SWAP32(*a32);
    return rv;
}

NS_IMETHODIMP
nsBinaryInputStream::Read64(PRUint64* a64)
{
    nsresult rv;
    PRUint32* raw32 = (PRUint32*)a64;

#ifdef IS_BIG_ENDIAN
    rv = Read32(&raw32[0]);
    if (NS_FAILED(rv)) return rv;
    return Read32(&raw32[1]);
#else
    rv = Read32(&raw32[1]);
    if (NS_FAILED(rv)) return rv;
    return Read32(&raw32[0]);
#endif
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

NS_IMETHODIMP
nsBinaryInputStream::ReadStringZ(char* *aString)
{
    nsresult rv;
    nsAutoString result;
    char c;

    for ( ; ; )
    {
        PRUint32 actualBytesRead;
        rv = Read(&c, 1, &actualBytesRead);
        if (NS_FAILED(rv) || actualBytesRead != 1)
            return NS_ERROR_FAILURE;
        if (!c)
            break;

        result.AppendWithConversion(c);
    }

    *aString = result.ToNewCString();

    return NS_OK;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadWStringZ(PRUnichar* *aString)
{
    NS_NOTREACHED("ReadWStringZ");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadUtf8Z(PRUnichar* *aString)
{
    NS_NOTREACHED("ReadUtf8Z");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadBytes(char* *aString, PRUint32 aLength)
{
    nsresult rv;
    PRUint32 bytesRead;
    char* s;

    s = (char*)nsMemory::Alloc(aLength);
    if (!s)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = Read(s, aLength, &bytesRead);
    if (NS_FAILED(rv)) return rv;
    if (bytesRead != aLength)
	return NS_ERROR_FAILURE;

    *aString = s;
    return NS_OK;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadString(nsString* *aString)
{
    NS_NOTREACHED("ReadString");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_COM nsresult
NS_NewBinaryOutputStream(nsIBinaryOutputStream* *aResult, nsIOutputStream* aDestStream)
{
    NS_ENSURE_ARG_POINTER(aResult);
    nsIBinaryOutputStream *stream = new nsBinaryOutputStream(aDestStream);
    if (!stream)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(stream);
    *aResult = stream;
    return NS_OK;
}

NS_COM nsresult
NS_NewBinaryInputStream(nsIBinaryInputStream* *aResult, nsIInputStream* aSrcStream)
{
    NS_ENSURE_ARG_POINTER(aResult);
    nsIBinaryInputStream *stream = new nsBinaryInputStream(aSrcStream);
    if (!stream)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(stream);
    *aResult = stream;
    return NS_OK;
}
