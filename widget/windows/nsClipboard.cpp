/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsNativeCharsetUtils.h"
#include "nsIFormatConverter.h"
#include "nsITransferable.h"
#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsPrimitiveHelpers.h"
#include "nsImageClipboard.h"
#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsEscape.h"

#ifdef PR_LOGGING
PRLogModuleInfo* gWin32ClipboardLog = nsnull;
#endif

// oddly, this isn't in the MSVC headers anywhere.
UINT nsClipboard::CF_HTML = ::RegisterClipboardFormatW(L"HTML Format");


//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsBaseClipboard()
{
#ifdef PR_LOGGING
  if (!gWin32ClipboardLog) {
    gWin32ClipboardLog = PR_NewLogModule("nsClipboard");
  }
#endif

  mIgnoreEmptyNotification = false;
  mWindow         = nsnull;
}

//-------------------------------------------------------------------------
// nsClipboard destructor
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{

}

//-------------------------------------------------------------------------
UINT nsClipboard::GetFormat(const char* aMimeStr)
{
  UINT format;

  if (strcmp(aMimeStr, kTextMime) == 0)
    format = CF_TEXT;
  else if (strcmp(aMimeStr, kUnicodeMime) == 0)
    format = CF_UNICODETEXT;
  else if (strcmp(aMimeStr, kJPEGImageMime) == 0 ||
           strcmp(aMimeStr, kJPGImageMime) == 0 ||
           strcmp(aMimeStr, kPNGImageMime) == 0)
    format = CF_DIB;
  else if (strcmp(aMimeStr, kFileMime) == 0 ||
           strcmp(aMimeStr, kFilePromiseMime) == 0)
    format = CF_HDROP;
  else if (strcmp(aMimeStr, kNativeHTMLMime) == 0)
    format = CF_HTML;
  else
    format = ::RegisterClipboardFormatW(NS_ConvertASCIItoUTF16(aMimeStr).get());

  return format;
}

