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

#include "nsDataObj.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsITransferable.h"
#include "nsISupportsPrimitives.h"
#include "IENUMFE.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"

#include "OLE2.h"
#include "URLMON.h"

#if 0
#define PRNTDEBUG(_x) printf(_x);
#define PRNTDEBUG2(_x1, _x2) printf(_x1, _x2);
#define PRNTDEBUG3(_x1, _x2, _x3) printf(_x1, _x2, _x3);
#else
#define PRNTDEBUG(_x) // printf(_x);
#define PRNTDEBUG2(_x1, _x2) // printf(_x1, _x2);
#define PRNTDEBUG3(_x1, _x2, _x3) // printf(_x1, _x2, _x3);
#endif

ULONG nsDataObj::g_cRef = 0;

EXTERN_C GUID CDECL CLSID_nsDataObj =
	{ 0x1bba7640, 0xdf52, 0x11cf, { 0x82, 0x7b, 0, 0xa0, 0x24, 0x3a, 0xe5, 0x05 } };


// I don't like having to define these here, but all this should be going away for a generic
// mechanism at some point in the future.
void CreateDataFromPrimitive ( const char* aFlavor, nsISupports* aPrimitive, void** aDataBuff, PRUint32 aDataLen ) ;
void CreatePrimitiveForData ( const char* aFlavor, void* aDataBuff, PRUint32 aDataLen, nsISupports** aPrimitive ) ;


/*
 * Class nsDataObj
 */

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsDataObj::nsDataObj()
{
	m_cRef	        = 0;
  mTransferable   = nsnull;
  mDataFlavors    = new nsVoidArray();

  m_enumFE = new CEnumFormatEtc(32);
  m_enumFE->AddRef();

}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsDataObj::~nsDataObj()
{
  NS_IF_RELEASE(mTransferable);
  PRInt32 i;
  for (i=0;i<mDataFlavors->Count();i++) {
    nsString * df = (nsString *)mDataFlavors->ElementAt(i);
    delete df;
  }

  delete mDataFlavors;

	m_cRef = 0;
  m_enumFE->Release();

}


//-----------------------------------------------------
// IUnknown interface methods - see inknown.h for documentation
//-----------------------------------------------------
STDMETHODIMP nsDataObj::QueryInterface(REFIID riid, void** ppv)
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
STDMETHODIMP_(ULONG) nsDataObj::AddRef()
{
	++g_cRef;
  //PRNTDEBUG3("nsDataObj::AddRef  >>>>>>>>>>>>>>>>>> %d on %p\n", (m_cRef+1), this);
	return ++m_cRef;
}


//-----------------------------------------------------
STDMETHODIMP_(ULONG) nsDataObj::Release()
{
  //PRNTDEBUG3("nsDataObj::Release >>>>>>>>>>>>>>>>>> %d on %p\n", (m_cRef-1), this);
	if (0 < g_cRef)
		--g_cRef;

	if (0 != --m_cRef)
		return m_cRef;

	delete this;

	return 0;
}

//-----------------------------------------------------
BOOL nsDataObj::FormatsMatch(const FORMATETC& source, const FORMATETC& target) const
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
STDMETHODIMP nsDataObj::GetData(LPFORMATETC pFE, LPSTGMEDIUM pSTM)
{
  printf("nsDataObj::GetData2\n");
  PRNTDEBUG("nsDataObj::GetData\n");
  PRNTDEBUG3("  format: %d  Text: %d\n", pFE->cfFormat, CF_TEXT);
  if (nsnull == mTransferable) {
	  return ResultFromScode(DATA_E_FORMATETC);
  }

  PRUint32 dfInx = 0;

  ULONG count;
  FORMATETC fe;
  m_enumFE->Reset();
  while (NOERROR == m_enumFE->Next(1, &fe, &count)) {
    nsString * df = (nsString *)mDataFlavors->ElementAt(dfInx);
    if (nsnull != df) {
		  if (FormatsMatch(fe, *pFE)) {
			  pSTM->pUnkForRelease = NULL;
			  CLIPFORMAT format = pFE->cfFormat;
			  switch(format) {
				  case CF_TEXT:
					  return GetText(df, *pFE, *pSTM);
				  //case CF_BITMAP:
				  //	return GetBitmap(*pFE, *pSTM);
				  //case CF_DIB:
				  //	return GetDib(*pFE, *pSTM);
				  //case CF_METAFILEPICT:
				  //	return GetMetafilePict(*pFE, *pSTM);
				  default:
            PRNTDEBUG2("***** nsDataObj::GetData - Unknown format %x\n", format);
					  return GetText(df, *pFE, *pSTM);
            break;
        } //switch
      } // if
    }
    dfInx++;
  } // while

	return ResultFromScode(DATA_E_FORMATETC);
}


