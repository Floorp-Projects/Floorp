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

#include "nsDataObjCollection.h"
//#include "nsString.h"
#include "nsVoidArray.h"
#include "nsITransferable.h"
#include "nsClipboard.h"
#include "IENUMFE.h"

#include "OLE2.h"
#include "URLMON.h"

#if 1
#define PRNTDEBUG(_x) printf(_x);
#define PRNTDEBUG2(_x1, _x2) printf(_x1, _x2);
#define PRNTDEBUG3(_x1, _x2, _x3) printf(_x1, _x2, _x3);
#else
#define PRNTDEBUG(_x) // printf(_x);
#define PRNTDEBUG2(_x1, _x2) // printf(_x1, _x2);
#define PRNTDEBUG3(_x1, _x2, _x3) // printf(_x1, _x2, _x3);
#endif

ULONG nsDataObjCollection::g_cRef = 0;

EXTERN_C GUID CDECL CLSID_nsDataObjCollection =
{ 0x2d851b91, 0xd4c, 0x11d3, { 0x96, 0xd4, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } };


/*
 * Class nsDataObjCollection
 */

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsDataObjCollection::nsDataObjCollection()
{
	m_cRef	        = 0;
  mTransferable   = nsnull;
  mDataFlavors    = new nsVoidArray();
  mDataObjects    = new nsVoidArray();

  m_enumFE = new CEnumFormatEtc(32);
  m_enumFE->AddRef();

}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsDataObjCollection::~nsDataObjCollection()
{
  NS_IF_RELEASE(mTransferable);
  PRInt32 i;
  for (i=0;i<mDataFlavors->Count();i++) {
    nsString * df = (nsString *)mDataFlavors->ElementAt(i);
    delete df;
  }
  delete mDataFlavors;

  for (i=0;i<mDataObjects->Count();i++) {
    IDataObject * dataObj = (IDataObject *)mDataObjects->ElementAt(i);
    NS_RELEASE(dataObj);
  }
  delete mDataObjects;

	m_cRef = 0;
  m_enumFE->Release();

}


//-----------------------------------------------------
// IUnknown interface methods - see inknown.h for documentation
//-----------------------------------------------------
STDMETHODIMP nsDataObjCollection::QueryInterface(REFIID riid, void** ppv)
{
	*ppv=NULL;

	if ( (IID_IUnknown == riid) || (IID_IDataObject	== riid) ) {
		*ppv = this;
		AddRef();
		return NOERROR;
	}

	return ResultFromScode(E_NOINTERFACE);
}

//-----------------------------------------------------
STDMETHODIMP_(ULONG) nsDataObjCollection::AddRef()
{
	++g_cRef;
  //PRNTDEBUG3("nsDataObjCollection::AddRef  >>>>>>>>>>>>>>>>>> %d on %p\n", (m_cRef+1), this);
	return ++m_cRef;
}


//-----------------------------------------------------
STDMETHODIMP_(ULONG) nsDataObjCollection::Release()
{
  //PRNTDEBUG3("nsDataObjCollection::Release >>>>>>>>>>>>>>>>>> %d on %p\n", (m_cRef-1), this);
	if (0 < g_cRef)
		--g_cRef;

	if (0 != --m_cRef)
		return m_cRef;

	delete this;

	return 0;
}

//-----------------------------------------------------
BOOL nsDataObjCollection::FormatsMatch(const FORMATETC& source, const FORMATETC& target) const
{
	if ((source.cfFormat == target.cfFormat) &&
		 (source.dwAspect  & target.dwAspect)  &&
		 (source.tymed     & target.tymed))       {
		return TRUE;
	} else {
		return FALSE;
	}
}

//-----------------------------------------------------
// IDataObject methods
//-----------------------------------------------------
STDMETHODIMP nsDataObjCollection::GetData(LPFORMATETC pFE, LPSTGMEDIUM pSTM)
{
  printf("nsDataObjCollection::GetData2\n");
  PRNTDEBUG("nsDataObjCollection::GetData\n");
  PRNTDEBUG3("  format: %d  Text: %d\n", pFE->cfFormat, CF_TEXT);

  for (PRInt32 i=0;i<mDataObjects->Count();i++) {
    IDataObject * dataObj = (IDataObject *)mDataObjects->ElementAt(i);
    if (S_OK == dataObj->GetData(pFE, pSTM)) {
      return S_OK;
    }
  }

	return ResultFromScode(DATA_E_FORMATETC);
}


//-----------------------------------------------------
STDMETHODIMP nsDataObjCollection::GetDataHere(LPFORMATETC pFE, LPSTGMEDIUM pSTM)
{
  PRNTDEBUG("nsDataObjCollection::GetDataHere\n");
		return ResultFromScode(E_FAIL);
}


//-----------------------------------------------------
// Other objects querying to see if we support a 
// particular format
//-----------------------------------------------------
STDMETHODIMP nsDataObjCollection::QueryGetData(LPFORMATETC pFE)
{
  UINT format = nsClipboard::GetFormat(MULTI_MIME);
  PRNTDEBUG("nsDataObjCollection::QueryGetData  ");

  PRNTDEBUG3("format: %d  Mulitple: %d\n", pFE->cfFormat, format);

  if (format == pFE->cfFormat) {
    return S_OK;
  }


  for (PRInt32 i=0;i<mDataObjects->Count();i++) {
    IDataObject * dataObj = (IDataObject *)mDataObjects->ElementAt(i);
    if (S_OK == dataObj->QueryGetData(pFE)) {
      return S_OK;
    }
  }

  PRNTDEBUG2("***** nsDataObjCollection::QueryGetData - Unknown format %d\n", pFE->cfFormat);
	return ResultFromScode(E_FAIL);
}

