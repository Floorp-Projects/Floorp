/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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


#include "nsIUnicharInputStream.h"
#include "nsIByteBuffer.h"
#include "nsIUnicharBuffer.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsCRT.h"
#include <fcntl.h>
#if defined(NS_WIN32) || defined(XP_OS2_VACPP)
#include <io.h>
#else
#include <unistd.h>
#endif

class StringUnicharInputStream : public nsIUnicharInputStream {
public:
  StringUnicharInputStream(nsString* aString);
  virtual ~StringUnicharInputStream();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Read(PRUnichar* aBuf,
                  PRUint32 aOffset,
                  PRUint32 aCount,
                  PRUint32 *aReadCount);
  NS_IMETHOD Close();

  nsString* mString;
  PRUint32 mPos;
  PRUint32 mLen;
};

StringUnicharInputStream::StringUnicharInputStream(nsString* aString)
{
  NS_INIT_REFCNT();
  mString = aString;
  mPos = 0;
  mLen = aString->Length();
}

StringUnicharInputStream::~StringUnicharInputStream()
{
  if (nsnull != mString) {
    delete mString;
  }
}

nsresult StringUnicharInputStream::Read(PRUnichar* aBuf,
                                        PRUint32 aOffset,
                                        PRUint32 aCount,
                                        PRUint32 *aReadCount)
{
  if (mPos >= mLen) {
    *aReadCount = 0;
    return (nsresult)-1;
  }
  const PRUnichar* us = mString->get();
  NS_ASSERTION(mLen >= mPos, "unsigned madness");
  PRUint32 amount = mLen - mPos;
  if (amount > aCount) {
    amount = aCount;
  }
  nsCRT::memcpy(aBuf, us + mPos, sizeof(PRUnichar) * amount);
  mPos += amount;
  *aReadCount = amount;
  return NS_OK;
}

