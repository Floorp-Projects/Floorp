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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

#include "nsUnknownDecoder.h"
#include "nsIServiceManager.h"
#include "nsIStreamConverterService.h"

#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsMimeTypes.h"
#include "netCore.h"
#include "nsXPIDLString.h"
#include "nsIPref.h"
#include "imgILoader.h"

#include "nsIMIMEService.h"

#include "nsIViewSourceChannel.h"

#include "prcpucfg.h" // To get IS_LITTLE_ENDIAN / IS_BIG_ENDIAN

#define MAX_BUFFER_SIZE 1024

static NS_DEFINE_IID(kPrefServiceCID, NS_PREF_CID);

#if defined WORDS_BIGENDIAN || defined IS_BIG_ENDIAN
#define LITTLE_TO_NATIVE16(x) ((((x) & 0xFF) << 8) | ((x) >> 8))
#else
#define LITTLE_TO_NATIVE16(x) x
#endif

nsUnknownDecoder::nsUnknownDecoder()
{
  mBuffer = nsnull;
  mBufferLen = 0;
  mRequireHTMLsuffix = PR_FALSE;

  nsresult rv;
  nsCOMPtr<nsIPref> pPrefService = do_GetService(kPrefServiceCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = pPrefService->GetBoolPref("security.requireHTMLsuffix", &mRequireHTMLsuffix);
  }


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
   NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
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
nsUnknownDecoder::OnDataAvailable(nsIRequest* request, 
                                  nsISupports *aCtxt,
                                  nsIInputStream *aStream, 
                                  PRUint32 aSourceOffset, 
                                  PRUint32 aCount)
{
  nsresult rv = NS_OK;

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

      DetermineContentType(request);

      NS_ASSERTION(!mContentType.IsEmpty(), 
                   "Content type should be known by now.");
      rv = FireListenerNotifications(request, aCtxt);
    }
  }

  if (aCount) {
    NS_ASSERTION(!mContentType.IsEmpty(), 
                 "Content type should be known by now.");

    rv = mNextListener->OnDataAvailable(request, aCtxt, aStream, 
                                        aSourceOffset, aCount);
  }

  return rv;
}

// ----
//
// nsIRequestObserver methods...
//
// ----

NS_IMETHODIMP
nsUnknownDecoder::OnStartRequest(nsIRequest* request, nsISupports *aCtxt) 
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
nsUnknownDecoder::OnStopRequest(nsIRequest* request, nsISupports *aCtxt,
                                nsresult aStatus)
{
  nsresult rv = NS_OK;

  if (!mNextListener) return NS_ERROR_FAILURE;

  //
  // The total amount of data is less than the size of the sniffer buffer.
  // Analyze the buffer now...
  //
  if (mContentType.IsEmpty()) {
    DetermineContentType(request);

    NS_ASSERTION(!mContentType.IsEmpty(), 
                 "Content type should be known by now.");
    rv = FireListenerNotifications(request, aCtxt);

    if (NS_FAILED(rv)) {
      aStatus = rv;
    }
  }

  rv = mNextListener->OnStopRequest(request, aCtxt, aStatus);
  mNextListener = 0;

  return rv;
}


