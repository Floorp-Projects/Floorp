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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsClipboard.h"
#include <windows.h>
#include <OLE2.h>
#include <SHLOBJ.H>

#include "nsCOMPtr.h"
#include "nsDataObj.h"
#include "nsIClipboardOwner.h"
#include "nsString.h"
#include "nsIFormatConverter.h"
#include "nsITransferable.h"
#include "nsCOMPtr.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"

#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"

#include "nsVoidArray.h"
#include "nsFileSpec.h"

#include "nsGfxCIID.h"
#include "nsIImage.h"

#define DEBUG_PINK 0

static NS_DEFINE_IID(kCImageCID, NS_IMAGE_CID);

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsBaseClipboard()
{
  //NS_INIT_REFCNT();
  mDataObj        = nsnull;
  mIgnoreEmptyNotification = PR_FALSE;
  mWindow         = nsnull;

}

//-------------------------------------------------------------------------
// nsClipboard destructor
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
  if ( mDataObj )
    mDataObj->Release();

}

//-------------------------------------------------------------------------
UINT nsClipboard::GetFormat(const char* aMimeStr)
{
  nsCAutoString mimeStr ( CBufDescriptor(NS_CONST_CAST(char*,aMimeStr), PR_TRUE, PL_strlen(aMimeStr)+1) );
  UINT format = 0;

  if (mimeStr.Equals(kTextMime)) {
    format = CF_TEXT;
  } else if (mimeStr.Equals(kUnicodeMime)) {
    format = CF_UNICODETEXT;
  } else if (mimeStr.Equals(kJPEGImageMime)) {
    format = CF_DIB;
  } else if (mimeStr.Equals(kFileMime)) {
    format = CF_HDROP;
  } else {
    format = ::RegisterClipboardFormat(aMimeStr);
  }
#if DEBUG_PINK
   printf("nsClipboard::GetFormat [%s] 0x%x\n", aMimeStr, format);
#endif
  return format;
}

//-------------------------------------------------------------------------
nsresult nsClipboard::CreateNativeDataObject(nsITransferable * aTransferable, IDataObject ** aDataObj)
{
  if (nsnull == aTransferable) {
    return NS_ERROR_FAILURE;
  }

  // Create our native DataObject that implements 
  // the OLE IDataObject interface
  nsDataObj * dataObj;
  dataObj = new nsDataObj();
  dataObj->AddRef();

  // No set it up with all the right data flavors & enums
  nsresult res = SetupNativeDataObject(aTransferable, dataObj);
  if (NS_OK == res) {
    *aDataObj = dataObj; 
  } else {
    delete dataObj;
  }
  return res;
}

//-------------------------------------------------------------------------
nsresult nsClipboard::SetupNativeDataObject(nsITransferable * aTransferable, IDataObject * aDataObj)
{
  if (nsnull == aTransferable || nsnull == aDataObj) {
    return NS_ERROR_FAILURE;
  }

  nsDataObj * dObj = NS_STATIC_CAST(nsDataObj *, aDataObj);

  // Now give the Transferable to the DataObject 
  // for getting the data out of it
  dObj->SetTransferable(aTransferable);

  // Get the transferable list of data flavors
  nsCOMPtr<nsISupportsArray> dfList;
  aTransferable->FlavorsTransferableCanExport(getter_AddRefs(dfList));

  // Walk through flavors that contain data and register them
  // into the DataObj as supported flavors
  PRUint32 i;
  PRUint32 cnt;
  dfList->Count(&cnt);
  for (i=0;i<cnt;i++) {
    nsCOMPtr<nsISupports> genericFlavor;
    dfList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
    nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericFlavor) );
    if ( currentFlavor ) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));
      UINT format = GetFormat(flavorStr);

      // check here to see if we can the data back from global member
      // XXX need IStream support, or file support
      FORMATETC fe;
      SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);

      // Now tell the native IDataObject about both the DataFlavor and 
      // the native data format
      dObj->AddDataFlavor(flavorStr, &fe);
      
      // if we find text/unicode, also advertise text/plain (which we will convert
      // on our own in nsDataObj::GetText().
      if ( strcmp(flavorStr, kUnicodeMime) == 0 ) {
        FORMATETC fe2;
        SET_FORMATETC(fe2, CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
        dObj->AddDataFlavor("text/plain", &fe2);
      }
    }
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::SetNativeClipboardData ( PRInt32 aWhichClipboard )
{
  if ( aWhichClipboard != kGlobalClipboard )
    return NS_ERROR_FAILURE;

  mIgnoreEmptyNotification = PR_TRUE;

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  // Clear the native clipboard
  ::OleSetClipboard(NULL);

  // Release the existing DataObject
  if (mDataObj) {
    mDataObj->Release();
  }

  IDataObject * dataObj;
  if (NS_OK == CreateNativeDataObject(mTransferable, &dataObj)) { // this add refs
    // cast our native DataObject to its IDataObject pointer
    // and put it on the clipboard
    mDataObj = (IDataObject *)dataObj;
    ::OleSetClipboard(mDataObj);
  }

  mIgnoreEmptyNotification = PR_FALSE;

  return NS_OK;
}