nsresult StringUnicharInputStream::Close()
{
  mPos = mLen;
  if (nsnull != mString) {
    delete mString;
    mString = 0;
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(StringUnicharInputStream, nsIUnicharInputStream)

NS_COM nsresult
NS_NewStringUnicharInputStream(nsIUnicharInputStream** aInstancePtrResult,
                               nsString* aString)
{
  NS_PRECONDITION(nsnull != aString, "null ptr");
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if ((nsnull == aString) || (nsnull == aInstancePtrResult)) {
    return NS_ERROR_NULL_POINTER;
  }

  StringUnicharInputStream* it = new StringUnicharInputStream(aString);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(NS_GET_IID(nsIUnicharInputStream),
                            (void**) aInstancePtrResult);
}

//----------------------------------------------------------------------

class UTF8InputStream : public nsIUnicharInputStream {
public:
  UTF8InputStream(nsIInputStream* aStream,
                  PRUint32 aBufSize);
  virtual ~UTF8InputStream();

  NS_DECL_ISUPPORTS
  NS_IMETHOD Read(PRUnichar* aBuf,
                  PRUint32 aOffset,
                  PRUint32 aCount,
                  PRUint32 *aReadCount);
  NS_IMETHOD Close();

protected:
  PRInt32 Fill(nsresult * aErrorCode);

  static PRInt32 CountValidUTF8Bytes(const char *aBuf, PRInt32 aMaxBytes);

  nsCOMPtr<nsIInputStream> mInput;
  nsCOMPtr<nsIByteBuffer> mByteData;
  nsCOMPtr<nsIUnicharBuffer> mUnicharData;
  
  PRUint32 mByteDataOffset;
  PRUint32 mUnicharDataOffset;
  PRUint32 mUnicharDataLength;
};

UTF8InputStream::UTF8InputStream(nsIInputStream* aStream,
                                 PRUint32 aBufferSize) :
  mInput(aStream)
{
  NS_INIT_REFCNT();
  if (aBufferSize == 0) {
    aBufferSize = 8192;
  }

  // XXX what if these fail?
  NS_NewByteBuffer(getter_AddRefs(mByteData), nsnull, aBufferSize);
  NS_NewUnicharBuffer(getter_AddRefs(mUnicharData), nsnull, aBufferSize);

  mByteDataOffset = 0;
  mUnicharDataOffset = 0;
  mUnicharDataLength = 0;
}

NS_IMPL_ISUPPORTS1(UTF8InputStream,nsIUnicharInputStream)

UTF8InputStream::~UTF8InputStream()
{
  Close();
}

nsresult UTF8InputStream::Close()
{
  mInput = nsnull;
  mByteData = nsnull;
  mUnicharData = nsnull;

  return NS_OK;
}

nsresult UTF8InputStream::Read(PRUnichar* aBuf,
                               PRUint32 aOffset,
                               PRUint32 aCount,
                               PRUint32 *aReadCount)
{
  NS_ASSERTION(mUnicharDataLength >= mUnicharDataOffset, "unsigned madness");
  PRUint32 rv = mUnicharDataLength - mUnicharDataOffset;
  nsresult errorCode;
  if (0 == rv) {
    // Fill the unichar buffer
    rv = Fill(&errorCode);
    if (rv <= 0) {
      *aReadCount = 0;
      return errorCode;
    }
  }
  if (rv > aCount) {
    rv = aCount;
  }
  nsCRT::memcpy(aBuf + aOffset, mUnicharData->GetBuffer() + mUnicharDataOffset,
                rv * sizeof(PRUnichar));
  mUnicharDataOffset += rv;
  *aReadCount = rv;
  return NS_OK;
}

PRInt32 UTF8InputStream::Fill(nsresult * aErrorCode)
{
  if (nsnull == mInput) {
    // We already closed the stream!
    *aErrorCode = NS_BASE_STREAM_CLOSED;
    return -1;
  }

  NS_ASSERTION(mByteData->GetLength() >= mByteDataOffset, "unsigned madness");
  PRUint32 remainder = mByteData->GetLength() - mByteDataOffset;
  mByteDataOffset = remainder;
  PRInt32 nb = mByteData->Fill(aErrorCode, mInput, remainder);
  if (nb <= 0) {
    // Because we assume a many to one conversion, the lingering data
    // in the byte buffer must be a partial conversion
    // fragment. Because we know that we have recieved no more new
    // data to add to it, we can't convert it. Therefore, we discard
    // it.
    return nb;
  }
  NS_ASSERTION(remainder + nb == mByteData->GetLength(), "bad nb");

  // Now convert as much of the byte buffer to unicode as possible
  PRInt32 srcLen = CountValidUTF8Bytes(mByteData->GetBuffer(),remainder + nb);

  NS_ConvertUTF8toUCS2
    unicodeValue(Substring(mByteData->GetBuffer(),
                           mByteData->GetBuffer() + srcLen));
  PRInt32 dstLen = unicodeValue.Length();
  
  // the number of UCS2 characters should always be <= the number of
  // UTF8 chars
  NS_ASSERTION(dstLen <= mUnicharData->GetBufferSize(),
               "Ouch. I would overflow my buffer if I wasn't so careful.");
  if (dstLen > mUnicharData->GetBufferSize()) return 0;
  
  nsCRT::memcpy((void *)mUnicharData->GetBuffer(),
                (void *)unicodeValue.get(), dstLen*sizeof(PRUnichar));

  mUnicharDataOffset = 0;
  mUnicharDataLength = dstLen;
  mByteDataOffset += srcLen;
  
  return dstLen;
}

PRInt32
UTF8InputStream::CountValidUTF8Bytes(const char* aBuffer, PRInt32 aMaxBytes)
{
  const char *c = aBuffer;
  const char *end = aBuffer + aMaxBytes;
  
  while (c < end && *c) {
    if (UTF8traits::isASCII(*c))
      c++;
    else if (UTF8traits::is2byte(*c))
      c += 2;
    else if (UTF8traits::is3byte(*c))
      c += 3;
    else if (UTF8traits::is4byte(*c))
      c += 4;
    else if (UTF8traits::is5byte(*c))
      c += 5;
    else if (UTF8traits::is6byte(*c))
      c += 6;
    else {
      NS_WARNING("Unrecognized UTF8 string in UTF8InputStream::CountValidUTF8Bytes()");
      break; // Otherwise we go into an infinite loop.  But what happens now?
    }
  }

  return c - aBuffer;
}

NS_COM nsresult
NS_NewUTF8ConverterStream(nsIUnicharInputStream** aInstancePtrResult,
                          nsIInputStream* aStreamToWrap,
                          PRInt32 aBufferSize)
{
  // Create converter input stream
  UTF8InputStream* it =
    new UTF8InputStream(aStreamToWrap, aBufferSize);
  
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIUnicharInputStream), 
                            (void **) aInstancePtrResult);
}
