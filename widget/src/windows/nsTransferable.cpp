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

#include "nsTransferable.h"
#include "nsIDataFlavor.h"
#include "nsDataFlavor.h"
#include "nsWidgetsCID.h"
#include "nsISupportsArray.h"
#include "nsRepository.h"

#include "nsDataObj.h"

#include "DDCOMM.h"

static NS_DEFINE_IID(kITransferableIID,  NS_ITRANSFERABLE_IID);
static NS_DEFINE_IID(kIDataFlavorIID,    NS_IDATAFLAVOR_IID);
static NS_DEFINE_IID(kCDataFlavorCID,    NS_DATAFLAVOR_CID);

NS_IMPL_ADDREF(nsTransferable)
NS_IMPL_RELEASE(nsTransferable)

//-------------------------------------------------------------------------
//
// Transferable constructor
//
//-------------------------------------------------------------------------
nsTransferable::nsTransferable()
{
  NS_INIT_REFCNT();
  mDataObj  = nsnull;
  mStrCache = nsnull;
  mDataPtr  = nsnull;

  nsresult rv = NS_NewISupportsArray(&mDFList);

}

//-------------------------------------------------------------------------
//
// Transferable destructor
//
//-------------------------------------------------------------------------
nsTransferable::~nsTransferable()
{
  if (mStrCache) {
    delete mStrCache;
  }
  if (mDataPtr) {
    delete mDataPtr;
  }
  
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsTransferable::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  if (aIID.Equals(kITransferableIID)) {
    *aInstancePtr = (void*) ((nsITransferable*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::GetTransferDataFlavors(nsISupportsArray ** aDataFlavorList)
{
  *aDataFlavorList = mDFList;
  NS_ADDREF(mDFList);
  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::IsDataFlavorSupported(nsIDataFlavor * aDataFlavor)
{
  nsAutoString  mimeInQuestion;
  aDataFlavor->GetMimeType(mimeInQuestion);

  PRUint32 i;
  for (i=0;i<mDFList->Count();i++) {
    nsIDataFlavor * df;
    nsISupports * supports = mDFList->ElementAt(i);
    if (NS_OK == supports->QueryInterface(kIDataFlavorIID, (void **)&df)) {
      nsAutoString mime;
      df->GetMimeType(mime);
      if (mimeInQuestion.Equals(mime)) {
        return NS_OK;
      }
      NS_RELEASE(df);
    }
    NS_RELEASE(supports);
  }
  return NS_ERROR_FAILURE;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::GetTransferData(nsIDataFlavor * aDataFlavor, void ** aData, PRUint32 * aDataLen)
{
  if (mDataPtr) {
    delete mDataPtr;
  }

  nsAutoString  mimeInQuestion;
  aDataFlavor->GetMimeType(mimeInQuestion);

  if (mimeInQuestion.Equals(kTextMime)) {
    char * str = mStrCache->ToNewCString();
    *aDataLen = mStrCache->Length()+1;

    mDataPtr = (void *)str;
    *aData = mDataPtr; 
    //mDataPtr = (void *)new char[*aDataLen];
    //memcpy(*mDataPtr, str, *aDataLen);
    //delete[] str;

  } else if (mimeInQuestion.Equals(kHTMLMime)) {

  }

  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::SetTransferData(nsIDataFlavor * aDataFlavor, void * aData, PRUint32 aDataLen)
{
  nsAutoString  mimeInQuestion;
  aDataFlavor->GetMimeType(mimeInQuestion);

  if (mimeInQuestion.Equals(kTextMime)) {
    if (nsnull == mStrCache) {
      mStrCache = new nsString();
    }
    mStrCache->SetString((char *)aData, aDataLen-1);
  } else if (mimeInQuestion.Equals(kHTMLMime)) {

  }

  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::SetTransferString(const nsString & aStr)
{
  if (!mStrCache) {
    mStrCache = new nsString(aStr);
  } else {
    *mStrCache = aStr;
  }

  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::GetTransferString(nsString & aStr)
{
  if (!mStrCache) {
    mStrCache = new nsString();
  }
  aStr = *mStrCache;

  return NS_OK;
}

#if 0
static void PlaceStringOnClipboard(PRUint32 aFormat, char* aData, int aLength)
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
#endif

/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::SetNativeClipboard()
{
#if 0
  ::OleFlushClipboard();

  if (!mStrCache) {
    return NS_OK;
  }

  char * str = mStrCache->ToNewCString();
#if 0
  ::OpenClipboard(NULL);
  ::EmptyClipboard();
	
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
#endif
  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::AddDataFlavor(const nsString & aMimeType, const nsString & aHumanPresentableName)
{
  nsIDataFlavor * df;
  nsresult rv = nsComponentManager::CreateInstance(kCDataFlavorCID, nsnull, kIDataFlavorIID, (void**) &df);
  if (nsnull == df) {
    return rv;
  }

  df->Init(aMimeType, aHumanPresentableName);
  mDFList->AppendElement(df);
  
  /*FORMATETC fe;
  if (aMimeType.Equals(kTextMime)) {
    SET_FORMATETC(fe, CF_TEXT, 0, DVASPECT_CONTENT, 0, TYMED_HGLOBAL);
    m_getFormats[m_numGetFormats++] = fe;

    SET_FORMATETC(fe, CF_TEXT, 0, DVASPECT_CONTENT, 0, TYMED_HGLOBAL);
    m_setFormats[m_numSetFormats++] = fe;
  } else {
    nsDataFlavor * ndf = (nsDataFlavor *)df;
    SET_FORMATETC(fe, ndf->GetFormat(), 0, DVASPECT_CONTENT, 0, TYMED_HGLOBAL);
    m_getFormats[m_numGetFormats++] = fe;

    SET_FORMATETC(fe, ndf->GetFormat(), 0, DVASPECT_CONTENT, 0, TYMED_HGLOBAL);
    m_setFormats[m_numSetFormats++] = fe;
  }*/

  return rv;
}