//-------------------------------------------------------------------------
nsresult nsClipboard::GetGlobalData(HGLOBAL aHGBL, void ** aData, PRUint32 * aLen)
{
  // Allocate a new memory buffer and copy the data from global memory.
  // Recall that win98 allocates to nearest DWORD boundary.
  nsresult  result = NS_ERROR_FAILURE;
  if (aHGBL != NULL) {
    LPSTR lpStr = (LPSTR)::GlobalLock(aHGBL);
    DWORD allocSize = ::GlobalSize(aHGBL);
    char* data = NS_STATIC_CAST(char*, nsAllocator::Alloc(allocSize));
    if ( data ) {    
       nsCRT::memcpy ( data, lpStr, allocSize );
    
      ::GlobalUnlock(aHGBL);
      *aData = data;
      *aLen = allocSize;
      result = NS_OK;
    }
  } 
  else {
    // We really shouldn't ever get here
    // but just in case
    *aData = nsnull;
    *aLen  = 0;
    LPVOID lpMsgBuf;

    FormatMessage( 
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        (LPTSTR) &lpMsgBuf,
        0,
        NULL 
    );

    // Display the string.
    MessageBox( NULL, (const char *)lpMsgBuf, "GetLastError", MB_OK|MB_ICONINFORMATION );

    // Free the buffer.
    LocalFree( lpMsgBuf );    
  }

  return result;
}

//-------------------------------------------------------------------------
nsresult nsClipboard::GetNativeDataOffClipboard(nsIWidget * aWindow, UINT /*aIndex*/, UINT aFormat, void ** aData, PRUint32 * aLen)
{
  HGLOBAL   hglb; 
  nsresult  result = NS_ERROR_FAILURE;

  HWND nativeWin = nsnull;//(HWND)aWindow->GetNativeData(NS_NATIVE_WINDOW);
  if (::OpenClipboard(nativeWin)) { 
    hglb = ::GetClipboardData(aFormat); 
    result = GetGlobalData(hglb, aData, aLen);
    ::CloseClipboard();
  }
  return result;
}

#if 1
static void DisplayErrCode(HRESULT hres) 
{
  if (hres == E_INVALIDARG) {
    printf("E_INVALIDARG\n");
  } else
  if (hres == E_UNEXPECTED) {
    printf("E_UNEXPECTED\n");
  } else
  if (hres == E_OUTOFMEMORY) {
    printf("E_OUTOFMEMORY\n");
  } else
  if (hres == DV_E_LINDEX ) {
    printf("DV_E_LINDEX\n");
  } else
  if (hres == DV_E_FORMATETC) {
    printf("DV_E_FORMATETC\n");
  }  else
  if (hres == DV_E_TYMED) {
    printf("DV_E_TYMED\n");
  }  else
  if (hres == DV_E_DVASPECT) {
    printf("DV_E_DVASPECT\n");
  }  else
  if (hres == OLE_E_NOTRUNNING) {
    printf("OLE_E_NOTRUNNING\n");
  }  else
  if (hres == STG_E_MEDIUMFULL) {
    printf("STG_E_MEDIUMFULL\n");
  }  else
  if (hres == DV_E_CLIPFORMAT) {
    printf("DV_E_CLIPFORMAT\n");
  }  else
  if (hres == S_OK) {
    printf("S_OK\n");
  } else {
    printf("****** DisplayErrCode 0x%X\n", hres);
  }
}
#endif
//-------------------------------------------------------------------------
static HRESULT FillSTGMedium(IDataObject * aDataObject, UINT aFormat, LPFORMATETC pFE, LPSTGMEDIUM pSTM, DWORD aTymed)
{
  SET_FORMATETC(*pFE, aFormat, 0, DVASPECT_CONTENT, -1, aTymed);

  // Starting by querying for the data to see if we can get it as from global memory
  HRESULT hres = S_FALSE;
  hres = aDataObject->QueryGetData(pFE);
  DisplayErrCode(hres);
  if (S_OK == hres) {
    hres = aDataObject->GetData(pFE, pSTM);
    DisplayErrCode(hres);
  }
  return hres;
}