void nsUnknownDecoder::DetermineContentType(nsIRequest* request)
{
  PRUint32 i;

  NS_ASSERTION(mContentType.IsEmpty(), "Content type is already known.");
  if (!mContentType.IsEmpty()) return;

  CBufDescriptor bufDesc((const char*)mBuffer, PR_TRUE, mBufferLen, mBufferLen);
  nsCAutoString str(bufDesc);

  NS_NAMED_LITERAL_CSTRING(pdfStart, "%PDF-");
  NS_NAMED_LITERAL_CSTRING(psStart1, "%!PS-Adobe-");
  NS_NAMED_LITERAL_CSTRING(psStart2, "%! PS-Adobe-");
  
  //
  // If the buffer begins with "%PDF-" it's PDF
  //
  if (Substring(str, 0, pdfStart.Length()) == pdfStart) {
    mContentType = APPLICATION_PDF;
  }
  //
  // If the buffer begins with "%!PS-Adobe-" or "%! PS-Adobe-" it's PostScript
  //
  else if (Substring(str, 0, psStart1.Length()) == psStart1 ||
           Substring(str, 0, psStart2.Length()) == psStart2) {
    mContentType = APPLICATION_POSTSCRIPT;
  }
  //
  // If the buffer begins with "#!" or "%!" then it is a script of
  // some sort...  "Scripts" can include arbitrary data to be passed
  // to an interpreter, so we check the buffer for nulls.
  //
  // This false match happened all the time...  For example, CGI scripts
  // written in sh or perl that emit HTML.
  //
  else if (Substring(str, 0, 2).Equals(NS_LITERAL_CSTRING("#!")) || 
           Substring(str, 0, 2).Equals(NS_LITERAL_CSTRING("%!"))) {
    for (i=0; i<mBufferLen && mBuffer[i]; i++);
    if (i == mBufferLen) {
      mContentType = TEXT_PLAIN;
    } else {
      mContentType = APPLICATION_OCTET_STREAM;
    }
  }
  //
  // If the buffer begins with a mailbox delimiter then it is not HTML
  //
  else if (Substring(str, 0, 5).Equals(NS_LITERAL_CSTRING("From "),
                                       nsCaseInsensitiveCStringComparator()) ||
           Substring(str, 0, 6).Equals(NS_LITERAL_CSTRING(">From "),
                                       nsCaseInsensitiveCStringComparator())) {
    mContentType = TEXT_PLAIN;
  }
  //
  // If the buffer contains "common" HTML tags then lets call it HTML :-)
  //
  else {

    /*
     * To prevent a possible attack, we will not consider this to be html 
     * content if it comes from the local file system
     */

    PRBool isLocalFile = PR_FALSE;
    if (request) {
      nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
      if (!aChannel) { NS_WARNING("QI failed"); return; }

      nsCOMPtr<nsIURI> pURL;
      if (NS_SUCCEEDED(aChannel->GetURI(getter_AddRefs(pURL))))
          pURL->SchemeIs("file", &isLocalFile);
    }

    if (!mRequireHTMLsuffix || !isLocalFile) {
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
              if (offset < 0) {
                offset = str.Find("<TABLE", PR_TRUE);
                if (offset < 0) {
                  offset = str.Find("<DIV", PR_TRUE);
                  if (offset < 0) {
                    offset = str.Find("<A HREF", PR_TRUE);
                    if (offset < 0) {
                      offset = str.Find("<APPLET", PR_TRUE);
                      if (offset < 0) {
                        offset = str.Find("<META", PR_TRUE);
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }

      if (offset >= 0) {
        mContentType = TEXT_HTML;
      }
    }
  }

  if (mContentType.IsEmpty()) {
    // sniff the first couple bytes to see if we are dealing with an image 
    // Unlike html, it is safe to sniff for image content types for local files so 
    // we don't need to check for isLocalFile here..
    SniffForImageMimeType((const char*)mBuffer, mBufferLen);
  }

  if (mContentType.IsEmpty()) {
    // We don't know what this is yet.  Before we just give up, try
    // the URI from the request.
    nsCOMPtr<nsIMIMEService> mimeService(do_GetService("@mozilla.org/mime;1"));
    if (mimeService) {
      nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
      if (channel) {
        nsCOMPtr<nsIURI> uri;
        nsresult result = channel->GetURI(getter_AddRefs(uri));
        if (NS_SUCCEEDED(result) && uri) {
          nsXPIDLCString type;
          result = mimeService->GetTypeFromURI(uri, getter_Copies(type));
          if (NS_SUCCEEDED(result)) {
            mContentType = type;
          }
        }
      }
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

// the following routine was ripped directly from some code in libpr0n...
void nsUnknownDecoder::SniffForImageMimeType(const char *buf, PRUint32 len)
{
  nsCOMPtr<imgILoader> loader(do_GetService("@mozilla.org/image/loader;1"));
  char* temp;
  loader->SupportImageWithContents(buf, len, &temp);
  if (temp)
    mContentType.Adopt(temp);
}


nsresult nsUnknownDecoder::FireListenerNotifications(nsIRequest* request,
                                                     nsISupports *aCtxt)
{
  nsresult rv = NS_OK;

  if (!mNextListener) return NS_ERROR_FAILURE;

  if (!mBuffer) return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIViewSourceChannel> viewSourceChannel = do_QueryInterface(request);
  if (viewSourceChannel) {
    rv = viewSourceChannel->SetOriginalContentType(mContentType);
  } else {
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request, &rv);
    if (NS_FAILED(rv)) return rv;
    
    // Set the new content type on the channel...
    rv = channel->SetContentType(mContentType);
  }

  NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to set content type on channel!");
  if (NS_FAILED(rv))
    return rv;

  // Fire the OnStartRequest(...)
  rv = mNextListener->OnStartRequest(request, aCtxt);

  // Fire the first OnDataAvailable for the data that was read from the
  // stream into the sniffer buffer...
  if (NS_SUCCEEDED(rv) && (mBufferLen > 0)) {
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
          rv = mNextListener->OnDataAvailable(request, aCtxt, in, 0, len);
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
