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
 */

#include "nsDataObj.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsITransferable.h"
#include "nsISupportsPrimitives.h"
#include "IENUMFE.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsPrimitiveHelpers.h"
#include "nsXPIDLString.h"

#include "OLE2.h"
#include "URLMON.h"
#include "shlobj.h"

#if 1
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
    nsCAutoString* df = NS_REINTERPRET_CAST(nsCAutoString *, mDataFlavors->ElementAt(i));
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
  if ( !mTransferable )
	  return ResultFromScode(DATA_E_FORMATETC);

  PRUint32 dfInx = 0;

  static CLIPFORMAT fileDescriptorFlavor = ::RegisterClipboardFormat( CFSTR_FILEDESCRIPTOR ); 
  static CLIPFORMAT fileFlavor = ::RegisterClipboardFormat( CFSTR_FILECONTENTS ); 

  ULONG count;
  FORMATETC fe;
  m_enumFE->Reset();
  while (NOERROR == m_enumFE->Next(1, &fe, &count)) {
    nsCAutoString * df = NS_REINTERPRET_CAST(nsCAutoString*, mDataFlavors->ElementAt(dfInx));
    if ( df ) {
		  if (FormatsMatch(fe, *pFE)) {
			  pSTM->pUnkForRelease = NULL;
			  CLIPFORMAT format = pFE->cfFormat;
			  switch(format) {
				  case CF_TEXT:
				  case CF_UNICODETEXT:
					  return GetText(df, *pFE, *pSTM);
					  break;					  			  
				  // ... not yet implemented ...
				  //case CF_BITMAP:
				  //	return GetBitmap(*pFE, *pSTM);
				  //case CF_DIB:
				  //	return GetDib(*pFE, *pSTM);
				  //case CF_METAFILEPICT:
				  //	return GetMetafilePict(*pFE, *pSTM);
            
				  default:

            if ( format == fileDescriptorFlavor )
              return GetFileDescriptor ( *pFE, *pSTM );
            else if ( format == fileFlavor )
              return GetFileContents ( *pFE, *pSTM );
            else {
              PRNTDEBUG2("***** nsDataObj::GetData - Unknown format %x\n", format);
					    return GetText(df, *pFE, *pSTM);
            }
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
	return E_NOTIMPL;
}



//
// GetFileDescriptor
//

HRESULT 
nsDataObj :: GetFileDescriptor ( FORMATETC& aFE, STGMEDIUM& aSTG )
{
  HRESULT res = S_OK;
  
  // How we handle this depends on if we're dealing with an internet
  // shortcut, since those are done under the covers.
  if ( IsInternetShortcut() )
    res = GetFileDescriptorInternetShortcut ( aFE, aSTG );
  else
    NS_WARNING ( "Not yet implemented\n" );
  
	return res;
	
} // GetFileDescriptor


//
HRESULT 
nsDataObj :: GetFileContents ( FORMATETC& aFE, STGMEDIUM& aSTG )
{
  HRESULT res = S_OK;
  
  // How we handle this depends on if we're dealing with an internet
  // shortcut, since those are done under the covers.
  if ( IsInternetShortcut() )  
    res = GetFileContentsInternetShortcut ( aFE, aSTG );
  else
    NS_WARNING ( "Not yet implemented\n" );

	return res;
	
} // GetFileContents


//
// GetFileDescriptorInternetShortcut
//
// Create the special format for an internet shortcut and build up the data
// structures the shell is expecting.
//
HRESULT
nsDataObj :: GetFileDescriptorInternetShortcut ( FORMATETC& aFE, STGMEDIUM& aSTG )
{
  HRESULT result = S_OK;
  
  // setup format structure
  FORMATETC fmetc = { 0, NULL, DVASPECT_CONTENT, 0, TYMED_HGLOBAL };
  fmetc.cfFormat = RegisterClipboardFormat ( CFSTR_FILEDESCRIPTOR );

  HGLOBAL fileGroupDescHand = ::GlobalAlloc(GMEM_ZEROINIT|GMEM_SHARE,sizeof(FILEGROUPDESCRIPTOR));
  if ( fileGroupDescHand ) {
    LPFILEGROUPDESCRIPTOR fileGroupDesc = NS_REINTERPRET_CAST(LPFILEGROUPDESCRIPTOR, ::GlobalLock(fileGroupDescHand));

    char* url = "http://www.dvdresource.com";     // TEMP!

    // one file in the file descriptor block
    fileGroupDesc->cItems = 1;
    fileGroupDesc->fgd[0].dwFlags = FD_LINKUI;
    
    // create the filename string -- |.URL| extensions imply an internet shortcut file. Make
    // sure the filename isn't too long as to blow out the array, and still have enough room
    // for the |.URL| suffix.
    int urlLength = strlen(url) > MAX_PATH - 5 ? MAX_PATH - 5 : strlen(url);
    url[urlLength] = nsnull;
    sprintf(fileGroupDesc->fgd[0].cFileName, "%s.URL", url);

    ::GlobalUnlock ( fileGroupDescHand );
    aSTG.hGlobal = fileGroupDescHand;
  }
  else
    result = E_OUTOFMEMORY;
    
  return result;
  
} // GetFileDescriptorInternetShortcut


//
// GetFileContentsInternetShortcut
//
// Create the special format for an internet shortcut and build up the data
// structures the shell is expecting.
//
HRESULT
nsDataObj :: GetFileContentsInternetShortcut ( FORMATETC& aFE, STGMEDIUM& aSTG )
{
  HRESULT result = S_OK;
  
  char* url = "foopy";    // TEMP!
  
  // setup format structure
  FORMATETC fmetc = { 0, NULL, DVASPECT_CONTENT, 0, TYMED_HGLOBAL };
  fmetc.cfFormat = RegisterClipboardFormat ( CFSTR_FILECONTENTS );

  // create a global memory area and build up the file contents w/in it
  static const char* shortcutPrefix = "[InternetShortcut]\nURL=";
  HGLOBAL hGlobalMemory = ::GlobalAlloc(GMEM_SHARE, strlen(shortcutPrefix) + strlen(url) + 1);
  if ( hGlobalMemory ) {
    char* contents = NS_REINTERPRET_CAST(char*, ::GlobalLock(hGlobalMemory));
    sprintf( contents, "%s%s", shortcutPrefix, url );
    ::GlobalUnlock(hGlobalMemory);
    aSTG.hGlobal = hGlobalMemory;
  }
  else
    result = E_OUTOFMEMORY;
    
  return result;
  
} // GetFileContentsInternetShortcut


//
// IsInternetShortcut
//
// Is the data that we're handling destined for an internet shortcut if
// dragged to the desktop? We can tell because there will be a mime type
// of |kURLMime| present in the transferable
//
PRBool
nsDataObj :: IsInternetShortcut ( )
{
  PRBool retval = PR_FALSE;
  
  if ( !mTransferable )
    return PR_FALSE;
  
  // get the list of flavors available in the transferable
  nsCOMPtr<nsISupportsArray> flavorList;
  mTransferable->FlavorsTransferableCanExport ( getter_AddRefs(flavorList) );
  if ( !flavorList )
    return PR_FALSE;
  
  // go spelunking for the url flavor
  PRUint32 cnt;
  flavorList->Count(&cnt);
  for ( PRUint32 i = 0;i < cnt; ++i ) {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt (i, getter_AddRefs(genericFlavor));
    nsCOMPtr<nsISupportsString> currentFlavor (do_QueryInterface(genericFlavor));
    if (currentFlavor) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));
      if ( strcmp(flavorStr, kURLMime) == 0 ) {
        retval = PR_TRUE;         // found it!
        break;
      }
    }
  } // for each flavor

  return retval;
  
} // IsInternetShortcut


