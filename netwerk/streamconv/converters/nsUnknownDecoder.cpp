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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIContentSniffer.h"

#include "nsCRT.h"

#include "nsIMIMEService.h"

#include "nsIViewSourceChannel.h"

#include "prcpucfg.h" // To get IS_LITTLE_ENDIAN / IS_BIG_ENDIAN

#define MAX_BUFFER_SIZE 1024

#if defined WORDS_BIGENDIAN || defined IS_BIG_ENDIAN
#define LITTLE_TO_NATIVE16(x) ((((x) & 0xFF) << 8) | ((x) >> 8))
#else
#define LITTLE_TO_NATIVE16(x) x
#endif

nsUnknownDecoder::nsUnknownDecoder()
  : mBuffer(nsnull)
  , mBufferLen(0)
  , mRequireHTMLsuffix(PR_FALSE)
{
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    PRBool val;
    if (NS_SUCCEEDED(prefs->GetBoolPref("security.requireHTMLsuffix", &val)))
      mRequireHTMLsuffix = val;
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

NS_IMPL_ADDREF(nsUnknownDecoder)
NS_IMPL_RELEASE(nsUnknownDecoder)

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
                          const char *aFromType,
                          const char *aToType,
                          nsISupports *aCtxt, 
                          nsIInputStream **aResultStream) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUnknownDecoder::AsyncConvertData(const char *aFromType, 
                                   const char *aToType,
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

// Actual sniffing code

PRBool nsUnknownDecoder::AllowSniffing(nsIRequest* aRequest)
{
  if (!mRequireHTMLsuffix) {
    return PR_TRUE;
  }
  
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (!channel) {
    NS_ERROR("QI failed");
    return PR_FALSE;
  }

  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(channel->GetURI(getter_AddRefs(uri))) || !uri) {
    return PR_FALSE;
  }
  
  PRBool isLocalFile = PR_FALSE;
  if (NS_FAILED(uri->SchemeIs("file", &isLocalFile)) || isLocalFile) {
    return PR_FALSE;
  }

  return PR_TRUE;
}

/**
 * This is the array of sniffer entries that depend on "magic numbers"
 * in the file.  Each entry has either a type associated with it (set
 * these with the SNIFFER_ENTRY macro) or a function to be executed
 * (set these with the SNIFFER_ENTRY_WITH_FUNC macro).  The function
 * should take a single nsIRequest* and returns PRBool -- PR_TRUE if
 * it sets mContentType, PR_FALSE otherwise
 */
nsUnknownDecoder::nsSnifferEntry nsUnknownDecoder::sSnifferEntries[] = {
  SNIFFER_ENTRY("%PDF-", APPLICATION_PDF),

  SNIFFER_ENTRY("%!PS-Adobe-", APPLICATION_POSTSCRIPT),
  SNIFFER_ENTRY("%! PS-Adobe-", APPLICATION_POSTSCRIPT),

  // Files that start with mailbox delimeters let's provisionally call
  // text/plain
  SNIFFER_ENTRY("From", TEXT_PLAIN),
  SNIFFER_ENTRY(">From", TEXT_PLAIN),

  // If the buffer begins with "#!" or "%!" then it is a script of
  // some sort...  "Scripts" can include arbitrary data to be passed
  // to an interpreter, so we need to decide whether we can call this
  // text or whether it's data.
  SNIFFER_ENTRY_WITH_FUNC("#!", &nsUnknownDecoder::LastDitchSniff),

  // XXXbz should (and can) we also include the various ways that <?xml can
  // appear as UTF-16 and such?  See http://www.w3.org/TR/REC-xml#sec-guessing
  SNIFFER_ENTRY_WITH_FUNC("<?xml", &nsUnknownDecoder::SniffForXML)
};

PRUint32 nsUnknownDecoder::sSnifferEntryNum =
  sizeof(nsUnknownDecoder::sSnifferEntries) /
    sizeof(nsUnknownDecoder::nsSnifferEntry);

