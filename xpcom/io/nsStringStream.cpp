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

/**
 * Based on original code from nsIStringStream.cpp
 */

#include "nsStringStream.h"
#include "nsIFileStream.h" // for nsIRandomAccessStore

#include "prerror.h"
#include "nsFileSpec.h"
#include "plstr.h"
#include "nsReadableUtils.h"

//========================================================================================
class BasicStringImpl
    : public nsIOutputStream
    , public nsIInputStream
    , public nsIRandomAccessStore
//========================================================================================
{
    public:
                                        BasicStringImpl();
        virtual                         ~BasicStringImpl();

        NS_DECL_NSISEEKABLESTREAM
        NS_IMETHOD                      GetAtEOF(PRBool* outAtEOF);
        NS_IMETHOD                      SetAtEOF(PRBool inAtEOF);
        NS_IMETHOD                      Available(PRUint32 *aLength);
        NS_IMETHOD                      Read(char* aBuf,
                                            PRUint32 aCount,
                                            PRUint32 *aReadCount);
        NS_IMETHOD                      ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval) = 0;

        // nsIOutputStream interface
        NS_IMETHOD                      Write(const char* aBuf,
                                            PRUint32 aCount,
                                            PRUint32 *aWriteCount);
        NS_IMETHOD                      WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval);
        NS_IMETHOD                      WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval);
        NS_IMETHOD                      IsNonBlocking(PRBool *aNonBlocking);

    public:
        
        // nsISupports interface
                                        NS_DECL_ISUPPORTS

        NS_IMETHOD                      Close() { return NS_OK; }

        // nsIInputStream interface
        NS_IMETHOD                      Flush() { return NS_OK; }

    
    public:
        nsresult                        get_result() const { return mLastResult; }
        
    protected:
    
        virtual PRInt32                 length() const = 0;
        virtual PRInt32                 read(char* buf, PRUint32 count) = 0;
        virtual PRInt32                 write(const char*, PRUint32);

    protected:
 
        PRUint32                        mOffset;
        nsresult                        mLastResult;
        PRBool                          mEOF;
        
}; // class BasicStringImpl


//----------------------------------------------------------------------------------------
BasicStringImpl::BasicStringImpl()
//----------------------------------------------------------------------------------------
: mOffset(0)
, mLastResult(NS_OK)
, mEOF(PR_FALSE)
{
}

