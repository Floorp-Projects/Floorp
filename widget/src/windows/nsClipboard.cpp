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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Sean Echevarria <sean@beatnik.com>
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

#include "nsClipboard.h"
#include <ole2.h>
#include <shlobj.h>
#include <intshcut.h>

// shellapi.h is needed to build with WIN32_LEAN_AND_MEAN
#include <shellapi.h>

#include "nsCOMPtr.h"
#include "nsDataObj.h"
#include "nsIClipboardOwner.h"
#include "nsString.h"
#include "nsIFormatConverter.h"
#include "nsITransferable.h"
#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsPrimitiveHelpers.h"
#include "nsImageClipboard.h"
#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"
#include "nsCRT.h"

#include "nsVoidArray.h"
#include "nsNetUtil.h"

#include "nsIImage.h"
#include "nsIObserverService.h"


// oddly, this isn't in the MSVC headers anywhere.
UINT nsClipboard::CF_HTML = ::RegisterClipboardFormat("HTML Format");


//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsBaseClipboard()
{
  mIgnoreEmptyNotification = PR_FALSE;
  mWindow         = nsnull;

  // Register for a shutdown notification so that we can flush data
  // to the OS clipboard.
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");
  if (observerService)
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
}

//-------------------------------------------------------------------------
// nsClipboard destructor
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{

}

NS_IMPL_ISUPPORTS_INHERITED1(nsClipboard, nsBaseClipboard, nsIObserver)

//-------------------------------------------------------------------------
UINT nsClipboard::GetFormat(const char* aMimeStr)
{
  UINT format;

  if (strcmp(aMimeStr, kTextMime) == 0)
    format = CF_TEXT;
  else if (strcmp(aMimeStr, kUnicodeMime) == 0)
    format = CF_UNICODETEXT;
  else if (strcmp(aMimeStr, kJPEGImageMime) == 0)
    format = CF_DIB;
  else if (strcmp(aMimeStr, kFileMime) == 0 || 
           strcmp(aMimeStr, kFilePromiseMime) == 0)
    format = CF_HDROP;
  else if (strcmp(aMimeStr, kURLMime) == 0 || 
           strcmp(aMimeStr, kURLDataMime) == 0 || 
           strcmp(aMimeStr, kURLDescriptionMime) == 0 || 
           strcmp(aMimeStr, kFilePromiseURLMime) == 0)
    format = CF_UNICODETEXT;
  else if (strcmp(aMimeStr, kNativeHTMLMime) == 0)
    format = CF_HTML;
  else
    format = ::RegisterClipboardFormat(aMimeStr);

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
    nsCOMPtr<nsISupportsCString> currentFlavor ( do_QueryInterface(genericFlavor) );
    if ( currentFlavor ) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));
      UINT format = GetFormat(flavorStr);

      // Now tell the native IDataObject about both our mime type and 
      // the native data format
      FORMATETC fe;
      SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
      dObj->AddDataFlavor(flavorStr, &fe);
      
      // Do various things internal to the implementation, like map one
      // flavor to another or add additional flavors based on what's required
      // for the win32 impl.
      if ( strcmp(flavorStr, kUnicodeMime) == 0 ) {
        // if we find text/unicode, also advertise text/plain (which we will convert
        // on our own in nsDataObj::GetText().
        FORMATETC textFE;
        SET_FORMATETC(textFE, CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
        dObj->AddDataFlavor(kTextMime, &textFE);
      }
      else if ( strcmp(flavorStr, kHTMLMime) == 0 ) {      
        // if we find text/html, also advertise win32's html flavor (which we will convert
        // on our own in nsDataObj::GetText().
        FORMATETC htmlFE;
        SET_FORMATETC(htmlFE, CF_HTML, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
        dObj->AddDataFlavor(kHTMLMime, &htmlFE);     
      }
      else if ( strcmp(flavorStr, kURLMime) == 0 ) {
        // if we're a url, in addition to also being text, we need to register
        // the "file" flavors so that the win32 shell knows to create an internet
        // shortcut when it sees one of these beasts.
        FORMATETC shortcutFE;
        SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR), 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
        dObj->AddDataFlavor(kURLMime, &shortcutFE);      
        SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_FILECONTENTS), 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
        dObj->AddDataFlavor(kURLMime, &shortcutFE);  
#ifdef CFSTR_SHELLURL
        SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_SHELLURL), 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
        dObj->AddDataFlavor(kURLMime, &shortcutFE);      