void nsUnknownDecoder::DetermineContentType(nsIRequest* aRequest)
{
  NS_ASSERTION(mContentType.IsEmpty(), "Content type is already known.");
  if (!mContentType.IsEmpty()) return;

  // First, run through all the types we can detect reliably based on
  // magic numbers
  PRUint32 i;
  for (i = 0; i < sSnifferEntryNum; ++i) {
    if (mBufferLen >= sSnifferEntries[i].mByteLen &&  // enough data
        memcmp(mBuffer, sSnifferEntries[i].mBytes, sSnifferEntries[i].mByteLen) == 0) {  // and type matches
      NS_ASSERTION(sSnifferEntries[i].mMimeType ||
                   sSnifferEntries[i].mContentTypeSniffer,
                   "Must have either a type string or a function to set the type");
      NS_ASSERTION(!sSnifferEntries[i].mMimeType ||
                   !sSnifferEntries[i].mContentTypeSniffer,
                   "Both at type string and a type sniffing function set;"
                   " using type string");
      if (sSnifferEntries[i].mMimeType) {
        mContentType = sSnifferEntries[i].mMimeType;
        return;
      }
      else if ((this->*(sSnifferEntries[i].mContentTypeSniffer))(aRequest)) {
        return;
      }        
    }
  }

  if (TryContentSniffers(aRequest)) {
    return;
  }

  if (SniffForHTML(aRequest)) {
    return;
  }
  
  // We don't know what this is yet.  Before we just give up, try
  // the URI from the request.
  if (SniffURI(aRequest)) {
    return;
  }
  
  LastDitchSniff(aRequest);
}

