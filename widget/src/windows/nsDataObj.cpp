/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Sean Echevarria <Sean@Beatnik.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDataObj.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsVoidArray.h"
#include "nsITransferable.h"
#include "nsISupportsPrimitives.h"
#include "IENUMFE.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsPrimitiveHelpers.h"
#include "nsXPIDLString.h"
#include "nsIImage.h"
#include "nsImageClipboard.h"
#include "prprf.h"

#include "OLE2.h"
#include "URLMON.h"
#include "shlobj.h"

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
			  pSTM->pUnkForRelease = NULL;        // caller is responsible for deleting this data
			  CLIPFORMAT format = pFE->cfFormat;
			  switch(format) {
			    
			    // Someone is asking for plain or unicode text
				  case CF_TEXT:
				  case CF_UNICODETEXT:
					  return GetText(*df, *pFE, *pSTM);
					  break;
					  
			    // Someone is asking for an image
				  case CF_DIB:
				  	return GetDib(*df, *pFE, *pSTM);
					  				  			  
				  // ... not yet implemented ...
				  //case CF_BITMAP:
				  //	return GetBitmap(*pFE, *pSTM);
				  //case CF_METAFILEPICT:
				  //	return GetMetafilePict(*pFE, *pSTM);
            
				  default:

            if ( format == fileDescriptorFlavor )
              return GetFileDescriptor ( *pFE, *pSTM );
            else if ( format == fileFlavor )
              return GetFileContents ( *pFE, *pSTM );
            else {
              PRNTDEBUG2("***** nsDataObj::GetData - Unknown format %x\n", format);
					    return GetText(*df, *pFE, *pSTM);
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
HRESULT 
nsDataObj::GetBitmap ( nsAReadableCString& , FORMATETC&, STGMEDIUM& )
{
  PRNTDEBUG("nsDataObj::GetBitmap\n");
	return ResultFromScode(E_NOTIMPL);
}


//
// GetDIB
//
// Someone is asking for a bitmap. The data in the transferable will be a straight
// nsIImage, so just QI it.
//
HRESULT 
nsDataObj :: GetDib ( nsAReadableCString& inFlavor, FORMATETC &, STGMEDIUM & aSTG )
{
  PRNTDEBUG("nsDataObj::GetDib\n");
  ULONG result = E_FAIL;
  
  PRUint32 len = 0;
  nsCOMPtr<nsISupports> genericDataWrapper;
  mTransferable->GetTransferData(PromiseFlatCString(inFlavor).get(), getter_AddRefs(genericDataWrapper), &len);
  nsCOMPtr<nsIImage> image ( do_QueryInterface(genericDataWrapper) );
  if ( image ) {
    // use a the helper class to build up a bitmap. We now own the bits,
    // and pass them back to the OS in |aSTG|.
    nsImageToClipboard converter ( image );
    HANDLE bits = nsnull;
    nsresult rv = converter.GetPicture ( &bits );
    if ( NS_SUCCEEDED(rv) && bits ) {
      aSTG.hGlobal = bits;
      aSTG.tymed = TYMED_HGLOBAL;
      result = S_OK;
    }
  } // if we have an image
  else  
    NS_WARNING ( "Definately not an image on clipboard" );
  
	return ResultFromScode(result);
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

    nsAutoString title;
    if ( NS_FAILED(ExtractShortcutTitle(title)) )
      return E_OUTOFMEMORY;

    char titleStr[MAX_PATH+1];
    int lenTitleStr = WideCharToMultiByte(CP_ACP, 0, title.get(), title.Length(), titleStr, MAX_PATH, NULL, NULL);
    if (!lenTitleStr && (GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
      // this is a very rare situation
      int len = title.Length() - 1;
      while ((len > 0) && (GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
        lenTitleStr = WideCharToMultiByte(CP_ACP, 0, title.get(), len--, titleStr, MAX_PATH, NULL, NULL);  
      } 
    }
    titleStr[lenTitleStr] = '\0';

    // one file in the file descriptor block
    fileGroupDesc->cItems = 1;
    fileGroupDesc->fgd[0].dwFlags = FD_LINKUI;
    
    // create the filename string -- |.URL| extensions imply an internet shortcut file. Make
    // sure the filename isn't too long as to blow out the array, and still have enough room
    // for our .URL suffix.
    
    static const char* shortcutSuffix = ".URL";
    static int suffixLen = strlen(shortcutSuffix);
    int titleLen = strlen(titleStr);
    int trimmedLen = titleLen > MAX_PATH - (suffixLen + 1) ? MAX_PATH - (suffixLen + 1) : titleLen;
    titleStr[trimmedLen] = '\0';
    sprintf(fileGroupDesc->fgd[0].cFileName, "%s%s", titleStr, shortcutSuffix);

    ::GlobalUnlock ( fileGroupDescHand );
    aSTG.hGlobal = fileGroupDescHand;
    aSTG.tymed = TYMED_HGLOBAL;
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
  
  nsAutoString url;
  if ( NS_FAILED(ExtractShortcutURL(url)) )
    return E_OUTOFMEMORY;
  char* urlStr = ToNewCString(url);         // need to revisit here once we implement iDNS?!?!
  if ( !urlStr )
    return E_OUTOFMEMORY;
    
  // setup format structure
  FORMATETC fmetc = { 0, NULL, DVASPECT_CONTENT, 0, TYMED_HGLOBAL };
  fmetc.cfFormat = RegisterClipboardFormat ( CFSTR_FILECONTENTS );

  // create a global memory area and build up the file contents w/in it
  static const char* shortcutFormatStr = "[InternetShortcut]\nURL=%s\r\n";
  static const int formatLen = strlen(shortcutFormatStr) - 2; // don't include %s in the len
  const int totalLen = formatLen + strlen(urlStr) + 1;
  HGLOBAL hGlobalMemory = ::GlobalAlloc(GMEM_SHARE, totalLen);
  if ( hGlobalMemory ) {
    char* contents = NS_REINTERPRET_CAST(char*, ::GlobalLock(hGlobalMemory));
    PR_snprintf( contents, totalLen, shortcutFormatStr, urlStr );
    ::GlobalUnlock(hGlobalMemory);
    aSTG.hGlobal = hGlobalMemory;
    aSTG.tymed = TYMED_HGLOBAL;
  }
  else
    result = E_OUTOFMEMORY;

  nsMemory::Free ( urlStr );
    
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
HRESULT nsDataObj::GetText(nsAReadableCString & aDataFlavor, FORMATETC& aFE, STGMEDIUM& aSTG)
{
  void* data;
  PRUint32   len;
  
  // if someone asks for text/plain, look up text/unicode instead in the transferable.
  const char* flavorStr;
  const nsPromiseFlatCString& flat = PromiseFlatCString(aDataFlavor);
  if ( aDataFlavor.Equals("text/plain") )
    flavorStr = kUnicodeMime;
  else
    flavorStr = flat.get();

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

  // oddly, this isn't in the MSVC headers anywhere.
  static int CF_HTML = ::RegisterClipboardFormat("HTML Format");

  // We play games under the hood and advertise flavors that we know we
  // can support, only they require a bit of conversion or munging of the data.
  // Do that here.
  //
  // The transferable gives us data that is null-terminated, but this isn't reflected in
  // the |len| parameter. Windoze apps expect this null to be there so bump our data buffer
  // by the appropriate size to account for the null (one char for CF_TEXT, one PRUnichar for
  // CF_UNICODETEXT).
  DWORD allocLen = (DWORD)len;
  if ( aFE.cfFormat == CF_TEXT ) {
    // Someone is asking for text/plain; convert the unicode (assuming it's present)
    // to text with the correct platform encoding.
    char* plainTextData = nsnull;
    PRUnichar* castedUnicode = NS_REINTERPRET_CAST(PRUnichar*, data);
    PRInt32 plainTextLen = 0;
    nsPrimitiveHelpers::ConvertUnicodeToPlatformPlainText ( castedUnicode, len / 2, &plainTextData, &plainTextLen );
   
    // replace the unicode data with our plaintext data. Recall that |plainTextLen| doesn't include
    // the null in the length.
    nsMemory::Free(data);
    if ( plainTextData ) {
      data = plainTextData;
      allocLen = plainTextLen + sizeof(char);
    }
    else {
      NS_WARNING ( "Oh no, couldn't convert unicode to plain text" );
      return ResultFromScode(S_OK);
    }
  }
  else if ( aFE.cfFormat == CF_HTML ) {
    // Someone is asking for win32's HTML flavor. Convert our html fragment
    // from unicode to UTF-8 then put it into a format specified by msft.
    NS_ConvertUCS2toUTF8 converter ( NS_REINTERPRET_CAST(PRUnichar*, data) );
    char* utf8HTML = nsnull;
    nsresult rv = BuildPlatformHTML ( converter.get(), &utf8HTML );      // null terminates
    
    nsMemory::Free(data);
    if ( NS_SUCCEEDED(rv) && utf8HTML ) {
      // replace the unicode data with our HTML data. Don't forget the null.
      data = utf8HTML;
      allocLen = strlen(utf8HTML) + sizeof(char);
    }
    else {
      NS_WARNING ( "Oh no, couldn't convert to HTML" );
      return ResultFromScode(S_OK);
    }
  }
  else {
    // we assume that any data that isn't caught above is unicode. This may
    // be an erroneous assumption, but is true so far.
    allocLen += sizeof(PRUnichar);
  }

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
  nsMemory::Free(data);

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


//
// ExtractURL
//
// Roots around in the transferable for the appropriate flavor that indicates
// a url and pulls out the url portion of the data. Used mostly for creating
// internet shortcuts on the desktop. The url flavor is of the format:
//
//   <url> <linefeed> <page title>
//
nsresult
nsDataObj :: ExtractShortcutURL ( nsString & outURL )
{
  NS_ASSERTION ( mTransferable, "We'd don't have a good transferable" );
  nsresult rv = NS_ERROR_FAILURE;
  
  PRUint32 len = 0;
  nsCOMPtr<nsISupports> genericURL;
  if ( NS_SUCCEEDED(mTransferable->GetTransferData(kURLMime, getter_AddRefs(genericURL), &len)) ) {
    nsCOMPtr<nsISupportsWString> urlObject ( do_QueryInterface(genericURL) );
    if ( urlObject ) {
      nsXPIDLString url;
      urlObject->GetData ( getter_Copies(url) );
      outURL = url;

      // find the first linefeed in the data, that's where the url ends. trunc the 
      // result string at that point.
      PRInt32 lineIndex = outURL.FindChar ( '\n' );
      NS_ASSERTION ( lineIndex > 0, "Format for url flavor is <url> <linefeed> <page title>" );
      if ( lineIndex > 0 ) {
        outURL.Truncate ( lineIndex );
        rv = NS_OK;    
      }
    }
  } // if found flavor
  
  return rv;

} // ExtractShortcutURL


//
// ExtractShortcutTitle
//
// Roots around in the transferable for the appropriate flavor that indicates
// a url and pulls out the title portion of the data. Used mostly for creating
// internet shortcuts on the desktop. The url flavor is of the format:
//
//   <url> <linefeed> <page title>
//
nsresult
nsDataObj :: ExtractShortcutTitle ( nsString & outTitle )
{
  NS_ASSERTION ( mTransferable, "We'd don't have a good transferable" );
  nsresult rv = NS_ERROR_FAILURE;
  
  PRUint32 len = 0;
  nsCOMPtr<nsISupports> genericURL;
  if ( NS_SUCCEEDED(mTransferable->GetTransferData(kURLMime, getter_AddRefs(genericURL), &len)) ) {
    nsCOMPtr<nsISupportsWString> urlObject ( do_QueryInterface(genericURL) );
    if ( urlObject ) {
      nsXPIDLString url;
      nsAutoString holder;
      urlObject->GetData ( getter_Copies(url) );
      holder = url;

      // find the first linefeed in the data, that's where the url ends. we want
      // everything after that linefeed. FindChar() returns -1 if we can't find
      PRInt32 lineIndex = holder.FindChar ( '\n' );
      NS_ASSERTION ( lineIndex != -1, "Format for url flavor is <url> <linefeed> <page title>" );
      if ( lineIndex != -1 ) {
        holder.Mid ( outTitle, lineIndex + 1, (len/2) - (lineIndex + 1) );
        rv = NS_OK;    
      }
    }
  } // if found flavor
  
  return rv;

} // ExtractShortcutTitle


//
// BuildPlatformHTML
//
// Munge our HTML data to win32's CF_HTML spec. Basically, put the requisite
// header information on it. This will null terminate |outPlatformHTML|. See
//  http://msdn.microsoft.com/workshop/networking/clipboard/htmlclipboard.asp
// for details.
//
// We assume that |inOurHTML| is already a fragment (ie, doesn't have <HTML>
// or <BODY> tags). We'll wrap the fragment with them to make other apps
// happy.
//
// This code is derived from sample code from MSDN. It's ugly, I agree.
//
nsresult 
nsDataObj :: BuildPlatformHTML ( const char* inOurHTML, char** outPlatformHTML ) 
{
  *outPlatformHTML = nsnull;
  
  // Create temporary buffer for HTML header... 
  char *buf = NS_REINTERPRET_CAST(char*, nsMemory::Alloc(400 + strlen(inOurHTML)));
  if( !buf )
    return NS_ERROR_FAILURE;

  // Get clipboard id for HTML format...
  static int cfid = RegisterClipboardFormat("HTML Format");;

  // Create a template string for the HTML header...
  strcpy(buf,
      "Version:0.9\r\n"
      "StartHTML:00000000\r\n"
      "EndHTML:00000000\r\n"
      "StartFragment:00000000\r\n"
      "EndFragment:00000000\r\n"
      "<html><body>\r\n"
      "<!--StartFragment -->\r\n");

  // Append the HTML...
  strcat(buf, inOurHTML);
  strcat(buf, "\r\n");
  // Finish up the HTML format...
  strcat(buf,
      "<!--EndFragment-->\r\n"
      "</body>\r\n"
      "</html>");

  // Now go back, calculate all the lengths, and write out the
  // necessary header information. Note, wsprintf() truncates the
  // string when you overwrite it so you follow up with code to replace
  // the 0 appended at the end with a '\r'...
  char *ptr = strstr(buf, "StartHTML");
  wsprintf(ptr+10, "%08u", strstr(buf, "<html>") - buf);
  *(ptr+10+8) = '\r';

  ptr = strstr(buf, "EndHTML");
  wsprintf(ptr+8, "%08u", strlen(buf));
  *(ptr+8+8) = '\r';

  ptr = strstr(buf, "StartFragment");
  wsprintf(ptr+14, "%08u", strstr(buf, "<!--StartFrag") - buf);
  *(ptr+14+8) = '\r';

  ptr = strstr(buf, "EndFragment");
  wsprintf(ptr+12, "%08u", strstr(buf, "<!--EndFrag") - buf);
  *(ptr+12+8) = '\r';

  *outPlatformHTML = buf;

  return NS_OK;
}
