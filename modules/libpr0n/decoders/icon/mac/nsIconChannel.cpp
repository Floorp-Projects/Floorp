/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Brian Ryner.
 * Portions created by Brian Ryner are Copyright (C) 2000 Brian Ryner.
 * All Rights Reserved.
 *
 * Contributor(s): 
 *   Scott MacGregor <mscott@netscape.com>
 */


#include "nsIconChannel.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsXPIDLString.h"
#include "nsMimeTypes.h"
#include "nsMemory.h"
#include "nsIStringStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIMimeService.h"
#include "nsCExternalHandlerService.h"
#include "plstr.h"

#include <Files.h>
#include <QuickDraw.h>

// nsIconChannel methods
nsIconChannel::nsIconChannel()
{
  NS_INIT_REFCNT();
  mStatus = NS_OK;
}

nsIconChannel::~nsIconChannel() 
{}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsIconChannel, 
                              nsIChannel, 
                              nsIRequest)

nsresult nsIconChannel::Init(nsIURI* uri)
{
  nsresult rv;
  NS_ASSERTION(uri, "no uri");
  mUrl = uri;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP nsIconChannel::GetName(PRUnichar* *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsIconChannel::IsPending(PRBool *result)
{
  NS_NOTREACHED("nsIconChannel::IsPending");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsIconChannel::GetStatus(nsresult *status)
{
  *status = mStatus;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::Cancel(nsresult status)
{
  NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
  nsresult rv = NS_ERROR_FAILURE;

  mStatus = status;
  return rv;
}

NS_IMETHODIMP nsIconChannel::Suspend(void)
{
  NS_NOTREACHED("nsIconChannel::Suspend");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsIconChannel::Resume(void)
{
  NS_NOTREACHED("nsIconChannel::Resume");
  return NS_ERROR_NOT_IMPLEMENTED;
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

NS_IMETHODIMP nsIconChannel::SetURI(nsIURI* aURI)
{
  mUrl = aURI;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::Open(nsIInputStream **_retval)
{
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsIconChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
  // get the file name from the url
  nsXPIDLCString fileName; // will contain a dummy file we'll use to figure out the type of icon desired.
  nsXPIDLCString filePath; // will contain an optional parameter for small vs. large icon. default is small
  mUrl->GetHost(getter_Copies(fileName));
  nsCOMPtr<nsIURL> url (do_QueryInterface(mUrl));
  if (url)
    url->GetFileBaseName(getter_Copies(filePath));
  nsresult rv = NS_OK;  
  nsCOMPtr<nsIMIMEService> mimeService (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  
  // extract the extension out of the dummy file so we can look it up in the mime service.
  char * chFileName = fileName.get(); // get the underlying buffer
  char * fileExtension = PL_strrchr(chFileName, '.');
  if (!fileExtension) return NS_ERROR_FAILURE; // no file extension to work from.
  
  // look the file extension up in the registry.
  nsCOMPtr<nsIMIMEInfo> mimeInfo;
  mimeService->GetFromExtension(fileExtension, getter_AddRefs(mimeInfo));
  NS_ENSURE_TRUE(mimeInfo, NS_ERROR_FAILURE);
  
  // get the mac creator and file type for this mime object
  PRUint32 macType;
  PRUint32 macCreator;
  
  mimeInfo->GetMacType(&macType);
  mimeInfo->GetMacCreator(&macCreator);
  
  // get a refernce to the desktop database  
  DTPBRec 	pb;
  OSErr err = noErr;
  
  memset(&pb, 0, sizeof(DTPBRec));
  pb.ioCompletion = nil;
  pb.ioVRefNum = 0; // default desktop volume
  pb.ioNamePtr = nil;
  err = PBDTGetPath(&pb);
  if (err != noErr) return NS_ERROR_FAILURE;
  
  pb.ioFileCreator = macCreator;
  pb.ioFileType = macType;
  pb.ioCompletion = nil;
  pb.ioTagInfo = 0;
  
  PRUint32 numPixelsInRow = 0;
  if (filePath && !nsCRT::strcmp(filePath, "large"))
  {
    pb.ioDTReqCount = kLarge8BitIconSize;
    pb.ioIconType = kLarge8BitIcon;
    numPixelsInRow = 32;
  }
  else
  {
    pb.ioDTReqCount = kSmall8BitIconSize;
    pb.ioIconType = kSmall8BitIcon;  
    numPixelsInRow = 16;
  }
  
  // allocate a buffer large enough to handle the icon
  PRUint8 * bitmapData = (PRUint8 *) nsMemory::Alloc (pb.ioDTReqCount);
  pb.ioDTBuffer = (Ptr) bitmapData;
  
  err = PBDTGetIcon(&pb, false);
  if (err != noErr) return NS_ERROR_FAILURE; // unable to fetch the icon....
  
  nsCString iconBuffer;
  iconBuffer.Assign((char) numPixelsInRow);
  iconBuffer.Append((char) numPixelsInRow);
  CTabHandle cTabHandle = GetCTable(72);
  if (!cTabHandle) return NS_ERROR_FAILURE;
  
  HLock((Handle) cTabHandle);
  CTabPtr colTable = *cTabHandle;
  RGBColor rgbCol;
  PRUint8 redValue, greenValue, blueValue;
  
  for (PRUint32 index = 0; index < pb.ioDTReqCount; index ++)
  {

  	// each byte in bitmapData needs to be converted from an 8 bit system color into 
    // 24 bit RGB data which our special icon image decoder can understand.
    ColorSpec colSpec =  colTable->ctTable[ bitmapData[index]];
    rgbCol = colSpec.rgb;
    
    redValue = rgbCol.red & 0xff;
    greenValue = rgbCol.green & 0xff;
    blueValue = rgbCol.blue & 0xff;
    
    // for some reason the image code on the mac expects each RGB pixel value to be padded with a preceding byte.
    // so add the padding here....
    iconBuffer.Append((char) 0);
    iconBuffer.Append((char) redValue);
    iconBuffer.Append((char) greenValue);
    iconBuffer.Append((char) blueValue);
  }
  
  
  HUnlock((Handle) cTabHandle);
  DisposeCTable(cTabHandle);
  nsMemory::Free(bitmapData);
  
  // now that the color bitmask is taken care of, we need to do the same thing again for the transparency
  // bit mask....
  if (filePath && !nsCRT::strcmp(filePath, "large"))
  {
    pb.ioDTReqCount = kLargeIconSize;
    pb.ioIconType = kLargeIcon;
  }
  else
  {
    pb.ioDTReqCount = kSmallIconSize;
    pb.ioIconType = kSmallIcon; 
  }
  
  // allocate a buffer large enough to handle the icon
  bitmapData = (PRUint8 *) nsMemory::Alloc (pb.ioDTReqCount);
  pb.ioDTBuffer = (Ptr) bitmapData;
  err = PBDTGetIcon(&pb, false);
  PRUint32 index = pb.ioDTReqCount/2;
  while (index < pb.ioDTReqCount)
  {
   iconBuffer.Append((char) bitmapData[index]);
   iconBuffer.Append((char) bitmapData[index + 1]);
   if (numPixelsInRow == 32)
   {
     iconBuffer.Append((char) bitmapData[index + 2]);
     iconBuffer.Append((char) bitmapData[index + 3]);
     index += 4;
   }
   else
   {
    iconBuffer.Append((char) 255); // 2 bytes of padding
    iconBuffer.Append((char) 255);
    index += 2;
   }
  }
  
  nsMemory::Free(bitmapData);
  
  // turn our nsString into a stream looking object...
  aListener->OnStartRequest(this, ctxt);

 // turn our string into a stream...
 nsCOMPtr<nsISupports> streamSupports;
 NS_NewByteInputStream(getter_AddRefs(streamSupports), iconBuffer.get(), iconBuffer.Length());

 nsCOMPtr<nsIInputStream> inputStr (do_QueryInterface(streamSupports));
 aListener->OnDataAvailable(this, ctxt, inputStr, 0, iconBuffer.Length());
 aListener->OnStopRequest(this, ctxt, NS_OK, nsnull);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetLoadAttributes(PRUint32 *aLoadAttributes)
{
  *aLoadAttributes = mLoadAttributes;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetLoadAttributes(PRUint32 aLoadAttributes)
{
  mLoadAttributes = aLoadAttributes;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetContentType(char* *aContentType) 
{
  if (!aContentType) return NS_ERROR_NULL_POINTER;

  *aContentType = nsCRT::strdup("image/icon");
  if (!*aContentType) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentType(const char *aContentType)
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

