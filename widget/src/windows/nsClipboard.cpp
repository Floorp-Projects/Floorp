/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsClipboard.h"
#include <windows.h>
#include <OLE2.h>

#include "nsDataObj.h"
#include "nsISupportsArray.h"
#include "nsIClipboardOwner.h"
#include "nsIDataFlavor.h"

#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"

#include "DDCOMM.h"

// interface definitions
static NS_DEFINE_IID(kIDataFlavorIID,    NS_IDATAFLAVOR_IID);

static NS_DEFINE_IID(kIWidgetIID,        NS_IWIDGET_IID);
static NS_DEFINE_IID(kWindowCID,         NS_WINDOW_CID);

NS_IMPL_ADDREF(nsClipboard)
NS_IMPL_RELEASE(nsClipboard)

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard()
{
  NS_INIT_REFCNT();
  mClipboardOwner = nsnull;
  mTransferable   = nsnull;
  mDataObj        = nsnull;
  mIgnoreEmptyNotification = PR_FALSE;

  // Create a Native window for the shell container...
  //nsresult rv = nsComponentManager::CreateInstance(kWindowCID, nsnull, kIWidgetIID, (void**)&mWindow);
  //mWindow->Show(PR_FALSE);
  //mWindow->Resize(1,1,PR_FALSE);
}

