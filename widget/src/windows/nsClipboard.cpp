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

#include "DDCOMM.h"

// interface definitions
static NS_DEFINE_IID(kIDataFlavorIID,    NS_IDATAFLAVOR_IID);

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
  mTransferable = nsnull;
  mDataObj  = nsnull;
}

//-------------------------------------------------------------------------
//
// nsClipboard destructor
//
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
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
NS_IMETHODIMP nsClipboard::SetTransferable(nsITransferable * aTransferable, nsIClipboardOwner * anOwner)
{
  if (nsnull != mClipboardOwner) {
    mClipboardOwner->LosingOwnership(mTransferable);
    NS_RELEASE(mClipboardOwner);
  }

  mClipboardOwner = anOwner;
  if (nsnull != anOwner) {
    NS_ADDREF(mClipboardOwner);
  }

  if (nsnull != mTransferable) {
    NS_RELEASE(mTransferable);
  }

  mTransferable = aTransferable;
  if (nsnull == mTransferable) {
    return NS_OK;
  }

  NS_ADDREF(mTransferable);

  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsClipboard::SetClipboard()
{
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

  return NS_OK;
}

/**
  * 
  *
  */
static nsresult GetNativeDataOffClipboard(UINT aFormat, void ** aData, PRUint32 * aLen)
{
  HGLOBAL   hglb; 
  LPSTR     lpStr; 
  nsresult  result = NS_ERROR_FAILURE;
  DWORD     dataSize;

  if (::OpenClipboard(NULL)) { // Just Grab TEXT for now, later we will grab HTML, XIF, etc.
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
    }
    ::CloseClipboard();
  }
  return NS_OK;
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
      UINT format;

      void   * data;
      PRUint32 dataLen;

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

      if (NS_OK == GetNativeDataOffClipboard(format, &data, &dataLen)) {
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
  return NS_ERROR_FAILURE;
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
