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

#include "nsScriptableOutputStream.h"
#include "nsIProgrammingLanguage.h"
#include "nsAutoPtr.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsIClassInfoImpl.h"

// needed for NS_SWAP macros
#include "nsIStreamBufferAccess.h"

NS_IMPL_THREADSAFE_ADDREF(nsScriptableOutputStream)
NS_IMPL_THREADSAFE_RELEASE(nsScriptableOutputStream)
NS_IMPL_QUERY_INTERFACE3_CI(nsScriptableOutputStream,
                            nsIOutputStream,
                            nsIScriptableIOOutputStream,
                            nsISeekableStream)
NS_IMPL_CI_INTERFACE_GETTER3(nsScriptableOutputStream,
                             nsIOutputStream,
                             nsIScriptableIOOutputStream,
                             nsISeekableStream)

// nsIBaseStream methods
NS_IMETHODIMP
nsScriptableOutputStream::Close(void)
{
  if (mUnicharOutputStream)
    return mUnicharOutputStream->Close();

  if (mOutputStream)
    return mOutputStream->Close();

  return NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
nsScriptableOutputStream::Flush(void)
{
  if (mUnicharOutputStream)
    return mUnicharOutputStream->Flush();

  if (mOutputStream)
    return mOutputStream->Flush();

  return NS_ERROR_NOT_INITIALIZED;
}

// nsIScriptableIOOutputStream methods

NS_IMETHODIMP
nsScriptableOutputStream::InitWithStreams(nsIOutputStream* aOutputStream,
                                          nsIUnicharOutputStream *aCharStream)
{
  NS_ENSURE_ARG(aOutputStream);

  mOutputStream = aOutputStream;
  mUnicharOutputStream = aCharStream;
  return NS_OK;
}

// nsIOutputStream methods

NS_IMETHODIMP
nsScriptableOutputStream::IsNonBlocking(PRBool *aIsNonBlocking)
{
  if (mOutputStream)
    return mOutputStream->IsNonBlocking(aIsNonBlocking);

  if (mUnicharOutputStream) {
    *aIsNonBlocking = PR_FALSE;
    return NS_OK;
  }

  return NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
nsScriptableOutputStream::Write(const char* aBuffer, PRUint32 aCount, PRUint32 *aWriteCount)
{
  if (mUnicharOutputStream) {
    nsAutoString str(NS_ConvertASCIItoUTF16(aBuffer, aCount));
    PRBool ok;
    mUnicharOutputStream->WriteString(str, &ok);
    *aWriteCount = ok ? aCount : 0;
    return NS_OK;
  }

  if (mOutputStream)
    return mOutputStream->Write(aBuffer, aCount, aWriteCount);

  return NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
nsScriptableOutputStream::WriteFrom(nsIInputStream *aStream, PRUint32 aCount, PRUint32 *aWriteCount)
{
  if (mOutputStream)
    return mOutputStream->WriteFrom(aStream, aCount, aWriteCount);

  return NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
nsScriptableOutputStream::WriteSegments(nsReadSegmentFun aFn,
                                        void* aClosure,
                                        PRUint32 aCount,
                                        PRUint32 *aReadCount)
{
  NS_ENSURE_TRUE(mOutputStream, NS_ERROR_NOT_INITIALIZED);
  return mOutputStream->WriteSegments(aFn, aClosure, aCount, aReadCount);
}

NS_IMETHODIMP
nsScriptableOutputStream::WriteString(const nsAString& aString, PRBool *aOK)
{
  if (mUnicharOutputStream)
    return mUnicharOutputStream->WriteString(aString, aOK);

  if (!mOutputStream)
    return NS_ERROR_NOT_INITIALIZED;

  // just convert to ASCII and call Write
  nsCAutoString cstr = NS_LossyConvertUTF16toASCII(aString);
  PRUint32 count;
  nsresult rv = mOutputStream->Write(cstr.get(), (PRUint32)cstr.Length(), &count);
  NS_ENSURE_SUCCESS(rv, rv);
  *aOK = (count == cstr.Length());
  return NS_OK;
}

NS_IMETHODIMP
nsScriptableOutputStream::WriteBoolean(PRBool aBoolean)
{
  return Write8(aBoolean);
}

NS_IMETHODIMP
nsScriptableOutputStream::Write8(PRUint8 aByte)
{
  return WriteFully((const char *)&aByte, sizeof aByte);
}

NS_IMETHODIMP
nsScriptableOutputStream::Write16(PRUint16 a16)
{
  a16 = NS_SWAP16(a16);
  return WriteFully((const char *)&a16, sizeof a16);
}

NS_IMETHODIMP
nsScriptableOutputStream::Write32(PRUint32 a32)
{
  a32 = NS_SWAP32(a32);
  return WriteFully((const char *)&a32, sizeof a32);
}

NS_IMETHODIMP
nsScriptableOutputStream::WriteFloat(float aFloat)
{
  NS_ASSERTION(sizeof(float) == sizeof (PRUint32),
               "False assumption about sizeof(float)");
  return Write32(*reinterpret_cast<PRUint32*>(&aFloat));
}

NS_IMETHODIMP
nsScriptableOutputStream::WriteDouble(double aDouble)
{
  NS_ASSERTION(sizeof(double) == sizeof(PRUint64),
               "False assumption about sizeof(double)");

  PRUint64 val = NS_SWAP64(*reinterpret_cast<PRUint64*>(&aDouble));
  return WriteFully(reinterpret_cast<char*>(&val), sizeof val);
}

NS_IMETHODIMP
nsScriptableOutputStream::WriteByteArray(PRUint8 *aBytes, PRUint32 aCount)
{
  return WriteFully((char *)aBytes, aCount);
}

nsresult
nsScriptableOutputStream::WriteFully(const char *aBuf, PRUint32 aCount)
{
  NS_ENSURE_TRUE(mOutputStream, NS_ERROR_NOT_INITIALIZED);

  PRUint32 bytesWritten;
  nsresult rv = mOutputStream->Write(aBuf, aCount, &bytesWritten);
  NS_ENSURE_SUCCESS(rv, rv);
  return (bytesWritten != aCount) ? NS_ERROR_FAILURE : NS_OK;
}

// nsISeekableStream
NS_IMETHODIMP
nsScriptableOutputStream::Seek(PRInt32 aWhence, PRInt64 aOffset)
{
  nsCOMPtr<nsISeekableStream> cstream = do_QueryInterface(mOutputStream);
  NS_ENSURE_TRUE(cstream, NS_ERROR_NOT_AVAILABLE);
  return cstream->Seek(aWhence, aOffset);
}

NS_IMETHODIMP
nsScriptableOutputStream::Tell(PRInt64* aOffset)
{
  nsCOMPtr<nsISeekableStream> cstream = do_QueryInterface(mOutputStream);
  NS_ENSURE_TRUE(cstream, NS_ERROR_NOT_AVAILABLE);
  return cstream->Tell(aOffset);
}

NS_IMETHODIMP
nsScriptableOutputStream::SetEOF()
{
  nsCOMPtr<nsISeekableStream> cstream = do_QueryInterface(mOutputStream);
  NS_ENSURE_TRUE(cstream, NS_ERROR_NOT_AVAILABLE);
  return cstream->SetEOF();
}

NS_METHOD
nsScriptableOutputStream::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  nsRefPtr<nsScriptableOutputStream> sos = new nsScriptableOutputStream();
  NS_ENSURE_TRUE(sos, NS_ERROR_OUT_OF_MEMORY);
  return sos->QueryInterface(aIID, aResult);
}