#endif
      }
      else if ( strcmp(flavorStr, kPNGImageMime) == 0 || strcmp(flavorStr, kJPEGImageMime) == 0 ||
                  strcmp(flavorStr, kGIFImageMime) == 0 || strcmp(flavorStr, kNativeImageMime) == 0  ) {
        // if we're an image, register the native bitmap flavor
        FORMATETC imageFE;
        SET_FORMATETC(imageFE, CF_DIB, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
        dObj->AddDataFlavor(flavorStr, &imageFE);      
      }
      else if ( strcmp(flavorStr, kFilePromiseMime) == 0 ) {
         // if we're a file promise flavor, also register the 
         // CFSTR_PREFERREDDROPEFFECT format.  The data object
         // returns a value of DROPEFFECTS_MOVE to the drop target
         // when it asks for the value of this format.  This causes
         // the file to be moved from the temporary location instead
         // of being copied.  The right thing to do here is to call
         // SetData() on the data object and set the value of this format
         // to DROPEFFECTS_MOVE on this particular data object.  But,
         // since all the other clipboard formats follow the model of setting
         // data on the data object only when the drop object calls GetData(),
         // I am leaving this format's value hard coded in the data object.
         // We can change this if other consumers of this format get added to this
         // codebase and they need different values.
        FORMATETC shortcutFE;
        SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT), 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
        dObj->AddDataFlavor(kFilePromiseMime, &shortcutFE);
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

  IDataObject * dataObj;
  if ( NS_SUCCEEDED(CreateNativeDataObject(mTransferable, &dataObj)) ) { // this add refs
    ::OleSetClipboard(dataObj);
    dataObj->Release();
  }

  mIgnoreEmptyNotification = PR_FALSE;

  return NS_OK;
}