//-----------------------------------------------------
STDMETHODIMP nsDataObj::GetDataHere(LPFORMATETC pFE, LPSTGMEDIUM pSTM)
{
  PRNTDEBUG("nsDataObj::GetDataHere\n");
		return ResultFromScode(E_FAIL);
}


//-----------------------------------------------------
// Other objects querying to see if we support a 
// particular format
//-----------------------------------------------------
STDMETHODIMP nsDataObj::QueryGetData(LPFORMATETC pFE)
{
  PRNTDEBUG("nsDataObj::QueryGetData  ");
  PRNTDEBUG3("format: %d  Text: %d\n", pFE->cfFormat, CF_TEXT);

  PRUint32 dfInx = 0;

  ULONG count;
  FORMATETC fe;
  m_enumFE->Reset();
  while (NOERROR == m_enumFE->Next(1, &fe, &count)) {
    if (fe.cfFormat == pFE->cfFormat) {
      return S_OK;
    }
  }

  PRNTDEBUG2("***** nsDataObj::QueryGetData - Unknown format %d\n", pFE->cfFormat);
	return ResultFromScode(E_FAIL);
}

//-----------------------------------------------------
STDMETHODIMP nsDataObj::GetCanonicalFormatEtc
	 (LPFORMATETC pFEIn, LPFORMATETC pFEOut)
{
  PRNTDEBUG("nsDataObj::GetCanonicalFormatEtc\n");
		return ResultFromScode(E_FAIL);
}


//-----------------------------------------------------
STDMETHODIMP nsDataObj::SetData(LPFORMATETC pFE, LPSTGMEDIUM pSTM, BOOL fRelease)
{
  PRNTDEBUG("nsDataObj::SetData\n");

  return ResultFromScode(E_FAIL);
}