//----------------------------------------------------------------------------------------
BasicStringImpl::~BasicStringImpl()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP BasicStringImpl::Seek(PRInt32 whence, PRInt32 offset)
//----------------------------------------------------------------------------------------
{
    mLastResult = NS_OK; // reset on a seek.
    mEOF = PR_FALSE; // reset on a seek.
    PRInt32 fileSize = length();
    PRInt32 newPosition=-1;
    switch (whence)
    {
        case NS_SEEK_CUR: newPosition = mOffset + offset; break;
        case NS_SEEK_SET: newPosition = offset; break;
        case NS_SEEK_END: newPosition = fileSize + offset; break;
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
} // BasicStringImpl::Seek

//----------------------------------------------------------------------------------------
NS_IMETHODIMP BasicStringImpl::Tell(PRUint32* outWhere)
//----------------------------------------------------------------------------------------
{
  *outWhere = mOffset;
  return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP BasicStringImpl::SetEOF()
//----------------------------------------------------------------------------------------
{
  NS_NOTYETIMPLEMENTED("BasicStringImpl::SetEOF");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP BasicStringImpl::GetAtEOF(PRBool* outAtEOF)
//----------------------------------------------------------------------------------------
{
  *outAtEOF = mEOF;
  return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP BasicStringImpl::SetAtEOF(PRBool inAtEOF)
//----------------------------------------------------------------------------------------
{
  mEOF = inAtEOF;
  return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP BasicStringImpl::Available(PRUint32 *aLength)
//----------------------------------------------------------------------------------------
{
  NS_PRECONDITION(aLength != nsnull, "null ptr");
  if (!aLength)
    return NS_ERROR_NULL_POINTER;
  *aLength = length();
  return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP BasicStringImpl::Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount)
//----------------------------------------------------------------------------------------
{
  NS_PRECONDITION(aBuf != nsnull, "null ptr");
  if (!aBuf)
    return NS_ERROR_NULL_POINTER;
  NS_PRECONDITION(aReadCount != nsnull, "null ptr");
  if (!aReadCount)
    return NS_ERROR_NULL_POINTER;
  if (NS_FAILED(mLastResult))
   return mLastResult;
  PRInt32 bytesRead = read(aBuf, aCount);
  if (NS_FAILED(mLastResult))
  {
    *aReadCount = 0;
    return mLastResult;
  }
  *aReadCount = bytesRead;
  if (bytesRead < (PRInt32)aCount)
    SetAtEOF(PR_TRUE);
  return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP BasicStringImpl::Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount)
//----------------------------------------------------------------------------------------
{
  NS_PRECONDITION(aBuf != nsnull, "null ptr");
  NS_PRECONDITION(aWriteCount != nsnull, "null ptr");

  if (NS_FAILED(mLastResult))
  return mLastResult;
  PRInt32 bytesWrit = write(aBuf, aCount);
  if (NS_FAILED(mLastResult))
  {
    *aWriteCount = 0;
    return mLastResult;
  }
  *aWriteCount = bytesWrit;
  return NS_OK;
}
    
NS_IMETHODIMP 
BasicStringImpl::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *result)
{
    NS_NOTREACHED("WriteFrom");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
BasicStringImpl::WriteSegments(nsReadSegmentFun reader, void * closure, 
                               PRUint32 count, PRUint32 *result)
{
    NS_NOTREACHED("WriteSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
BasicStringImpl::IsNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = PR_TRUE;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
PRInt32 BasicStringImpl::write(const char*, PRUint32)
//----------------------------------------------------------------------------------------
{
  NS_ASSERTION(PR_FALSE, "Write to a const string");
  mLastResult = NS_FILE_RESULT(PR_ILLEGAL_ACCESS_ERROR);
  return -1;
}


#ifdef XP_MAC
#pragma mark -
#endif

//========================================================================================
class ConstCharImpl
    : public BasicStringImpl
//========================================================================================
{
    public:
                                        ConstCharImpl(const char* inString, PRInt32 inLength = -1)
                                            : mConstString(inString),
                                            mLength(inLength == -1 ?
                                                    (inString ? strlen(inString) : 0) : inLength) {}
                                       
    protected:
    
        virtual PRInt32                 length() const
                                        {
                                            return mLength - mOffset;
                                        }

        virtual PRInt32                 read(char* buf, PRUint32 aCount)
                                        {
                                            PRInt32 maxCount = mLength - mOffset;
                                            if ((PRInt32)aCount > maxCount)
                                                aCount = maxCount;
                                            memcpy(buf, mConstString + mOffset, aCount);
                                            mOffset += aCount;
                                            return aCount;
                                        }
        
        NS_IMETHOD                      ReadSegments(nsWriteSegmentFun writer, void * closure,
                                                     PRUint32 aCount, PRUint32 *result) {
                                            nsresult rv;
                                            PRInt32 maxCount = mLength - mOffset;
                                            if ((PRInt32)aCount > maxCount)
                                                aCount = maxCount;
                                            rv = writer(this, closure, mConstString + mOffset, 
                                                        0, aCount, result);
                                            if (NS_SUCCEEDED(rv))
                                                mOffset += *result;
                                            return rv;
                                        }

    protected:
    
        const char*                     mConstString;
        size_t                          mLength;
        
}; // class ConstCharImpl

//========================================================================================
class ConstStringImpl
    : public ConstCharImpl
//========================================================================================
{
    public:
                                        ConstStringImpl(const nsAString& inString)
                                            : ConstCharImpl(ToNewCString(inString),
                                                            inString.Length())
                                        {
                                        }

                                        ConstStringImpl(const nsACString& inString)
                                            : ConstCharImpl(ToNewCString(inString),
                                                            inString.Length())
                                        {
                                        }

                                        ~ConstStringImpl()
                                        {
                                            Recycle((char*)mConstString);
                                        }

    protected:
    
    protected:
    
}; // class ConstStringImpl


NS_IMPL_THREADSAFE_ADDREF(BasicStringImpl)
NS_IMPL_THREADSAFE_RELEASE(BasicStringImpl)

NS_IMPL_QUERY_HEAD(BasicStringImpl)
  NS_IMPL_QUERY_BODY(nsISeekableStream)
  NS_IMPL_QUERY_BODY(nsIRandomAccessStore)
  NS_IMPL_QUERY_BODY(nsIOutputStream)
  NS_IMPL_QUERY_BODY(nsIInputStream)
NS_IMPL_QUERY_TAIL(nsIOutputStream)

//----------------------------------------------------------------------------------------
extern "C" NS_COM nsresult NS_NewStringInputStream(
    nsIInputStream** aStreamResult,
    const nsAString& aStringToRead)
    // Factory method to get an nsInputStream from a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(aStreamResult != nsnull, "null ptr");
    if (! aStreamResult)
        return NS_ERROR_NULL_POINTER;

    ConstStringImpl* stream = new ConstStringImpl(aStringToRead);
    if (! stream)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(stream);
    *aStreamResult = NS_STATIC_CAST(nsIInputStream*,stream);
    return NS_OK;
}

//----------------------------------------------------------------------------------------
extern "C" NS_COM nsresult NS_NewCStringInputStream(
    nsIInputStream** aStreamResult,
    const nsACString& aStringToRead)
    // Factory method to get an nsInputStream from a cstring.  Result will implement all the
    // file stream interfaces in nsIFileStream.h
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(aStreamResult != nsnull, "null ptr");
    if (! aStreamResult)
        return NS_ERROR_NULL_POINTER;

    ConstStringImpl* stream = new ConstStringImpl(aStringToRead);
    if (! stream)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(stream);
    *aStreamResult = NS_STATIC_CAST(nsIInputStream*,stream);
    return NS_OK;
}

//----------------------------------------------------------------------------------------
extern "C" NS_COM nsresult NS_NewCharInputStream(
    nsISupports** aStreamResult,
    const char* aStringToRead)
    // Factory method to get an nsInputStream from a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(aStreamResult != nsnull, "null ptr");
    if (! aStreamResult)
        return NS_ERROR_NULL_POINTER;

    ConstCharImpl* stream = new ConstCharImpl(aStringToRead);
    if (! stream)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(stream);
    *aStreamResult = (nsISupports*)(void*)stream;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
extern "C" NS_COM nsresult NS_NewByteInputStream(
    nsISupports** aStreamResult,
    const char* aStringToRead,
     PRInt32 aLength)
    // Factory method to get an nsInputStream from a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(aStreamResult != nsnull, "null ptr");
    if (! aStreamResult)
        return NS_ERROR_NULL_POINTER;

    ConstCharImpl* stream = new ConstCharImpl(aStringToRead, aLength);
    if (! stream)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(stream);
    *aStreamResult = (nsISupports*)(void*)stream;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
// nsIStringInputStream implementation
//----------------------------------------------------------------------------------------

class nsStringInputStream : public ConstCharImpl
                          , public nsIStringInputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSISTRINGINPUTSTREAM

    nsStringInputStream() : ConstCharImpl(nsnull, -1), mOwned(PR_FALSE) {}
    virtual ~nsStringInputStream();

protected:
    PRBool mOwned;
};

nsStringInputStream::~nsStringInputStream()
{
    if (mOwned)
        nsMemory::Free((char *) mConstString);
}

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

    mConstString = data;
    mLength = dataLen;
    mOwned = PR_FALSE;
    return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED1(nsStringInputStream,
                             ConstCharImpl,
                             nsIStringInputStream)

// factory method for constructing a nsStringInputStream object
NS_METHOD
nsStringInputStreamConstructor(nsISupports *outer, REFNSIID iid, void **result)
{
    *result = nsnull;

    if (nsnull != outer)
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