//-----------------------------------------------------
HRESULT nsDataObj::GetText(nsCAutoString * aDataFlavor, FORMATETC& aFE, STGMEDIUM& aSTG)
{
  void* data;
  PRUint32   len;
  
  // if someone asks for text/plain, look up text/unicode instead in the transferable.
  char* flavorStr;
  if ( aDataFlavor->Equals("text/plain") )
    flavorStr = kUnicodeMime;
  else
    flavorStr = *aDataFlavor;

  // NOTE: CreateDataFromPrimitive creates new memory, that needs to be deleted
  nsCOMPtr<nsISupports> genericDataWrapper;
  mTransferable->GetTransferData(flavorStr, getter_AddRefs(genericDataWrapper), &len);
  if ( !len )
    return ResultFromScode(E_FAIL);
  nsPrimitiveHelpers::CreateDataFromPrimitive ( flavorStr, genericDataWrapper, &data, len );

  HGLOBAL     hGlobalMemory = NULL;
  PSTR        pGlobalMemory = NULL;

  aSTG.tymed          = TYMED_HGLOBAL;
  aSTG.pUnkForRelease = NULL;

  // If someone is asking for text/plain, we need to convert unicode (assuming it's present)
  // to text with the correct platform encoding.
  //
  // The transferable gives us data that is null-terminated, but this isn't reflected in
  // the |len| parameter. Windoze apps expect this null to be there so bump our data buffer
  // by the appropriate size to account for the null (one char for CF_TEXT, one PRUnichar for
  // CF_UNICODETEXT).
  DWORD allocLen = (DWORD)len;
  if ( aFE.cfFormat == CF_TEXT ) {
    char* plainTextData = nsnull;
    PRUnichar* castedUnicode = NS_REINTERPRET_CAST(PRUnichar*, data);
    PRInt32 plainTextLen = 0;
    nsPrimitiveHelpers::ConvertUnicodeToPlatformPlainText ( castedUnicode, len / 2, &plainTextData, &plainTextLen );
   
    // replace the unicode data with our plaintext data. Recall that |plainTextLen| doesn't include
    // the null in the length.
    nsAllocator::Free(data);
    if ( plainTextData ) {
      data = plainTextData;
      allocLen = plainTextLen + sizeof(char);
    }
    else {
      NS_WARNING ( "Oh no, couldn't convert unicode to plain text" );
      return ResultFromScode(S_OK);
    }
  }
  else if ( aFE.cfFormat == CF_UNICODETEXT )
    allocLen += sizeof(PRUnichar);
    
  hGlobalMemory = (HGLOBAL)::GlobalAlloc(GMEM_MOVEABLE, allocLen);

  // Copy text to Global Memory Area
  if ( hGlobalMemory ) {
    char* dest = NS_REINTERPRET_CAST(char*, ::GlobalLock(hGlobalMemory));
    char* source = NS_REINTERPRET_CAST(char*, data);
    memcpy ( dest, source, allocLen );                         // copies the null as well
    BOOL status = ::GlobalUnlock(hGlobalMemory);
  }
  aSTG.hGlobal = hGlobalMemory;

  // Now, delete the memory that was created by CreateDataFromPrimitive (or our text/plain data)
  nsAllocator::Free(data);

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
    nsAutoString str; str.AssignWithConversion(pString);

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
void nsDataObj::AddDataFlavor(const char* aDataFlavor, LPFORMATETC aFE)
{
  // These two lists are the mapping to and from data flavors and FEs
  // Later, OLE will tell us it's needs a certain type of FORMATETC (text, unicode, etc)
  // so we will look up data flavor that corresponds to the FE
  // and then ask the transferable for that type of data
  mDataFlavors->AppendElement(new nsCAutoString(aDataFlavor));
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

