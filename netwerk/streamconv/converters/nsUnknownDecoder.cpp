/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsUnknownDecoder.h"

#include "nsIServiceManager.h"
#include "nsIStreamConverterService.h"

#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsMimeTypes.h"
#include "netCore.h"

#define MAX_BUFFER_SIZE 1024

static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);


nsUnknownDecoder::nsUnknownDecoder()
{
  NS_INIT_ISUPPORTS();

  mBuffer = nsnull;
  mBufferLen = 0;

}

nsUnknownDecoder::~nsUnknownDecoder()
{
  if (mBuffer) {
    delete [] mBuffer;
    mBuffer = nsnull;
  }
}

// ----
//
// nsISupports implementation...
//
// ----

NS_IMPL_ADDREF(nsUnknownDecoder);
NS_IMPL_RELEASE(nsUnknownDecoder);

NS_INTERFACE_MAP_BEGIN(nsUnknownDecoder)
   NS_INTERFACE_MAP_ENTRY(nsIStreamConverter)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
   NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


// ----
//
// nsIStreamConverter methods...
//
// ----

NS_IMETHODIMP
nsUnknownDecoder::Convert(nsIInputStream *aFromStream,
                          const PRUnichar *aFromType,
                          const PRUnichar *aToType,
                          nsISupports *aCtxt, 
                          nsIInputStream **aResultStream) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUnknownDecoder::AsyncConvertData(const PRUnichar *aFromType, 
                                   const PRUnichar *aToType,
                                   nsIStreamListener *aListener, 
                                   nsISupports *aCtxt)
{
  NS_ASSERTION(aListener && aFromType && aToType, 
               "null pointer passed into multi mixed converter");

  // hook up our final listener. this guy gets the various On*() calls we want to throw
  // at him.
  //
  mNextListener = aListener;
  return (aListener) ? NS_OK : NS_ERROR_FAILURE;
}

// ----
//
// nsIStreamListener methods...
//
// ----

NS_IMETHODIMP
nsUnknownDecoder::OnDataAvailable(nsIChannel *aChannel, 
                                  nsISupports *aCtxt,
                                  nsIInputStream *aStream, 
                                  PRUint32 aSourceOffset, 
                                  PRUint32 aCount)
{
  nsresult rv;

  if (!mNextListener) return NS_ERROR_FAILURE;

  if (mContentType.IsEmpty()) {
    PRUint32 count, len;

    // If the buffer has not been allocated by now, just fail...
    if (!mBuffer) return NS_ERROR_OUT_OF_MEMORY;

    //
    // Determine how much of the stream should be read to fill up the
    // sniffer buffer...
    //
    if (mBufferLen + aCount >= MAX_BUFFER_SIZE) {
      count = MAX_BUFFER_SIZE-mBufferLen;
    } else {
      count = aCount;
    }

    // Read the data into the buffer...
    rv = aStream->Read((mBuffer+mBufferLen), count, &len);
    if (NS_FAILED(rv)) return rv;

    mBufferLen += len;
    aCount     -= len;

    if (aCount) {
      //
      // Adjust the source offset...  The call to FireListenerNotifications(...)
      // will make the first OnDataAvailable(...) call with an offset of 0.
      // So, this offset needs to be adjusted to reflect that...
      //
      aSourceOffset += mBufferLen;

      DetermineContentType();

      NS_ASSERTION(!mContentType.IsEmpty(), 
                   "Content type should be known by now.");
      rv = FireListenerNotifications(aChannel, aCtxt);
    }
  }

  if (aCount) {
    NS_ASSERTION(!mContentType.IsEmpty(), 
                 "Content type should be known by now.");

    rv = mNextListener->OnDataAvailable(aChannel, aCtxt, aStream, 
                                        aSourceOffset, aCount);
  }

  return rv;
}

// ----
//
// nsIStreamObserver methods...
//
// ----

