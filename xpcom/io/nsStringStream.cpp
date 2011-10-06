/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sts=4 sw=4 cin et: */
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

#include "IPC/IPCMessageUtils.h"

#include "nsStringStream.h"
#include "nsStreamUtils.h"
#include "nsReadableUtils.h"
#include "nsISeekableStream.h"
#include "nsISupportsPrimitives.h"
#include "nsCRT.h"
#include "prerror.h"
#include "plstr.h"
#include "nsIClassInfoImpl.h"
#include "nsIIPCSerializable.h"

//-----------------------------------------------------------------------------
// nsIStringInputStream implementation
//-----------------------------------------------------------------------------

class nsStringInputStream : public nsIStringInputStream
                          , public nsISeekableStream
                          , public nsISupportsCString
                          , public nsIIPCSerializable
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSISTRINGINPUTSTREAM
    NS_DECL_NSISEEKABLESTREAM
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSCSTRING
    NS_DECL_NSIIPCSERIALIZABLE

    nsStringInputStream()
        : mData(nsnull)
        , mOffset(0)
        , mLength(0)
        , mOwned(PR_FALSE)
    {}

private:
    ~nsStringInputStream()
    {
        if (mData)
            Clear();
    }

    PRInt32 LengthRemaining() const
    {
        return mLength - mOffset;
    }

    void Clear()
    {
        NS_ASSERTION(mData || !mOwned, "bad state");
        if (mOwned)
            NS_Free(const_cast<char*>(mData));

        // We're about to get a new string; reset the offset.
        mOffset = 0;
    }

    const char*    mData;
    PRUint32       mOffset;
    PRUint32       mLength;
    bool           mOwned;
};

// This class needs to support threadsafe refcounting since people often
// allocate a string stream, and then read it from a background thread.
NS_IMPL_THREADSAFE_ADDREF(nsStringInputStream)
NS_IMPL_THREADSAFE_RELEASE(nsStringInputStream)

NS_IMPL_CLASSINFO(nsStringInputStream, NULL, nsIClassInfo::THREADSAFE,
                  NS_STRINGINPUTSTREAM_CID)
NS_IMPL_QUERY_INTERFACE5_CI(nsStringInputStream,
                            nsIStringInputStream,
                            nsIInputStream,
                            nsISupportsCString,
                            nsISeekableStream,
                            nsIIPCSerializable)
NS_IMPL_CI_INTERFACE_GETTER5(nsStringInputStream,
                             nsIStringInputStream,
                             nsIInputStream,
                             nsISupportsCString,
                             nsISeekableStream,
                             nsIIPCSerializable)

/////////
// nsISupportsCString implementation
/////////