//-------------------------------------------------------------------------
nsresult nsClipboard::GetGlobalData(HGLOBAL aHGBL, void ** aData, PRUint32 * aLen)
{
  // Allocate a new memory buffer and copy the data from global memory.
  // Recall that win98 allocates to nearest DWORD boundary. As a safety
  // precaution, allocate an extra 2 bytes (but don't report them!) and
  // null them out to ensure that all of our strlen calls will succeed.
  nsresult  result = NS_ERROR_FAILURE;
  if (aHGBL != NULL) {
    LPSTR lpStr = (LPSTR)::GlobalLock(aHGBL);
    DWORD allocSize = ::GlobalSize(aHGBL);
    char* data = NS_STATIC_CAST(char*, nsMemory::Alloc(allocSize + sizeof(PRUnichar)));
    if ( data ) {    
      memcpy ( data, lpStr, allocSize );
      data[allocSize] = data[allocSize + 1] = '\0';     // null terminate for safety

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

static void DisplayErrCode(HRESULT hres) 
{
#if defined(DEBUG_rods) || defined(DEBUG_pinkerton)
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
#endif
}

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
    static CLIPFORMAT fileDescriptorFlavor = ::RegisterClipboardFormat( CFSTR_FILEDESCRIPTOR ); 
    static CLIPFORMAT fileFlavor = ::RegisterClipboardFormat( CFSTR_FILECONTENTS ); 
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
                  *aLen = strlen ( NS_REINTERPRET_CAST(char*, *aData) );
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
                HGLOBAL hGlobal = stm.hGlobal;
                BYTE  * pGlobal = (BYTE  *)::GlobalLock (hGlobal) ;
                BITMAPV4HEADER * header = (BITMAPV4HEADER *)pGlobal;

                nsImageFromClipboard converter ( header );
                nsIImage* image;
                converter.GetImage ( &image );   // addrefs for us, don't release
                if ( image ) {
                  *aData = image;
                  *aLen = sizeof(nsIImage*);
                  result = NS_OK;
                }

                ::GlobalUnlock (hGlobal) ;
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
                  char* buffer = NS_REINTERPRET_CAST(char*, nsMemory::Alloc(fileNameLen + 1));
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
              if ( fe.cfFormat == fileDescriptorFlavor || fe.cfFormat == fileFlavor ) {
                NS_WARNING ( "Mozilla doesn't yet understand how to read this type of file flavor" );
              } 
              else {
                // Get the data out of the global data handle. The size we return
                // should not include the null because the other platforms don't
                // use nulls, so just return the length we get back from strlen(),
                // since we know CF_UNICODETEXT is null terminated. Recall that GetGlobalData() 
                // returns the size of the allocated buffer, not the size of the data 
                // (on 98, these are not the same) so we can't use that.
                //
                // NOTE: we are assuming that anything that falls into this default case
                //        is unicode. As we start to get more kinds of binary data, this
                //        may become an incorrect assumption. Stay tuned.
                PRUint32 allocLen = 0;
                if ( NS_SUCCEEDED(GetGlobalData(stm.hGlobal, aData, &allocLen)) ) {
                  if ( fe.cfFormat == CF_HTML ) {
                    // CF_HTML is actually UTF8, not unicode, so disregard the assumption
                    // above. We have to check the header for the actual length, and we'll
                    // do that in FindPlatformHTML(). For now, return the allocLen. This
                    // case is mostly to ensure we don't try to call strlen on the buffer.
                    *aLen = allocLen;
                  }
                  else
                    *aLen = nsCRT::strlen(NS_REINTERPRET_CAST(PRUnichar*, *aData)) * sizeof(PRUnichar);
                  result = NS_OK;
                }
              }
            } break;
          } // switch
        } break;

      case TYMED_GDI: 
        {
#ifdef DEBUG
          printf("*********************** TYMED_GDI\n");
#endif
        } break;

      default:
        break;
    } //switch
    
    ReleaseStgMedium(&stm);
  }

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
    nsCOMPtr<nsISupportsCString> currentFlavor ( do_QueryInterface(genericFlavor) );
    if ( currentFlavor ) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));
      UINT format = GetFormat(flavorStr);

      // Try to get the data using the desired flavor. This might fail, but all is
      // not lost.
      void* data = nsnull;
      PRUint32 dataLen = 0;
      PRBool dataFound = PR_FALSE;
      if (nsnull != aDataObject) {
        if ( NS_SUCCEEDED(GetNativeDataOffClipboard(aDataObject, anIndex, format, &data, &dataLen)) )
          dataFound = PR_TRUE;
      } 
      else if (nsnull != aWindow) {
        if ( NS_SUCCEEDED(GetNativeDataOffClipboard(aWindow, anIndex, format, &data, &dataLen)) )
          dataFound = PR_TRUE;
      }

      // This is our second chance to try to find some data, having not found it
      // when directly asking for the flavor. Let's try digging around in other
      // flavors to help satisfy our craving for data.
      if ( !dataFound ) {
        if ( strcmp(flavorStr, kUnicodeMime) == 0 )
          dataFound = FindUnicodeFromPlainText ( aDataObject, anIndex, &data, &dataLen );
        else if ( strcmp(flavorStr, kURLMime) == 0 )
          dataFound = FindURLFromLocalFile ( aDataObject, anIndex, &data, &dataLen );
      } // if we try one last ditch effort to find our data

      // Hopefully by this point we've found it and can go about our business
      if ( dataFound ) {
        nsCOMPtr<nsISupports> genericDataWrapper;
	      if ( strcmp(flavorStr, kFileMime) == 0 ) {
	        // we have a file path in |data|. Create an nsLocalFile object.
	        nsDependentCString filepath(NS_REINTERPRET_CAST(char*, data));
	        nsCOMPtr<nsILocalFile> file;
	        if ( NS_SUCCEEDED(NS_NewNativeLocalFile(filepath, PR_FALSE, getter_AddRefs(file))) )
	          genericDataWrapper = do_QueryInterface(file);
	      }
        else if ( strcmp(flavorStr, kNativeHTMLMime) == 0) {
          // the editor folks want CF_HTML exactly as it's on the clipboard, no conversions,
          // no fancy stuff. Pull it off the clipboard, stuff it into a wrapper and hand
          // it back to them.
          if ( FindPlatformHTML(aDataObject, anIndex, &data, &dataLen) )
            nsPrimitiveHelpers::CreatePrimitiveForData ( flavorStr, data, dataLen, getter_AddRefs(genericDataWrapper) );
          else
            continue;     // something wrong with this flavor, keep looking for other data
        }
        else {
          // we probably have some form of text. The DOM only wants LF, so convert from Win32 line 
          // endings to DOM line endings.
          PRInt32 signedLen = NS_STATIC_CAST(PRInt32, dataLen);
          nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks ( flavorStr, &data, &signedLen );
          dataLen = signedLen;

          nsPrimitiveHelpers::CreatePrimitiveForData ( flavorStr, data, dataLen, getter_AddRefs(genericDataWrapper) );
        }
        
        NS_ASSERTION ( genericDataWrapper, "About to put null data into the transferable" );
        aTransferable->SetTransferData(flavorStr, genericDataWrapper, dataLen);

        nsMemory::Free ( NS_REINTERPRET_CAST(char*, data) );        
        res = NS_OK;
        
        // we found one, get out of the loop
        break;
      }

    }
  } // foreach flavor

  return res;

}