/*------------------------------------------------------------------
   DibCopyFromPackedDib is generally used for pasting DIBs from the 
     clipboard.
  ------------------------------------------------------------------*/

PRUint8 * GetDIBBits(BITMAPINFO * aBitmapInfo)
{
  BYTE  * bits ;     
  DWORD   headerSize;
  DWORD   maskSize;
  DWORD   colorSize;

  // Get the size of the information header
     
  headerSize = aBitmapInfo->bmiHeader.biSize ;

  if (headerSize != sizeof (BITMAPCOREHEADER) &&
      headerSize != sizeof (BITMAPINFOHEADER) &&
      //headerSize != sizeof (BITMAPV5HEADER) &&
      headerSize != sizeof (BITMAPV4HEADER)) {
    return NULL ;
  }

  // Get the size of the color masks
  if (headerSize == sizeof (BITMAPINFOHEADER) &&
      aBitmapInfo->bmiHeader.biCompression == BI_BITFIELDS) {
    maskSize = 3 * sizeof (DWORD);
  } else {
    maskSize = 0;
  }

  // Get the size of the color table
  if (headerSize == sizeof (BITMAPCOREHEADER)) {
    int iBitCount = ((BITMAPCOREHEADER *) aBitmapInfo)->bcBitCount;
    if (iBitCount <= 8) {
       colorSize = (1 << iBitCount) * sizeof (RGBTRIPLE);
    } else {
     colorSize = 0 ;
    }
  } else {         // All non-OS/2 compatible DIBs
    if (aBitmapInfo->bmiHeader.biClrUsed > 0)      {
      colorSize = aBitmapInfo->bmiHeader.biClrUsed * sizeof (RGBQUAD);
    } else if (aBitmapInfo->bmiHeader.biBitCount <= 8) {
      colorSize = (1 << aBitmapInfo->bmiHeader.biBitCount) * sizeof (RGBQUAD);
    } else {
      colorSize = 0;
    }
  }

  bits = (BYTE *) aBitmapInfo + headerSize + maskSize + colorSize ;

  return bits;
}

