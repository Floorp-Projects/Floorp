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
 *   Scott MacGregor          <mscott@netscape.com>
 *   Robert John Churchill    <rjc@netscape.com>
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

#include <Files.h>
#include <Folders.h>
#include <Icons.h>
#include <Quickdraw.h>
#include <MacMemory.h>

// nsIconChannel methods
nsIconChannel::nsIconChannel()
{
  mStatus = NS_OK;
}

nsIconChannel::~nsIconChannel() 
{}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsIconChannel, 
                              nsIChannel, 
                              nsIRequest)

nsresult nsIconChannel::Init(nsIURI* uri)
{
  NS_ASSERTION(uri, "no uri");
  mUrl = uri;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP nsIconChannel::GetName(nsACString &result)
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

NS_IMETHODIMP
nsIconChannel::Open(nsIInputStream **_retval)
{
  return NS_ERROR_FAILURE;
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

nsresult
nsIconChannel::GetLockedIconData(IconFamilyHandle iconFamilyH, PRUint32 iconType,
                           Handle iconDataH, PRUint32 *iconDataSize)
{
  *iconDataSize = 0;

  if (::GetIconFamilyData(iconFamilyH, iconType, iconDataH) != noErr)
    return(NS_ERROR_FAILURE);

  *iconDataSize = (PRUint32)::GetHandleSize(iconDataH);
  if (*iconDataSize > 0)
  {
    ::HLock(iconDataH);
  }
  return(NS_OK);
}


nsresult
nsIconChannel::GetLockedIcons(IconFamilyHandle icnFamily, PRUint32 desiredImageSize,
            Handle iconH, PRUint32 *dataCount, PRBool *isIndexedData,
            Handle iconMaskH, PRUint32 *maskCount)
{
  // note: this code could be improved by:
  //
  // o  adding support for icon scaling, i.e. if we want a
  //    32x32 icon but can only get a 16x16 icon (or vice versa),
  //    scale the pixels appropriately
  //
  // o  adding support for Mac OS X "huge" 128x128 icons with alpha
  //    which is also tricky as the alpha data defines the mask for the icon

  *dataCount = *maskCount = 0L;

  // make sure icon/mask handles are unlocked
  // so that they can move in memory
  HUnlock(iconH);
  HUnlock(iconMaskH);

  // Note: Always try and get 32bit non-indexed icons first
  // so that we don't have to worry about color palette issues        
  nsresult rv = GetLockedIconData(icnFamily, (desiredImageSize > 16) ?
                                  kLarge32BitData : kSmall32BitData,
                                  iconH, dataCount);
  if (NS_SUCCEEDED(rv) && (*dataCount > 0))
  {
    *isIndexedData = PR_FALSE;
  }
  else
  {
    // if couldn't get a 32bit non-indexed icon,
    // then try getting an 8-bit icon
    rv = GetLockedIconData(icnFamily, (desiredImageSize > 16) ?
                           kLarge8BitData : kSmall8BitData,
                           iconH, dataCount);
    if (NS_SUCCEEDED(rv) && (*dataCount > 0))
    {
      *isIndexedData = PR_TRUE;
    }
  }

  // if we have an icon, try getting a mask too
  if (NS_SUCCEEDED(rv) && (*dataCount > 0))
  {
    // moz-icons are RGB_A1, so get 1-bit icon mask
    rv = GetLockedIconData(icnFamily, (desiredImageSize > 16) ?
                           kLarge1BitMask : kSmall1BitMask,
                           iconMaskH, maskCount);
    if (NS_FAILED(rv) || (*maskCount == 0))
    {
      // if we can't get a mask, the file's BNDL might be
      // messed up, etc.  Let's just fake a 1-bit mask
      // which will blit the entire icon... its not perfect,
      // but its better than no icon at all
      *maskCount = (desiredImageSize > 16) ? 256 : 64;
      rv = NS_OK;
    }
  }
  return(rv);
}


NS_IMETHODIMP nsIconChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
  nsCOMPtr<nsIFile> localFile; // file we want an icon for
  nsCAutoString     contentType;
  nsCAutoString     fileExtension;
  PRUint32          desiredImageSize;
  nsresult rv = ExtractIconInfoFromUrl(getter_AddRefs(localFile), &desiredImageSize,
                contentType, fileExtension);
  if (NS_FAILED(rv))  return(rv);

  // ensure that we DO NOT resolve aliases, very important for file views
  nsCOMPtr<nsILocalFile>  aFileLocal = do_QueryInterface(localFile);
  if (aFileLocal)
  {
    aFileLocal->SetFollowLinks(PR_FALSE);
  }

  PRBool fileExists = PR_FALSE;
  if (localFile)
  {
    localFile->Exists(&fileExists);
  }

  IconRef icnRef = nsnull;
  if (fileExists)
  {
    // if the file exists, first try getting icons via Icon Services
    nsCOMPtr<nsILocalFileMac> localFileMac (do_QueryInterface(localFile, &rv));
    if (NS_FAILED(rv))  return(rv);

    FSSpec spec;
    if (NS_FAILED(localFileMac->GetFSSpec(&spec)))
      return(NS_ERROR_FAILURE);

    SInt16  label;
    if (::GetIconRefFromFile (&spec, &icnRef, &label) != noErr)
      return(NS_ERROR_FAILURE);
  }

  // note: once we have an IconRef,
  // we MUST release it before exiting this method!

  // start with zero-sized icons which ::GetIconFamilyData will resize
  PRUint32  dataCount = 0L, maskCount = 0L;
  Handle    iconH = ::NewHandle(dataCount);
  Handle    iconMaskH = ::NewHandle(maskCount);
  if (!iconH || !iconMaskH)
  {
    // sigh; REALLY low-mem, bail
    if (iconH)      ::DisposeHandle(iconH);
    if (iconMaskH)  ::DisposeHandle(iconMaskH);
    if (fileExists) ::ReleaseIconRef(icnRef);
    return(NS_ERROR_OUT_OF_MEMORY);
  }

  PRUint8 *iconBitmapData = nsnull, *maskBitmapData = nsnull;

  PRBool  isIndexedData = PR_FALSE;
  IconFamilyHandle icnFamily;
  if (fileExists && (::IconRefToIconFamily(icnRef,
      kSelectorAllAvailableData, &icnFamily) == noErr))
  {
    GetLockedIcons(icnFamily, desiredImageSize, iconH,  &dataCount,
                   &isIndexedData, iconMaskH, &maskCount);
    if (dataCount > 0)
    {
      iconBitmapData = (PRUint8 *)*iconH;
      if (maskCount > 0)  maskBitmapData = (PRUint8 *)*iconMaskH;
    }
    ::DisposeHandle((Handle)icnFamily);
  }
  ::ReleaseIconRef(icnRef);
  icnRef = nsnull;

  if (!dataCount)
  {
    // if file didn't exist, or couldn't get an appropriate
    // icon resource, then try again by mimetype mapping

    nsCOMPtr<nsIMIMEService> mimeService (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    // if we were given an explicit content type, use it....
    nsCOMPtr<nsIMIMEInfo> mimeInfo;
    if (mimeService && (!contentType.IsEmpty() || !fileExtension.IsEmpty()))
    {
      mimeService->GetFromTypeAndExtension(contentType,
                                           fileExtension,
                                           getter_AddRefs(mimeInfo));
    }

    // if we don't have enough info to fetch an application icon, bail
    if (!mimeInfo)
    {
      if (iconH)      ::DisposeHandle(iconH);
      if (iconMaskH)  ::DisposeHandle(iconMaskH);
      return NS_ERROR_FAILURE;
    }

    // get the mac creator and file type for this mime object
    PRUint32 macType;
    PRUint32 macCreator;
    mimeInfo->GetMacType(&macType);
    mimeInfo->GetMacCreator(&macCreator);

    if (::GetIconRef(kOnSystemDisk, macCreator, macType, &icnRef) != noErr)
    {
      if (iconH)      ::DisposeHandle(iconH);
      if (iconMaskH)  ::DisposeHandle(iconMaskH);
      return(NS_ERROR_FAILURE);
    }

    if (::IconRefToIconFamily(icnRef, kSelectorAllAvailableData, &icnFamily) == noErr)
    {
      GetLockedIcons(icnFamily, desiredImageSize, iconH, &dataCount,
                     &isIndexedData, iconMaskH, &maskCount);
      if (dataCount > 0)
      {
        iconBitmapData = (PRUint8 *)*iconH;
        if (maskCount > 0)  maskBitmapData = (PRUint8 *)*iconMaskH;
      }
      ::DisposeHandle((Handle)icnFamily);
    }
    ::ReleaseIconRef(icnRef);
    icnRef = nsnull;
  }

  // note: we must have icon data, but it is OK to not
  // have maskBitmapData (we fake a mask in that case)
  if (!iconBitmapData)
  {
    if (iconH)      DisposeHandle(iconH);
    if (iconMaskH)  DisposeHandle(iconMaskH);
    return(NS_ERROR_FAILURE);
  }

  PRUint32 numPixelsInRow = (desiredImageSize > 16) ? 32 : 16;
  PRUint8  *bitmapData = (PRUint8 *)iconBitmapData;

  nsCString iconBuffer;
  iconBuffer.Assign((char) numPixelsInRow);
  iconBuffer.Append((char) numPixelsInRow);

  CTabHandle cTabHandle = nsnull;
  CTabPtr colTable = nsnull;
  if (isIndexedData)
  {
    // only need the CTable if we have an palette-indexed icon
    cTabHandle = ::GetCTable(72);
    if (!cTabHandle)
    {
      if (iconH)      ::DisposeHandle(iconH);
      if (iconMaskH)  ::DisposeHandle(iconMaskH);
      return NS_ERROR_FAILURE;
    }
    ::HLock((Handle) cTabHandle);
    colTable = *cTabHandle;
  }

  RGBColor rgbCol;
  PRUint8 redValue, greenValue, blueValue;
  PRUint32 index = 0L;
  while (index < dataCount)
  {
    if (!isIndexedData)
    {
      // 32 bit icon data
      iconBuffer.Append((char) bitmapData[index++]);
      iconBuffer.Append((char) bitmapData[index++]);
      iconBuffer.Append((char) bitmapData[index++]);
      iconBuffer.Append((char) bitmapData[index++]);
    }
    else
    {
      // each byte in bitmapData needs to be converted from an 8 bit system color into 
      // 24 bit RGB data which our special icon image decoder can understand.
      ColorSpec colSpec =  colTable->ctTable[ bitmapData[index]];
      rgbCol = colSpec.rgb;

      redValue = rgbCol.red & 0xff;
      greenValue = rgbCol.green & 0xff;
      blueValue = rgbCol.blue & 0xff;

      iconBuffer.Append((char) 0);        // alpha channel byte
      iconBuffer.Append((char) redValue);
      iconBuffer.Append((char) greenValue);
      iconBuffer.Append((char) blueValue);
      index++;
    }
  }
  
  if (cTabHandle)
  {
    ::HUnlock((Handle) cTabHandle);
    ::DisposeCTable(cTabHandle);
  }

  ::DisposeHandle(iconH);
  iconH = nsnull;

  bitmapData = (PRUint8 *)maskBitmapData;
  if (maskBitmapData)
  {
    // skip over ICN# data to get to mask
    // which is half way into data
    index = maskCount/2;
    while (index < maskCount)
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
  }
  else
  {
    index = 0L;
    while (index++ < maskCount)
    {
      // edgecase: without a mask, just blit entire icon
      iconBuffer.Append((char) 255);
    }
  }

  if (iconMaskH)
  {
    ::DisposeHandle(iconMaskH);
    iconMaskH = nsnull;
  }

  // turn our nsString into a stream looking object...
  aListener->OnStartRequest(this, ctxt);

  // turn our string into a stream...
  nsCOMPtr<nsIInputStream> inputStr;
  rv = NS_NewByteInputStream(getter_AddRefs(inputStr), iconBuffer.get(),
                             iconBuffer.Length());

  if (NS_SUCCEEDED(rv))
  {
      aListener->OnDataAvailable(this, ctxt, inputStr, 0, iconBuffer.Length());
  }
  aListener->OnStopRequest(this, ctxt, rv);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetLoadFlags(PRUint32 *aLoadAttributes)
{
  *aLoadAttributes = mLoadAttributes;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetLoadFlags(PRUint32 aLoadAttributes)
{
  mLoadAttributes = aLoadAttributes;
  return NS_OK;
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