PRBool nsUnknownDecoder::TryContentSniffers(nsIRequest* aRequest)
{
  // Enumerate content sniffers
  nsCOMPtr<nsICategoryManager> catMan(do_GetService("@mozilla.org/categorymanager;1"));
  if (!catMan) {
    return PR_FALSE;
  }

  nsCOMPtr<nsISimpleEnumerator> sniffers;
  catMan->EnumerateCategory("content-sniffing-services", getter_AddRefs(sniffers));
  if (!sniffers) {
    return PR_FALSE;
  }

  PRBool hasMore;
  while (NS_SUCCEEDED(sniffers->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> elem;
    sniffers->GetNext(getter_AddRefs(elem));
    NS_ASSERTION(elem, "No element even though hasMore returned true!?");

    nsCOMPtr<nsISupportsCString> sniffer_id(do_QueryInterface(elem));
    NS_ASSERTION(sniffer_id, "element is no nsISupportsCString!?");
    nsCAutoString contractid;
    nsresult rv = sniffer_id->GetData(contractid);
    if (NS_FAILED(rv)) {
      continue;
    }

    nsCOMPtr<nsIContentSniffer> sniffer(do_GetService(contractid.get()));
    if (!sniffer) {
      continue;
    }

    rv = sniffer->GetMIMETypeFromContent((const PRUint8*)mBuffer, mBufferLen, mContentType);
    if (NS_SUCCEEDED(rv)) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

PRBool nsUnknownDecoder::SniffForHTML(nsIRequest* aRequest)
{
  /*
   * To prevent a possible attack, we will not consider this to be
   * html content if it comes from the local file system and our prefs
   * are set right
   */
  if (!AllowSniffing(aRequest)) {
    return PR_FALSE;
  }
  
  // Now look for HTML.
  const char* str = mBuffer;
  const char* end = mBuffer + mBufferLen;

  // skip leading whitespace
  while (str != end && nsCRT::IsAsciiSpace(*str)) {
    ++str;
  }

  // did we find something like a start tag?
  if (str == end || *str != '<' || ++str == end) {
    return PR_FALSE;
  }

  // If we seem to be SGML or XML and we got down here, just pretend we're HTML
  if (*str == '!' || *str == '?') {
    mContentType = TEXT_HTML;
    return PR_TRUE;
  }
  
  PRUint32 bufSize = end - str;
  // We use sizeof(_tagstr) below because that's the length of _tagstr
  // with the one char " " or ">" appended.
#define MATCHES_TAG(_tagstr)                                              \
  (bufSize >= sizeof(_tagstr) &&                                          \
   (PL_strncasecmp(str, _tagstr " ", sizeof(_tagstr)) == 0 ||             \
    PL_strncasecmp(str, _tagstr ">", sizeof(_tagstr)) == 0))
    
  if (MATCHES_TAG("html")     ||
      MATCHES_TAG("frameset") ||
      MATCHES_TAG("body")     ||
      MATCHES_TAG("head")     ||
      MATCHES_TAG("script")   ||
      MATCHES_TAG("iframe")   ||
      MATCHES_TAG("a")        ||
      MATCHES_TAG("img")      ||
      MATCHES_TAG("table")    ||
      MATCHES_TAG("title")    ||
      MATCHES_TAG("link")     ||
      MATCHES_TAG("base")     ||
      MATCHES_TAG("style")    ||
      MATCHES_TAG("div")      ||
      MATCHES_TAG("p")        ||
      MATCHES_TAG("font")     ||
      MATCHES_TAG("applet")   ||
      MATCHES_TAG("meta")     ||
      MATCHES_TAG("center")   ||
      MATCHES_TAG("form")     ||
      MATCHES_TAG("isindex")  ||
      MATCHES_TAG("h1")       ||
      MATCHES_TAG("h2")       ||
      MATCHES_TAG("h3")       ||
      MATCHES_TAG("h4")       ||
      MATCHES_TAG("h5")       ||
      MATCHES_TAG("h6")       ||
      MATCHES_TAG("b")        ||
      MATCHES_TAG("pre")) {
  
    mContentType = TEXT_HTML;
    return PR_TRUE;
  }

#undef MATCHES_TAG
  
  return PR_FALSE;
}

PRBool nsUnknownDecoder::SniffForXML(nsIRequest* aRequest)
{
  // Just like HTML, this should be able to be shut off.
  if (!AllowSniffing(aRequest)) {
    return PR_FALSE;
  }

  // First see whether we can glean anything from the uri...
  if (!SniffURI(aRequest)) {
    // Oh well; just generic XML will have to do
    mContentType = TEXT_XML;
  }
  
  return PR_TRUE;
}

PRBool nsUnknownDecoder::SniffURI(nsIRequest* aRequest)
{
  nsCOMPtr<nsIMIMEService> mimeService(do_GetService("@mozilla.org/mime;1"));
  if (mimeService) {
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
    if (channel) {
      nsCOMPtr<nsIURI> uri;
      nsresult result = channel->GetURI(getter_AddRefs(uri));
      if (NS_SUCCEEDED(result) && uri) {
        nsCAutoString type;
        result = mimeService->GetTypeFromURI(uri, type);
        if (NS_SUCCEEDED(result)) {
          mContentType = type;
          return PR_TRUE;
        }
      }
    }
  }

  return PR_FALSE;
}

// This macro is based on RFC 2046 Section 4.1.2.  Treat any char 0-31
// except the 9-13 range (\t, \n, \v, \f, \r) and char 27 (used by
// encodings like Shift_JIS) as non-text
#define IS_TEXT_CHAR(ch)                                     \
  (((unsigned char)(ch)) > 31 || (9 <= (ch) && (ch) <= 13) || (ch) == 27)

PRBool nsUnknownDecoder::LastDitchSniff(nsIRequest* aRequest)
{
  // All we can do now is try to guess whether this is text/plain or
  // application/octet-stream

  // First, check for a BOM.  If we see one, assume this is text/plain
  // in whatever encoding.  If there is a BOM _and_ text we will
  // always have at least 4 bytes in the buffer (since the 2-byte BOMs
  // are for 2-byte encodings and the UTF-8 BOM is 3 bytes).
  if (mBufferLen >= 4) {
    const unsigned char* buf = (const unsigned char*)mBuffer;
    if ((buf[0] == 0xFE && buf[1] == 0xFF) || // UTF-16BE
        (buf[0] == 0xFF && buf[1] == 0xFE) || // UTF-16LE
        (buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) || // UTF-8
        (buf[0] == 0 && buf[1] == 0 && buf[2] == 0xFE && buf[3] == 0xFF) || // UCS-4BE
        (buf[0] == 0 && buf[1] == 0 && buf[2] == 0xFF && buf[3] == 0xFE)) { // UCS-4
        
      mContentType = TEXT_PLAIN;
      return PR_TRUE;
    }
  }
  
  // Now see whether the buffer has any non-text chars.  If not, then let's
  // just call it text/plain...
  //
  PRUint32 i;
  for (i=0; i<mBufferLen && IS_TEXT_CHAR(mBuffer[i]); i++);

  if (i == mBufferLen) {
    mContentType = TEXT_PLAIN;
  }
  else {
    mContentType = APPLICATION_OCTET_STREAM;
  }

  return PR_TRUE;    
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

void
nsBinaryDetector::DetermineContentType(nsIRequest* aRequest)
{
  LastDitchSniff(aRequest);
  if (mContentType.Equals(APPLICATION_OCTET_STREAM)) {
    // We want to guess at it instead
    mContentType = APPLICATION_GUESS_FROM_EXT;
  }
}