//-----------------------------------------------------
STDMETHODIMP nsDataObjCollection::GetCanonicalFormatEtc
	 (LPFORMATETC pFEIn, LPFORMATETC pFEOut)
{
  PRNTDEBUG("nsDataObjCollection::GetCanonicalFormatEtc\n");
		return ResultFromScode(E_FAIL);
}


//-----------------------------------------------------
STDMETHODIMP nsDataObjCollection::SetData(LPFORMATETC pFE, LPSTGMEDIUM pSTM, BOOL fRelease)
{
  PRNTDEBUG("nsDataObjCollection::SetData\n");

  return ResultFromScode(E_FAIL);
}


//-----------------------------------------------------
STDMETHODIMP nsDataObjCollection::EnumFormatEtc(DWORD dwDir, LPENUMFORMATETC *ppEnum)
{
  PRNTDEBUG("nsDataObjCollection::EnumFormatEtc\n");

  switch (dwDir) {
    case DATADIR_GET: {
       m_enumFE->Clone(ppEnum);
    } break;
    case DATADIR_SET:
        *ppEnum=NULL;
        break;
    default:
        *ppEnum=NULL;
        break;
  } // switch

  // Since a new one has been created, 
  // we will ref count the new clone here 
  // before giving it back
  if (NULL == *ppEnum)
    return ResultFromScode(E_FAIL);
  else
    (*ppEnum)->AddRef();

  return NOERROR;

}

//-----------------------------------------------------
STDMETHODIMP nsDataObjCollection::DAdvise(LPFORMATETC pFE, DWORD dwFlags,
										                      LPADVISESINK pIAdviseSink, DWORD* pdwConn)
{
  PRNTDEBUG("nsDataObjCollection::DAdvise\n");
	return ResultFromScode(E_FAIL);
}


//-----------------------------------------------------
STDMETHODIMP nsDataObjCollection::DUnadvise(DWORD dwConn)
{
  PRNTDEBUG("nsDataObjCollection::DUnadvise\n");
	return ResultFromScode(E_FAIL);
}

//-----------------------------------------------------
STDMETHODIMP nsDataObjCollection::EnumDAdvise(LPENUMSTATDATA *ppEnum)
{
  PRNTDEBUG("nsDataObjCollection::EnumDAdvise\n");
	return ResultFromScode(E_FAIL);
}

//-----------------------------------------------------
// other methods
//-----------------------------------------------------
ULONG nsDataObjCollection::GetCumRefCount()
{
	return g_cRef;
}

//-----------------------------------------------------
ULONG nsDataObjCollection::GetRefCount() const
{
	return m_cRef;
}

//-----------------------------------------------------
// GetData and SetData helper functions
//-----------------------------------------------------
HRESULT nsDataObjCollection::AddSetFormat(FORMATETC& aFE)
{
  PRNTDEBUG("nsDataObjCollection::AddSetFormat\n");
	return ResultFromScode(S_OK);
}

//-----------------------------------------------------
HRESULT nsDataObjCollection::AddGetFormat(FORMATETC& aFE)
{
  PRNTDEBUG("nsDataObjCollection::AddGetFormat\n");
	return ResultFromScode(S_OK);
}

//-----------------------------------------------------
HRESULT nsDataObjCollection::GetBitmap(FORMATETC&, STGMEDIUM&)
{
  PRNTDEBUG("nsDataObjCollection::GetBitmap\n");
	return ResultFromScode(E_NOTIMPL);
}

//-----------------------------------------------------
HRESULT nsDataObjCollection::GetDib(FORMATETC&, STGMEDIUM&)
{
  PRNTDEBUG("nsDataObjCollection::GetDib\n");
	return ResultFromScode(E_NOTIMPL);
}

//-----------------------------------------------------
HRESULT nsDataObjCollection::GetMetafilePict(FORMATETC&, STGMEDIUM&)
{
	return ResultFromScode(E_NOTIMPL);
}

//-----------------------------------------------------
HRESULT nsDataObjCollection::SetBitmap(FORMATETC&, STGMEDIUM&)
{
	return ResultFromScode(E_NOTIMPL);
}

//-----------------------------------------------------
HRESULT nsDataObjCollection::SetDib   (FORMATETC&, STGMEDIUM&)
{
	return ResultFromScode(E_FAIL);
}

//-----------------------------------------------------
HRESULT nsDataObjCollection::SetMetafilePict (FORMATETC&, STGMEDIUM&)
{
	return ResultFromScode(E_FAIL);
}

//-----------------------------------------------------
//-----------------------------------------------------
CLSID nsDataObjCollection::GetClassID() const
{
	return CLSID_nsDataObjCollection;
}

//-----------------------------------------------------
// Registers a the DataFlavor/FE pair
//-----------------------------------------------------
void nsDataObjCollection::AddDataFlavor(nsString * aDataFlavor, LPFORMATETC aFE)
{
  // These two lists are the mapping to and from data flavors and FEs
  // Later, OLE will tell us it's needs a certain type of FORMATETC (text, unicode, etc)
  // so we will look up data flavor that corresponds to the FE
  // and then ask the transferable for that type of data
  mDataFlavors->AppendElement(new nsString(*aDataFlavor));
  m_enumFE->AddFE(aFE);

}

//-----------------------------------------------------
// Registers a the DataFlavor/FE pair
//-----------------------------------------------------
void nsDataObjCollection::AddDataObject(IDataObject * aDataObj)
{
  NS_ADDREF(aDataObj);
  mDataObjects->AppendElement(aDataObj);

}