//-------------------------------------------------------------------------
nsresult nsClipboard::GetNativeDataOffClipboard(IDataObject * aDataObject, UINT aIndex, UINT aFormat, void ** aData, PRUint32 * aLen)
{
  nsresult result = NS_ERROR_FAILURE;
  *aData = nsnull;
  *aLen = 0;

  if ( !aDataObject )
    return result;

  UINT    format = aFormat;
  HRESULT hres   = S_FALSE;

  // XXX at the moment we only support global memory transfers
  // It is here where we will add support for native images 
  // and IStream
  FORMATETC fe;
  STGMEDIUM stm;
  hres = FillSTGMedium(aDataObject, format, &fe, &stm, TYMED_HGLOBAL);

  // Currently this is only handling TYMED_HGLOBAL data
  // For Text, Dibs, Files, and generic data (like HTML)
  if (S_OK == hres) {
    switch (stm.tymed) {

      case TYMED_HGLOBAL: 
        {
          switch (fe.cfFormat) {
            case CF_TEXT:
              {
                // Get the data out of the global data handle. The size we return
                // should not include the null because the other platforms don't
                // use nulls, so just return the length we get back from strlen(),
                // since we know CF_TEXT is null terminated. Recall that GetGlobalData() 
                // returns the size of the allocated buffer, not the size of the data 
                // (on 98, these are not the same) so we can't use that.
                PRUint32 allocLen = 0;
                if ( NS_SUCCEEDED(GetGlobalData(stm.hGlobal, aData, &allocLen)) ) {
                  *aLen = nsCRT::strlen ( NS_REINTERPRET_CAST(char*, *aData) );
                  result = NS_OK;
                }
              } break;

			case CF_UNICODETEXT:
              {
                // Get the data out of the global data handle. The size we return
                // should not include the null because the other platforms don't
                // use nulls, so just return the length we get back from strlen(),
                // since we know CF_UNICODETEXT is null terminated. Recall that GetGlobalData() 
                // returns the size of the allocated buffer, not the size of the data 
                // (on 98, these are not the same) so we can't use that.
                PRUint32 allocLen = 0;
                if ( NS_SUCCEEDED(GetGlobalData(stm.hGlobal, aData, &allocLen)) ) {
                  *aLen = nsCRT::strlen(NS_REINTERPRET_CAST(PRUnichar*, *aData)) * 2;
                  result = NS_OK;
                }
              } break;

            case CF_DIB :
              {
                // This creates an nsIImage from 
                // the DIB info on the clipboard
                HGLOBAL hGlobal = stm.hGlobal;
                BYTE  * pGlobal = (BYTE  *)::GlobalLock (hGlobal) ;
                BITMAPV4HEADER * header = (BITMAPV4HEADER *)pGlobal;

                nsIImage * image;
                nsresult rv = nsComponentManager::CreateInstance(kCImageCID, nsnull, 
                                          NS_GET_IID(nsIImage), 
                                          (void**) &image);
                if (NS_OK == rv) {
                  // pull the size informat out of the BITMAPINFO header and
                  // initializew the image
                  PRInt32 width  = header->bV4Width;
                  PRInt32 height = header->bV4Height;
                  PRInt32 depth  = header->bV4BitCount;
                  PRUint8 * bits = GetDIBBits((BITMAPINFO *)pGlobal);

                  image->Init(width, height, depth, nsMaskRequirements_kNoMask);

                  // Now, copy the image bits from the Dib into the nsIImage's buffer
                  PRUint8 * imageBits = image->GetBits();
                  depth = (depth >> 3);
                  PRUint32 size = width * height * depth;
                  CopyMemory(imageBits, bits, size);

                  // return a pointer to the nsIImage
                  *aData = (void *)image;
                  *aLen   = 4;

                  ::GlobalUnlock (hGlobal) ;
                  result = NS_OK;
                }
                
                // XXX NOTE this is temporary
                // until the rest of the image code gets in
                NS_ASSERTION(0, "Take this out when editor can handle images");
              } break;

            case CF_HDROP : 
              {
                // in the case of a file drop, multiple files are stashed within a
                // single data object. In order to match mozilla's D&D apis, we
                // just pull out the file at the requested index, pretending as
                // if there really are multiple drag items.
                HDROP dropFiles = (HDROP) ::GlobalLock(stm.hGlobal);

                UINT numFiles = ::DragQueryFile(dropFiles, 0xFFFFFFFF, NULL, 0);
                NS_ASSERTION ( numFiles > 0, "File drop flavor, but no files...hmmmm" );
                NS_ASSERTION ( aIndex < numFiles, "Asked for a file index out of range of list" );
                if (numFiles > 0) {
                  UINT fileNameLen = ::DragQueryFile(dropFiles, aIndex, nsnull, 0);
                  char* buffer = NS_REINTERPRET_CAST(char*, nsAllocator::Alloc(fileNameLen + 1));
                  if ( buffer ) {
                    ::DragQueryFile(dropFiles, aIndex, buffer, fileNameLen + 1);
                    *aData = buffer;
                    *aLen = fileNameLen;
                    result = NS_OK;
                  }
                  else
                    result = NS_ERROR_OUT_OF_MEMORY;
                }
                ::GlobalUnlock (stm.hGlobal) ;

              } break;

            default: {
              // Check to see if there is HTML on the clipboard
              // if not, then just get the data and return it
              /*UINT format = GetFormat(nsAutoString(kHTMLMime));
              if (fe.cfFormat == format) {
                result = GetGlobalData(stm.hGlobal, aData, aLen);
                //char * str = (char *)*aData;
                //while (str[*aLen-1] == 0) {
                //  (*aLen)--;
                //}
              } else {*/
                result = GetGlobalData(stm.hGlobal, aData, aLen);
              //}
              } break;
          } // switch
        } break;

      case TYMED_GDI: 
        {
          printf("*********************** TYMED_GDI\n");
        } break;

      default:
        break;
    } //switch
  }

  ReleaseStgMedium(&stm);
  return result;
}