//
// FindPlatformHTML
//
// Someone asked for the OS CF_HTML flavor. We give it back to them exactly as-is.
//
PRBool
nsClipboard :: FindPlatformHTML ( IDataObject* inDataObject, UINT inIndex, void** outData, PRUint32* outDataLen )
{
  PRBool dataFound = PR_FALSE;

  if ( outData && *outData ) {
    // CF_HTML is UTF8, not unicode. We also can't rely on it being null-terminated
    // so we have to check the CF_HTML header for the correct length. The length we return
    // is the bytecount from the beginning of the data to the end, without
    // the null termination. Because it's UTF8, we're guaranteed the header is ascii (yay!).
    float vers = 0.0;
    PRUint32 startOfData = 0;
    sscanf((char*)*outData, "Version:%f\nStartHTML:%d\nEndHTML:%d", &vers, &startOfData, outDataLen);
    NS_ASSERTION(startOfData && *outDataLen, "Couldn't parse CF_HTML description header");
 
    if ( *outDataLen )
      dataFound = PR_TRUE;
  }

  return dataFound;
}


//
// FindURLFromLocalFile
//
// we are looking for text/unicode and we failed to find it on the clipboard first,
// try again with text/plain. If that is present, convert it to unicode.
//
PRBool
nsClipboard :: FindUnicodeFromPlainText ( IDataObject* inDataObject, UINT inIndex, void** outData, PRUint32* outDataLen )
{
  PRBool dataFound = PR_FALSE;

  // we are looking for text/unicode and we failed to find it on the clipboard first,
  // try again with text/plain. If that is present, convert it to unicode.
  nsresult loadResult = GetNativeDataOffClipboard(inDataObject, inIndex, GetFormat(kTextMime), outData, outDataLen);
  if ( NS_SUCCEEDED(loadResult) && *outData ) {
    const char* castedText = NS_REINTERPRET_CAST(char*, *outData);          
    PRUnichar* convertedText = nsnull;
    PRInt32 convertedTextLen = 0;
    nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode ( castedText, *outDataLen, 
                                                              &convertedText, &convertedTextLen );
    if ( convertedText ) {
      // out with the old, in with the new 
      nsMemory::Free(*outData);
      *outData = convertedText;
      *outDataLen = convertedTextLen * sizeof(PRUnichar);
      dataFound = PR_TRUE;
    }
  } // if plain text data on clipboard

  return dataFound;

} // FindUnicodeFromPlainText