//-------------------------------------------------------------------------
nsresult nsClipboard::CreateNativeDataObject(nsITransferable * aTransferable, IDataObject ** aDataObj, nsIURI * uri)
{
  if (nsnull == aTransferable) {
    return NS_ERROR_FAILURE;
  }

  // Create our native DataObject that implements 
  // the OLE IDataObject interface
  nsDataObj * dataObj = new nsDataObj(uri);

  if (!dataObj) 
    return NS_ERROR_OUT_OF_MEMORY;

  dataObj->AddRef();

  // Now set it up with all the right data flavors & enums
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

  nsDataObj * dObj = static_cast<nsDataObj *>(aDataObj);

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
        SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORA), 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
        dObj->AddDataFlavor(kURLMime, &shortcutFE);      
        SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW), 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
        dObj->AddDataFlavor(kURLMime, &shortcutFE);      
        SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_FILECONTENTS), 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
        dObj->AddDataFlavor(kURLMime, &shortcutFE);  
        SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_INETURLA), 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
        dObj->AddDataFlavor(kURLMime, &shortcutFE);      
        SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_INETURLW), 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
        dObj->AddDataFlavor(kURLMime, &shortcutFE);      
      }
      else if ( strcmp(flavorStr, kPNGImageMime) == 0 || strcmp(flavorStr, kJPEGImageMime) == 0 ||
                strcmp(flavorStr, kJPGImageMime) == 0 || strcmp(flavorStr, kGIFImageMime) == 0 ||
                strcmp(flavorStr, kNativeImageMime) == 0  ) {
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

  mIgnoreEmptyNotification = true;

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  IDataObject * dataObj;
  if ( NS_SUCCEEDED(CreateNativeDataObject(mTransferable, &dataObj, NULL)) ) { // this add refs dataObj
    ::OleSetClipboard(dataObj);
    dataObj->Release();
  } else {
    // Clear the native clipboard
    ::OleSetClipboard(NULL);
  }

  mIgnoreEmptyNotification = false;

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
    LPSTR lpStr = (LPSTR) GlobalLock(aHGBL);
    DWORD allocSize = GlobalSize(aHGBL);
    char* data = static_cast<char*>(nsMemory::Alloc(allocSize + sizeof(PRUnichar)));
    if ( data ) {    
      memcpy ( data, lpStr, allocSize );
      data[allocSize] = data[allocSize + 1] = '\0';     // null terminate for safety

      GlobalUnlock(aHGBL);
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

    FormatMessageW( 
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        (LPWSTR) &lpMsgBuf,
        0,
        NULL 
    );

    // Display the string.
    MessageBoxW( NULL, (LPCWSTR) lpMsgBuf, L"GetLastError", MB_OK|MB_ICONINFORMATION );

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
    PR_LOG(gWin32ClipboardLog, PR_LOG_ALWAYS, ("E_INVALIDARG\n"));
  } else
  if (hres == E_UNEXPECTED) {
    PR_LOG(gWin32ClipboardLog, PR_LOG_ALWAYS, ("E_UNEXPECTED\n"));
  } else
  if (hres == E_OUTOFMEMORY) {
    PR_LOG(gWin32ClipboardLog, PR_LOG_ALWAYS, ("E_OUTOFMEMORY\n"));
  } else
  if (hres == DV_E_LINDEX ) {
    PR_LOG(gWin32ClipboardLog, PR_LOG_ALWAYS, ("DV_E_LINDEX\n"));
  } else
  if (hres == DV_E_FORMATETC) {
    PR_LOG(gWin32ClipboardLog, PR_LOG_ALWAYS, ("DV_E_FORMATETC\n"));
  }  else
  if (hres == DV_E_TYMED) {
    PR_LOG(gWin32ClipboardLog, PR_LOG_ALWAYS, ("DV_E_TYMED\n"));
  }  else
  if (hres == DV_E_DVASPECT) {
    PR_LOG(gWin32ClipboardLog, PR_LOG_ALWAYS, ("DV_E_DVASPECT\n"));
  }  else
  if (hres == OLE_E_NOTRUNNING) {
    PR_LOG(gWin32ClipboardLog, PR_LOG_ALWAYS, ("OLE_E_NOTRUNNING\n"));
  }  else
  if (hres == STG_E_MEDIUMFULL) {
    PR_LOG(gWin32ClipboardLog, PR_LOG_ALWAYS, ("STG_E_MEDIUMFULL\n"));
  }  else
  if (hres == DV_E_CLIPFORMAT) {
    PR_LOG(gWin32ClipboardLog, PR_LOG_ALWAYS, ("DV_E_CLIPFORMAT\n"));
  }  else
  if (hres == S_OK) {
    PR_LOG(gWin32ClipboardLog, PR_LOG_ALWAYS, ("S_OK\n"));
  } else {
    PR_LOG(gWin32ClipboardLog, PR_LOG_ALWAYS, 
           ("****** DisplayErrCode 0x%X\n", hres));
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
// If aFormat is CF_DIB, aMIMEImageFormat must be a type for which we have
// an image encoder (e.g. image/png).
// For other values of aFormat, it is OK to pass null for aMIMEImageFormat.
nsresult nsClipboard::GetNativeDataOffClipboard(IDataObject * aDataObject, UINT aIndex, UINT aFormat, const char * aMIMEImageFormat, void ** aData, PRUint32 * aLen)
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
    static CLIPFORMAT fileDescriptorFlavorA = ::RegisterClipboardFormat( CFSTR_FILEDESCRIPTORA ); 
    static CLIPFORMAT fileDescriptorFlavorW = ::RegisterClipboardFormat( CFSTR_FILEDESCRIPTORW ); 
    static CLIPFORMAT fileFlavor = ::RegisterClipboardFormat( CFSTR_FILECONTENTS ); 
    static CLIPFORMAT preferredDropEffect = ::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);

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
                  *aLen = strlen ( reinterpret_cast<char*>(*aData) );
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
                  *aLen = NS_strlen(reinterpret_cast<PRUnichar*>(*aData)) * 2;
                  result = NS_OK;
                }
              } break;

            case CF_DIB :
              if (aMIMEImageFormat)
              {
                PRUint32 allocLen = 0;
                unsigned char * clipboardData;
                if (NS_SUCCEEDED(GetGlobalData(stm.hGlobal, (void **)&clipboardData, &allocLen)))
                {
                  nsImageFromClipboard converter;
                  nsIInputStream * inputStream;
                  converter.GetEncodedImageStream(clipboardData, aMIMEImageFormat, &inputStream);   // addrefs for us, don't release
                  if ( inputStream ) {
                    *aData = inputStream;
                    *aLen = sizeof(nsIInputStream*);
                    result = NS_OK;
                  }
                }
              } break;

            case CF_HDROP : 
              {
                // in the case of a file drop, multiple files are stashed within a
                // single data object. In order to match mozilla's D&D apis, we
                // just pull out the file at the requested index, pretending as
                // if there really are multiple drag items.
                HDROP dropFiles = (HDROP) GlobalLock(stm.hGlobal);

                UINT numFiles = ::DragQueryFileW(dropFiles, 0xFFFFFFFF, NULL, 0);
                NS_ASSERTION ( numFiles > 0, "File drop flavor, but no files...hmmmm" );
                NS_ASSERTION ( aIndex < numFiles, "Asked for a file index out of range of list" );
                if (numFiles > 0) {
                  UINT fileNameLen = ::DragQueryFileW(dropFiles, aIndex, nsnull, 0);
                  PRUnichar* buffer = reinterpret_cast<PRUnichar*>(nsMemory::Alloc((fileNameLen + 1) * sizeof(PRUnichar)));
                  if ( buffer ) {
                    ::DragQueryFileW(dropFiles, aIndex, buffer, fileNameLen + 1);
                    *aData = buffer;
                    *aLen = fileNameLen * sizeof(PRUnichar);
                    result = NS_OK;
                  }
                  else
                    result = NS_ERROR_OUT_OF_MEMORY;
                }
                GlobalUnlock (stm.hGlobal) ;

              } break;

            default: {
              if ( fe.cfFormat == fileDescriptorFlavorA || fe.cfFormat == fileDescriptorFlavorW || fe.cfFormat == fileFlavor ) {
                NS_WARNING ( "Mozilla doesn't yet understand how to read this type of file flavor" );
              } 
              else
              {
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
                  } else if (fe.cfFormat == preferredDropEffect) {
                    // As per the MSDN doc entitled: "Shell Clipboard Formats"
                    // CFSTR_PREFERREDDROPEFFECT should return a DWORD
                    // Reference: http://msdn.microsoft.com/en-us/library/bb776902(v=vs.85).aspx
                    NS_ASSERTION(allocLen == sizeof(DWORD),
                      "CFSTR_PREFERREDDROPEFFECT should return a DWORD");
                    *aLen = allocLen;
                  } else {
                    *aLen = NS_strlen(reinterpret_cast<PRUnichar*>(*aData)) * 
                            sizeof(PRUnichar);
                  }
                  result = NS_OK;
                }
              }
            } break;
          } // switch
        } break;

      case TYMED_GDI: 
        {
#ifdef DEBUG
          PR_LOG(gWin32ClipboardLog, PR_LOG_ALWAYS, 
                 ("*********************** TYMED_GDI\n"));
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
      bool dataFound = false;
      if (nsnull != aDataObject) {
        if ( NS_SUCCEEDED(GetNativeDataOffClipboard(aDataObject, anIndex, format, flavorStr, &data, &dataLen)) )
          dataFound = true;
      } 
      else if (nsnull != aWindow) {
        if ( NS_SUCCEEDED(GetNativeDataOffClipboard(aWindow, anIndex, format, &data, &dataLen)) )
          dataFound = true;
      }

      // This is our second chance to try to find some data, having not found it
      // when directly asking for the flavor. Let's try digging around in other
      // flavors to help satisfy our craving for data.
      if ( !dataFound ) {
        if ( strcmp(flavorStr, kUnicodeMime) == 0 )
          dataFound = FindUnicodeFromPlainText ( aDataObject, anIndex, &data, &dataLen );
        else if ( strcmp(flavorStr, kURLMime) == 0 ) {
          // drags from other windows apps expose the native
          // CFSTR_INETURL{A,W} flavor
          dataFound = FindURLFromNativeURL ( aDataObject, anIndex, &data, &dataLen );
          if ( !dataFound )
            dataFound = FindURLFromLocalFile ( aDataObject, anIndex, &data, &dataLen );
        }
      } // if we try one last ditch effort to find our data

      // Hopefully by this point we've found it and can go about our business
      if ( dataFound ) {
        nsCOMPtr<nsISupports> genericDataWrapper;
          if ( strcmp(flavorStr, kFileMime) == 0 ) {
            // we have a file path in |data|. Create an nsLocalFile object.
            nsDependentString filepath(reinterpret_cast<PRUnichar*>(data));
            nsCOMPtr<nsIFile> file;
            if ( NS_SUCCEEDED(NS_NewLocalFile(filepath, false, getter_AddRefs(file))) )
              genericDataWrapper = do_QueryInterface(file);
            nsMemory::Free(data);
          }
        else if ( strcmp(flavorStr, kNativeHTMLMime) == 0) {
          // the editor folks want CF_HTML exactly as it's on the clipboard, no conversions,
          // no fancy stuff. Pull it off the clipboard, stuff it into a wrapper and hand
          // it back to them.
          if ( FindPlatformHTML(aDataObject, anIndex, &data, &dataLen) )
            nsPrimitiveHelpers::CreatePrimitiveForData ( flavorStr, data, dataLen, getter_AddRefs(genericDataWrapper) );
          else
          {
            nsMemory::Free(data);
            continue;     // something wrong with this flavor, keep looking for other data
          }
          nsMemory::Free(data);
        }
        else if ( strcmp(flavorStr, kJPEGImageMime) == 0 ||
                  strcmp(flavorStr, kJPGImageMime) == 0 ||
                  strcmp(flavorStr, kPNGImageMime) == 0) {
          nsIInputStream * imageStream = reinterpret_cast<nsIInputStream*>(data);
          genericDataWrapper = do_QueryInterface(imageStream);
          NS_IF_RELEASE(imageStream);
        }
        else {
          // we probably have some form of text. The DOM only wants LF, so convert from Win32 line 
          // endings to DOM line endings.
          PRInt32 signedLen = static_cast<PRInt32>(dataLen);
          nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks ( flavorStr, &data, &signedLen );
          dataLen = signedLen;

          nsPrimitiveHelpers::CreatePrimitiveForData ( flavorStr, data, dataLen, getter_AddRefs(genericDataWrapper) );
          nsMemory::Free(data);
        }
        
        NS_ASSERTION ( genericDataWrapper, "About to put null data into the transferable" );
        aTransferable->SetTransferData(flavorStr, genericDataWrapper, dataLen);
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
bool
nsClipboard :: FindPlatformHTML ( IDataObject* inDataObject, UINT inIndex, void** outData, PRUint32* outDataLen )
{
  // Reference: MSDN doc entitled "HTML Clipboard Format"
  // http://msdn.microsoft.com/en-us/library/aa767917(VS.85).aspx#unknown_854
  // CF_HTML is UTF8, not unicode. We also can't rely on it being null-terminated
  // so we have to check the CF_HTML header for the correct length. 
  // The length we return is the bytecount from the beginning of the selected data to the end
  // of the selected data, without the null termination. Because it's UTF8, we're guaranteed 
  // the header is ASCII.

  if (!outData || !*outData) {
    return false;
  }

  char version[8] = { 0 };
  PRInt32 startOfData = 0;
  PRInt32 endOfData = 0;
  int numFound = sscanf((char*)*outData, "Version:%7s\nStartHTML:%d\nEndHTML:%d", 
                        version, &startOfData, &endOfData);

  if (numFound != 3 || startOfData < -1 || endOfData < -1) {
    return false;
  }

  // Fixup the start and end markers if they have no context (set to -1)
  if (startOfData == -1) {
    startOfData = 0;
  }
  if (endOfData == -1) {
    endOfData = *outDataLen;
  }

  // Make sure we were passed sane values within our buffer size.
  if (!endOfData || startOfData >= endOfData || 
      endOfData > *outDataLen) {
    return false;
  }
  
  // We want to return the buffer not offset by startOfData because it will be 
  // parsed out later (probably by nsHTMLEditor::ParseCFHTML) when it is still
  // in CF_HTML format.
  *outDataLen = endOfData;
  return true;
}


//
// FindUnicodeFromPlainText
//
// we are looking for text/unicode and we failed to find it on the clipboard first,
// try again with text/plain. If that is present, convert it to unicode.
//
bool
nsClipboard :: FindUnicodeFromPlainText ( IDataObject* inDataObject, UINT inIndex, void** outData, PRUint32* outDataLen )
{
  bool dataFound = false;

  // we are looking for text/unicode and we failed to find it on the clipboard first,
  // try again with text/plain. If that is present, convert it to unicode.
  nsresult loadResult = GetNativeDataOffClipboard(inDataObject, inIndex, GetFormat(kTextMime), nsnull, outData, outDataLen);
  if ( NS_SUCCEEDED(loadResult) && *outData ) {
    const char* castedText = reinterpret_cast<char*>(*outData);          
    PRUnichar* convertedText = nsnull;
    PRInt32 convertedTextLen = 0;
    nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode ( castedText, *outDataLen, 
                                                              &convertedText, &convertedTextLen );
    if ( convertedText ) {
      // out with the old, in with the new 
      nsMemory::Free(*outData);
      *outData = convertedText;
      *outDataLen = convertedTextLen * sizeof(PRUnichar);
      dataFound = true;
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
bool
nsClipboard :: FindURLFromLocalFile ( IDataObject* inDataObject, UINT inIndex, void** outData, PRUint32* outDataLen )
{
  bool dataFound = false;

  nsresult loadResult = GetNativeDataOffClipboard(inDataObject, inIndex, GetFormat(kFileMime), nsnull, outData, outDataLen);
  if ( NS_SUCCEEDED(loadResult) && *outData ) {
    // we have a file path in |data|. Is it an internet shortcut or a normal file?
    const nsDependentString filepath(static_cast<PRUnichar*>(*outData));
    nsCOMPtr<nsIFile> file;
    nsresult rv = NS_NewLocalFile(filepath, true, getter_AddRefs(file));
    if (NS_FAILED(rv)) {
      nsMemory::Free(*outData);
      return dataFound;
    }

    if ( IsInternetShortcut(filepath) ) {
      nsMemory::Free(*outData);
      nsCAutoString url;
      ResolveShortcut( file, url );
      if ( !url.IsEmpty() ) {
        // convert it to unicode and pass it out
        nsDependentString urlString(UTF8ToNewUnicode(url));
        // the internal mozilla URL format, text/x-moz-url, contains
        // URL\ntitle.  We can guess the title from the file's name.
        nsAutoString title;
        file->GetLeafName(title);
        // We rely on IsInternetShortcut check that file has a .url extension.
        title.SetLength(title.Length() - 4);
        if (title.IsEmpty())
          title = urlString;
        *outData = ToNewUnicode(urlString + NS_LITERAL_STRING("\n") + title);
        *outDataLen = NS_strlen(static_cast<PRUnichar*>(*outData)) * sizeof(PRUnichar);

        dataFound = true;
      }
    }
    else {
      // we have a normal file, use some Necko objects to get our file path
      nsCAutoString urlSpec;
      NS_GetURLSpecFromFile(file, urlSpec);

      // convert it to unicode and pass it out
      nsMemory::Free(*outData);
      *outData = UTF8ToNewUnicode(urlSpec);
      *outDataLen = NS_strlen(static_cast<PRUnichar*>(*outData)) * sizeof(PRUnichar);
      dataFound = true;
    } // else regular file
  }

  return dataFound;
} // FindURLFromLocalFile

//
// FindURLFromNativeURL
//
// we are looking for a URL and couldn't find it using our internal
// URL flavor, so look for it using the native URL flavor,
// CF_INETURLSTRW (We don't handle CF_INETURLSTRA currently)
//
bool
nsClipboard :: FindURLFromNativeURL ( IDataObject* inDataObject, UINT inIndex, void** outData, PRUint32* outDataLen )
{
  bool dataFound = false;

  void* tempOutData = nsnull;
  PRUint32 tempDataLen = 0;

  nsresult loadResult = GetNativeDataOffClipboard(inDataObject, inIndex, ::RegisterClipboardFormat(CFSTR_INETURLW), nsnull, &tempOutData, &tempDataLen);
  if ( NS_SUCCEEDED(loadResult) && tempOutData ) {
    nsDependentString urlString(static_cast<PRUnichar*>(tempOutData));
    // the internal mozilla URL format, text/x-moz-url, contains
    // URL\ntitle.  Since we don't actually have a title here,
    // just repeat the URL to fake it.
    *outData = ToNewUnicode(urlString + NS_LITERAL_STRING("\n") + urlString);
    *outDataLen = NS_strlen(static_cast<PRUnichar*>(*outData)) * sizeof(PRUnichar);
    nsMemory::Free(tempOutData);
    dataFound = true;
  }
  else {
    loadResult = GetNativeDataOffClipboard(inDataObject, inIndex, ::RegisterClipboardFormat(CFSTR_INETURLA), nsnull, &tempOutData, &tempDataLen);
    if ( NS_SUCCEEDED(loadResult) && tempOutData ) {
      // CFSTR_INETURLA is (currently) equal to CFSTR_SHELLURL which is equal to CF_TEXT
      // which is by definition ANSI encoded.
      nsCString urlUnescapedA;
      bool unescaped = NS_UnescapeURL(static_cast<char*>(tempOutData), tempDataLen, esc_OnlyNonASCII | esc_SkipControl, urlUnescapedA);

      nsString urlString;
      if (unescaped)
        NS_CopyNativeToUnicode(urlUnescapedA, urlString);
      else
        NS_CopyNativeToUnicode(nsDependentCString(static_cast<char*>(tempOutData), tempDataLen), urlString);

      // the internal mozilla URL format, text/x-moz-url, contains
      // URL\ntitle.  Since we don't actually have a title here,
      // just repeat the URL to fake it.
      *outData = ToNewUnicode(urlString + NS_LITERAL_STRING("\n") + urlString);
      *outDataLen = NS_strlen(static_cast<PRUnichar*>(*outData)) * sizeof(PRUnichar);
      nsMemory::Free(tempOutData);
      dataFound = true;
    }
  }

  return dataFound;
} // FindURLFromNativeURL

//
// ResolveShortcut
//
void
nsClipboard :: ResolveShortcut ( nsIFile* aFile, nsACString& outURL )
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
bool
nsClipboard :: IsInternetShortcut ( const nsAString& inFileName ) 
{
  return StringEndsWith(inFileName, NS_LITERAL_STRING(".url"), nsCaseInsensitiveStringComparator());
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
NS_IMETHODIMP nsClipboard::HasDataMatchingFlavors(const char** aFlavorList,
                                                  PRUint32 aLength,
                                                  PRInt32 aWhichClipboard,
                                                  bool *_retval)
{
  *_retval = false;
  if (aWhichClipboard != kGlobalClipboard || !aFlavorList)
    return NS_OK;

  for (PRUint32 i = 0;i < aLength; ++i) {
#ifdef DEBUG
    if (strcmp(aFlavorList[i], kTextMime) == 0)
      NS_WARNING ( "DO NOT USE THE text/plain DATA FLAVOR ANY MORE. USE text/unicode INSTEAD" );
#endif

    UINT format = GetFormat(aFlavorList[i]);
    if (IsClipboardFormatAvailable(format)) {
      *_retval = true;
      break;
    }
    else {
      // We haven't found the exact flavor the client asked for, but maybe we can
      // still find it from something else that's on the clipboard...
      if (strcmp(aFlavorList[i], kUnicodeMime) == 0) {
        // client asked for unicode and it wasn't present, check if we have CF_TEXT.
        // We'll handle the actual data substitution in the data object.
        if (IsClipboardFormatAvailable(GetFormat(kTextMime)))
          *_retval = true;
      }
    }
  }

  return NS_OK;
}