NS_IMETHODIMP
nsUnknownDecoder::OnStartRequest(nsIChannel *aChannel, nsISupports *aCtxt) 
{
  nsresult rv = NS_OK;

  if (!mNextListener) return NS_ERROR_FAILURE;

  // Allocate the sniffer buffer...
  if (NS_SUCCEEDED(rv) && !mBuffer) {
    mBuffer = new char[MAX_BUFFER_SIZE];

    if (!mBuffer) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Do not pass the OnStartRequest on to the next listener (yet)...
  return rv;
}

NS_IMETHODIMP
nsUnknownDecoder::OnStopRequest(nsIChannel *aChannel, nsISupports *aCtxt,
                                nsresult aStatus, const PRUnichar* aStatusArg)
{
  nsresult rv = NS_OK;

  if (!mNextListener) return NS_ERROR_FAILURE;

  //
  // The total amount of data is less than the size of the sniffer buffer.
  // Analyze the buffer now...
  //
  if (mContentType.IsEmpty()) {
    DetermineContentType();

    NS_ASSERTION(!mContentType.IsEmpty(), 
                 "Content type should be known by now.");
    rv = FireListenerNotifications(aChannel, aCtxt);

    if (NS_FAILED(rv)) {
      aStatus = rv;
    }
  }

  rv = mNextListener->OnStopRequest(aChannel, aCtxt, aStatus, aStatusArg);
  mNextListener = 0;

  return rv;
}


void nsUnknownDecoder::DetermineContentType()
{
  PRUint32 i;

  NS_ASSERTION(mContentType.IsEmpty(), "Content type is already known.");
  if (!mContentType.IsEmpty()) return;

  CBufDescriptor bufDesc((const char*)mBuffer, PR_TRUE, mBufferLen, mBufferLen);
  nsCAutoString str(bufDesc);

  //
  // If the buffer begins with "#!" or "%!" then it is a script of some
  // sort...
  //
  // This false match happened all the time...  For example, CGI scripts
  // written in sh or perl that emit HTML.
  //
  if (str.EqualsWithConversion("#!", PR_FALSE, 2) || 
      str.EqualsWithConversion("%!", PR_FALSE, 2)) {
    mContentType = TEXT_PLAIN;
  }
  //
  // If the buffer begins with a mailbox delimiter then it is not HTML
  //
  else if (str.EqualsWithConversion("From ", PR_TRUE, 5) || 
           str.EqualsWithConversion(">From ", PR_TRUE, 6)) {
    mContentType = TEXT_PLAIN;
  }
  //
  // If the buffer contains "common" HTML tags then lets call it HTML :-)
  //
  else {
    PRInt32 offset;

    offset = str.Find("<HTML", PR_TRUE);
    if (offset < 0) {
      offset = str.Find("<TITLE", PR_TRUE);
      if (offset < 0) {
        offset = str.Find("<FRAMESET", PR_TRUE);
        if (offset < 0) {
          offset = str.Find("<SCRIPT", PR_TRUE);
          if (offset < 0) {
            offset = str.Find("<BODY", PR_TRUE);
          }
        }
      }
    }

    if (offset >= 0) {
      mContentType = TEXT_HTML;
    }
  }

  //
  // See if the buffer has any embedded nulls.  If not, then lets just
  // call it text/plain...
  //
  if (mContentType.IsEmpty()) {
    for (i=0; i<mBufferLen && mBuffer[i]; i++);
    if (i == mBufferLen) {
      mContentType = TEXT_PLAIN;
    }
  }

  //
  // If the buffer is not text, then just call it application/octet-stream
  //
  if (mContentType.IsEmpty()) {
    mContentType = APPLICATION_OCTET_STREAM;
  }
}

nsresult nsUnknownDecoder::FireListenerNotifications(nsIChannel *aChannel,
                                                     nsISupports *aCtxt)
{
  nsresult rv = NS_OK;

  if (!mNextListener) return NS_ERROR_FAILURE;

  if (!mBuffer) return NS_ERROR_OUT_OF_MEMORY;

  // Set the new content type on the channel...
  aChannel->SetContentType(mContentType);

  // Fire the OnStartRequest(...)
  rv = mNextListener->OnStartRequest(aChannel, aCtxt);

  // Fire the first OnDataAvailable for the data that was read from the
  // stream into the sniffer buffer...
  if (NS_SUCCEEDED(rv)) {
    PRUint32 len = 0;
    nsCOMPtr<nsIInputStream> in;
    nsCOMPtr<nsIOutputStream> out;

    // Create a pipe and fill it with the data from the sniffer buffer.
    rv = NS_NewPipe(getter_AddRefs(in), getter_AddRefs(out),
                    MAX_BUFFER_SIZE, MAX_BUFFER_SIZE);

    if (NS_SUCCEEDED(rv)) {
      rv = out->Write(mBuffer, mBufferLen, &len);
      if (NS_SUCCEEDED(rv)) {
        if (len == mBufferLen) {
          rv = mNextListener->OnDataAvailable(aChannel, aCtxt, in, 0, len);
        } else {
          NS_ASSERTION(0, "Unable to write all the data into the pipe.");
          rv = NS_ERROR_FAILURE;
        }
      }
    }
  }

  delete [] mBuffer;
  mBuffer = nsnull;
  mBufferLen = 0;

  return rv;
}