//
// FindURLFromLocalFile
//
// we are looking for a URL and couldn't find it, try again with looking for 
// a local file. If we have one, it may either be a normal file or an internet shortcut.
// In both cases, however, we can get a URL (it will be a file:// url in the
// local file case).
//
PRBool
nsClipboard :: FindURLFromLocalFile ( IDataObject* inDataObject, UINT inIndex, void** outData, PRUint32* outDataLen )
{
  PRBool dataFound = PR_FALSE;

  nsresult loadResult = GetNativeDataOffClipboard(inDataObject, inIndex, GetFormat(kFileMime), outData, outDataLen);
  if ( NS_SUCCEEDED(loadResult) && *outData ) {
    // we have a file path in |data|. Is it an internet shortcut or a normal file?
    const char* filepath = NS_REINTERPRET_CAST(char*, *outData);
    nsCOMPtr<nsILocalFile> file;
    nsresult rv = NS_NewNativeLocalFile(nsDependentCString(filepath), PR_TRUE, getter_AddRefs(file));
    if (NS_FAILED(rv))
      return dataFound;

    if ( IsInternetShortcut(filepath) ) {
      nsCAutoString url;
      ResolveShortcut( file, url );
      if ( !url.IsEmpty() ) {
        // convert it to unicode and pass it out
        nsMemory::Free(*outData);
        *outData = UTF8ToNewUnicode(url);
        *outDataLen = nsCRT::strlen(NS_STATIC_CAST(PRUnichar*, *outData));

        dataFound = PR_TRUE;
      }
    }
    else {
      // we have a normal file, use some Necko objects to get our file path
      nsCAutoString urlSpec;
      NS_GetURLSpecFromFile(file, urlSpec);

      // convert it to unicode and pass it out
      nsMemory::Free(*outData);
      *outData = UTF8ToNewUnicode(urlSpec);
      *outDataLen = nsCRT::strlen(NS_STATIC_CAST(PRUnichar*, *outData));
      dataFound = PR_TRUE;
    } // else regular file
  }

  return dataFound;
} // FindURLFromLocalFile


//
// ResolveShortcut
//
void
nsClipboard :: ResolveShortcut ( nsILocalFile* aFile, nsACString& outURL )
{
  nsCOMPtr<nsIFileProtocolHandler> fph;
  nsresult rv = NS_GetFileProtocolHandler(getter_AddRefs(fph));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIURI> uri;
  rv = fph->ReadURLFile(aFile, getter_AddRefs(uri));
  if (NS_FAILED(rv))
    return;

  uri->GetSpec(outURL);
} // ResolveShortcut


//
// IsInternetShortcut
//
// A file is an Internet Shortcut if it ends with .URL
//
PRBool
nsClipboard :: IsInternetShortcut ( const char* inFileName ) 
{
  if ( strstr(inFileName, ".URL") || strstr(inFileName, ".url") )
    return PR_TRUE;
  
  return PR_FALSE;
} // IsInternetShortcut


//-------------------------------------------------------------------------
NS_IMETHODIMP 
nsClipboard::GetNativeClipboardData ( nsITransferable * aTransferable, PRInt32 aWhichClipboard )
{
  // make sure we have a good transferable
  if ( !aTransferable || aWhichClipboard != kGlobalClipboard )
    return NS_ERROR_FAILURE;

  nsresult res;

  // This makes sure we can use the OLE functionality for the clipboard
  IDataObject * dataObj;
  if (S_OK == ::OleGetClipboard(&dataObj)) {
    // Use OLE IDataObject for clipboard operations
    res = GetDataFromDataObject(dataObj, 0, nsnull, aTransferable);
    dataObj->Release();
  } 
  else {
    // do it the old manual way
    res = GetDataFromDataObject(nsnull, 0, mWindow, aTransferable);
  }
  return res;

}


//-------------------------------------------------------------------------
NS_IMETHODIMP
nsClipboard::Observe(nsISupports *aSubject, const char *aTopic,
                     const PRUnichar *aData)
{
  // This will be called on shutdown.
  ::OleFlushClipboard();
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
  for ( PRUint32 i = 0;i < cnt; ++i ) {
    nsCOMPtr<nsISupports> genericFlavor;
    aFlavorList->GetElementAt (i, getter_AddRefs(genericFlavor));
    nsCOMPtr<nsISupportsCString> currentFlavor (do_QueryInterface(genericFlavor));
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
        // We haven't found the exact flavor the client asked for, but maybe we can
        // still find it from something else that's on the clipboard...
        if ( strcmp(flavorStr, kUnicodeMime) == 0 ) {
          // client asked for unicode and it wasn't present, check if we have CF_TEXT.
          // We'll handle the actual data substitution in the data object.
          if ( ::IsClipboardFormatAvailable(GetFormat(kTextMime)) )
            *_retval = PR_TRUE;
        }
      }
    }
  }

  return NS_OK;
}