//-------------------------------------------------------------------------
nsresult nsClipboard::GetDataFromDataObject(IDataObject     * aDataObject,
                                            UINT              anIndex,
                                            nsIWidget       * aWindow,
                                            nsITransferable * aTransferable)
{
  // make sure we have a good transferable
  if ( !aTransferable )
    return NS_ERROR_INVALID_ARG;

  nsresult res = NS_ERROR_FAILURE;

  // get flavor list that includes all flavors that can be written (including ones 
  // obtained through conversion)
  nsCOMPtr<nsISupportsArray> flavorList;
  res = aTransferable->FlavorsTransferableCanImport ( getter_AddRefs(flavorList) );
  if ( NS_FAILED(res) )
    return NS_ERROR_FAILURE;

  // Walk through flavors and see which flavor is on the clipboard them on the native clipboard,
  PRUint32 i;
  PRUint32 cnt;
  flavorList->Count(&cnt);
  for (i=0;i<cnt;i++) {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
    nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericFlavor) );
    if ( currentFlavor ) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));
      UINT format = GetFormat(flavorStr);

      void   * data;
      PRUint32 dataLen;
      PRBool dataFound = PR_FALSE;
      if (nsnull != aDataObject) {
        if ( NS_SUCCEEDED(GetNativeDataOffClipboard(aDataObject, anIndex, format, &data, &dataLen)) )
          dataFound = PR_TRUE;
      } 
      else if (nsnull != aWindow) {
        if ( NS_SUCCEEDED(GetNativeDataOffClipboard(aWindow, anIndex, format, &data, &dataLen)) )
          dataFound = PR_TRUE;
      }
      if ( !dataFound ) {
	    // if we are looking for text/unicode and we fail to find it on the clipboard first,
        // try again with text/plain. If that is present, convert it to unicode.
        if ( strcmp(flavorStr, kUnicodeMime) == 0 ) {
          nsresult loadResult = GetNativeDataOffClipboard(aDataObject, anIndex, GetFormat(kTextMime), &data, &dataLen);
          if ( NS_SUCCEEDED(loadResult) && data ) {
            const char* castedText = NS_REINTERPRET_CAST(char*, data);          
            PRUnichar* convertedText = nsnull;
            PRInt32 convertedTextLen = 0;
            nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode ( castedText, dataLen, 
                                                                      &convertedText, &convertedTextLen );
            if ( convertedText ) {
              // out with the old, in with the new 
              nsAllocator::Free(data);
              data = convertedText;
              dataLen = convertedTextLen * 2;
              dataFound = PR_TRUE;
            }
          } // if plain text data on clipboard
        } // if looking for text/unicode   
      } // if we try one last ditch effort to find our data

      if ( dataFound ) {
        nsCOMPtr<nsISupports> genericDataWrapper;
	    if ( strcmp(flavorStr, kFileMime) == 0 ) {
	      // we have a file path in |data|. Create an nsLocalFile object.
	      char* filepath = NS_REINTERPRET_CAST(char*, data);
	      nsCOMPtr<nsILocalFile> file;
	      if ( NS_SUCCEEDED(NS_NewLocalFile(filepath, getter_AddRefs(file))) )
	        genericDataWrapper = do_QueryInterface(file);
	    }
	    else {
          // we probably have some form of text. The DOM only wants LF, so convert from Win32 line 
          // endings to DOM line endings.
          PRInt32 signedLen = NS_STATIC_CAST(PRInt32, dataLen);
          nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks ( flavorStr, &data, &signedLen );
          dataLen = signedLen;

          nsCOMPtr<nsISupports> genericDataWrapper;
          nsPrimitiveHelpers::CreatePrimitiveForData ( flavorStr, data, dataLen, getter_AddRefs(genericDataWrapper) );
        }
        
        NS_ASSERTION ( genericDataWrapper, "About to put null data into the transferable" );
        aTransferable->SetTransferData(flavorStr, genericDataWrapper, dataLen);

        nsAllocator::Free ( NS_REINTERPRET_CAST(char*, data) );        
        res = NS_OK;
        
        // we found one, get out of the loop
        break;
      }

    }
  } // foreach flavor

  return res;

}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::GetNativeClipboardData ( nsITransferable * aTransferable, PRInt32 aWhichClipboard )
{
  // make sure we have a good transferable
  if ( !aTransferable || aWhichClipboard != kGlobalClipboard ) {
    return NS_ERROR_FAILURE;
  }

  nsresult res;

  // This makes sure we can use the OLE functionality for the clipboard
  IDataObject * dataObj;
  if (S_OK == ::OleGetClipboard(&dataObj)) {
    // Use OLE IDataObject for clipboard operations
    res = GetDataFromDataObject(dataObj, 0, nsnull, aTransferable);
  } else {
    // do it the old manula way
    res = GetDataFromDataObject(nsnull, 0, mWindow, aTransferable);
  }
  return res;

}


