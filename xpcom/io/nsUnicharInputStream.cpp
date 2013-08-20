/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicharInputStream.h"
#include "nsIInputStream.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsCRT.h"
#include "nsStreamUtils.h"
#include "nsUTF8Utils.h"
#include "mozilla/Attributes.h"
#include <fcntl.h>
#if defined(XP_WIN)
#include <io.h>
#else
#include <unistd.h>
#endif

#define STRING_BUFFER_SIZE 8192

class StringUnicharInputStream MOZ_FINAL : public nsIUnicharInputStream {
public:
  StringUnicharInputStream(const nsAString& aString) :
    mString(aString), mPos(0), mLen(aString.Length()) { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIUNICHARINPUTSTREAM

  nsString mString;
  uint32_t mPos;
  uint32_t mLen;

private:
  ~StringUnicharInputStream() { }
};

NS_IMETHODIMP
StringUnicharInputStream::Read(PRUnichar* aBuf,
                               uint32_t aCount,
                               uint32_t *aReadCount)
{
  if (mPos >= mLen) {
    *aReadCount = 0;
    return NS_OK;
  }
  nsAString::const_iterator iter;
  mString.BeginReading(iter);
  const PRUnichar* us = iter.get();
  uint32_t amount = mLen - mPos;
  if (amount > aCount) {
    amount = aCount;
  }
  memcpy(aBuf, us + mPos, sizeof(PRUnichar) * amount);
  mPos += amount;
  *aReadCount = amount;
  return NS_OK;
}

NS_IMETHODIMP
StringUnicharInputStream::ReadSegments(nsWriteUnicharSegmentFun aWriter,
                                       void* aClosure,
                                       uint32_t aCount, uint32_t *aReadCount)
{
  uint32_t bytesWritten;
  uint32_t totalBytesWritten = 0;

  nsresult rv;
  aCount = XPCOM_MIN(mString.Length() - mPos, aCount);
  
  nsAString::const_iterator iter;
  mString.BeginReading(iter);
  
  while (aCount) {
    rv = aWriter(this, aClosure, iter.get() + mPos,
                 totalBytesWritten, aCount, &bytesWritten);
    
    if (NS_FAILED(rv)) {
      // don't propagate errors to the caller
      break;
    }
    
    aCount -= bytesWritten;
    totalBytesWritten += bytesWritten;
    mPos += bytesWritten;
  }
  
  *aReadCount = totalBytesWritten;
  
  return NS_OK;
}

NS_IMETHODIMP
StringUnicharInputStream::ReadString(uint32_t aCount, nsAString& aString,
                                     uint32_t* aReadCount)
{
  if (mPos >= mLen) {
    *aReadCount = 0;
    return NS_OK;
  }
  uint32_t amount = mLen - mPos;
  if (amount > aCount) {
    amount = aCount;
  }
  aString = Substring(mString, mPos, amount);
  mPos += amount;
  *aReadCount = amount;
  return NS_OK;
}

nsresult StringUnicharInputStream::Close()
{
  mPos = mLen;
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(StringUnicharInputStream, nsIUnicharInputStream)

//----------------------------------------------------------------------

class UTF8InputStream MOZ_FINAL : public nsIUnicharInputStream {
public:
  UTF8InputStream();
  nsresult Init(nsIInputStream* aStream);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIUNICHARINPUTSTREAM

private:
  ~UTF8InputStream();

protected:
  int32_t Fill(nsresult * aErrorCode);

  static void CountValidUTF8Bytes(const char *aBuf, uint32_t aMaxBytes, uint32_t& aValidUTF8bytes, uint32_t& aValidUTF16CodeUnits);

  nsCOMPtr<nsIInputStream> mInput;
  FallibleTArray<char> mByteData;
  FallibleTArray<PRUnichar> mUnicharData;
  
  uint32_t mByteDataOffset;
  uint32_t mUnicharDataOffset;
  uint32_t mUnicharDataLength;
};

UTF8InputStream::UTF8InputStream() :
  mByteDataOffset(0),
  mUnicharDataOffset(0),
  mUnicharDataLength(0)
{
}

nsresult 
UTF8InputStream::Init(nsIInputStream* aStream)
{
  if (!mByteData.SetCapacity(STRING_BUFFER_SIZE) ||
      !mUnicharData.SetCapacity(STRING_BUFFER_SIZE)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInput = aStream;

  return NS_OK;
}

NS_IMPL_ISUPPORTS1(UTF8InputStream,nsIUnicharInputStream)

UTF8InputStream::~UTF8InputStream()
{
  Close();
}

nsresult UTF8InputStream::Close()
{
  mInput = nullptr;
  mByteData.Clear();
  mUnicharData.Clear();
  return NS_OK;
}

nsresult UTF8InputStream::Read(PRUnichar* aBuf,
                               uint32_t aCount,
                               uint32_t *aReadCount)
{
  NS_ASSERTION(mUnicharDataLength >= mUnicharDataOffset, "unsigned madness");
  uint32_t readCount = mUnicharDataLength - mUnicharDataOffset;
  nsresult errorCode;
  if (0 == readCount) {
    // Fill the unichar buffer
    int32_t bytesRead = Fill(&errorCode);
    if (bytesRead <= 0) {
      *aReadCount = 0;
      return errorCode;
    }
    readCount = bytesRead;
  }
  if (readCount > aCount) {
    readCount = aCount;
  }
  memcpy(aBuf, mUnicharData.Elements() + mUnicharDataOffset,
         readCount * sizeof(PRUnichar));
  mUnicharDataOffset += readCount;
  *aReadCount = readCount;
  return NS_OK;
}

NS_IMETHODIMP
UTF8InputStream::ReadSegments(nsWriteUnicharSegmentFun aWriter,
                              void* aClosure,
                              uint32_t aCount, uint32_t *aReadCount)
{
  NS_ASSERTION(mUnicharDataLength >= mUnicharDataOffset, "unsigned madness");
  uint32_t bytesToWrite = mUnicharDataLength - mUnicharDataOffset;
  nsresult rv = NS_OK;
  if (0 == bytesToWrite) {
    // Fill the unichar buffer
    int32_t bytesRead = Fill(&rv);
    if (bytesRead <= 0) {
      *aReadCount = 0;
      return rv;
    }
    bytesToWrite = bytesRead;
  }
  
  if (bytesToWrite > aCount)
    bytesToWrite = aCount;
  
  uint32_t bytesWritten;
  uint32_t totalBytesWritten = 0;

  while (bytesToWrite) {
    rv = aWriter(this, aClosure,
                 mUnicharData.Elements() + mUnicharDataOffset,
                 totalBytesWritten, bytesToWrite, &bytesWritten);

    if (NS_FAILED(rv)) {
      // don't propagate errors to the caller
      break;
    }
    
    bytesToWrite -= bytesWritten;
    totalBytesWritten += bytesWritten;
    mUnicharDataOffset += bytesWritten;
  }

  *aReadCount = totalBytesWritten;
  
  return NS_OK;
}

NS_IMETHODIMP
UTF8InputStream::ReadString(uint32_t aCount, nsAString& aString,
                            uint32_t* aReadCount)
{
  NS_ASSERTION(mUnicharDataLength >= mUnicharDataOffset, "unsigned madness");
  uint32_t readCount = mUnicharDataLength - mUnicharDataOffset;
  nsresult errorCode;
  if (0 == readCount) {
    // Fill the unichar buffer
    int32_t bytesRead = Fill(&errorCode);
    if (bytesRead <= 0) {
      *aReadCount = 0;
      return errorCode;
    }
    readCount = bytesRead;
  }
  if (readCount > aCount) {
    readCount = aCount;
  }
  const PRUnichar* buf = mUnicharData.Elements() + mUnicharDataOffset;
  aString.Assign(buf, readCount);

  mUnicharDataOffset += readCount;
  *aReadCount = readCount;
  return NS_OK;
}

int32_t UTF8InputStream::Fill(nsresult * aErrorCode)
{
  if (nullptr == mInput) {
    // We already closed the stream!
    *aErrorCode = NS_BASE_STREAM_CLOSED;
    return -1;
  }

  NS_ASSERTION(mByteData.Length() >= mByteDataOffset, "unsigned madness");
  uint32_t remainder = mByteData.Length() - mByteDataOffset;
  mByteDataOffset = remainder;
  uint32_t nb;
  *aErrorCode = NS_FillArray(mByteData, mInput, remainder, &nb);
  if (nb == 0) {
    // Because we assume a many to one conversion, the lingering data
    // in the byte buffer must be a partial conversion
    // fragment. Because we know that we have received no more new
    // data to add to it, we can't convert it. Therefore, we discard
    // it.
    return nb;
  }
  NS_ASSERTION(remainder + nb == mByteData.Length(), "bad nb");

  // Now convert as much of the byte buffer to unicode as possible
  uint32_t srcLen, dstLen;
  CountValidUTF8Bytes(mByteData.Elements(),remainder + nb, srcLen, dstLen);

  // the number of UCS2 characters should always be <= the number of
  // UTF8 chars
  NS_ASSERTION( (remainder+nb >= srcLen), "cannot be longer than out buffer");
  NS_ASSERTION(dstLen <= mUnicharData.Capacity(),
               "Ouch. I would overflow my buffer if I wasn't so careful.");
  if (dstLen > mUnicharData.Capacity()) return 0;
  
  ConvertUTF8toUTF16 converter(mUnicharData.Elements());
  
  nsASingleFragmentCString::const_char_iterator start = mByteData.Elements();
  nsASingleFragmentCString::const_char_iterator end = mByteData.Elements() + srcLen;
            
  copy_string(start, end, converter);
  if (converter.Length() != dstLen) {
    *aErrorCode = NS_BASE_STREAM_BAD_CONVERSION;
    return -1;
  }
               
  mUnicharDataOffset = 0;
  mUnicharDataLength = dstLen;
  mByteDataOffset = srcLen;
  
  return dstLen;
}

void
UTF8InputStream::CountValidUTF8Bytes(const char* aBuffer, uint32_t aMaxBytes, uint32_t& aValidUTF8bytes, uint32_t& aValidUTF16CodeUnits)
{
  const char *c = aBuffer;
  const char *end = aBuffer + aMaxBytes;
  const char *lastchar = c;     // pre-initialize in case of 0-length buffer
  uint32_t utf16length = 0;
  while (c < end && *c) {
    lastchar = c;
    utf16length++;
    
    if (UTF8traits::isASCII(*c))
      c++;
    else if (UTF8traits::is2byte(*c))
      c += 2;
    else if (UTF8traits::is3byte(*c))
      c += 3;
    else if (UTF8traits::is4byte(*c)) {
      c += 4;
      utf16length++; // add 1 more because this will be converted to a
                     // surrogate pair.
    }
    else if (UTF8traits::is5byte(*c))
      c += 5;
    else if (UTF8traits::is6byte(*c))
      c += 6;
    else {
      NS_WARNING("Unrecognized UTF8 string in UTF8InputStream::CountValidUTF8Bytes()");
      break; // Otherwise we go into an infinite loop.  But what happens now?
    }
  }
  if (c > end) {
    c = lastchar;
    utf16length--;
  }

  aValidUTF8bytes = c - aBuffer;
  aValidUTF16CodeUnits = utf16length;
}

NS_IMPL_QUERY_INTERFACE2(nsSimpleUnicharStreamFactory,
                         nsIFactory,
                         nsISimpleUnicharStreamFactory)

NS_IMETHODIMP_(nsrefcnt) nsSimpleUnicharStreamFactory::AddRef() { return 2; }
NS_IMETHODIMP_(nsrefcnt) nsSimpleUnicharStreamFactory::Release() { return 1; }

NS_IMETHODIMP
nsSimpleUnicharStreamFactory::CreateInstance(nsISupports* aOuter, REFNSIID aIID,
                                            void **aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSimpleUnicharStreamFactory::LockFactory(bool aLock)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleUnicharStreamFactory::CreateInstanceFromString(const nsAString& aString,
                                                      nsIUnicharInputStream* *aResult)
{
  StringUnicharInputStream* it = new StringUnicharInputStream(aString);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult = it);
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleUnicharStreamFactory::CreateInstanceFromUTF8Stream(nsIInputStream* aStreamToWrap,
                                                           nsIUnicharInputStream* *aResult)
{
  *aResult = nullptr;

  // Create converter input stream
  nsRefPtr<UTF8InputStream> it = new UTF8InputStream();
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = it->Init(aStreamToWrap);
  if (NS_FAILED(rv))
    return rv;

  NS_ADDREF(*aResult = it);
  return NS_OK;
}

nsSimpleUnicharStreamFactory*
nsSimpleUnicharStreamFactory::GetInstance()
{
  static const nsSimpleUnicharStreamFactory kInstance;
  return const_cast<nsSimpleUnicharStreamFactory*>(&kInstance);
}
