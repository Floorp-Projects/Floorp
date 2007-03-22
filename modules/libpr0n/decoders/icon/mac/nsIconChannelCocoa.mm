/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Brian Ryner.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor          <mscott@mozilla.org>
 *   Robert John Churchill    <rjc@netscape.com>
 *   Josh Aas                 <josh@mozilla.com>
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


#include "nsIconChannel.h"
#include "nsIIconURI.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsXPIDLString.h"
#include "nsMimeTypes.h"
#include "nsMemory.h"
#include "nsIStringStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "plstr.h"
#include "nsILocalFileMac.h"
#include "nsIFileURL.h"
#include "nsInt64.h"
#include "nsAutoBuffer.h"

#include <Cocoa/Cocoa.h>

// nsIconChannel methods
nsIconChannel::nsIconChannel()
{
}

nsIconChannel::~nsIconChannel() 
{}

NS_IMPL_THREADSAFE_ISUPPORTS4(nsIconChannel, 
                              nsIChannel, 
                              nsIRequest,
			       nsIRequestObserver,
			       nsIStreamListener)

nsresult nsIconChannel::Init(nsIURI* uri)
{
  NS_ASSERTION(uri, "no uri");
  mUrl = uri;
  
  nsresult rv;
  mPump = do_CreateInstance(NS_INPUTSTREAMPUMP_CONTRACTID, &rv);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP nsIconChannel::GetName(nsACString &result)
{
  return mUrl->GetSpec(result);
}

NS_IMETHODIMP nsIconChannel::IsPending(PRBool *result)
{
  return mPump->IsPending(result);
}

NS_IMETHODIMP nsIconChannel::GetStatus(nsresult *status)
{
  return mPump->GetStatus(status);
}

NS_IMETHODIMP nsIconChannel::Cancel(nsresult status)
{
  return mPump->Cancel(status);
}

NS_IMETHODIMP nsIconChannel::Suspend(void)
{
  return mPump->Suspend();
}

NS_IMETHODIMP nsIconChannel::Resume(void)
{
  return mPump->Resume();
}

// nsIRequestObserver methods
NS_IMETHODIMP nsIconChannel::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  if (mListener)
    return mListener->OnStartRequest(this, aContext);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext, nsresult aStatus)
{
  if (mListener) {
    mListener->OnStopRequest(this, aContext, aStatus);
    mListener = nsnull;
  }

  // Remove from load group
  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nsnull, aStatus);

  return NS_OK;
}

