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
 *   Scott MacGregor <mscott@netscape.com>
 *   Neil Rashbrook <neil@parkwaycc.co.uk>
 *   Ben Goodger <ben@mozilla.org>
 *   Simon Taylor <simontaylor2@gawab.com>
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
#include "nsReadableUtils.h"
#include "nsMimeTypes.h"
#include "nsMemory.h"
#include "nsIStringStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsInt64.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIMIMEService.h"
#include "nsDirectoryServiceDefs.h"

#include <Mime.h>
#include <Bitmap.h>
#include <Screen.h>
#include <Node.h>
#include <NodeInfo.h>

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

NS_IMETHODIMP nsIconChannel::GetLoadFlags(PRUint32 *aLoadAttributes)
{
  return mPump->GetLoadFlags(aLoadAttributes);
}

NS_IMETHODIMP nsIconChannel::SetLoadFlags(PRUint32 aLoadAttributes)
{
  return mPump->SetLoadFlags(aLoadAttributes);
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

  nsCOMPtr<nsIFileURL>    fileURL = do_QueryInterface(fileURI, &rv);
  if (NS_FAILED(rv) || !fileURL) return NS_OK;

  nsCOMPtr<nsIFile> file;
  rv = fileURL->GetFile(getter_AddRefs(file));
  if (NS_FAILED(rv) || !file) return NS_OK;

  *aLocalFile = file;
  NS_IF_ADDREF(*aLocalFile);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
  nsCOMPtr<nsIInputStream> inStream;
  nsresult rv = MakeInputStream(getter_AddRefs(inStream), PR_TRUE);
  if (NS_FAILED(rv))
    return rv;

  // Init our streampump
  rv = mPump->Init(inStream, nsInt64(-1), nsInt64(-1), 0, 0, PR_FALSE);
  if (NS_FAILED(rv))
    return rv;

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
  nsCAutoString filePath;
  nsCAutoString fileExtension;
  nsCOMPtr<nsIFile> localFile; // File we want an icon for
  PRUint32 desiredImageSize;
  nsresult rv = ExtractIconInfoFromUrl(getter_AddRefs(localFile), &desiredImageSize, contentType, fileExtension);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 iconSize = 16;
  if (desiredImageSize > 16)
    iconSize = 32;

  PRUint32 alphaBytesPerRow = (iconSize / 8);
  if (iconSize % 32 != 0)
    alphaBytesPerRow = ((iconSize / 32) + 1) * 4;
    
  PRBool fileExists = PR_FALSE;
  if (localFile)
  {
    localFile->GetNativePath(filePath);
    localFile->Exists(&fileExists);
  }
  
  // Get the native icon.
  // 1) If it is for an actual local file, BNodeInfo::GetTrackerIcon.
  // 2) If the local file does not exist, use the content type 
  //    and BMimeType::GetIcon
  BBitmap nativeIcon(BRect(0, 0, iconSize - 1, iconSize - 1), B_CMAP8);
  if (!nativeIcon.IsValid())
    return NS_ERROR_OUT_OF_MEMORY;

  PRBool gotBitmap = PR_FALSE;
  if (fileExists)
  {
    BNode localNode(filePath.get());
    // BeOS doesn't MIME type foreign files immediately - 
    // If there is no type attribute then we can force an identify
    if (localNode.ReadAttr("BEOS:TYPE", B_STRING_TYPE, 0, NULL, 0) != B_OK)
    	update_mime_info(filePath.get(), 0, 1, 1);

    BNodeInfo localNodeInfo(&localNode);
    if (iconSize == 16)
    {
      if (localNodeInfo.GetTrackerIcon(&nativeIcon, B_MINI_ICON) == B_OK)
        gotBitmap = PR_TRUE;
    }
    else
    {
      if (localNodeInfo.GetTrackerIcon(&nativeIcon, B_LARGE_ICON) == B_OK)
        gotBitmap = PR_TRUE;
    }
  }

  // If we haven't got a bitmap yet, use the content type
  if (!gotBitmap)    
  {
    // If no content type specified, use mozilla's mime service to guess a mime type
    if (contentType.IsEmpty())
    {
      nsCOMPtr<nsIMIMEService> mimeService (do_GetService("@mozilla.org/mime;1", &rv));
      if (NS_SUCCEEDED(rv))
        mimeService->GetTypeFromExtension(fileExtension, contentType);
      // If unrecognised extension - set to generic file
      if (contentType.IsEmpty())
        contentType = "application/octet-stream";
    }
    // Create BeOS-Native MIME type info - if unheard of, set to generic file
    BMimeType mimeType(contentType.get());
    if (!mimeType.IsInstalled())
    	mimeType.SetTo("application/octet-stream");
    if (iconSize == 16)
    {
      if (mimeType.GetIcon(&nativeIcon, B_MINI_ICON) == B_OK)
        gotBitmap = PR_TRUE;
    }
    else
    {
      if (mimeType.GetIcon(&nativeIcon, B_LARGE_ICON) == B_OK)
        gotBitmap = PR_TRUE;
    }
  }
  
  if (!gotBitmap)
    return NS_ERROR_NOT_AVAILABLE;

  BScreen mainScreen(B_MAIN_SCREEN_ID);
  if (!mainScreen.IsValid())
    return NS_ERROR_NOT_AVAILABLE;

  // Got a bitmap and color space info - convert data to mozilla's icon format
  PRUint32 iconLength = 2 + iconSize * iconSize * 4;
  uint8 *buffer = new uint8[iconLength];
  if (!buffer)
    return NS_ERROR_OUT_OF_MEMORY;

  uint8* destByte = buffer;
  *(destByte++) = iconSize;
  *(destByte++) = iconSize;

  // RGB data
  uint8* sourceByte = (uint8*)nativeIcon.Bits();
  for(PRUint32 iconRow = 0; iconRow < iconSize; iconRow++)
  {
    sourceByte = (uint8*)nativeIcon.Bits() + nativeIcon.BytesPerRow() * iconRow;
    for(PRUint32 iconCol = 0; iconCol < iconSize; iconCol++)
    {
      if (*sourceByte != B_TRANSPARENT_MAGIC_CMAP8)
      {
        rgb_color colorVal = mainScreen.ColorForIndex(*sourceByte);
#ifdef IS_LITTLE_ENDIAN
        *(destByte++) = colorVal.blue;
        *(destByte++) = colorVal.green;
        *(destByte++) = colorVal.red;
        *(destByte++) = uint8(255);
#else
        *(destByte++) = uint8(255);
        *(destByte++) = colorVal.red;
        *(destByte++) = colorVal.green;
        *(destByte++) = colorVal.blue;
#endif
      }
      else
      {
        *destByte++ = 0;
        *destByte++ = 0;
        *destByte++ = 0;
        *destByte++ = 0;
      }
      // original code had a conditional here:
      // if (iconCol < iconSize - 1) 
      // Leaving this comment in case complications arise later
      sourceByte++;
    }
  }

  NS_ASSERTION(buffer + iconLength == destByte, "size miscalculation");
  
  // Now, create a pipe and stuff our data into it
  nsCOMPtr<nsIInputStream> inStream;
  nsCOMPtr<nsIOutputStream> outStream;
  rv = NS_NewPipe(getter_AddRefs(inStream), getter_AddRefs(outStream),
                  iconLength, iconLength, nonBlocking);
  if (NS_SUCCEEDED(rv))
  {
    PRUint32 written;
    rv = outStream->Write((char*)buffer, iconLength, &written);
    if (NS_SUCCEEDED(rv))
      NS_ADDREF(*_retval = inStream);
  }
  delete [] buffer;
  
  return rv;
}

NS_IMETHODIMP nsIconChannel::GetContentType(nsACString &aContentType) 
{
  aContentType.AssignLiteral("image/icon");
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentType(const nsACString &aContentType)
{
  // It doesn't make sense to set the content-type on this type
  // of channel...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsIconChannel::GetContentCharset(nsACString &aContentCharset) 
{
  aContentCharset.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentCharset(const nsACString &aContentCharset)
{
  // It doesn't make sense to set the content-charset on this type
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