NS_IMETHODIMP
nsStringInputStream::GetType(PRUint16 *type)
{
    *type = TYPE_CSTRING;
    return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::GetData(nsACString &data)
{
    // The stream doesn't have any data when it is closed.  We could fake it
    // and return an empty string here, but it seems better to keep this return
    // value consistent with the behavior of the other 'getter' methods.
    NS_ENSURE_TRUE(mData, NS_BASE_STREAM_CLOSED);

    data.Assign(mData, mLength);
    return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::SetData(const nsACString &data)
{
    nsACString::const_iterator iter;
    data.BeginReading(iter);
    return SetData(iter.get(), iter.size_forward());
}

NS_IMETHODIMP
nsStringInputStream::ToString(char **result)
{
    // NOTE: This method may result in data loss, so we do not implement it.
    return NS_ERROR_NOT_IMPLEMENTED;
}

/////////
// nsIStringInputStream implementation
/////////

NS_IMETHODIMP
nsStringInputStream::SetData(const char *data, PRInt32 dataLen)
{
    NS_ENSURE_ARG_POINTER(data);

    if (dataLen < 0)
        dataLen = strlen(data);

    // NOTE: We do not use nsCRT::strndup here because that does not handle
    // null bytes in the middle of the given data.
 
    char *copy = static_cast<char *>(NS_Alloc(dataLen));
    if (!copy)
        return NS_ERROR_OUT_OF_MEMORY;
    memcpy(copy, data, dataLen);

    return AdoptData(copy, dataLen);
}

NS_IMETHODIMP
nsStringInputStream::AdoptData(char *data, PRInt32 dataLen)
{
    NS_ENSURE_ARG_POINTER(data);

    if (dataLen < 0)
        dataLen = strlen(data);

    Clear();
    
    mData = data;
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
    
    mData = data;
    mLength = dataLen;
    mOwned = PR_FALSE;
    return NS_OK;
}

/////////
// nsIInputStream implementation
/////////

NS_IMETHODIMP
nsStringInputStream::Close()
{
    Clear();
    mData = nsnull;
    mLength = 0;
    mOwned = PR_FALSE;
    return NS_OK;
}
    
NS_IMETHODIMP
nsStringInputStream::Available(PRUint32 *aLength)
{
    NS_ASSERTION(aLength, "null ptr");

    if (!mData)
        return NS_BASE_STREAM_CLOSED;

    *aLength = LengthRemaining();
    return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount)
{
    NS_ASSERTION(aBuf, "null ptr");
    return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, aReadCount);
}

NS_IMETHODIMP
nsStringInputStream::ReadSegments(nsWriteSegmentFun writer, void *closure,
                                  PRUint32 aCount, PRUint32 *result)
{
    NS_ASSERTION(result, "null ptr");
    NS_ASSERTION(mLength >= mOffset, "bad stream state");

    // We may be at end-of-file
    PRUint32 maxCount = LengthRemaining();
    if (maxCount == 0) {
        *result = 0;
        return NS_OK;
    }
    NS_ASSERTION(mData, "must have data if maxCount != 0");

    if (aCount > maxCount)
        aCount = maxCount;
    nsresult rv = writer(this, closure, mData + mOffset, 0, aCount, result);
    if (NS_SUCCEEDED(rv)) {
        NS_ASSERTION(*result <= aCount,
                     "writer should not write more than we asked it to write");
        mOffset += *result;
    }

    // errors returned from the writer end here!
    return NS_OK;
}
    
NS_IMETHODIMP
nsStringInputStream::IsNonBlocking(bool *aNonBlocking)
{
    *aNonBlocking = PR_TRUE;
    return NS_OK;
}

/////////
// nsISeekableStream implementation
/////////

NS_IMETHODIMP 
nsStringInputStream::Seek(PRInt32 whence, PRInt64 offset)
{
    if (!mData)
        return NS_BASE_STREAM_CLOSED;

    // Compute new stream position.  The given offset may be a negative value.
 
    PRInt64 newPos = offset;
    switch (whence) {
    case NS_SEEK_SET:
        break;
    case NS_SEEK_CUR:
        newPos += (PRInt32) mOffset;
        break;
    case NS_SEEK_END:
        newPos += (PRInt32) mLength;
        break;
    default:
        NS_ERROR("invalid whence");
        return NS_ERROR_INVALID_ARG;
    }

    // mLength is never larger than PR_INT32_MAX due to the way it is assigned.

    NS_ENSURE_ARG(newPos >= 0);
    NS_ENSURE_ARG(newPos <= (PRInt32) mLength);

    mOffset = (PRInt32) newPos;
    return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::Tell(PRInt64* outWhere)
{
    if (!mData)
        return NS_BASE_STREAM_CLOSED;

    *outWhere = mOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::SetEOF()
{
    if (!mData)
        return NS_BASE_STREAM_CLOSED;

    mLength = mOffset;
    return NS_OK;
}

/////////
// nsIIPCSerializable implementation
/////////

bool
nsStringInputStream::Read(const IPC::Message *aMsg, void **aIter)
{
    using IPC::ReadParam;

    nsCAutoString value;

    if (!ReadParam(aMsg, aIter, &value))
        return PR_FALSE;

    nsresult rv = SetData(value.get(), value.Length());
    if (NS_FAILED(rv))
        return PR_FALSE;

    return PR_TRUE;
}

void
nsStringInputStream::Write(IPC::Message *aMsg)
{
    using IPC::WriteParam;

    nsCAutoString value;
    GetData(value);

    WriteParam(aMsg, value);
}

nsresult
NS_NewByteInputStream(nsIInputStream** aStreamResult,
                      const char* aStringToRead, PRInt32 aLength,
                      nsAssignmentType aAssignment)
{
    NS_PRECONDITION(aStreamResult, "null out ptr");

    nsStringInputStream* stream = new nsStringInputStream();
    if (! stream)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(stream);

    nsresult rv;
    switch (aAssignment) {
    case NS_ASSIGNMENT_COPY:
        rv = stream->SetData(aStringToRead, aLength);
        break;
    case NS_ASSIGNMENT_DEPEND:
        rv = stream->ShareData(aStringToRead, aLength);
        break;
    case NS_ASSIGNMENT_ADOPT:
        rv = stream->AdoptData(const_cast<char*>(aStringToRead), aLength);
        break;
    default:
        NS_ERROR("invalid assignment type");
        rv = NS_ERROR_INVALID_ARG;
    }
    
    if (NS_FAILED(rv)) {
        NS_RELEASE(stream);
        return rv;
    }
    
    *aStreamResult = stream;
    return NS_OK;
}

nsresult
NS_NewStringInputStream(nsIInputStream** aStreamResult,
                        const nsAString& aStringToRead)
{
    char* data = ToNewCString(aStringToRead);  // truncates high-order bytes
    if (!data)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = NS_NewByteInputStream(aStreamResult, data,
                                        aStringToRead.Length(),
                                        NS_ASSIGNMENT_ADOPT);
    if (NS_FAILED(rv))
        NS_Free(data);
    return rv;
}

nsresult
NS_NewCStringInputStream(nsIInputStream** aStreamResult,
                         const nsACString& aStringToRead)
{
    nsACString::const_iterator data;
    aStringToRead.BeginReading(data);

    return NS_NewByteInputStream(aStreamResult, data.get(), data.size_forward(),
                                 NS_ASSIGNMENT_COPY);
}

// factory method for constructing a nsStringInputStream object
nsresult
nsStringInputStreamConstructor(nsISupports *outer, REFNSIID iid, void **result)
{
    *result = nsnull;

    NS_ENSURE_TRUE(!outer, NS_ERROR_NO_AGGREGATION);

    nsStringInputStream *inst = new nsStringInputStream();
    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(inst);
    nsresult rv = inst->QueryInterface(iid, result);
    NS_RELEASE(inst);

    return rv;
}