// nsIStreamListener methods
NS_IMETHODIMP nsIconChannel::OnDataAvailable(nsIRequest* aRequest,
                                             nsISupports* aContext,
                                             nsIInputStream* aStream,
                                             PRUint32 aOffset,
                                             PRUint32 aCount)
{
  if (mListener)
    return mListener->OnDataAvailable(this, aContext, aStream, aOffset, aCount);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP nsIconChannel::GetOriginalURI(nsIURI* *aURI)
{
  *aURI = mOriginalURI ? mOriginalURI : mUrl;
  NS_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetOriginalURI(nsIURI* aURI)
{
  mOriginalURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetURI(nsIURI* *aURI)
{
  *aURI = mUrl;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::Open(nsIInputStream **_retval)
{
  return MakeInputStream(_retval, PR_FALSE);
}

nsresult nsIconChannel::ExtractIconInfoFromUrl(nsIFile ** aLocalFile, PRUint32 * aDesiredImageSize, nsACString &aContentType, nsACString &aFileExtension)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMozIconURI> iconURI (do_QueryInterface(mUrl, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  iconURI->GetImageSize(aDesiredImageSize);
  iconURI->GetContentType(aContentType);
  iconURI->GetFileExtension(aFileExtension);
  
  nsCOMPtr<nsIURI> fileURI;
  rv = iconURI->GetIconFile(getter_AddRefs(fileURI));
  if (NS_FAILED(rv) || !fileURI) return NS_OK;

  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(fileURI, &rv);
  if (NS_FAILED(rv) || !fileURL) return NS_OK;

  nsCOMPtr<nsIFile> file;
  rv = fileURL->GetFile(getter_AddRefs(file));
  if (NS_FAILED(rv) || !file) return NS_OK;
  
  nsCOMPtr<nsILocalFileMac> localFileMac (do_QueryInterface(file, &rv));
  if (NS_FAILED(rv) || !localFileMac) return NS_OK;
  
  *aLocalFile = file;
  NS_IF_ADDREF(*aLocalFile);

  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
  nsCOMPtr<nsIInputStream> inStream;
  nsresult rv = MakeInputStream(getter_AddRefs(inStream), PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Init our stream pump
  rv = mPump->Init(inStream, nsInt64(-1), nsInt64(-1), 0, 0, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = mPump->AsyncRead(this, ctxt);
  if (NS_SUCCEEDED(rv)) {
    // Store our real listener
    mListener = aListener;
    // Add ourself to the load group, if available
    if (mLoadGroup)
      mLoadGroup->AddRequest(this, nsnull);
  }

  return rv;
}

nsresult nsIconChannel::MakeInputStream(nsIInputStream** _retval, PRBool nonBlocking)
{
  nsXPIDLCString contentType;
  nsCAutoString fileExt;
  nsCOMPtr<nsIFile> fileloc; // file we want an icon for
  PRUint32 desiredImageSize;
  nsresult rv = ExtractIconInfoFromUrl(getter_AddRefs(fileloc), &desiredImageSize, contentType, fileExt);
  NS_ENSURE_SUCCESS(rv, rv);

  // ensure that we DO NOT resolve aliases, very important for file views
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(fileloc);
  if (localFile)
    localFile->SetFollowLinks(PR_FALSE);

  PRBool fileExists = PR_FALSE;
  if (fileloc)
    localFile->Exists(&fileExists);

  NSImage* iconImage = nil;
  
  // first try to get the icon from the file if it exists
  if (fileExists) {
    nsCOMPtr<nsILocalFileMac> localFileMac(do_QueryInterface(fileloc, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    
    CFURLRef macURL;
    if (NS_SUCCEEDED(localFileMac->GetCFURL(&macURL))) {
      iconImage = [[NSWorkspace sharedWorkspace] iconForFile:[(NSURL*)macURL path]];
      ::CFRelease(macURL);
    }
  }

  // try by HFS type if we don't have an icon yet
  if (!iconImage) {
    nsCOMPtr<nsIMIMEService> mimeService (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    // if we were given an explicit content type, use it....
    nsCOMPtr<nsIMIMEInfo> mimeInfo;
    if (mimeService && (!contentType.IsEmpty() || !fileExt.IsEmpty()))
      mimeService->GetFromTypeAndExtension(contentType, fileExt, getter_AddRefs(mimeInfo));

    if (mimeInfo) {
      // get the icon by HFS type
      PRUint32 macType;
      if (NS_SUCCEEDED(mimeInfo->GetMacType(&macType)))
        iconImage = [[NSWorkspace sharedWorkspace] iconForFileType:NSFileTypeForHFSTypeCode(macType)];
    }
  }
  
  // if we still don't have an icon, try to get one by extension
  if (!iconImage && !fileExt.IsEmpty()) {
    NSString* fileExtension = [NSString stringWithUTF8String:PromiseFlatCString(fileExt).get()];
    iconImage = [[NSWorkspace sharedWorkspace] iconForFileType:fileExtension];
  }
  
  if (!iconImage)
    return NS_ERROR_FAILURE;
  
  // we have an icon now, size it
  NSRect desiredSizeRect = NSMakeRect(0, 0, desiredImageSize, desiredImageSize);
  [iconImage setSize:desiredSizeRect.size];
  
  [iconImage lockFocus];
  NSBitmapImageRep* bitmapRep = [[[NSBitmapImageRep alloc] initWithFocusedViewRect:desiredSizeRect] autorelease];
  [iconImage unlockFocus];
  
  // we expect the following things to be true about our bitmapRep
  NS_ENSURE_TRUE(![bitmapRep isPlanar] &&
                 (unsigned int)[bitmapRep bytesPerPlane] == desiredImageSize * desiredImageSize * 4 &&
                 [bitmapRep bitsPerPixel] == 32 &&
                 [bitmapRep samplesPerPixel] == 4 &&
                 [bitmapRep hasAlpha] == YES,
                 NS_ERROR_UNEXPECTED);
  
  // rgba, pre-multiplied data
  PRUint8* bitmapRepData = (PRUint8*)[bitmapRep bitmapData];
  
  // create our buffer
  PRInt32 bufferCapacity = 2 + desiredImageSize * desiredImageSize * 4;
  nsAutoBuffer<PRUint8, 3 + 16 * 16 * 5> iconBuffer; // initial size is for 16x16
  if (!iconBuffer.EnsureElemCapacity(bufferCapacity))
    return NS_ERROR_OUT_OF_MEMORY;
  
  PRUint8* iconBufferPtr = iconBuffer.get();
  
  // write header data into buffer
  *iconBufferPtr++ = desiredImageSize;
  *iconBufferPtr++ = desiredImageSize;

  PRUint32 dataCount = (desiredImageSize * desiredImageSize) * 4;
  PRUint32 index = 0;
  while (index < dataCount) {
    // get data from the bitmap
    PRUint8 r = bitmapRepData[index++];
    PRUint8 g = bitmapRepData[index++];
    PRUint8 b = bitmapRepData[index++];
    PRUint8 a = bitmapRepData[index++];

    // write data out to our buffer
    // non-cairo uses native image format, but the A channel is ignored.
    // cairo uses ARGB (highest to lowest bits)
#if defined(IS_LITTLE_ENDIAN)
    *iconBufferPtr++ = b;
    *iconBufferPtr++ = g;
    *iconBufferPtr++ = r;
    *iconBufferPtr++ = a;
#else
    *iconBufferPtr++ = a;
    *iconBufferPtr++ = r;
    *iconBufferPtr++ = g;
    *iconBufferPtr++ = b;
#endif
  }

  NS_ASSERTION(iconBufferPtr == iconBuffer.get() + bufferCapacity,
               "buffer size miscalculation");
  
  // Now, create a pipe and stuff our data into it
  nsCOMPtr<nsIInputStream> inStream;
  nsCOMPtr<nsIOutputStream> outStream;
  rv = NS_NewPipe(getter_AddRefs(inStream), getter_AddRefs(outStream), bufferCapacity, bufferCapacity, nonBlocking);  

  if (NS_SUCCEEDED(rv)) {
    PRUint32 written;
    rv = outStream->Write((char*)iconBuffer.get(), bufferCapacity, &written);
    if (NS_SUCCEEDED(rv))
      NS_IF_ADDREF(*_retval = inStream);
  }

  // Drop notification callbacks to prevent cycles.
  mCallbacks = nsnull;

  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetLoadFlags(PRUint32 *aLoadAttributes)
{
  return mPump->GetLoadFlags(aLoadAttributes);
}

NS_IMETHODIMP nsIconChannel::SetLoadFlags(PRUint32 aLoadAttributes)
{
  return mPump->SetLoadFlags(aLoadAttributes);
}

NS_IMETHODIMP nsIconChannel::GetContentType(nsACString &aContentType) 
{
  aContentType.AssignLiteral("image/icon");
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentType(const nsACString &aContentType)
{
  //It doesn't make sense to set the content-type on this type
  // of channel...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsIconChannel::GetContentCharset(nsACString &aContentCharset) 
{
  aContentCharset.AssignLiteral("image/icon");
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentCharset(const nsACString &aContentCharset)
{
  //It doesn't make sense to set the content-type on this type
  // of channel...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsIconChannel::GetContentLength(PRInt32 *aContentLength)
{
  *aContentLength = mContentLength;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetContentLength(PRInt32 aContentLength)
{
  NS_NOTREACHED("nsIconChannel::SetContentLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsIconChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetOwner(nsISupports* *aOwner)
{
  *aOwner = mOwner.get();
  NS_IF_ADDREF(*aOwner);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetOwner(nsISupports* aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  *aSecurityInfo = nsnull;
  return NS_OK;
}

