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
 *   Siddharth Agarwal <sid.bugzilla@gmail.com>
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
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "nsDirectoryServiceDefs.h"

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600
#endif

// we need windows.h to read out registry information...
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <wchar.h>

struct ICONFILEHEADER {
  PRUint16 ifhReserved;
  PRUint16 ifhType;
  PRUint16 ifhCount;
};

struct ICONENTRY {
  PRInt8 ieWidth;
  PRInt8 ieHeight;
  PRUint8 ieColors;
  PRUint8 ieReserved;
  PRUint16 iePlanes;
  PRUint16 ieBitCount;
  PRUint32 ieSizeImage;
  PRUint32 ieFileOffset;
};

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
typedef HRESULT (WINAPI*SHGetStockIconInfoPtr) (SHSTOCKICONID siid, UINT uFlags, SHSTOCKICONINFO *psii);

// Match stock icons with names
static SHSTOCKICONID GetStockIconIDForName(const nsACString &aStockName)
{
  // UAC shield icon
  if (aStockName == NS_LITERAL_CSTRING("uac-shield"))
    return SIID_SHIELD;

  return SIID_INVALID;
}
#endif

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
  mOriginalURI = uri;
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
  *aURI = mOriginalURI;
  NS_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetOriginalURI(nsIURI* aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
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

nsresult nsIconChannel::ExtractIconInfoFromUrl(nsIFile ** aLocalFile, PRUint32 * aDesiredImageSize, nsCString &aContentType, nsCString &aFileExtension)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMozIconURI> iconURI (do_QueryInterface(mUrl, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  iconURI->GetImageSize(aDesiredImageSize);
  iconURI->GetContentType(aContentType);
  iconURI->GetFileExtension(aFileExtension);

  nsCOMPtr<nsIURL> url;
  rv = iconURI->GetIconURL(getter_AddRefs(url));
  if (NS_FAILED(rv) || !url) return NS_OK;

  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(url, &rv);
  if (NS_FAILED(rv) || !fileURL) return NS_OK;

  nsCOMPtr<nsIFile> file;
  rv = fileURL->GetFile(getter_AddRefs(file));
  if (NS_FAILED(rv) || !file) return NS_OK;

  return file->Clone(aLocalFile);
}

NS_IMETHODIMP nsIconChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
  nsCOMPtr<nsIInputStream> inStream;
  nsresult rv = MakeInputStream(getter_AddRefs(inStream), PR_TRUE);
  if (NS_FAILED(rv))
    return rv;

  // Init our streampump
  rv = mPump->Init(inStream, PRInt64(-1), PRInt64(-1), 0, 0, PR_FALSE);
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

static DWORD GetSpecialFolderIcon(nsIFile* aFile, int aFolder, SHFILEINFOW* aSFI, UINT aInfoFlags)
{
  DWORD shellResult = 0;

  if (!aFile)
    return shellResult;

  PRUnichar fileNativePath[MAX_PATH];
  nsAutoString fileNativePathStr;
  aFile->GetPath(fileNativePathStr);
  ::GetShortPathNameW(fileNativePathStr.get(), fileNativePath, NS_ARRAY_LENGTH(fileNativePath));

  LPITEMIDLIST idList;
  HRESULT hr = ::SHGetSpecialFolderLocation(NULL, aFolder, &idList);
  if (SUCCEEDED(hr)) {
    PRUnichar specialNativePath[MAX_PATH];
    ::SHGetPathFromIDListW(idList, specialNativePath);
    ::GetShortPathNameW(specialNativePath, specialNativePath, NS_ARRAY_LENGTH(specialNativePath));
  
    if (!wcsicmp(fileNativePath, specialNativePath)) {
      aInfoFlags |= (SHGFI_PIDL | SHGFI_SYSICONINDEX);
      shellResult = ::SHGetFileInfoW((LPCWSTR)(LPCITEMIDLIST)idList, 0, aSFI,
                                     sizeof(*aSFI), aInfoFlags);
    }
  }
  CoTaskMemFree(idList);
  return shellResult;
}

static UINT GetSizeInfoFlag(PRUint32 aDesiredImageSize)
{
  UINT infoFlag;
  if (aDesiredImageSize > 16)
    infoFlag = SHGFI_SHELLICONSIZE;
  else
    infoFlag = SHGFI_SMALLICON;

  return infoFlag;
}

nsresult nsIconChannel::GetHIconFromFile(HICON *hIcon)
{
  nsXPIDLCString contentType;
  nsCString fileExt;
  nsCOMPtr<nsIFile> localFile; // file we want an icon for
  PRUint32 desiredImageSize;
  nsresult rv = ExtractIconInfoFromUrl(getter_AddRefs(localFile), &desiredImageSize, contentType, fileExt);
  NS_ENSURE_SUCCESS(rv, rv);

  // if the file exists, we are going to use it's real attributes...otherwise we only want to use it for it's extension...
  SHFILEINFOW      sfi;
  UINT infoFlags = SHGFI_ICON;
  
  PRBool fileExists = PR_FALSE;
 
  nsAutoString filePath;
  CopyASCIItoUTF16(fileExt, filePath);
  if (localFile)
  {
    rv = localFile->Normalize();
    NS_ENSURE_SUCCESS(rv, rv);

    localFile->GetPath(filePath);
    if (filePath.Length() < 2 || filePath[1] != ':')
      return NS_ERROR_MALFORMED_URI; // UNC

    if (filePath.Last() == ':')
      filePath.Append('\\');
    else {
      localFile->Exists(&fileExists);
      if (!fileExists)
       localFile->GetLeafName(filePath);
    }
  }

  if (!fileExists)
   infoFlags |= SHGFI_USEFILEATTRIBUTES;

  infoFlags |= GetSizeInfoFlag(desiredImageSize);

  // if we have a content type... then use it! but for existing files, we want
  // to show their real icon.
  if (!fileExists && !contentType.IsEmpty())
  {
    nsCOMPtr<nsIMIMEService> mimeService (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString defFileExt;
    mimeService->GetPrimaryExtension(contentType, fileExt, defFileExt);
    // If the mime service does not know about this mime type, we show
    // the generic icon.
    // In any case, we need to insert a '.' before the extension.
    filePath = NS_LITERAL_STRING(".") + NS_ConvertUTF8toUTF16(defFileExt);
  }

  // Is this the "Desktop" folder?
  DWORD shellResult = GetSpecialFolderIcon(localFile, CSIDL_DESKTOP, &sfi, infoFlags);
  if (!shellResult) {
    // Is this the "My Documents" folder?
    shellResult = GetSpecialFolderIcon(localFile, CSIDL_PERSONAL, &sfi, infoFlags);
  }

  // There are other "Special Folders" and Namespace entities that we are not 
  // fetching icons for, see: 
  // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/reference/enums/csidl.asp
  // If we ever need to get them, code to do so would be inserted here. 

  // Not a special folder, or something else failed above.
  if (!shellResult)
    shellResult = ::SHGetFileInfoW(filePath.get(),
                                   FILE_ATTRIBUTE_ARCHIVE, &sfi, sizeof(sfi), infoFlags);

  if (shellResult && sfi.hIcon)
    *hIcon = sfi.hIcon;
  else
    rv = NS_ERROR_NOT_AVAILABLE;

  return rv;
}

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
nsresult nsIconChannel::GetStockHIcon(nsIMozIconURI *aIconURI, HICON *hIcon)
{
  nsresult rv = NS_OK;

  // We can only do this on Vista or above
  HMODULE hShellDLL = ::LoadLibraryW(L"shell32.dll");
  SHGetStockIconInfoPtr pSHGetStockIconInfo =
    (SHGetStockIconInfoPtr) ::GetProcAddress(hShellDLL, "SHGetStockIconInfo");

  if (pSHGetStockIconInfo)
  {
    PRUint32 desiredImageSize;
    aIconURI->GetImageSize(&desiredImageSize);
    nsCAutoString stockIcon;
    aIconURI->GetStockIcon(stockIcon);

    SHSTOCKICONID stockIconID = GetStockIconIDForName(stockIcon);
    if (stockIconID == SIID_INVALID)
      return NS_ERROR_NOT_AVAILABLE;

    UINT infoFlags = SHGSI_ICON;
    infoFlags |= GetSizeInfoFlag(desiredImageSize);

    SHSTOCKICONINFO sii = {0};
    sii.cbSize = sizeof(sii);
    HRESULT hr = pSHGetStockIconInfo(stockIconID, infoFlags, &sii);

    if (SUCCEEDED(hr))
      *hIcon = sii.hIcon;
    else
      rv = NS_ERROR_FAILURE;
  }
  else
  {
    rv = NS_ERROR_NOT_AVAILABLE;
  }

  if (hShellDLL)
    ::FreeLibrary(hShellDLL);

  return rv;
}
#endif

// Given a BITMAPINFOHEADER, returns the size of the color table.
static int GetColorTableSize(BITMAPINFOHEADER* aHeader)
{
  int colorTableSize = -1;

  // http://msdn.microsoft.com/en-us/library/dd183376%28v=VS.85%29.aspx
  switch (aHeader->biBitCount) {
  case 0:
    colorTableSize = 0;
    break;
  case 1:
    colorTableSize = 2 * sizeof(RGBQUAD);
    break;
  case 4:
  case 8:
  {
    // The maximum possible size for the color table is 2**bpp, so check for
    // that and fail if we're not in those bounds
    unsigned int maxEntries = 1 << (aHeader->biBitCount);
    if (aHeader->biClrUsed > 0 && aHeader->biClrUsed <= maxEntries)
      colorTableSize = aHeader->biClrUsed * sizeof(RGBQUAD);
    else if (aHeader->biClrUsed == 0)
      colorTableSize = maxEntries * sizeof(RGBQUAD);
    break;
  }
  case 16:
  case 32:
    // If we have BI_BITFIELDS compression, we would normally need 3 DWORDS for
    // the bitfields mask which would be stored in the color table; However, 
    // we instead force the bitmap to request data of type BI_RGB so the color
    // table should be of size 0.  
    // Setting aHeader->biCompression = BI_RGB forces the later call to 
    // GetDIBits to return to us BI_RGB data.
    if (aHeader->biCompression == BI_BITFIELDS) {
      aHeader->biCompression = BI_RGB;
    }
    colorTableSize = 0;
    break;
  case 24:
    colorTableSize = 0;
    break;
  }

  if (colorTableSize < 0)
    NS_WARNING("Unable to figure out the color table size for this bitmap");

  return colorTableSize;
}

// Given a header and a size, creates a freshly allocated BITMAPINFO structure.
// It is the caller's responsibility to null-check and delete the structure.
static BITMAPINFO* CreateBitmapInfo(BITMAPINFOHEADER* aHeader,
                                    size_t aColorTableSize)
{
  BITMAPINFO* bmi = (BITMAPINFO*) ::operator new(sizeof(BITMAPINFOHEADER) +
                                                 aColorTableSize,
                                                 mozilla::fallible_t());
  if (bmi) {
    memcpy(bmi, aHeader, sizeof(BITMAPINFOHEADER));
    memset(bmi->bmiColors, 0, aColorTableSize);
  }
  return bmi;
}

nsresult nsIconChannel::MakeInputStream(nsIInputStream** _retval, PRBool nonBlocking)
{
  // Check whether the icon requested's a file icon or a stock icon
  nsresult rv = NS_ERROR_NOT_AVAILABLE;

  // GetDIBits does not exist on windows mobile.
  HICON hIcon = NULL;

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
  nsCOMPtr<nsIMozIconURI> iconURI(do_QueryInterface(mUrl, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString stockIcon;
  iconURI->GetStockIcon(stockIcon);
  if (!stockIcon.IsEmpty())
    rv = GetStockHIcon(iconURI, &hIcon);
  else
#endif
    rv = GetHIconFromFile(&hIcon);

  NS_ENSURE_SUCCESS(rv, rv);

  if (hIcon)
  {
    // we got a handle to an icon. Now we want to get a bitmap for the icon using GetIconInfo....
    ICONINFO iconInfo;
    if (GetIconInfo(hIcon, &iconInfo))
    {
      // we got the bitmaps, first find out their size
      HDC hDC = CreateCompatibleDC(NULL); // get a device context for the screen.
      BITMAPINFOHEADER maskHeader  = {sizeof(BITMAPINFOHEADER)};
      BITMAPINFOHEADER colorHeader = {sizeof(BITMAPINFOHEADER)};
      int colorTableSize, maskTableSize;
      if (GetDIBits(hDC, iconInfo.hbmMask,  0, 0, NULL, (BITMAPINFO*)&maskHeader,  DIB_RGB_COLORS) &&
          GetDIBits(hDC, iconInfo.hbmColor, 0, 0, NULL, (BITMAPINFO*)&colorHeader, DIB_RGB_COLORS) &&
          maskHeader.biHeight == colorHeader.biHeight &&
          maskHeader.biWidth  == colorHeader.biWidth  &&
          colorHeader.biBitCount > 8 &&
          colorHeader.biSizeImage > 0 &&
          maskHeader.biSizeImage > 0  &&
          (colorTableSize = GetColorTableSize(&colorHeader)) >= 0 &&
          (maskTableSize  = GetColorTableSize(&maskHeader))  >= 0) {
        PRUint32 iconSize = sizeof(ICONFILEHEADER) +
                            sizeof(ICONENTRY) +
                            sizeof(BITMAPINFOHEADER) +
                            colorHeader.biSizeImage +
                            maskHeader.biSizeImage;

        char *buffer = new char[iconSize];
        if (!buffer)
          rv = NS_ERROR_OUT_OF_MEMORY;
        else {
          char *whereTo = buffer;
          int howMuch;

          // the data starts with an icon file header
          ICONFILEHEADER iconHeader;
          iconHeader.ifhReserved = 0;
          iconHeader.ifhType = 1;
          iconHeader.ifhCount = 1;
          howMuch = sizeof(ICONFILEHEADER);
          memcpy(whereTo, &iconHeader, howMuch);
          whereTo += howMuch;

          // followed by the single icon entry
          ICONENTRY iconEntry;
          iconEntry.ieWidth = colorHeader.biWidth;
          iconEntry.ieHeight = colorHeader.biHeight;
          iconEntry.ieColors = 0;
          iconEntry.ieReserved = 0;
          iconEntry.iePlanes = 1;
          iconEntry.ieBitCount = colorHeader.biBitCount;
          iconEntry.ieSizeImage = sizeof(BITMAPINFOHEADER) +
                                  colorHeader.biSizeImage +
                                  maskHeader.biSizeImage;
          iconEntry.ieFileOffset = sizeof(ICONFILEHEADER) + sizeof(ICONENTRY);
          howMuch = sizeof(ICONENTRY);
          memcpy(whereTo, &iconEntry, howMuch);
          whereTo += howMuch;

          // followed by the bitmap info header
          // (doubling the height because icons have two bitmaps)
          colorHeader.biHeight *= 2;
          colorHeader.biSizeImage += maskHeader.biSizeImage;
          howMuch = sizeof(BITMAPINFOHEADER);
          memcpy(whereTo, &colorHeader, howMuch);
          whereTo += howMuch;
          colorHeader.biHeight /= 2;
          colorHeader.biSizeImage -= maskHeader.biSizeImage;

          // followed by the XOR bitmap data (colorHeader)
          // (you'd expect the color table to come here, but it apparently doesn't)
          BITMAPINFO* colorInfo = CreateBitmapInfo(&colorHeader, colorTableSize);
          if (colorInfo && GetDIBits(hDC, iconInfo.hbmColor, 0,
                                     colorHeader.biHeight, whereTo, colorInfo,
                                     DIB_RGB_COLORS)) {
            whereTo += colorHeader.biSizeImage;

            // and finally the AND bitmap data (maskHeader)
            BITMAPINFO* maskInfo = CreateBitmapInfo(&maskHeader, maskTableSize);
            if (maskInfo && GetDIBits(hDC, iconInfo.hbmMask, 0,
                                      maskHeader.biHeight, whereTo, maskInfo,
                                      DIB_RGB_COLORS)) {
              // Now, create a pipe and stuff our data into it
              nsCOMPtr<nsIInputStream> inStream;
              nsCOMPtr<nsIOutputStream> outStream;
              rv = NS_NewPipe(getter_AddRefs(inStream), getter_AddRefs(outStream),
                              iconSize, iconSize, nonBlocking);
              if (NS_SUCCEEDED(rv)) {
                PRUint32 written;
                rv = outStream->Write(buffer, iconSize, &written);
                if (NS_SUCCEEDED(rv)) {
                  NS_ADDREF(*_retval = inStream);
                }
              }

            } // if we got bitmap bits
            delete maskInfo;
          } // if we got mask bits
          delete colorInfo;
          delete [] buffer;
        } // if we allocated the buffer
      } // if we got mask size

      DeleteDC(hDC);
      DeleteObject(iconInfo.hbmColor);
      DeleteObject(iconInfo.hbmMask);
    } // if we got icon info
    DestroyIcon(hIcon);
  } // if we got an hIcon

  // If we didn't make a stream, then fail.
  if (!*_retval && NS_SUCCEEDED(rv))
    rv = NS_ERROR_NOT_AVAILABLE;
  return rv;
}

NS_IMETHODIMP nsIconChannel::GetContentType(nsACString &aContentType) 
{
  aContentType.AssignLiteral("image/x-icon");
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

  // Drop notification callbacks to prevent cycles.
  mCallbacks = nsnull;

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