//-------------------------------------------------------------------------
//
// nsClipboard destructor
//
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
  NS_IF_RELEASE(mWindow);

  EmptyClipboard();
  if (nsnull != mDataObj) {
    mDataObj->Release();
  }

}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsClipboard::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  static NS_DEFINE_IID(kIClipboard, NS_ICLIPBOARD_IID);
  if (aIID.Equals(kIClipboard)) {
    *aInstancePtr = (void*) ((nsIClipboard*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


/**
  * Sets the transferable object
  *
  */
NS_IMETHODIMP nsClipboard::SetData(nsITransferable * aTransferable, nsIClipboardOwner * anOwner)
{
  if (aTransferable == mTransferable && anOwner == mClipboardOwner) {
    return NS_OK;
  }

  EmptyClipboard();

  mClipboardOwner = anOwner;
  if (nsnull != anOwner) {
    NS_ADDREF(mClipboardOwner);
  }

  mTransferable = aTransferable;
  if (nsnull != mTransferable) {
    NS_ADDREF(mTransferable);
  }

  return NS_OK;
}

/**
  * Gets the transferable object
  *
  */
NS_IMETHODIMP nsClipboard::GetData(nsITransferable ** aTransferable)
{
  *aTransferable = mTransferable;
  if (nsnull != mTransferable) {
    NS_ADDREF(mTransferable);
  }

  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsClipboard::SetClipboard()
{
  mIgnoreEmptyNotification = PR_TRUE;

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  // Clear the native clipboard
  ::OleFlushClipboard();

  // Get the transferable list of data flavors
  nsISupportsArray * dfList;
  mTransferable->GetTransferDataFlavors(&dfList);

  // Release the existing DataObject
  if (mDataObj) {
    mDataObj->Release();
  }

  // Create our native DataObject that implements 
  // the OLE IDataObject interface
  mDataObj = new nsDataObj();
  mDataObj->AddRef();

  // Now give the Transferable to the DataObject 
  // for getting the data out of it
  mDataObj->SetTransferable(mTransferable);

  // Walk through flavors and register them on the native clipboard,
  PRUint32 i;
  for (i=0;i<dfList->Count();i++) {
    nsIDataFlavor * df;
    nsISupports * supports = dfList->ElementAt(i);
    if (NS_OK == supports->QueryInterface(kIDataFlavorIID, (void **)&df)) {
      nsString mime;
      df->GetMimeType(mime);
      UINT format;

      if (mime.Equals(kTextMime)) {
        format = CF_TEXT;
      } else if (mime.Equals(kUnicodeMime)) {
        format = CF_UNICODETEXT;
      } else if (mime.Equals(kImageMime)) {
        format = CF_BITMAP;
      } else {
        char * str = mime.ToNewCString();
        format = ::RegisterClipboardFormat(str);
        delete[] str;
      }
      FORMATETC fe;
      SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, 0, TYMED_HGLOBAL);

      // Now tell the native IDataObject about both the DataFlavor and 
      // the native data format
      mDataObj->AddDataFlavor(df, &fe);
      NS_RELEASE(df);
    }
    NS_RELEASE(supports);
  }

  // cast our native DataObject to its IDataObject pointer
  // and put it on the clipboard
  IDataObject * ido = (IDataObject *)mDataObj;
  ::OleSetClipboard(ido);

  mIgnoreEmptyNotification = PR_FALSE;

  return NS_OK;
}

/**
  * 
  *
  */
static nsresult GetNativeDataOffClipboard(nsIWidget * aWindow, UINT aFormat, void ** aData, PRUint32 * aLen)
{
  HGLOBAL   hglb; 
  LPSTR     lpStr; 
  nsresult  result = NS_ERROR_FAILURE;
  DWORD     dataSize;

  HWND nativeWin = nsnull;//(HWND)aWindow->GetNativeData(NS_NATIVE_WINDOW);
  if (::OpenClipboard(nativeWin)) { 
    hglb = ::GetClipboardData(aFormat); 
    if (hglb != NULL) {
      lpStr       = (LPSTR)::GlobalLock(hglb);
      dataSize    = ::GlobalSize(hglb);
      *aLen       = dataSize;
      char * data = new char[dataSize];

      char*    ptr  = data;
      LPSTR    pMem = lpStr;
      PRUint32 inx;
      for (inx=0; inx < dataSize; inx++) {
	      *ptr++ = *pMem++;
      }
      ::GlobalUnlock(hglb);

      *aData = data;
      result = NS_OK;
    } else {
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
    ::CloseClipboard();
  }
  return NS_OK;
}

/**
  * 
  *
  */
static UINT GetFormat(const nsString & aMimeStr)
{
  UINT format = 0;

  if (aMimeStr.Equals(kTextMime)) {
    format = CF_TEXT;
  } else if (aMimeStr.Equals(kUnicodeMime)) {
    format = CF_UNICODETEXT;
  } else if (aMimeStr.Equals(kImageMime)) {
    format = CF_BITMAP;
  } else {
    char * str = aMimeStr.ToNewCString();
    format = ::RegisterClipboardFormat(str);
    delete[] str;
  }

  return format;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsClipboard::GetClipboard()
{
  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  // Get the transferable list of data flavors
  nsISupportsArray * dfList;
  mTransferable->GetTransferDataFlavors(&dfList);

  // Walk through flavors and see which flavor is on the clipboard them on the native clipboard,
  PRUint32 i;
  for (i=0;i<dfList->Count();i++) {
    nsIDataFlavor * df;
    nsISupports * supports = dfList->ElementAt(i);
    if (NS_OK == supports->QueryInterface(kIDataFlavorIID, (void **)&df)) {
      nsString mime;
      df->GetMimeType(mime);
      UINT format = GetFormat(mime);

      void   * data;
      PRUint32 dataLen;

      if (NS_OK == GetNativeDataOffClipboard(mWindow, format, &data, &dataLen)) {
        mTransferable->SetTransferData(df, data, dataLen);
        //NS_RELEASE(df);
        //NS_RELEASE(supports);
        //return NS_OK;
      }
      NS_RELEASE(df);
    }
    NS_RELEASE(supports);
  }

  return NS_OK;
}

/**
  * This enumarates the native clipboard checking to see if the data flavor is on it
  *
  */
NS_IMETHODIMP nsClipboard::IsDataFlavorSupported(nsIDataFlavor * aDataFlavor)
{
  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  return mTransferable->IsDataFlavorSupported(aDataFlavor);
}

/**
  * 
  *
  */
NS_IMETHODIMP nsClipboard::EmptyClipboard()
{
  if (mIgnoreEmptyNotification) {
    return NS_OK;
  }

  if (nsnull != mClipboardOwner) {
    mClipboardOwner->LosingOwnership(mTransferable);
    NS_RELEASE(mClipboardOwner);
  }

  if (nsnull != mTransferable) {
    NS_RELEASE(mTransferable);
  }

  return NS_OK;
}

/**
  * 
  *
  */
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

/**
  * 
  *
  */
NS_IMETHODIMP nsClipboard::ForceDataToClipboard()
{
  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  HWND nativeWin = nsnull;//(HWND)mWindow->GetNativeData(NS_NATIVE_WINDOW);
  ::OpenClipboard(nativeWin);
  ::EmptyClipboard();

  // Get the transferable list of data flavors
  nsISupportsArray * dfList;
  mTransferable->GetTransferDataFlavors(&dfList);

  // Walk through flavors and see which flavor is on the clipboard them on the native clipboard,
  PRUint32 i;
  for (i=0;i<dfList->Count();i++) {
    nsIDataFlavor * df;
    nsISupports * supports = dfList->ElementAt(i);
    if (NS_OK == supports->QueryInterface(kIDataFlavorIID, (void **)&df)) {
      nsString mime;
      df->GetMimeType(mime);

      UINT format = GetFormat(mime);

      void   * data;
      PRUint32 dataLen;

      mTransferable->GetTransferData(df, &data, &dataLen);
      if (nsnull != data) {
        PlaceDataOnClipboard(format, (char *)data, dataLen);
      }
      NS_RELEASE(df);
    }
    NS_RELEASE(supports);
  }

  ::CloseClipboard();

  return NS_OK;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

  	// make the IDataObject
  /*IDataObject * dataObj = NULL;
  
  IClassFactory * pCF    = NULL;

  HRESULT hr = ::CoGetClassObject(CLSID_CfDataObj, CLSCTX_INPROC_SERVER, NULL, IID_IClassFactory, (void **)&pCF); 
	if (REGDB_E_CLASSNOTREG == hr) {
		return hr;
	}
  //hresult = pCF->CreateInstance(pUnkOuter, riid, ppvObj) 
  pCF->Release();

  HRESULT ret = ::CoCreateInstance(CLSID_CfDataObj, NULL, CLSCTX_INPROC_SERVER, IID_IDataObject, (void **)&dataObj);
	if (REGDB_E_CLASSNOTREG == ret) {
		//return ret;
	}
	if (FAILED(ret)) {
		//return ret;
	}*/

#if 0
NS_IMETHODIMP nsTransferable::SetNativeClipboard()
{
  ::OleFlushClipboard();

  if (!mStrCache) {
    return NS_OK;
  }

  char * str = mStrCache->ToNewCString();
#if 0
	
  PlaceStringOnClipboard(CF_TEXT, str, mStrCache->Length());
			
  ::CloseClipboard();
#else 

  if (mDataObj) {
    mDataObj->Release();
  }

  	// make the IDataObject
  /*IDataObject * dataObj = NULL;
  
  IClassFactory * pCF    = NULL;

  HRESULT hr = ::CoGetClassObject(CLSID_CfDataObj, CLSCTX_INPROC_SERVER, NULL, IID_IClassFactory, (void **)&pCF); 
	if (REGDB_E_CLASSNOTREG == hr) {
		return hr;
	}
  //hresult = pCF->CreateInstance(pUnkOuter, riid, ppvObj) 
  pCF->Release();

  HRESULT ret = ::CoCreateInstance(CLSID_CfDataObj, NULL, CLSCTX_INPROC_SERVER, IID_IDataObject, (void **)&dataObj);
	if (REGDB_E_CLASSNOTREG == ret) {
		//return ret;
	}
	if (FAILED(ret)) {
		//return ret;
	}*/

  mDataObj = new nsDataObj();
  mDataObj->AddRef();

  mDataObj->SetText(*mStrCache);

  PRUint32 i;
  for (i=0;i<mDFList->Count();i++) {
    nsIDataFlavor * df;
    nsISupports * supports = mDFList->ElementAt(i);
    if (NS_OK == supports->QueryInterface(kIDataFlavorIID, (void **)&df)) {
      nsString mime;
      df->GetMimeType(mime);
      UINT format;
      if (mime.Equals(kTextMime)) {
        format = CF_TEXT;
      } else {
        char * str = mime.ToNewCString();
        UINT format = ::RegisterClipboardFormat(str);
        delete[] str;
      }
      FORMATETC fe;
      SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, 0, TYMED_HGLOBAL);
      mDataObj->AddDataFlavor(df, &fe);
      NS_RELEASE(df);
    }
    NS_RELEASE(supports);
  }


  IDataObject * ido = (IDataObject *)mDataObj;
  ::OleSetClipboard(ido);

#endif

  delete[] str;
  return NS_OK;
}
#endif
