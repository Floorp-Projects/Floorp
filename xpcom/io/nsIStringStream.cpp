/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIStringStream.h"
#include "nsIFileStream.h"

#include "prerror.h"
#include "nsFileSpec.h"
#include "plstr.h"

//========================================================================================
class BasicStringImpl
    : public nsIOutputStream
    , public nsIInputStream
    , public nsIRandomAccessStore
//========================================================================================
{
    public:
                                        BasicStringImpl()
                                            : mOffset(0)
                                            , mLastResult(NS_OK)
                                            , mEOF(PR_FALSE)
                                        {
                                            NS_INIT_REFCNT();
                                        }
        virtual                         ~BasicStringImpl()
                                        {
                                        }
        NS_IMETHOD                      Seek(PRSeekWhence whence, PRInt32 offset);

        NS_IMETHOD                      Tell(PRIntn* outWhere)
                                        {
                                            *outWhere = mOffset;
                                            return NS_OK;
                                        }

        NS_IMETHOD                      GetAtEOF(PRBool* outAtEOF)
        						        {
        						            *outAtEOF = mEOF;
        						            return NS_OK;
        						        }
        NS_IMETHOD                      SetAtEOF(PRBool inAtEOF)
        						        {
        						            mEOF = inAtEOF;
        						            return NS_OK;
        						        }

        NS_IMETHOD                      GetLength(PRUint32 *aLength)
                                        {
                                            NS_PRECONDITION(aLength != nsnull, "null ptr");
                                            if (!aLength)
                                                return NS_ERROR_NULL_POINTER;
                                            *aLength = length();
                                            return NS_OK;
                                        }
		NS_IMETHOD                      Read(char* aBuf,
			                                PRUint32 aCount,
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
		// nsIOutputStream interface
		NS_IMETHOD                      Write(const char* aBuf,
			                                PRUint32 aCount,
					                        PRUint32 *aWriteCount)
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
        NS_IMETHOD                      Write(nsIInputStream* fromStream, PRUint32 *aWriteCount)
                                        {
                                            return NS_ERROR_NOT_IMPLEMENTED;
                                        }
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
        virtual PRInt32                 write(const char*, PRUint32)
                                        {
                                            NS_ASSERTION(PR_FALSE, "Write to a const string");
                                            mLastResult = NS_FILE_RESULT(PR_ILLEGAL_ACCESS_ERROR);
                                            return -1;
                                        }
        
    protected:
 
        PRUint32                        mOffset;
        nsresult                        mLastResult;
        PRBool                          mEOF;
}; // class BasicStringImpl

//========================================================================================
class ConstCharImpl
    : public BasicStringImpl
//========================================================================================
{
    public:
                                        ConstCharImpl(const char* inString)
                                            : mConstString(inString)
                                            , mLength(inString ? strlen(inString) : 0)
                                        {
                                        }
                                       
    protected:
    
        virtual PRInt32                 length() const
                                        {
                                            return mLength;
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
        

    protected:
    
        const char*                     mConstString;
        size_t                          mLength;
        
}; // class ConstCharImpl

//========================================================================================
class CharImpl
    : public ConstCharImpl
//========================================================================================
{
    enum { kAllocQuantum = 256 };
    
    public:
                                        CharImpl(char** inString)
                                            : ConstCharImpl(*inString)
                                            , mString(*inString)
                                            , mAllocLength(mLength + 1)
                                            , mOriginalLength(mLength)
                                        {
                                            if (!mString)
                                            {
                                                mAllocLength += kAllocQuantum;
                                                mString = new char[mAllocLength];
	                                            if (!mString)
	                                            {
	                                                mLastResult = NS_ERROR_OUT_OF_MEMORY;
	                                                return;
	                                            }
	                                            mConstString = mString;
	                                            *mString = '\0';
	                                            
                                            }
                                        }

        virtual PRInt32                 write(const char* buf, PRUint32 aCount)
                                        {
                                            PRInt32 maxCount = mAllocLength - 1 - mOffset;
                                            if ((PRInt32)aCount > maxCount)
                                            {
                                                
                                                do {
                                                    maxCount += kAllocQuantum;
                                                } while ((PRInt32)aCount > maxCount);
                                                mAllocLength = maxCount + 1 + mOffset;
                                                char* newString = new char[mAllocLength];
	                                            if (!newString)
	                                            {
	                                                mLastResult = NS_ERROR_OUT_OF_MEMORY;
	                                                return 0;
	                                            }
                                                strcpy(newString, mString);
                                                delete [] mString;
                                                mString = newString;
                                                mConstString = newString;
                                            }
                                            memcpy(mString + mOffset, buf, aCount);
                                            mOffset += aCount;
                                            if (mOffset > mOriginalLength)
                                                mString[mOffset] = 0;
                                            return aCount;
                                        }
    protected:
    
        char*&                          mString;
        size_t                          mAllocLength;
        size_t                          mOriginalLength;
        
}; // class CharImpl
                                       
//========================================================================================
class ConstStringImpl
    : public ConstCharImpl
//========================================================================================
{
    public:
                                        ConstStringImpl(const nsString& inString)
                                            : ConstCharImpl(inString.ToNewCString())
                                        {
                                        }

                                        ~ConstStringImpl()
                                        {
                                            delete [] (char*)mConstString;
                                        }

    protected:
    
    protected:
    
}; // class ConstStringImpl


//========================================================================================
class StringImpl
    : public ConstStringImpl
//========================================================================================
{
    public:
                                        StringImpl(nsString& inString)
                                            : ConstStringImpl(inString)
                                            , mString(inString)
                                        {
                                        }

    protected:
    
        virtual PRInt32                 write(const char* buf, PRUint32 count)
                                        {
                                            // Clone our string as chars
                                            char* cstring = mString.ToNewCString();
                                            // Make a CharImpl and do the write
                                            CharImpl chars(&cstring);
                                            chars.Seek(PR_SEEK_SET, mOffset);
                                            // Get the bytecount and result from the CharImpl
                                            PRInt32 result = chars.write(buf,count);
                                            mLastResult = chars.get_result();
                                            // Set our string to match the new chars
                                            mString = cstring;
                                            // Set our const string also...
                                            delete [] (char*)mConstString;
                                            mConstString = cstring;
                                            return result;
                                        }
    protected:
    
        nsString&                        mString;
        
}; // class StringImpl

#define SAY_I_IMPLEMENT(classname)               \
  if (aIID.Equals(classname::GetIID()))          \
  {                                              \
      *aInstancePtr = (void*)((classname*)this); \
      NS_ADDREF_THIS();                          \
      return NS_OK;                              \
  }

NS_IMPL_RELEASE(BasicStringImpl)
NS_IMPL_ADDREF(BasicStringImpl)

//----------------------------------------------------------------------------------------
NS_IMETHODIMP BasicStringImpl::QueryInterface(REFNSIID aIID, void** aInstancePtr)
//----------------------------------------------------------------------------------------
{
  if (!aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  *aInstancePtr = nsnull;

  SAY_I_IMPLEMENT(nsIRandomAccessStore)
  SAY_I_IMPLEMENT(nsIOutputStream)
  SAY_I_IMPLEMENT(nsIInputStream)
  // Note that we derive from two copies of nsIBaseStream (and hence
  // of nsISupports), one through
  // nsIOutputStream, the other through nsIInputStream.  Resolve this
  // by giving them a specific one
  if (aIID.Equals(((nsIBaseStream*)(nsIOutputStream*)this)->GetIID()))
  {
      *aInstancePtr = (void*)((nsIBaseStream*)(nsIOutputStream*)this);
      NS_ADDREF_THIS();
      return NS_OK;
  }
  if (aIID.Equals(((nsISupports*)(nsIOutputStream*)this)->GetIID()))
  {
      *aInstancePtr = (void*)((nsISupports*)(nsIOutputStream*)this);
      NS_ADDREF_THIS();
      return NS_OK;
  }
  return NS_NOINTERFACE;
} // StringImpl::QueryInterface

//----------------------------------------------------------------------------------------
NS_IMETHODIMP BasicStringImpl::Seek(PRSeekWhence whence, PRInt32 offset)
//----------------------------------------------------------------------------------------
{
    mLastResult = NS_OK; // reset on a seek.
    mEOF = PR_FALSE; // reset on a seek.
    PRInt32 fileSize = length();
    PRInt32 newPosition;
    switch (whence)
    {
        case PR_SEEK_CUR: newPosition = mOffset + offset; break;
        case PR_SEEK_SET: newPosition = offset; break;
        case PR_SEEK_END: newPosition = fileSize + offset; break;
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
} // StringImpl::Seek

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewStringInputStream(
    nsISupports** aStreamResult,
    const nsString& aStringToRead)
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
    *aStreamResult = (nsISupports*)(void*)stream;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewStringOutputStream(
    nsISupports** aStreamResult,
    nsString& aStringToChange)
    // Factory method to get an nsOutputStream from a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(aStreamResult != nsnull, "null ptr");
    if (! aStreamResult)
        return NS_ERROR_NULL_POINTER;

    StringImpl* stream = new StringImpl(aStringToChange);
    if (! stream)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(stream);
    *aStreamResult = (nsISupports*)(void*)stream;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewCharInputStream(
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
extern "C" NS_BASE nsresult NS_NewCharOutputStream(
    nsISupports** aStreamResult,
    char** aStringToChange)
    // Factory method to get an nsOutputStream to a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(aStreamResult != nsnull, "null ptr");
    NS_PRECONDITION(aStringToChange != nsnull, "null ptr");
    if (!aStreamResult || !aStringToChange)
        return NS_ERROR_NULL_POINTER;

    CharImpl* stream = new CharImpl(aStringToChange);
    if (! stream)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(stream);
    *aStreamResult = (nsISupports*)(void*)stream;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewStringIOStream(
    nsISupports** aStreamResult,
    nsString& aStringToChange)
    // Factory method to get an nsOutputStream to a string.  Result will implement all the
// file stream interfaces in nsIFileStream.h
//----------------------------------------------------------------------------------------
{
    return NS_NewStringOutputStream(aStreamResult, aStringToChange);
}

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewCharIOStream(
    nsISupports** aStreamResult,
    char** aStringToChange)
    // Factory method to get an nsOutputStream to a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h
//----------------------------------------------------------------------------------------
{
    return NS_NewCharOutputStream(aStreamResult, aStringToChange);
}