//-------------------------------------------------------------------------
static void PlaceDataOnClipboard(PRUint32 aFormat, char * aData, int aLength)
{
  HGLOBAL     hGlobalMemory;
  PSTR        pGlobalMemory;

  PRInt32 size = aLength + 1;

  if (aLength) {
    // Copy text to Global Memory Area
    hGlobalMemory = (HGLOBAL)::GlobalAlloc(GHND, size);
    if (hGlobalMemory != NULL) {
      pGlobalMemory = (PSTR) ::GlobalLock(hGlobalMemory);

      int i;

      char * s  = aData;
      PRInt32 len = aLength;
      for (i=0;i< len;i++) {
	      *pGlobalMemory++ = *s++;
      }

      // Put data on Clipboard
      ::GlobalUnlock(hGlobalMemory);
      ::SetClipboardData(aFormat, hGlobalMemory);
    }
  }  
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::ForceDataToClipboard ( PRInt32 aWhichClipboard )
{
  // make sure we have a good transferable
  if ( !mTransferable || aWhichClipboard != kGlobalClipboard ) {
    return NS_ERROR_FAILURE;
  }

  HWND nativeWin = nsnull;//(HWND)mWindow->GetNativeData(NS_NATIVE_WINDOW);
  ::OpenClipboard(nativeWin);
  ::EmptyClipboard();

  // get flavor list that includes all flavors that can be written (including ones 
  // obtained through conversion)
  nsCOMPtr<nsISupportsArray> flavorList;
  nsresult errCode = mTransferable->FlavorsTransferableCanExport ( getter_AddRefs(flavorList) );
  if ( NS_FAILED(errCode) )
    return NS_ERROR_FAILURE;

  // Walk through flavors and see which flavor is on the native clipboard,
  PRUint32 i;
  PRUint32 cnt;
  flavorList->Count(&cnt);
  for (i=0;i<cnt;i++) {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
    nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericFlavor) );
    if ( currentFlavor ) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));
      UINT format = GetFormat(flavorStr);

      void   * data;
      PRUint32 dataLen = 0;

      // Get the data as a bunch-o-bytes from the clipboard
      // this call hands back new memory with the contents copied into it
      nsCOMPtr<nsISupports> genericDataWrapper;
      mTransferable->GetTransferData(flavorStr, getter_AddRefs(genericDataWrapper), &dataLen);
      nsPrimitiveHelpers::CreateDataFromPrimitive ( flavorStr, genericDataWrapper, &data, dataLen );

      // now place it on the Clipboard
      if (nsnull != data) {
        PlaceDataOnClipboard(format, (char *)data, dataLen);
      }

      // Now, delete the memory that was created by the transferable
      nsCRT::free ( (char *) data );
    }
  }

  ::CloseClipboard();

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::HasDataMatchingFlavors(nsISupportsArray *aFlavorList, PRInt32 aWhichClipboard,
                                                  PRBool           *_retval)
{
  *_retval = PR_FALSE;
  if ( aWhichClipboard != kGlobalClipboard )
    return NS_OK;

  PRUint32 cnt;
  aFlavorList->Count(&cnt);
  for (PRUint32 i = 0;i < cnt; i++) {
    nsCOMPtr<nsISupports> genericFlavor;
    aFlavorList->GetElementAt (i, getter_AddRefs(genericFlavor));
    nsCOMPtr<nsISupportsString> currentFlavor (do_QueryInterface(genericFlavor));
    if (currentFlavor) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));

#ifdef NS_DEBUG
      if ( strcmp(flavorStr, kTextMime) == 0 )
        NS_WARNING ( "DO NOT USE THE text/plain DATA FLAVOR ANY MORE. USE text/unicode INSTEAD" );
#endif

      UINT format = GetFormat(flavorStr);
      if (::IsClipboardFormatAvailable(format)) {
        *_retval = PR_TRUE;
        break;
      }
      else {
        // if the client asked for unicode and it wasn't present, check if we have CF_TEXT.
        // We'll handle the actual data substitution in the data object.
        if ( strcmp(flavorStr, kUnicodeMime) == 0 ) {
          if ( ::IsClipboardFormatAvailable(GetFormat(kTextMime)) )
            *_retval = PR_TRUE;
        }      
      }
    }
  }

  return NS_OK;
}
