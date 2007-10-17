/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsScriptableInputStream.h"
#include "nsIProgrammingLanguage.h"
#include "nsAutoPtr.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsIUnicharLineInputStream.h"
#include "nsIClassInfoImpl.h"

// needed for NS_SWAP macros
#include "nsIStreamBufferAccess.h"

NS_IMPL_THREADSAFE_ADDREF(nsScriptableInputStream)
NS_IMPL_THREADSAFE_RELEASE(nsScriptableInputStream)
NS_IMPL_QUERY_INTERFACE5_CI(nsScriptableInputStream,
                            nsIInputStream,
                            nsIScriptableInputStream,
                            nsIScriptableIOInputStream,
                            nsISeekableStream,
                            nsIMultiplexInputStream)
NS_IMPL_CI_INTERFACE_GETTER5(nsScriptableInputStream,
                             nsIInputStream,
                             nsIScriptableInputStream,
                             nsIScriptableIOInputStream,
                             nsISeekableStream,
                             nsIMultiplexInputStream)

// nsIBaseStream methods
NS_IMETHODIMP
nsScriptableInputStream::Close(void)
{
  if (mUnicharInputStream)
    return mUnicharInputStream->Close();

  if (mInputStream)
    return mInputStream->Close();

  return NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
nsScriptableInputStream::InitWithStreams(nsIInputStream *aInputStream,
                                         nsIUnicharInputStream *aCharStream)
{
  NS_ENSURE_ARG(aInputStream);
  mInputStream = aInputStream;
  mUnicharInputStream = aCharStream;
  return NS_OK;
}

// nsIScriptableInputStream methods
NS_IMETHODIMP
nsScriptableInputStream::Init(nsIInputStream *aInputStream)
{
  NS_ENSURE_ARG(aInputStream);
  mInputStream = aInputStream;
  mUnicharInputStream = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptableInputStream::Available(PRUint32 *aIsAvailable)
{
  NS_ENSURE_TRUE(mInputStream, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = mInputStream->Available(aIsAvailable);
  NS_ENSURE_SUCCESS(rv, rv);

  // if there is no data left in the stream, check if the character stream
  // has more text available. This is needed because the unichar stream
  // will read a buffer from the input stream, leaving it with no remaining
  // data, yet there is still text to read
  if (!*aIsAvailable && mUnicharInputStream)
    *aIsAvailable = mUnicharInputStreamHasMore;

  return NS_OK;
}

NS_IMETHODIMP
nsScriptableInputStream::Read(PRUint32 aCount, char **_retval)
{
  nsresult rv = NS_OK;
  PRUint32 count = 0;
  char *buffer = nsnull;

  NS_ENSURE_TRUE(mInputStream, NS_ERROR_NOT_INITIALIZED);

  rv = mInputStream->Available(&count);
  if (NS_FAILED(rv))
    return rv;

  count = PR_MIN(count, aCount);
  buffer = (char*)NS_Alloc(count+1); // make room for '\0'
  NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 amtRead = 0;
  rv = mInputStream->Read(buffer, count, &amtRead);
  if (NS_FAILED(rv)) {
      NS_Free(buffer);
      return rv;
  }

  buffer[amtRead] = '\0';
  *_retval = buffer;
  return NS_OK;
}

// nsIInputStream methods

NS_IMETHODIMP
nsScriptableInputStream::IsNonBlocking(PRBool *aIsNonBlocking)
{
  NS_ENSURE_TRUE(mInputStream, NS_ERROR_NOT_INITIALIZED);
  return mInputStream->IsNonBlocking(aIsNonBlocking);
}

NS_IMETHODIMP
nsScriptableInputStream::Read(char* aData,
                              PRUint32 aCount,
                              PRUint32 *aReadCount)
{
  if (mUnicharInputStream) {
    // XXXndeakin implement this
    return NS_OK;
  }

  if (mInputStream)
    return mInputStream->Read(aData, aCount, aReadCount);

  return NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
nsScriptableInputStream::ReadSegments(nsWriteSegmentFun aFn,
                                      void* aClosure,
                                      PRUint32 aCount,
                                      PRUint32 *aReadCount)
{
  NS_ENSURE_TRUE(mInputStream, NS_ERROR_NOT_INITIALIZED);
  return mInputStream->ReadSegments(aFn, aClosure, aCount, aReadCount);
}

NS_IMETHODIMP
nsScriptableInputStream::ReadString(PRUint32 aCount, nsAString& aString)
{
  if (mUnicharInputStream) {
    PRUint32 readCount;
    nsresult rv = mUnicharInputStream->ReadString(aCount, aString, &readCount);
    NS_ENSURE_SUCCESS(rv, rv);

    // If less characters are read than requested, then assume that the end
    // of the file has been reached.
    // XXXndeakin or it could be because the buffer is full
    mUnicharInputStreamHasMore = (aCount == readCount);
    return NS_OK;
  }

  // just call Read and convert to UTF-16
  nsXPIDLCString cstr;
  nsresult rv = Read(aCount, getter_Copies(cstr));
  NS_ENSURE_SUCCESS(rv, rv);
  aString.Assign(NS_ConvertASCIItoUTF16(cstr));
  return NS_OK;
}

NS_IMETHODIMP
nsScriptableInputStream::ReadLine(nsAString& aLine)
{
  nsCOMPtr<nsIUnicharLineInputStream> cstream = do_QueryInterface(mUnicharInputStream);
  NS_ENSURE_TRUE(cstream, NS_ERROR_NOT_AVAILABLE);

  return cstream->ReadLine(aLine, &mUnicharInputStreamHasMore);
}

NS_IMETHODIMP
nsScriptableInputStream::ReadBoolean(PRBool* aBoolean)
{
  PRUint8 byteResult;
  nsresult rv = Read8(&byteResult);
  *aBoolean = byteResult;
  return rv;
}

NS_IMETHODIMP
nsScriptableInputStream::Read8(PRUint8* aVal)
{
  return ReadFully(sizeof *aVal, reinterpret_cast<char*>(aVal));
}

NS_IMETHODIMP
nsScriptableInputStream::Read16(PRUint16* aVal)
{
  nsresult rv = ReadFully(sizeof *aVal, reinterpret_cast<char*>(aVal));
  NS_ENSURE_SUCCESS(rv, rv);
  *aVal = NS_SWAP16(*aVal);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptableInputStream::Read32(PRUint32* aVal)
{
  nsresult rv = ReadFully(sizeof *aVal, reinterpret_cast<char*>(aVal));
  NS_ENSURE_SUCCESS(rv, rv);
  *aVal = NS_SWAP32(*aVal);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptableInputStream::ReadFloat(float* aFloat)
{
  NS_ASSERTION(sizeof(float) == sizeof (PRUint32),
               "False assumption about sizeof(float)");
  return Read32(reinterpret_cast<PRUint32*>(aFloat));
}

NS_IMETHODIMP
nsScriptableInputStream::ReadDouble(double* aDouble)
{
  NS_ASSERTION(sizeof(double) == sizeof(PRUint64),
               "False assumption about sizeof(double)");

  nsresult rv = ReadFully(sizeof(double), reinterpret_cast<char*>(aDouble));

  NS_ENSURE_SUCCESS(rv, rv);
  PRUint64 i = NS_SWAP64(*reinterpret_cast<PRUint64*>(aDouble));
  *aDouble = *reinterpret_cast<double*>(&i);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptableInputStream::ReadByteArray(PRUint32 aCount, PRUint8 **aBytes)
{
  char* s = reinterpret_cast<char*>(NS_Alloc(aCount));
  NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 bytesRead;
  nsresult rv = mInputStream->Read(s, aCount, &bytesRead);
  if (NS_FAILED(rv)) {
    NS_Free(s);
    return rv;
  }
  if (bytesRead != aCount) {
    NS_Free(s);
    return NS_ERROR_FAILURE;
  }

  *aBytes = (PRUint8 *)s;
  return NS_OK;
}

nsresult
nsScriptableInputStream::ReadFully(PRUint32 aCount, char* aBuf)
{
  PRUint32 bytesRead;
  nsresult rv = Read(aBuf, aCount, &bytesRead);
  NS_ENSURE_SUCCESS(rv, rv);
  return (bytesRead != aCount) ? NS_ERROR_FAILURE : NS_OK;
}

// nsISeekableStream
NS_IMETHODIMP
nsScriptableInputStream::Seek(PRInt32 aWhence, PRInt64 aOffset)
{
  nsCOMPtr<nsISeekableStream> cstream = do_QueryInterface(mInputStream);
  NS_ENSURE_TRUE(cstream, NS_ERROR_NOT_AVAILABLE);
  return cstream->Seek(aWhence, aOffset);
}

NS_IMETHODIMP
nsScriptableInputStream::Tell(PRInt64* aOffset)
{
  nsCOMPtr<nsISeekableStream> cstream = do_QueryInterface(mInputStream);
  NS_ENSURE_TRUE(cstream, NS_ERROR_NOT_AVAILABLE);
  return cstream->Tell(aOffset);
}

NS_IMETHODIMP
nsScriptableInputStream::SetEOF()
{
  nsCOMPtr<nsISeekableStream> cstream = do_QueryInterface(mInputStream);
  NS_ENSURE_TRUE(cstream, NS_ERROR_NOT_AVAILABLE);
  return cstream->SetEOF();
}

// nsIMultiplexInputStream
NS_IMETHODIMP
nsScriptableInputStream::GetCount(PRUint32* aCount)
{
  nsCOMPtr<nsIMultiplexInputStream> cstream = do_QueryInterface(mInputStream);
  NS_ENSURE_TRUE(cstream, NS_ERROR_NOT_AVAILABLE);
  return cstream->GetCount(aCount);
}

NS_IMETHODIMP
nsScriptableInputStream::GetStream(PRUint32 aIndex, nsIInputStream** aStream)
{
  nsCOMPtr<nsIMultiplexInputStream> cstream = do_QueryInterface(mInputStream);
  NS_ENSURE_TRUE(cstream, NS_ERROR_NOT_AVAILABLE);
  return cstream->GetStream(aIndex, aStream);
}

NS_IMETHODIMP
nsScriptableInputStream::AppendStream(nsIInputStream* aStream)
{
  nsCOMPtr<nsIMultiplexInputStream> cstream = do_QueryInterface(mInputStream);
  NS_ENSURE_TRUE(cstream, NS_ERROR_NOT_AVAILABLE);
  return cstream->AppendStream(aStream);
}

NS_IMETHODIMP
nsScriptableInputStream::InsertStream(nsIInputStream* aStream, PRUint32 aIndex)
{
  nsCOMPtr<nsIMultiplexInputStream> cstream = do_QueryInterface(mInputStream);
  NS_ENSURE_TRUE(cstream, NS_ERROR_NOT_AVAILABLE);
  return cstream->InsertStream(aStream, aIndex);
}

NS_IMETHODIMP
nsScriptableInputStream::RemoveStream(PRUint32 aIndex)
{
  nsCOMPtr<nsIMultiplexInputStream> cstream = do_QueryInterface(mInputStream);
  NS_ENSURE_TRUE(cstream, NS_ERROR_NOT_AVAILABLE);
  return cstream->RemoveStream(aIndex);
}

NS_METHOD
nsScriptableInputStream::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  nsRefPtr<nsScriptableInputStream> sis = new nsScriptableInputStream();
  NS_ENSURE_TRUE(sis, NS_ERROR_OUT_OF_MEMORY);
  return sis->QueryInterface(aIID, aResult);
}