//-----------------------------------------------------
STDMETHODIMP nsDataObj::EnumFormatEtc(DWORD dwDir, LPENUMFORMATETC *ppEnum)
{
  PRNTDEBUG("nsDataObj::EnumFormatEtc\n");

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
STDMETHODIMP nsDataObj::DAdvise(LPFORMATETC pFE, DWORD dwFlags,
										            LPADVISESINK pIAdviseSink, DWORD* pdwConn)
{
  PRNTDEBUG("nsDataObj::DAdvise\n");
	return ResultFromScode(E_FAIL);
}


//-----------------------------------------------------
STDMETHODIMP nsDataObj::DUnadvise(DWORD dwConn)
{
  PRNTDEBUG("nsDataObj::DUnadvise\n");
	return ResultFromScode(E_FAIL);
}

//-----------------------------------------------------
STDMETHODIMP nsDataObj::EnumDAdvise(LPENUMSTATDATA *ppEnum)
{
  PRNTDEBUG("nsDataObj::EnumDAdvise\n");
	return ResultFromScode(E_FAIL);
}

//-----------------------------------------------------
// other methods
//-----------------------------------------------------
ULONG nsDataObj::GetCumRefCount()
{
	return g_cRef;
}

//-----------------------------------------------------
ULONG nsDataObj::GetRefCount() const
{
	return m_cRef;
}

//-----------------------------------------------------
// GetData and SetData helper functions
//-----------------------------------------------------
HRESULT nsDataObj::AddSetFormat(FORMATETC& aFE)
{
  PRNTDEBUG("nsDataObj::AddSetFormat\n");
	return ResultFromScode(S_OK);
}

//-----------------------------------------------------
HRESULT nsDataObj::AddGetFormat(FORMATETC& aFE)
{
  PRNTDEBUG("nsDataObj::AddGetFormat\n");
	return ResultFromScode(S_OK);
}

//-----------------------------------------------------
HRESULT nsDataObj::GetBitmap(FORMATETC&, STGMEDIUM&)
{
  PRNTDEBUG("nsDataObj::GetBitmap\n");
	return ResultFromScode(E_NOTIMPL);
}

//-----------------------------------------------------
HRESULT nsDataObj::GetDib(FORMATETC&, STGMEDIUM&)
{
  PRNTDEBUG("nsDataObj::GetDib\n");
	return ResultFromScode(E_NOTIMPL);
}

//-----------------------------------------------------
HRESULT nsDataObj::GetText(nsString * aDF, FORMATETC& aFE, STGMEDIUM& aSTG)
{
  void* data;
  PRUint32   len;
  
  char* flavorStr = aDF->ToNewCString();

  // NOTE: CreateDataFromPrimitive creates new memory, that needs to be deleted
  nsCOMPtr<nsISupports> genericDataWrapper;
  mTransferable->GetTransferData(flavorStr, getter_AddRefs(genericDataWrapper), &len);
  if (0 == len) {
	  return ResultFromScode(E_FAIL);
  }
  CreateDataFromPrimitive ( flavorStr, genericDataWrapper, &data, len );
  delete [] flavorStr;

  HGLOBAL     hGlobalMemory = NULL;
  PSTR        pGlobalMemory = NULL;

  aSTG.tymed          = TYMED_HGLOBAL;
  aSTG.pUnkForRelease = NULL;

  //***************
  // NOTE: On Win98 it allocates to the nearest DWORD boundary 
  // alloc 4 gets you 8
  // alloc 6 gets you 8
  // etc.
  // The GHND will zero fill, external windows apps expect 
  // text string to be zero terminated.
  // 
  // So if we are copying CF_TEXT or CF_UNICODE we need to add
  // the null terminator because the transferable only gives up the text
  // and no terminating zero.
  // So if the length of the text is modulo 8 we need to add extra space
  // for terminating zero

  DWORD allocLen = (DWORD)len;

  if (CF_TEXT        == aFE.cfFormat ||
      CF_UNICODETEXT == aFE.cfFormat) {
    if (len % 8 == 0) {
      allocLen++; // note: this by actually add more than one byte.
    }
  }

  // GHND zeroes the memory
  hGlobalMemory = (HGLOBAL)::GlobalAlloc(GHND, allocLen); 
  //DWORD newSize = ::GlobalSize(hGlobalMemory);

  // Copy text to Global Memory Area
  if (hGlobalMemory != NULL) {
    //PSTR pstr = (PSTR)::GlobalLock(hGlobalMemory);
    char * pstr = (char *)::GlobalLock(hGlobalMemory);
    //PSTR pstr = pGlobalMemory;

    // need to use memcpy here
    char* s = NS_REINTERPRET_CAST(char*, data);
    PRUint32 inx;
    for (inx=0; inx < len; inx++) {
	    *pstr++ = *s++;
    }

    // Put data on Clipboard
    BOOL status = ::GlobalUnlock(hGlobalMemory);
  }

  aSTG.hGlobal = hGlobalMemory;

  // Now, delete the memory that was created by CreateDataFromPrimitive
  delete [] data;

	return ResultFromScode(S_OK);
}

//-----------------------------------------------------
HRESULT nsDataObj::GetMetafilePict(FORMATETC&, STGMEDIUM&)
{
	return ResultFromScode(E_NOTIMPL);
}

//-----------------------------------------------------
HRESULT nsDataObj::SetBitmap(FORMATETC&, STGMEDIUM&)
{
	return ResultFromScode(E_NOTIMPL);
}

//-----------------------------------------------------
HRESULT nsDataObj::SetDib   (FORMATETC&, STGMEDIUM&)
{
	return ResultFromScode(E_FAIL);
}

//-----------------------------------------------------
HRESULT nsDataObj::SetText  (FORMATETC& aFE, STGMEDIUM& aSTG)
{
  if (aFE.cfFormat == CF_TEXT && aFE.tymed ==  TYMED_HGLOBAL) {
		HGLOBAL hString = (HGLOBAL)aSTG.hGlobal;
		if(hString == NULL)
			return(FALSE);

		// get a pointer to the actual bytes
		char *  pString = (char *) GlobalLock(hString);    
		if(!pString)
			return(FALSE);

		GlobalUnlock(hString);
    nsAutoString str(pString);

  }
	return ResultFromScode(E_FAIL);
}

//-----------------------------------------------------
HRESULT nsDataObj::SetMetafilePict (FORMATETC&, STGMEDIUM&)
{
	return ResultFromScode(E_FAIL);
}



//-----------------------------------------------------
//-----------------------------------------------------
CLSID nsDataObj::GetClassID() const
{
	return CLSID_nsDataObj;
}

//-----------------------------------------------------
// Registers a the DataFlavor/FE pair
//-----------------------------------------------------
void nsDataObj::AddDataFlavor(const nsString & aDataFlavor, LPFORMATETC aFE)
{
  // These two lists are the mapping to and from data flavors and FEs
  // Later, OLE will tell us it's needs a certain type of FORMATETC (text, unicode, etc)
  // so we will look up data flavor that corresponds to the FE
  // and then ask the transferable for that type of data
  mDataFlavors->AppendElement(new nsString(aDataFlavor));
  m_enumFE->AddFE(aFE);

}

//-----------------------------------------------------
// Sets the transferable object
//-----------------------------------------------------
void nsDataObj::SetTransferable(nsITransferable * aTransferable)
{
    NS_IF_RELEASE(mTransferable);

  mTransferable = aTransferable;
  if (nsnull == mTransferable) {
    return;
  }

  NS_ADDREF(mTransferable);

  return;
}


//еее skanky hack until i can correctly re-create primitives from native data. i know this code sucks,
//еее please forgive me.
void
CreatePrimitiveForData ( const char* aFlavor, void* aDataBuff, PRUint32 aDataLen, nsISupports** aPrimitive )
{
  if ( !aPrimitive )
    return;

  if ( strcmp(aFlavor,kTextMime) == 0 ) {
    nsCOMPtr<nsISupportsString> primitive;
    nsresult rv = nsComponentManager::CreateInstance(NS_SUPPORTS_STRING_PROGID, nsnull, 
                                                      NS_GET_IID(nsISupportsString), getter_AddRefs(primitive));
    if ( primitive ) {
      primitive->SetData ( (char*)aDataBuff );
      nsCOMPtr<nsISupports> genericPrimitive ( do_QueryInterface(primitive) );
      *aPrimitive = genericPrimitive;
      NS_ADDREF(*aPrimitive);
    }
  }
  else {
    nsCOMPtr<nsISupportsWString> primitive;
    nsresult rv = nsComponentManager::CreateInstance(NS_SUPPORTS_WSTRING_PROGID, nsnull, 
                                                      NS_GET_IID(nsISupportsWString), getter_AddRefs(primitive));
    if ( primitive ) {
      primitive->SetData ( (unsigned short*)aDataBuff );
      nsCOMPtr<nsISupports> genericPrimitive ( do_QueryInterface(primitive) );
      *aPrimitive = genericPrimitive;
      NS_ADDREF(*aPrimitive);
    }  
  }

} // CreatePrimitiveForData


void
CreateDataFromPrimitive ( const char* aFlavor, nsISupports* aPrimitive, void** aDataBuff, PRUint32 aDataLen )
{
  if ( !aDataBuff )
    return;

  if ( strcmp(aFlavor,kTextMime) == 0 ) {
    nsCOMPtr<nsISupportsString> plainText ( do_QueryInterface(aPrimitive) );
    if ( plainText )
      plainText->GetData ( (char**)aDataBuff );
  }
  else {
    nsCOMPtr<nsISupportsWString> doubleByteText ( do_QueryInterface(aPrimitive) );
    if ( doubleByteText )
      doubleByteText->GetData ( (unsigned short**)aDataBuff );
  }

}

