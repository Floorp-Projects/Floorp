/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include <ole2.h>
#include <shlobj.h>

#include "nsDataObj.h"
#include "nsClipboard.h"
#include "nsReadableUtils.h"
#include "nsITransferable.h"
#include "nsISupportsPrimitives.h"
#include "IEnumFE.h"
#include "nsPrimitiveHelpers.h"
#include "nsXPIDLString.h"
#include "nsImageClipboard.h"
#include "nsCRT.h"
#include "nsPrintfCString.h"
#include "nsIStringBundle.h"
#include "nsEscape.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsXPCOMStrings.h"
#include "nscore.h"
#include "prtypes.h"
#include "nsDirectoryServiceDefs.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS1(nsDataObj::CStream, nsIStreamListener)

//-----------------------------------------------------------------------------
// CStream implementation
nsDataObj::CStream::CStream() :
  mChannelRead(false),
  mStreamRead(0)
{
}

//-----------------------------------------------------------------------------
nsDataObj::CStream::~CStream()
{
}

//-----------------------------------------------------------------------------
// helper - initializes the stream
nsresult nsDataObj::CStream::Init(nsIURI *pSourceURI)
{
  nsresult rv;
  rv = NS_NewChannel(getter_AddRefs(mChannel), pSourceURI,
                     nsnull, nsnull, nsnull,
                     nsIRequest::LOAD_FROM_CACHE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mChannel->AsyncOpen(this, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// IUnknown's QueryInterface, nsISupport's AddRef and Release are shared by
// IUnknown and nsIStreamListener.
STDMETHODIMP nsDataObj::CStream::QueryInterface(REFIID refiid, void** ppvResult)
{
  *ppvResult = NULL;
  if (IID_IUnknown == refiid ||
      refiid == IID_IStream)

  {
    *ppvResult = this;
  }

  if (NULL != *ppvResult)
  {
    ((LPUNKNOWN)*ppvResult)->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

// nsIStreamListener implementation
NS_IMETHODIMP nsDataObj::CStream::OnDataAvailable(nsIRequest *aRequest,
                                                  nsISupports *aContext,
                                                  nsIInputStream *aInputStream,
                                                  PRUint32 aOffset, // offset within the stream
                                                  PRUint32 aCount) // bytes available on this call
{
    // Extend the write buffer for the incoming data.
    PRUint8* buffer = mChannelData.AppendElements(aCount);
    if (buffer == NULL)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ASSERTION((mChannelData.Length() == (aOffset + aCount)),
      "stream length mismatch w/write buffer");

    // Read() may not return aCount on a single call, so loop until we've
    // accumulated all the data OnDataAvailable has promised.
    nsresult rv;
    PRUint32 odaBytesReadTotal = 0;
    do {
      PRUint32 bytesReadByCall = 0;
      rv = aInputStream->Read((char*)(buffer + odaBytesReadTotal),
                              aCount, &bytesReadByCall);
      odaBytesReadTotal += bytesReadByCall;
    } while (aCount < odaBytesReadTotal && NS_SUCCEEDED(rv));
    return rv;
}

NS_IMETHODIMP nsDataObj::CStream::OnStartRequest(nsIRequest *aRequest,
                                                 nsISupports *aContext)
{
    mChannelResult = NS_OK;
    return NS_OK;
}

NS_IMETHODIMP nsDataObj::CStream::OnStopRequest(nsIRequest *aRequest,
                                                nsISupports *aContext,
                                                nsresult aStatusCode)
{
    mChannelRead = true;
    mChannelResult = aStatusCode;
    return NS_OK;
}

// Pumps thread messages while waiting for the async listener operation to
// complete. Failing this call will fail the stream incall from Windows
// and cancel the operation.
nsresult nsDataObj::CStream::WaitForCompletion()
{
  // We are guaranteed OnStopRequest will get called, so this should be ok.
  while (!mChannelRead) {
    // Pump messages
    NS_ProcessNextEvent(nsnull, true);
  }

  if (!mChannelData.Length())
    mChannelResult = NS_ERROR_FAILURE;

  return mChannelResult;
}

//-----------------------------------------------------------------------------
// IStream
STDMETHODIMP nsDataObj::CStream::Clone(IStream** ppStream)
{
  return E_NOTIMPL;
}

//-----------------------------------------------------------------------------
STDMETHODIMP nsDataObj::CStream::Commit(DWORD dwFrags)
{
  return E_NOTIMPL;
}

//-----------------------------------------------------------------------------
STDMETHODIMP nsDataObj::CStream::CopyTo(IStream* pDestStream,
                                        ULARGE_INTEGER nBytesToCopy,
                                        ULARGE_INTEGER* nBytesRead,
                                        ULARGE_INTEGER* nBytesWritten)
{
  return E_NOTIMPL;
}

//-----------------------------------------------------------------------------
STDMETHODIMP nsDataObj::CStream::LockRegion(ULARGE_INTEGER nStart,
                                            ULARGE_INTEGER nBytes,
                                            DWORD dwFlags)
{
  return E_NOTIMPL;
}

//-----------------------------------------------------------------------------
STDMETHODIMP nsDataObj::CStream::Read(void* pvBuffer,
                                      ULONG nBytesToRead,
                                      ULONG* nBytesRead)
{
  // Wait for the write into our buffer to complete via the stream listener.
  // We can't respond to this by saying "call us back later".
  if (NS_FAILED(WaitForCompletion()))
    return E_FAIL;

  // Bytes left for Windows to read out of our buffer
  ULONG bytesLeft = mChannelData.Length() - mStreamRead;
  // Let Windows know what we will hand back, usually this is the entire buffer
  *nBytesRead = NS_MIN(bytesLeft, nBytesToRead);
  // Copy the buffer data over
  memcpy(pvBuffer, ((char*)mChannelData.Elements() + mStreamRead), *nBytesRead);
  // Update our bytes read tracking
  mStreamRead += *nBytesRead;
  return S_OK;
}

//-----------------------------------------------------------------------------
STDMETHODIMP nsDataObj::CStream::Revert(void)
{
  return E_NOTIMPL;
}

//-----------------------------------------------------------------------------
STDMETHODIMP nsDataObj::CStream::Seek(LARGE_INTEGER nMove,
                                      DWORD dwOrigin,
                                      ULARGE_INTEGER* nNewPos)
{
  if (nNewPos == NULL)
    return STG_E_INVALIDPOINTER;

  if (nMove.LowPart == 0 && nMove.HighPart == 0 &&
      (dwOrigin == STREAM_SEEK_SET || dwOrigin == STREAM_SEEK_CUR)) { 
    nNewPos->LowPart = 0;
    nNewPos->HighPart = 0;
    return S_OK;
  }

  return E_NOTIMPL;
}

//-----------------------------------------------------------------------------
STDMETHODIMP nsDataObj::CStream::SetSize(ULARGE_INTEGER nNewSize)
{
  return E_NOTIMPL;
}

//-----------------------------------------------------------------------------
STDMETHODIMP nsDataObj::CStream::Stat(STATSTG* statstg, DWORD dwFlags)
{
  if (statstg == NULL)
    return STG_E_INVALIDPOINTER;

  if (!mChannel || NS_FAILED(WaitForCompletion()))
    return E_FAIL;

  memset((void*)statstg, 0, sizeof(STATSTG));

  if (dwFlags != STATFLAG_NONAME) 
  {
    nsCOMPtr<nsIURI> sourceURI;
    if (NS_FAILED(mChannel->GetURI(getter_AddRefs(sourceURI)))) {
      return E_FAIL;
    }

    nsCAutoString strFileName;
    nsCOMPtr<nsIURL> sourceURL = do_QueryInterface(sourceURI);
    sourceURL->GetFileName(strFileName);

    if (strFileName.IsEmpty())
      return E_FAIL;

    NS_UnescapeURL(strFileName);
    NS_ConvertUTF8toUTF16 wideFileName(strFileName);

    PRUint32 nMaxNameLength = (wideFileName.Length()*2) + 2;
    void * retBuf = CoTaskMemAlloc(nMaxNameLength); // freed by caller
    if (!retBuf) 
      return STG_E_INSUFFICIENTMEMORY;

    ZeroMemory(retBuf, nMaxNameLength);
    memcpy(retBuf, wideFileName.get(), wideFileName.Length()*2);
    statstg->pwcsName = (LPOLESTR)retBuf;
  }

  SYSTEMTIME st;

  statstg->type = STGTY_STREAM;

  GetSystemTime(&st);
  SystemTimeToFileTime((const SYSTEMTIME*)&st, (LPFILETIME)&statstg->mtime);
  statstg->ctime = statstg->atime = statstg->mtime;

  statstg->cbSize.LowPart = (DWORD)mChannelData.Length();
  statstg->grfMode = STGM_READ;
  statstg->grfLocksSupported = LOCK_ONLYONCE;
  statstg->clsid = CLSID_NULL;

  return S_OK;
}

//-----------------------------------------------------------------------------
STDMETHODIMP nsDataObj::CStream::UnlockRegion(ULARGE_INTEGER nStart,
                                              ULARGE_INTEGER nBytes,
                                              DWORD dwFlags)
{
  return E_NOTIMPL;
}

//-----------------------------------------------------------------------------
STDMETHODIMP nsDataObj::CStream::Write(const void* pvBuffer,
                                       ULONG nBytesToRead,
                                       ULONG* nBytesRead)
{
  return E_NOTIMPL;
}

//-----------------------------------------------------------------------------
HRESULT nsDataObj::CreateStream(IStream **outStream)
{
  NS_ENSURE_TRUE(outStream, E_INVALIDARG);

  nsresult rv = NS_ERROR_FAILURE;
  nsAutoString wideFileName;
  nsCOMPtr<nsIURI> sourceURI;

  rv = GetDownloadDetails(getter_AddRefs(sourceURI),
                          wideFileName);
  NS_ENSURE_SUCCESS(rv, rv);

  nsDataObj::CStream *pStream = new nsDataObj::CStream();
  NS_ENSURE_TRUE(pStream, E_OUTOFMEMORY);

  pStream->AddRef();

  rv = pStream->Init(sourceURI);
  if (NS_FAILED(rv))
  {
    pStream->Release();
    return E_FAIL;
  }
  *outStream = pStream;

  return S_OK;
}

static GUID CLSID_nsDataObj =
	{ 0x1bba7640, 0xdf52, 0x11cf, { 0x82, 0x7b, 0, 0xa0, 0x24, 0x3a, 0xe5, 0x05 } };

/* 
 * deliberately not using MAX_PATH. This is because on platforms < XP
 * a file created with a long filename may be mishandled by the shell
 * resulting in it not being able to be deleted or moved. 
 * See bug 250392 for more details.
 */
#define NS_MAX_FILEDESCRIPTOR 128 + 1

/*
 * Class nsDataObj
 */

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsDataObj::nsDataObj(nsIURI * uri)
  : m_cRef(0), mTransferable(nsnull),
    mIsAsyncMode(FALSE), mIsInOperation(FALSE)
{
  m_enumFE = new CEnumFormatEtc();
  m_enumFE->AddRef();

  if (uri) {
    // A URI was obtained, so pass this through to the DataObject
    // so it can create a SourceURL for CF_HTML flavour
    uri->GetSpec(mSourceURL);
  }
}
//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsDataObj::~nsDataObj()
{
  NS_IF_RELEASE(mTransferable);

  mDataFlavors.Clear();

  m_enumFE->Release();

  // Free arbitrary system formats
  for (PRUint32 idx = 0; idx < mDataEntryList.Length(); idx++) {
      CoTaskMemFree(mDataEntryList[idx]->fe.ptd);
      ReleaseStgMedium(&mDataEntryList[idx]->stgm);
      CoTaskMemFree(mDataEntryList[idx]);
  }
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
		return S_OK;
  } else if (IID_IAsyncOperation == riid) {
    *ppv = static_cast<IAsyncOperation*>(this);
    AddRef();
    return S_OK;
  }

	return E_NOINTERFACE;
}

//-----------------------------------------------------
STDMETHODIMP_(ULONG) nsDataObj::AddRef()
{
	++m_cRef;
	NS_LOG_ADDREF(this, m_cRef, "nsDataObj", sizeof(*this));
	return m_cRef;
}


//-----------------------------------------------------
STDMETHODIMP_(ULONG) nsDataObj::Release()
{
	--m_cRef;
	
	NS_LOG_RELEASE(this, m_cRef, "nsDataObj");
	if (0 != m_cRef)
		return m_cRef;

  // We have released our last ref on this object and need to delete the
  // temp file. External app acting as drop target may still need to open the
  // temp file. Addref a timer so it can delay deleting file and destroying
  // this object. Delete file anyway and destroy this obj if there's a problem.
  if (mCachedTempFile) {
    nsresult rv;
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      mTimer->InitWithFuncCallback(nsDataObj::RemoveTempFile, this,
                                   500, nsITimer::TYPE_ONE_SHOT);
      return AddRef();
    }
    mCachedTempFile->Remove(false);
    mCachedTempFile = NULL;
  }

	delete this;

	return 0;
}

//-----------------------------------------------------
BOOL nsDataObj::FormatsMatch(const FORMATETC& source, const FORMATETC& target) const
{
  if ((source.cfFormat == target.cfFormat) &&
      (source.dwAspect & target.dwAspect) &&
      (source.tymed & target.tymed)) {
    return TRUE;
  } else {
    return FALSE;
  }
}

//-----------------------------------------------------
// IDataObject methods
//-----------------------------------------------------
STDMETHODIMP nsDataObj::GetData(LPFORMATETC aFormat, LPSTGMEDIUM pSTM)
{
  if (!mTransferable)
    return DV_E_FORMATETC;

  PRUint32 dfInx = 0;

  static CLIPFORMAT fileDescriptorFlavorA = ::RegisterClipboardFormat( CFSTR_FILEDESCRIPTORA ); 
  static CLIPFORMAT fileDescriptorFlavorW = ::RegisterClipboardFormat( CFSTR_FILEDESCRIPTORW ); 
  static CLIPFORMAT uniformResourceLocatorA = ::RegisterClipboardFormat( CFSTR_INETURLA );
  static CLIPFORMAT uniformResourceLocatorW = ::RegisterClipboardFormat( CFSTR_INETURLW );
  static CLIPFORMAT fileFlavor = ::RegisterClipboardFormat( CFSTR_FILECONTENTS ); 
  static CLIPFORMAT PreferredDropEffect = ::RegisterClipboardFormat( CFSTR_PREFERREDDROPEFFECT );

  // Arbitrary system formats are used for image feedback during drag
  // and drop. We are responsible for storing these internally during
  // drag operations.
  LPDATAENTRY pde;
  if (LookupArbitraryFormat(aFormat, &pde, FALSE)) {
    return CopyMediumData(pSTM, &pde->stgm, aFormat, FALSE)
           ? S_OK : E_UNEXPECTED;
  }

  // Firefox internal formats
  ULONG count;
  FORMATETC fe;
  m_enumFE->Reset();
  while (NOERROR == m_enumFE->Next(1, &fe, &count)
         && dfInx < mDataFlavors.Length()) {
    nsCString& df = mDataFlavors.ElementAt(dfInx);
    if (FormatsMatch(fe, *aFormat)) {
      pSTM->pUnkForRelease = NULL;        // caller is responsible for deleting this data
      CLIPFORMAT format = aFormat->cfFormat;
      switch(format) {

      // Someone is asking for plain or unicode text
      case CF_TEXT:
      case CF_UNICODETEXT:
      return GetText(df, *aFormat, *pSTM);

      // Some 3rd party apps that receive drag and drop files from the browser
      // window require support for this.
      case CF_HDROP:
        return GetFile(*aFormat, *pSTM);

      // Someone is asking for an image
      case CF_DIB:
        return GetDib(df, *aFormat, *pSTM);

      default:
        if ( format == fileDescriptorFlavorA )
          return GetFileDescriptor ( *aFormat, *pSTM, false );
        if ( format == fileDescriptorFlavorW )
          return GetFileDescriptor ( *aFormat, *pSTM, true);
        if ( format == uniformResourceLocatorA )
          return GetUniformResourceLocator( *aFormat, *pSTM, false);
        if ( format == uniformResourceLocatorW )
          return GetUniformResourceLocator( *aFormat, *pSTM, true);
        if ( format == fileFlavor )
          return GetFileContents ( *aFormat, *pSTM );
        if ( format == PreferredDropEffect )
          return GetPreferredDropEffect( *aFormat, *pSTM );
        //PR_LOG(gWindowsLog, PR_LOG_ALWAYS, 
        //       ("***** nsDataObj::GetData - Unknown format %u\n", format));
        return GetText(df, *aFormat, *pSTM);
      } //switch
    } // if
    dfInx++;
  } // while

  return DATA_E_FORMATETC;
}

//-----------------------------------------------------
STDMETHODIMP nsDataObj::GetDataHere(LPFORMATETC pFE, LPSTGMEDIUM pSTM)
{
  return E_FAIL;
}


//-----------------------------------------------------
// Other objects querying to see if we support a 
// particular format
//-----------------------------------------------------
STDMETHODIMP nsDataObj::QueryGetData(LPFORMATETC pFE)
{
  // Arbitrary system formats are used for image feedback during drag
  // and drop. We are responsible for storing these internally during
  // drag operations.
  LPDATAENTRY pde;
  if (LookupArbitraryFormat(pFE, &pde, FALSE))
    return S_OK;

  // Firefox internal formats
  ULONG count;
  FORMATETC fe;
  m_enumFE->Reset();
  while (NOERROR == m_enumFE->Next(1, &fe, &count)) {
    if (fe.cfFormat == pFE->cfFormat) {
      return S_OK;
    }
  }
  return E_FAIL;
}

//-----------------------------------------------------
STDMETHODIMP nsDataObj::GetCanonicalFormatEtc
	 (LPFORMATETC pFEIn, LPFORMATETC pFEOut)
{
  return E_FAIL;
}

//-----------------------------------------------------
STDMETHODIMP nsDataObj::SetData(LPFORMATETC aFormat, LPSTGMEDIUM aMedium, BOOL shouldRel)
{
  // Arbitrary system formats are used for image feedback during drag
  // and drop. We are responsible for storing these internally during
  // drag operations.
  LPDATAENTRY pde;
  if (LookupArbitraryFormat(aFormat, &pde, TRUE)) {
    // Release the old data the lookup handed us for this format. This
    // may have been set in CopyMediumData when we originally stored the
    // data.
    if (pde->stgm.tymed) {
      ReleaseStgMedium(&pde->stgm);
      memset(&pde->stgm, 0, sizeof(STGMEDIUM));
    }

    bool result = true;
    if (shouldRel) {
      // If shouldRel is TRUE, the data object called owns the storage medium
      // after the call returns. Store the incoming data in our data array for
      // release when we are destroyed. This is the common case with arbitrary
      // data from explorer.
      pde->stgm = *aMedium;
    } else {
      // Copy the incoming data into our data array. (AFAICT, this never gets
      // called with arbitrary formats for drag images.)
      result = CopyMediumData(&pde->stgm, aMedium, aFormat, TRUE);
    }
    pde->fe.tymed = pde->stgm.tymed;

    return result ? S_OK : DV_E_TYMED;
  }

  if (shouldRel)
    ReleaseStgMedium(aMedium);

  return S_OK;
}

bool
nsDataObj::LookupArbitraryFormat(FORMATETC *aFormat, LPDATAENTRY *aDataEntry, BOOL aAddorUpdate)
{
  *aDataEntry = NULL;

  if (aFormat->ptd != NULL)
    return false;

  // See if it's already in our list. If so return the data entry.
  for (PRUint32 idx = 0; idx < mDataEntryList.Length(); idx++) {
    if (mDataEntryList[idx]->fe.cfFormat == aFormat->cfFormat &&
        mDataEntryList[idx]->fe.dwAspect == aFormat->dwAspect &&
        mDataEntryList[idx]->fe.lindex == aFormat->lindex) {
      if (aAddorUpdate || (mDataEntryList[idx]->fe.tymed & aFormat->tymed)) {
        // If the caller requests we update, or if the 
        // medium type matches, return the entry. 
        *aDataEntry = mDataEntryList[idx];
        return true;
      } else {
        // Medium does not match, not found.
        return false;
      }
    }
  }

  if (!aAddorUpdate)
    return false;

  // Add another entry to mDataEntryList
  LPDATAENTRY dataEntry = (LPDATAENTRY)CoTaskMemAlloc(sizeof(DATAENTRY));
  if (!dataEntry)
    return false;
  
  dataEntry->fe = *aFormat;
  *aDataEntry = dataEntry;
  memset(&dataEntry->stgm, 0, sizeof(STGMEDIUM));

  // Add this to our IEnumFORMATETC impl. so we can return it when
  // it's requested.
  m_enumFE->AddFormatEtc(aFormat);

  // Store a copy internally in the arbitrary formats array.
  mDataEntryList.AppendElement(dataEntry);

  return true;
}

bool
nsDataObj::CopyMediumData(STGMEDIUM *aMediumDst, STGMEDIUM *aMediumSrc, LPFORMATETC aFormat, BOOL aSetData)
{
  STGMEDIUM stgmOut = *aMediumSrc;
  
  switch (stgmOut.tymed) {
    case TYMED_ISTREAM:
      stgmOut.pstm->AddRef();
    break;
    case TYMED_ISTORAGE:
      stgmOut.pstg->AddRef();
    break;
    case TYMED_HGLOBAL:
      if (!aMediumSrc->pUnkForRelease) {
        if (aSetData) {
          if (aMediumSrc->tymed != TYMED_HGLOBAL)
            return false;
          stgmOut.hGlobal = OleDuplicateData(aMediumSrc->hGlobal, aFormat->cfFormat, 0);
          if (!stgmOut.hGlobal)
            return false;
        } else {
          // We are returning this data from LookupArbitraryFormat, indicate to the
          // shell we hold it and will free it.
          stgmOut.pUnkForRelease = static_cast<IDataObject*>(this);
        }
      }
    break;
    default:
      return false;
  }

  if (stgmOut.pUnkForRelease)
    stgmOut.pUnkForRelease->AddRef();

  *aMediumDst = stgmOut;

  return true;
}

//-----------------------------------------------------
STDMETHODIMP nsDataObj::EnumFormatEtc(DWORD dwDir, LPENUMFORMATETC *ppEnum)
{
  switch (dwDir) {
    case DATADIR_GET:
      m_enumFE->Clone(ppEnum);
      break;
    case DATADIR_SET:
      // fall through
    default:
      *ppEnum = NULL;
  } // switch

  if (NULL == *ppEnum)
    return E_FAIL;

  (*ppEnum)->Reset();
  // Clone already AddRefed the result so don't addref it again.
  return NOERROR;
}

//-----------------------------------------------------
STDMETHODIMP nsDataObj::DAdvise(LPFORMATETC pFE, DWORD dwFlags,
										            LPADVISESINK pIAdviseSink, DWORD* pdwConn)
{
  return E_FAIL;
}


//-----------------------------------------------------
STDMETHODIMP nsDataObj::DUnadvise(DWORD dwConn)
{
  return E_FAIL;
}

//-----------------------------------------------------
STDMETHODIMP nsDataObj::EnumDAdvise(LPENUMSTATDATA *ppEnum)
{
  return E_FAIL;
}

// IAsyncOperation methods
STDMETHODIMP nsDataObj::EndOperation(HRESULT hResult,
                                     IBindCtx *pbcReserved,
                                     DWORD dwEffects)
{
  mIsInOperation = FALSE;
  return S_OK;
}

STDMETHODIMP nsDataObj::GetAsyncMode(BOOL *pfIsOpAsync)
{
  *pfIsOpAsync = mIsAsyncMode;

  return S_OK;
}

STDMETHODIMP nsDataObj::InOperation(BOOL *pfInAsyncOp)
{
  *pfInAsyncOp = mIsInOperation;

  return S_OK;
}

STDMETHODIMP nsDataObj::SetAsyncMode(BOOL fDoOpAsync)
{
  mIsAsyncMode = fDoOpAsync;
  return S_OK;
}

STDMETHODIMP nsDataObj::StartOperation(IBindCtx *pbcReserved)
{
  mIsInOperation = TRUE;
  return S_OK;
}

//-----------------------------------------------------
// GetData and SetData helper functions
//-----------------------------------------------------
HRESULT nsDataObj::AddSetFormat(FORMATETC& aFE)
{
  return S_OK;
}

//-----------------------------------------------------
HRESULT nsDataObj::AddGetFormat(FORMATETC& aFE)
{
  return S_OK;
}

//
// GetDIB
//
// Someone is asking for a bitmap. The data in the transferable will be a straight
// imgIContainer, so just QI it.
//
HRESULT 
nsDataObj :: GetDib ( const nsACString& inFlavor, FORMATETC &, STGMEDIUM & aSTG )
{
  ULONG result = E_FAIL;
  PRUint32 len = 0;
  nsCOMPtr<nsISupports> genericDataWrapper;
  mTransferable->GetTransferData(PromiseFlatCString(inFlavor).get(), getter_AddRefs(genericDataWrapper), &len);
  nsCOMPtr<imgIContainer> image ( do_QueryInterface(genericDataWrapper) );
  if ( !image ) {
    // Check if the image was put in an nsISupportsInterfacePointer wrapper.
    // This might not be necessary any more, but could be useful for backwards
    // compatibility.
    nsCOMPtr<nsISupportsInterfacePointer> ptr(do_QueryInterface(genericDataWrapper));
    if ( ptr )
      ptr->GetData(getter_AddRefs(image));
  }
  
  if ( image ) {
    // use the |nsImageToClipboard| helper class to build up a bitmap. We now own
    // the bits, and pass them back to the OS in |aSTG|.
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
    NS_WARNING ( "Definitely not an image on clipboard" );
	return result;
}



//
// GetFileDescriptor
//

HRESULT 
nsDataObj :: GetFileDescriptor ( FORMATETC& aFE, STGMEDIUM& aSTG, bool aIsUnicode )
{
  HRESULT res = S_OK;
  
  // How we handle this depends on if we're dealing with an internet
  // shortcut, since those are done under the covers.
  if (IsFlavourPresent(kFilePromiseMime) ||
      IsFlavourPresent(kFileMime))
  {
    if (aIsUnicode)
      return GetFileDescriptor_IStreamW(aFE, aSTG);
    else
      return GetFileDescriptor_IStreamA(aFE, aSTG);
  }
  else if (IsFlavourPresent(kURLMime))
  {
    if ( aIsUnicode )
      res = GetFileDescriptorInternetShortcutW ( aFE, aSTG );
    else
      res = GetFileDescriptorInternetShortcutA ( aFE, aSTG );
  }
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
  if (IsFlavourPresent(kFilePromiseMime) ||
      IsFlavourPresent(kFileMime))
    return GetFileContents_IStream(aFE, aSTG);
  else if (IsFlavourPresent(kURLMime))
    return GetFileContentsInternetShortcut ( aFE, aSTG );
  else
    NS_WARNING ( "Not yet implemented\n" );

	return res;
	
} // GetFileContents

// 
// Given a unicode string, we ensure that it contains only characters which are valid within
// the file system. Remove all forbidden characters from the name, and completely disallow 
// any title that starts with a forbidden name and extension (e.g. "nul" is invalid, but 
// "nul." and "nul.txt" are also invalid and will cause problems).
//
// It would seem that this is more functionality suited to being in nsIFile.
//
static void
MangleTextToValidFilename(nsString & aText)
{
  static const char* forbiddenNames[] = {
    "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", 
    "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
    "CON", "PRN", "AUX", "NUL", "CLOCK$"
  };

  aText.StripChars(FILE_PATH_SEPARATOR  FILE_ILLEGAL_CHARACTERS);
  aText.CompressWhitespace(true, true);
  PRUint32 nameLen;
  for (size_t n = 0; n < ArrayLength(forbiddenNames); ++n) {
    nameLen = (PRUint32) strlen(forbiddenNames[n]);
    if (aText.EqualsIgnoreCase(forbiddenNames[n], nameLen)) {
      // invalid name is either the entire string, or a prefix with a period
      if (aText.Length() == nameLen || aText.CharAt(nameLen) == PRUnichar('.')) {
        aText.Truncate();
        break;
      }
    }
  }
}

// 
// Given a unicode string, convert it down to a valid local charset filename
// with the supplied extension. This ensures that we do not cut MBCS characters
// in the middle.
//
// It would seem that this is more functionality suited to being in nsIFile.
//
static bool
CreateFilenameFromTextA(nsString & aText, const char * aExtension, 
                         char * aFilename, PRUint32 aFilenameLen)
{
  // ensure that the supplied name doesn't have invalid characters. If 
  // a valid mangled filename couldn't be created then it will leave the
  // text empty.
  MangleTextToValidFilename(aText);
  if (aText.IsEmpty())
    return false;

  // repeatably call WideCharToMultiByte as long as the title doesn't fit in the buffer 
  // available to us. Continually reduce the length of the source title until the MBCS
  // version will fit in the buffer with room for the supplied extension. Doing it this
  // way ensures that even in MBCS environments there will be a valid MBCS filename of
  // the correct length.
  int maxUsableFilenameLen = aFilenameLen - strlen(aExtension) - 1; // space for ext + null byte
  int currLen, textLen = (int) NS_MIN(aText.Length(), aFilenameLen);
  char defaultChar = '_';
  do {
    currLen = WideCharToMultiByte(CP_ACP, 
      WC_COMPOSITECHECK|WC_DEFAULTCHAR,
      aText.get(), textLen--, aFilename, maxUsableFilenameLen, &defaultChar, NULL);
  }
  while (currLen == 0 && textLen > 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
  if (currLen > 0 && textLen > 0) {
    strcpy(&aFilename[currLen], aExtension);
    return true;
  }
  else {
    // empty names aren't permitted
    return false;
  }
}

static bool
CreateFilenameFromTextW(nsString & aText, const wchar_t * aExtension, 
                         wchar_t * aFilename, PRUint32 aFilenameLen)
{
  // ensure that the supplied name doesn't have invalid characters. If 
  // a valid mangled filename couldn't be created then it will leave the
  // text empty.
  MangleTextToValidFilename(aText);
  if (aText.IsEmpty())
    return false;

  const int extensionLen = wcslen(aExtension);
  if (aText.Length() + extensionLen + 1 > aFilenameLen)
    aText.Truncate(aFilenameLen - extensionLen - 1);
  wcscpy(&aFilename[0], aText.get());
  wcscpy(&aFilename[aText.Length()], aExtension);
  return true;
}

#define PAGEINFO_PROPERTIES "chrome://navigator/locale/pageInfo.properties"

static bool
GetLocalizedString(const PRUnichar * aName, nsXPIDLString & aString)
{
  nsCOMPtr<nsIStringBundleService> stringService =
    mozilla::services::GetStringBundleService();
  if (!stringService)
    return false;

  nsCOMPtr<nsIStringBundle> stringBundle;
  nsresult rv = stringService->CreateBundle(PAGEINFO_PROPERTIES,
                                            getter_AddRefs(stringBundle));
  if (NS_FAILED(rv))
    return false;

  rv = stringBundle->GetStringFromName(aName, getter_Copies(aString));
  return NS_SUCCEEDED(rv);
}

//
// GetFileDescriptorInternetShortcut
//
// Create the special format for an internet shortcut and build up the data
// structures the shell is expecting.
//
HRESULT
nsDataObj :: GetFileDescriptorInternetShortcutA ( FORMATETC& aFE, STGMEDIUM& aSTG )
{
  // get the title of the shortcut
  nsAutoString title;
  if ( NS_FAILED(ExtractShortcutTitle(title)) )
    return E_OUTOFMEMORY;

  HGLOBAL fileGroupDescHandle = ::GlobalAlloc(GMEM_ZEROINIT|GMEM_SHARE,sizeof(FILEGROUPDESCRIPTORA));
  if (!fileGroupDescHandle)
    return E_OUTOFMEMORY;

  LPFILEGROUPDESCRIPTORA fileGroupDescA = reinterpret_cast<LPFILEGROUPDESCRIPTORA>(::GlobalLock(fileGroupDescHandle));
  if (!fileGroupDescA) {
    ::GlobalFree(fileGroupDescHandle);
    return E_OUTOFMEMORY;
  }

  // get a valid filename in the following order: 1) from the page title, 
  // 2) localized string for an untitled page, 3) just use "Untitled.URL"
  if (!CreateFilenameFromTextA(title, ".URL", 
                               fileGroupDescA->fgd[0].cFileName, NS_MAX_FILEDESCRIPTOR)) {
    nsXPIDLString untitled;
    if (!GetLocalizedString(NS_LITERAL_STRING("noPageTitle").get(), untitled) ||
        !CreateFilenameFromTextA(untitled, ".URL", 
                                 fileGroupDescA->fgd[0].cFileName, NS_MAX_FILEDESCRIPTOR)) {
      strcpy(fileGroupDescA->fgd[0].cFileName, "Untitled.URL");
    }
  }

  // one file in the file block
  fileGroupDescA->cItems = 1;
  fileGroupDescA->fgd[0].dwFlags = FD_LINKUI;

  ::GlobalUnlock( fileGroupDescHandle );
  aSTG.hGlobal = fileGroupDescHandle;
  aSTG.tymed = TYMED_HGLOBAL;

  return S_OK;
} // GetFileDescriptorInternetShortcutA

HRESULT
nsDataObj :: GetFileDescriptorInternetShortcutW ( FORMATETC& aFE, STGMEDIUM& aSTG )
{
  // get the title of the shortcut
  nsAutoString title;
  if ( NS_FAILED(ExtractShortcutTitle(title)) )
    return E_OUTOFMEMORY;

  HGLOBAL fileGroupDescHandle = ::GlobalAlloc(GMEM_ZEROINIT|GMEM_SHARE,sizeof(FILEGROUPDESCRIPTORW));
  if (!fileGroupDescHandle)
    return E_OUTOFMEMORY;

  LPFILEGROUPDESCRIPTORW fileGroupDescW = reinterpret_cast<LPFILEGROUPDESCRIPTORW>(::GlobalLock(fileGroupDescHandle));
  if (!fileGroupDescW) {
    ::GlobalFree(fileGroupDescHandle);
    return E_OUTOFMEMORY;
  }

  // get a valid filename in the following order: 1) from the page title, 
  // 2) localized string for an untitled page, 3) just use "Untitled.URL"
  if (!CreateFilenameFromTextW(title, NS_LITERAL_STRING(".URL").get(), 
                               fileGroupDescW->fgd[0].cFileName, NS_MAX_FILEDESCRIPTOR)) {
    nsXPIDLString untitled;
    if (!GetLocalizedString(NS_LITERAL_STRING("noPageTitle").get(), untitled) ||
        !CreateFilenameFromTextW(untitled, NS_LITERAL_STRING(".URL").get(), 
                                 fileGroupDescW->fgd[0].cFileName, NS_MAX_FILEDESCRIPTOR)) {
      wcscpy(fileGroupDescW->fgd[0].cFileName, NS_LITERAL_STRING("Untitled.URL").get());
    }
  }

  // one file in the file block
  fileGroupDescW->cItems = 1;
  fileGroupDescW->fgd[0].dwFlags = FD_LINKUI;

  ::GlobalUnlock( fileGroupDescHandle );
  aSTG.hGlobal = fileGroupDescHandle;
  aSTG.tymed = TYMED_HGLOBAL;

  return S_OK;
} // GetFileDescriptorInternetShortcutW


//
// GetFileContentsInternetShortcut
//
// Create the special format for an internet shortcut and build up the data
// structures the shell is expecting.
//
HRESULT
nsDataObj :: GetFileContentsInternetShortcut ( FORMATETC& aFE, STGMEDIUM& aSTG )
{
  nsAutoString url;
  if ( NS_FAILED(ExtractShortcutURL(url)) )
    return E_OUTOFMEMORY;

  // will need to change if we ever support iDNS
  nsCAutoString asciiUrl;
  LossyCopyUTF16toASCII(url, asciiUrl);
    
  static const char* shortcutFormatStr = "[InternetShortcut]\r\nURL=%s\r\n";
  static const int formatLen = strlen(shortcutFormatStr) - 2; // don't include %s in the len
  const int totalLen = formatLen + asciiUrl.Length(); // we don't want a null character on the end

  // create a global memory area and build up the file contents w/in it
  HGLOBAL hGlobalMemory = ::GlobalAlloc(GMEM_SHARE, totalLen);
  if ( !hGlobalMemory )
    return E_OUTOFMEMORY;

  char* contents = reinterpret_cast<char*>(::GlobalLock(hGlobalMemory));
  if ( !contents ) {
    ::GlobalFree( hGlobalMemory );
    return E_OUTOFMEMORY;
  }
    
  //NOTE: we intentionally use the Microsoft version of snprintf here because it does NOT null 
  // terminate strings which reach the maximum size of the buffer. Since we know that the 
  // formatted length here is totalLen, this call to _snprintf will format the string into 
  // the buffer without appending the null character.
  _snprintf( contents, totalLen, shortcutFormatStr, asciiUrl.get() );
    
  ::GlobalUnlock(hGlobalMemory);
  aSTG.hGlobal = hGlobalMemory;
  aSTG.tymed = TYMED_HGLOBAL;

  return S_OK;
} // GetFileContentsInternetShortcut

// check if specified flavour is present in the transferable
bool nsDataObj :: IsFlavourPresent(const char *inFlavour)
{
  bool retval = false;
  NS_ENSURE_TRUE(mTransferable, false);
  
  // get the list of flavors available in the transferable
  nsCOMPtr<nsISupportsArray> flavorList;
  mTransferable->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
  NS_ENSURE_TRUE(flavorList, false);

  // try to find requested flavour
  PRUint32 cnt;
  flavorList->Count(&cnt);
  for (PRUint32 i = 0; i < cnt; ++i) {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt (i, getter_AddRefs(genericFlavor));
    nsCOMPtr<nsISupportsCString> currentFlavor (do_QueryInterface(genericFlavor));
    if (currentFlavor) {
      nsCAutoString flavorStr;
      currentFlavor->GetData(flavorStr);
      if (flavorStr.Equals(inFlavour)) {
        retval = true;         // found it!
        break;
      }
    }
  } // for each flavor

  return retval;
}

HRESULT nsDataObj::GetPreferredDropEffect ( FORMATETC& aFE, STGMEDIUM& aSTG )
{
  HRESULT res = S_OK;
  aSTG.tymed = TYMED_HGLOBAL;
  aSTG.pUnkForRelease = NULL;    
  HGLOBAL hGlobalMemory = NULL;
  hGlobalMemory = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
  if (hGlobalMemory) {
    DWORD* pdw = (DWORD*) GlobalLock(hGlobalMemory);
    // The PreferredDropEffect clipboard format is only registered if a drag/drop
    // of an image happens from Mozilla to the desktop.  We want its value
    // to be DROPEFFECT_MOVE in that case so that the file is moved from the
    // temporary location, not copied.
    // This value should, ideally, be set on the data object via SetData() but 
    // our IDataObject implementation doesn't implement SetData.  It adds data
    // to the data object lazily only when the drop target asks for it.
    *pdw = (DWORD) DROPEFFECT_MOVE;
    GlobalUnlock(hGlobalMemory);
  }
  else {
    res = E_OUTOFMEMORY;
  }
  aSTG.hGlobal = hGlobalMemory;
  return res;
}

//-----------------------------------------------------
HRESULT nsDataObj::GetText(const nsACString & aDataFlavor, FORMATETC& aFE, STGMEDIUM& aSTG)
{
  void* data = NULL;
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
    return E_FAIL;
  nsPrimitiveHelpers::CreateDataFromPrimitive ( flavorStr, genericDataWrapper, &data, len );
  if ( !data )
    return E_FAIL;

  HGLOBAL     hGlobalMemory = NULL;

  aSTG.tymed          = TYMED_HGLOBAL;
  aSTG.pUnkForRelease = NULL;

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
    PRUnichar* castedUnicode = reinterpret_cast<PRUnichar*>(data);
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
      return S_OK;
    }
  }
  else if ( aFE.cfFormat == nsClipboard::CF_HTML ) {
    // Someone is asking for win32's HTML flavor. Convert our html fragment
    // from unicode to UTF-8 then put it into a format specified by msft.
    NS_ConvertUTF16toUTF8 converter ( reinterpret_cast<PRUnichar*>(data) );
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
      return S_OK;
    }
  }
  else {
    // we assume that any data that isn't caught above is unicode. This may
    // be an erroneous assumption, but is true so far.
    allocLen += sizeof(PRUnichar);
  }

  hGlobalMemory = (HGLOBAL)GlobalAlloc(GMEM_MOVEABLE, allocLen);

  // Copy text to Global Memory Area
  if ( hGlobalMemory ) {
    char* dest = reinterpret_cast<char*>(GlobalLock(hGlobalMemory));
    char* source = reinterpret_cast<char*>(data);
    memcpy ( dest, source, allocLen );                         // copies the null as well
    GlobalUnlock(hGlobalMemory);
  }
  aSTG.hGlobal = hGlobalMemory;

  // Now, delete the memory that was created by CreateDataFromPrimitive (or our text/plain data)
  nsMemory::Free(data);

  return S_OK;
}

//-----------------------------------------------------
HRESULT nsDataObj::GetFile(FORMATETC& aFE, STGMEDIUM& aSTG)
{
  PRUint32 dfInx = 0;
  ULONG count;
  FORMATETC fe;
  m_enumFE->Reset();
  while (NOERROR == m_enumFE->Next(1, &fe, &count)
    && dfInx < mDataFlavors.Length()) {
      if (mDataFlavors[dfInx].EqualsLiteral(kNativeImageMime))
        return DropImage(aFE, aSTG);
      if (mDataFlavors[dfInx].EqualsLiteral(kFileMime))
        return DropFile(aFE, aSTG);
      if (mDataFlavors[dfInx].EqualsLiteral(kFilePromiseMime))
        return DropTempFile(aFE, aSTG);
      dfInx++;
  }
  return E_FAIL;
}

HRESULT nsDataObj::DropFile(FORMATETC& aFE, STGMEDIUM& aSTG)
{
  nsresult rv;
  PRUint32 len = 0;
  nsCOMPtr<nsISupports> genericDataWrapper;

  mTransferable->GetTransferData(kFileMime, getter_AddRefs(genericDataWrapper),
                                 &len);
  nsCOMPtr<nsIFile> file ( do_QueryInterface(genericDataWrapper) );

  if (!file)
  {
    nsCOMPtr<nsISupportsInterfacePointer> ptr(do_QueryInterface(genericDataWrapper));
    if (ptr)
      ptr->GetData(getter_AddRefs(file));
  }

  if (!file)
    return E_FAIL;

  aSTG.tymed = TYMED_HGLOBAL;
  aSTG.pUnkForRelease = NULL;

  nsAutoString path;
  rv = file->GetPath(path);
  if (NS_FAILED(rv))
    return E_FAIL;

  PRUint32 allocLen = path.Length() + 2;
  HGLOBAL hGlobalMemory = NULL;
  PRUnichar *dest;

  hGlobalMemory = GlobalAlloc(GMEM_MOVEABLE, sizeof(DROPFILES) +
                                             allocLen * sizeof(PRUnichar));
  if (!hGlobalMemory)
    return E_FAIL;

  DROPFILES* pDropFile = (DROPFILES*)GlobalLock(hGlobalMemory);

  // First, populate the drop file structure
  pDropFile->pFiles = sizeof(DROPFILES); //Offset to start of file name string
  pDropFile->fNC    = 0;
  pDropFile->pt.x   = 0;
  pDropFile->pt.y   = 0;
  pDropFile->fWide  = TRUE;

  // Copy the filename right after the DROPFILES structure
  dest = (PRUnichar*)(((char*)pDropFile) + pDropFile->pFiles);
  memcpy(dest, path.get(), (allocLen - 1) * sizeof(PRUnichar));

  // Two null characters are needed at the end of the file name.
  // Lookup the CF_HDROP shell clipboard format for more info.
  // Add the second null character right after the first one.
  dest[allocLen - 1] = L'\0';

  GlobalUnlock(hGlobalMemory);

  aSTG.hGlobal = hGlobalMemory;

  return S_OK;
}

HRESULT nsDataObj::DropImage(FORMATETC& aFE, STGMEDIUM& aSTG)
{
  nsresult rv;
  if (!mCachedTempFile) {
    PRUint32 len = 0;
    nsCOMPtr<nsISupports> genericDataWrapper;

    mTransferable->GetTransferData(kNativeImageMime, getter_AddRefs(genericDataWrapper), &len);
    nsCOMPtr<imgIContainer> image(do_QueryInterface(genericDataWrapper));

    if (!image) {
      // Check if the image was put in an nsISupportsInterfacePointer wrapper.
      // This might not be necessary any more, but could be useful for backwards
      // compatibility.
      nsCOMPtr<nsISupportsInterfacePointer> ptr(do_QueryInterface(genericDataWrapper));
      if (ptr)
        ptr->GetData(getter_AddRefs(image));
    }

    if (!image) 
      return E_FAIL;

    // Use the clipboard helper class to build up a memory bitmap.
    nsImageToClipboard converter(image);
    HANDLE bits = nsnull;
    rv = converter.GetPicture(&bits); // Clipboard routines return a global handle we own.

    if (NS_FAILED(rv) || !bits)
      return E_FAIL;

    // We now own these bits!
    PRUint32 bitmapSize = GlobalSize(bits);
    if (!bitmapSize) {
      GlobalFree(bits);
      return E_FAIL;
    }

    // Save the bitmap to a temporary location.      
    nsCOMPtr<nsIFile> dropFile;
    rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(dropFile));
    if (!dropFile) {
      GlobalFree(bits);
      return E_FAIL;
    }

    // Filename must be random so as not to confuse apps like
    // Photoshop which handle multiple drags into a single window.
    char buf[13];
    nsCString filename;
    NS_MakeRandomString(buf, 8);
    memcpy(buf+8, ".bmp", 5);
    filename.Append(nsDependentCString(buf, 12));
    dropFile->AppendNative(filename);
    rv = dropFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0660);
    if (NS_FAILED(rv)) { 
      GlobalFree(bits);
      return E_FAIL;
    }

    // Cache the temp file so we can delete it later and so
    // it doesn't get recreated over and over on multiple calls
    // which does occur from windows shell.
    dropFile->Clone(getter_AddRefs(mCachedTempFile));

    // Write the data to disk.
    nsCOMPtr<nsIOutputStream> outStream;
    rv = NS_NewLocalFileOutputStream(getter_AddRefs(outStream), dropFile);
    if (NS_FAILED(rv)) { 
      GlobalFree(bits);
      return E_FAIL;
    }

    char * bm = (char *)GlobalLock(bits);

    BITMAPFILEHEADER	fileHdr;
    BITMAPINFOHEADER *bmpHdr = (BITMAPINFOHEADER*)bm;

    fileHdr.bfType        = ((WORD) ('M' << 8) | 'B');
    fileHdr.bfSize        = GlobalSize (bits) + sizeof(fileHdr);
    fileHdr.bfReserved1   = 0;
    fileHdr.bfReserved2   = 0;
    fileHdr.bfOffBits     = (DWORD) (sizeof(fileHdr) + bmpHdr->biSize);

    PRUint32 writeCount = 0;
    if (NS_FAILED(outStream->Write((const char *)&fileHdr, sizeof(fileHdr), &writeCount)) ||
        NS_FAILED(outStream->Write((const char *)bm, bitmapSize, &writeCount)))
      rv = NS_ERROR_FAILURE;

    outStream->Close();

    GlobalUnlock(bits);
    GlobalFree(bits);

    if (NS_FAILED(rv))
      return E_FAIL;
  }
  
  // Pass the file name back to the drop target so that it can access the file.
  nsAutoString path;
  rv = mCachedTempFile->GetPath(path);
  if (NS_FAILED(rv))
    return E_FAIL;

  // Two null characters are needed to terminate the file name list.
  HGLOBAL hGlobalMemory = NULL;

  PRUint32 allocLen = path.Length() + 2;

  aSTG.tymed = TYMED_HGLOBAL;
  aSTG.pUnkForRelease = NULL;

  hGlobalMemory = GlobalAlloc(GMEM_MOVEABLE, sizeof(DROPFILES) + allocLen * sizeof(PRUnichar));
  if (!hGlobalMemory)
    return E_FAIL;

  DROPFILES* pDropFile = (DROPFILES*)GlobalLock(hGlobalMemory);

  // First, populate the drop file structure.
  pDropFile->pFiles = sizeof(DROPFILES); // Offset to start of file name char array.
  pDropFile->fNC    = 0;
  pDropFile->pt.x   = 0;
  pDropFile->pt.y   = 0;
  pDropFile->fWide  = TRUE;

  // Copy the filename right after the DROPFILES structure.
  PRUnichar* dest = (PRUnichar*)(((char*)pDropFile) + pDropFile->pFiles);
  memcpy(dest, path.get(), (allocLen - 1) * sizeof(PRUnichar)); // Copies the null character in path as well.

  // Two null characters are needed at the end of the file name.  
  // Lookup the CF_HDROP shell clipboard format for more info.
  // Add the second null character right after the first one.
  dest[allocLen - 1] = L'\0';

  GlobalUnlock(hGlobalMemory);

  aSTG.hGlobal = hGlobalMemory;

  return S_OK;
}

HRESULT nsDataObj::DropTempFile(FORMATETC& aFE, STGMEDIUM& aSTG)
{
  nsresult rv;
  if (!mCachedTempFile) {
    // Tempfile will need a temporary location.      
    nsCOMPtr<nsIFile> dropFile;
    rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(dropFile));
    if (!dropFile)
      return E_FAIL;

    // Filename must be random
    nsCString filename;
    nsAutoString wideFileName;
    nsCOMPtr<nsIURI> sourceURI;
    rv = GetDownloadDetails(getter_AddRefs(sourceURI),
      wideFileName);
    if (NS_FAILED(rv))
      return E_FAIL;
    NS_UTF16ToCString(wideFileName, NS_CSTRING_ENCODING_NATIVE_FILESYSTEM, filename);

    dropFile->AppendNative(filename);
    rv = dropFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0660);
    if (NS_FAILED(rv))
      return E_FAIL;

    // Cache the temp file so we can delete it later and so
    // it doesn't get recreated over and over on multiple calls
    // which does occur from windows shell.
    dropFile->Clone(getter_AddRefs(mCachedTempFile));

    // Write the data to disk.
    nsCOMPtr<nsIOutputStream> outStream;
    rv = NS_NewLocalFileOutputStream(getter_AddRefs(outStream), dropFile);
    if (NS_FAILED(rv))
      return E_FAIL;

    IStream *pStream = NULL;
    nsDataObj::CreateStream(&pStream);
    NS_ENSURE_TRUE(pStream, E_FAIL);

    char buffer[512];
    ULONG readCount = 0;
    PRUint32 writeCount = 0;
    while (1) {
      rv = pStream->Read(buffer, sizeof(buffer), &readCount);
      if (NS_FAILED(rv))
        return E_FAIL;
      if (readCount == 0)
        break;
      rv = outStream->Write(buffer, readCount, &writeCount);
      if (NS_FAILED(rv))
        return E_FAIL;
    }
    outStream->Close();
    pStream->Release();
  }

  // Pass the file name back to the drop target so that it can access the file.
  nsAutoString path;
  rv = mCachedTempFile->GetPath(path);
  if (NS_FAILED(rv))
    return E_FAIL;

  PRUint32 allocLen = path.Length() + 2;

  // Two null characters are needed to terminate the file name list.
  HGLOBAL hGlobalMemory = NULL;

  aSTG.tymed = TYMED_HGLOBAL;
  aSTG.pUnkForRelease = NULL;

  hGlobalMemory = GlobalAlloc(GMEM_MOVEABLE, sizeof(DROPFILES) + allocLen * sizeof(PRUnichar));
  if (!hGlobalMemory)
    return E_FAIL;

  DROPFILES* pDropFile = (DROPFILES*)GlobalLock(hGlobalMemory);

  // First, populate the drop file structure.
  pDropFile->pFiles = sizeof(DROPFILES); // Offset to start of file name char array.
  pDropFile->fNC    = 0;
  pDropFile->pt.x   = 0;
  pDropFile->pt.y   = 0;
  pDropFile->fWide  = TRUE;

  // Copy the filename right after the DROPFILES structure.
  PRUnichar* dest = (PRUnichar*)(((char*)pDropFile) + pDropFile->pFiles);
  memcpy(dest, path.get(), (allocLen - 1) * sizeof(PRUnichar)); // Copies the null character in path as well.

  // Two null characters are needed at the end of the file name.  
  // Lookup the CF_HDROP shell clipboard format for more info.
  // Add the second null character right after the first one.
  dest[allocLen - 1] = L'\0';

  GlobalUnlock(hGlobalMemory);

  aSTG.hGlobal = hGlobalMemory;

  return S_OK;
}

//-----------------------------------------------------
HRESULT nsDataObj::GetMetafilePict(FORMATETC&, STGMEDIUM&)
{
	return E_NOTIMPL;
}

//-----------------------------------------------------
HRESULT nsDataObj::SetBitmap(FORMATETC&, STGMEDIUM&)
{
	return E_NOTIMPL;
}

//-----------------------------------------------------
HRESULT nsDataObj::SetDib(FORMATETC&, STGMEDIUM&)
{
	return E_FAIL;
}

//-----------------------------------------------------
HRESULT nsDataObj::SetText  (FORMATETC& aFE, STGMEDIUM& aSTG)
{
	return E_FAIL;
}

//-----------------------------------------------------
HRESULT nsDataObj::SetMetafilePict(FORMATETC&, STGMEDIUM&)
{
	return E_FAIL;
}



//-----------------------------------------------------
//-----------------------------------------------------
CLSID nsDataObj::GetClassID() const
{
	return CLSID_nsDataObj;
}

//-----------------------------------------------------
// Registers the DataFlavor/FE pair.
//-----------------------------------------------------
void nsDataObj::AddDataFlavor(const char* aDataFlavor, LPFORMATETC aFE)
{
  // These two lists are the mapping to and from data flavors and FEs.
  // Later, OLE will tell us it needs a certain type of FORMATETC (text, unicode, etc)
  // unicode, etc), so we will look up the data flavor that corresponds to
  // the FE and then ask the transferable for that type of data.
  mDataFlavors.AppendElement(aDataFlavor);
  m_enumFE->AddFormatEtc(aFE);
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
  NS_ASSERTION ( mTransferable, "We don't have a good transferable" );
  nsresult rv = NS_ERROR_FAILURE;
  
  PRUint32 len = 0;
  nsCOMPtr<nsISupports> genericURL;
  if ( NS_SUCCEEDED(mTransferable->GetTransferData(kURLMime, getter_AddRefs(genericURL), &len)) ) {
    nsCOMPtr<nsISupportsString> urlObject ( do_QueryInterface(genericURL) );
    if ( urlObject ) {
      nsAutoString url;
      urlObject->GetData ( url );
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
  } else if ( NS_SUCCEEDED(mTransferable->GetTransferData(kURLDataMime, getter_AddRefs(genericURL), &len)) ||
              NS_SUCCEEDED(mTransferable->GetTransferData(kURLPrivateMime, getter_AddRefs(genericURL), &len)) ) {
    nsCOMPtr<nsISupportsString> urlObject ( do_QueryInterface(genericURL) );
    if ( urlObject ) {
      nsAutoString url;
      urlObject->GetData ( url );
      outURL = url;

      rv = NS_OK;    
    }

  }  // if found flavor
  
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
    nsCOMPtr<nsISupportsString> urlObject ( do_QueryInterface(genericURL) );
    if ( urlObject ) {
      nsAutoString url;
      urlObject->GetData ( url );

      // find the first linefeed in the data, that's where the url ends. we want
      // everything after that linefeed. FindChar() returns -1 if we can't find
      PRInt32 lineIndex = url.FindChar ( '\n' );
      NS_ASSERTION ( lineIndex != -1, "Format for url flavor is <url> <linefeed> <page title>" );
      if ( lineIndex != -1 ) {
        url.Mid ( outTitle, lineIndex + 1, (len/2) - (lineIndex + 1) );
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
nsresult 
nsDataObj :: BuildPlatformHTML ( const char* inOurHTML, char** outPlatformHTML ) 
{
  *outPlatformHTML = nsnull;

  nsDependentCString inHTMLString(inOurHTML);
  const char* const numPlaceholder  = "00000000";
  const char* const startHTMLPrefix = "Version:0.9\r\nStartHTML:";
  const char* const endHTMLPrefix   = "\r\nEndHTML:";
  const char* const startFragPrefix = "\r\nStartFragment:";
  const char* const endFragPrefix   = "\r\nEndFragment:";
  const char* const startSourceURLPrefix = "\r\nSourceURL:";
  const char* const endFragTrailer  = "\r\n";

  // Do we already have mSourceURL from a drag?
  if (mSourceURL.IsEmpty()) {
    nsAutoString url;
    ExtractShortcutURL(url);

    AppendUTF16toUTF8(url, mSourceURL);
  }

  const PRInt32 kSourceURLLength    = mSourceURL.Length();
  const PRInt32 kNumberLength       = strlen(numPlaceholder);

  const PRInt32 kTotalHeaderLen     = strlen(startHTMLPrefix) +
                                      strlen(endHTMLPrefix) +
                                      strlen(startFragPrefix) + 
                                      strlen(endFragPrefix) + 
                                      strlen(endFragTrailer) +
                                      (kSourceURLLength > 0 ? strlen(startSourceURLPrefix) : 0) +
                                      kSourceURLLength +
                                      (4 * kNumberLength);

  NS_NAMED_LITERAL_CSTRING(htmlHeaderString, "<html><body>\r\n");

  NS_NAMED_LITERAL_CSTRING(fragmentHeaderString, "<!--StartFragment-->");

  nsDependentCString trailingString(
      "<!--EndFragment-->\r\n"
      "</body>\r\n"
      "</html>");

  // calculate the offsets
  PRInt32 startHTMLOffset = kTotalHeaderLen;
  PRInt32 startFragOffset = startHTMLOffset
                              + htmlHeaderString.Length()
			      + fragmentHeaderString.Length();

  PRInt32 endFragOffset   = startFragOffset
                              + inHTMLString.Length();

  PRInt32 endHTMLOffset   = endFragOffset
                              + trailingString.Length();

  // now build the final version
  nsCString clipboardString;
  clipboardString.SetCapacity(endHTMLOffset);

  clipboardString.Append(startHTMLPrefix);
  clipboardString.Append(nsPrintfCString("%08u", startHTMLOffset));

  clipboardString.Append(endHTMLPrefix);  
  clipboardString.Append(nsPrintfCString("%08u", endHTMLOffset));

  clipboardString.Append(startFragPrefix);
  clipboardString.Append(nsPrintfCString("%08u", startFragOffset));

  clipboardString.Append(endFragPrefix);
  clipboardString.Append(nsPrintfCString("%08u", endFragOffset));

  if (kSourceURLLength > 0) {
    clipboardString.Append(startSourceURLPrefix);
    clipboardString.Append(mSourceURL);
  }

  clipboardString.Append(endFragTrailer);

  clipboardString.Append(htmlHeaderString);
  clipboardString.Append(fragmentHeaderString);
  clipboardString.Append(inHTMLString);
  clipboardString.Append(trailingString);

  *outPlatformHTML = ToNewCString(clipboardString);
  if (!*outPlatformHTML)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

HRESULT 
nsDataObj :: GetUniformResourceLocator( FORMATETC& aFE, STGMEDIUM& aSTG, bool aIsUnicode )
{
  HRESULT res = S_OK;
  if (IsFlavourPresent(kURLMime)) {
    if ( aIsUnicode )
      res = ExtractUniformResourceLocatorW( aFE, aSTG );
    else
      res = ExtractUniformResourceLocatorA( aFE, aSTG );
  }
  else
    NS_WARNING ("Not yet implemented\n");
  return res;
}

HRESULT
nsDataObj::ExtractUniformResourceLocatorA(FORMATETC& aFE, STGMEDIUM& aSTG )
{
  HRESULT result = S_OK;

  nsAutoString url;
  if (NS_FAILED(ExtractShortcutURL(url)))
    return E_OUTOFMEMORY;

  NS_LossyConvertUTF16toASCII asciiUrl(url);
  const int totalLen = asciiUrl.Length() + 1;
  HGLOBAL hGlobalMemory = GlobalAlloc(GMEM_ZEROINIT|GMEM_SHARE, totalLen);
  if (!hGlobalMemory)
    return E_OUTOFMEMORY;

  char* contents = reinterpret_cast<char*>(GlobalLock(hGlobalMemory));
  if (!contents) {
    GlobalFree(hGlobalMemory);
    return E_OUTOFMEMORY;
  }

  strcpy(contents, asciiUrl.get());
  GlobalUnlock(hGlobalMemory);
  aSTG.hGlobal = hGlobalMemory;
  aSTG.tymed = TYMED_HGLOBAL;

  return result;
}

HRESULT
nsDataObj::ExtractUniformResourceLocatorW(FORMATETC& aFE, STGMEDIUM& aSTG )
{
  HRESULT result = S_OK;

  nsAutoString url;
  if (NS_FAILED(ExtractShortcutURL(url)))
    return E_OUTOFMEMORY;

  const int totalLen = (url.Length() + 1) * sizeof(PRUnichar);
  HGLOBAL hGlobalMemory = GlobalAlloc(GMEM_ZEROINIT|GMEM_SHARE, totalLen);
  if (!hGlobalMemory)
    return E_OUTOFMEMORY;

  wchar_t* contents = reinterpret_cast<wchar_t*>(GlobalLock(hGlobalMemory));
  if (!contents) {
    GlobalFree(hGlobalMemory);
    return E_OUTOFMEMORY;
  }

  wcscpy(contents, url.get());
  GlobalUnlock(hGlobalMemory);
  aSTG.hGlobal = hGlobalMemory;
  aSTG.tymed = TYMED_HGLOBAL;

  return result;
}


// Gets the filename from the kFilePromiseURLMime flavour
nsresult nsDataObj::GetDownloadDetails(nsIURI **aSourceURI,
                                       nsAString &aFilename)
{
  *aSourceURI = nsnull;

  NS_ENSURE_TRUE(mTransferable, NS_ERROR_FAILURE);

  // get the URI from the kFilePromiseURLMime flavor
  nsCOMPtr<nsISupports> urlPrimitive;
  PRUint32 dataSize = 0;
  mTransferable->GetTransferData(kFilePromiseURLMime, getter_AddRefs(urlPrimitive), &dataSize);
  nsCOMPtr<nsISupportsString> srcUrlPrimitive = do_QueryInterface(urlPrimitive);
  NS_ENSURE_TRUE(srcUrlPrimitive, NS_ERROR_FAILURE);
  
  nsAutoString srcUri;
  srcUrlPrimitive->GetData(srcUri);
  if (srcUri.IsEmpty())
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIURI> sourceURI;
  NS_NewURI(getter_AddRefs(sourceURI), srcUri);

  nsAutoString srcFileName;
  nsCOMPtr<nsISupports> fileNamePrimitive;
  mTransferable->GetTransferData(kFilePromiseDestFilename, getter_AddRefs(fileNamePrimitive), &dataSize);
  nsCOMPtr<nsISupportsString> srcFileNamePrimitive = do_QueryInterface(fileNamePrimitive);
  if (srcFileNamePrimitive) {
    srcFileNamePrimitive->GetData(srcFileName);
  } else {
    nsCOMPtr<nsIURL> sourceURL = do_QueryInterface(sourceURI);
    if (!sourceURL)
      return NS_ERROR_FAILURE;
    
    nsCAutoString urlFileName;
    sourceURL->GetFileName(urlFileName);
    NS_UnescapeURL(urlFileName);
    CopyUTF8toUTF16(urlFileName, srcFileName);
  }
  if (srcFileName.IsEmpty())
    return NS_ERROR_FAILURE;

  // make the name safe for the filesystem
  MangleTextToValidFilename(srcFileName);

  sourceURI.swap(*aSourceURI);
  aFilename = srcFileName;
  return NS_OK;
}

HRESULT nsDataObj::GetFileDescriptor_IStreamA(FORMATETC& aFE, STGMEDIUM& aSTG)
{
  HGLOBAL fileGroupDescHandle = ::GlobalAlloc(GMEM_ZEROINIT|GMEM_SHARE,sizeof(FILEGROUPDESCRIPTORW));
  NS_ENSURE_TRUE(fileGroupDescHandle, E_OUTOFMEMORY);

  LPFILEGROUPDESCRIPTORA fileGroupDescA = reinterpret_cast<LPFILEGROUPDESCRIPTORA>(GlobalLock(fileGroupDescHandle));
  if (!fileGroupDescA) {
    ::GlobalFree(fileGroupDescHandle);
    return E_OUTOFMEMORY;
  }

  nsAutoString wideFileName;
  nsresult rv;
  nsCOMPtr<nsIURI> sourceURI;
  rv = GetDownloadDetails(getter_AddRefs(sourceURI),
                          wideFileName);
  if (NS_FAILED(rv))
  {
    ::GlobalFree(fileGroupDescHandle);
    return E_FAIL;
  }

  nsCAutoString nativeFileName;
  NS_UTF16ToCString(wideFileName, NS_CSTRING_ENCODING_NATIVE_FILESYSTEM, nativeFileName);
  
  strncpy(fileGroupDescA->fgd[0].cFileName, nativeFileName.get(), NS_MAX_FILEDESCRIPTOR - 1);
  fileGroupDescA->fgd[0].cFileName[NS_MAX_FILEDESCRIPTOR - 1] = '\0';

  // one file in the file block
  fileGroupDescA->cItems = 1;
  fileGroupDescA->fgd[0].dwFlags = FD_PROGRESSUI;

  GlobalUnlock( fileGroupDescHandle );
  aSTG.hGlobal = fileGroupDescHandle;
  aSTG.tymed = TYMED_HGLOBAL;

  return S_OK;
}

HRESULT nsDataObj::GetFileDescriptor_IStreamW(FORMATETC& aFE, STGMEDIUM& aSTG)
{
  HGLOBAL fileGroupDescHandle = ::GlobalAlloc(GMEM_ZEROINIT|GMEM_SHARE,sizeof(FILEGROUPDESCRIPTORW));
  NS_ENSURE_TRUE(fileGroupDescHandle, E_OUTOFMEMORY);

  LPFILEGROUPDESCRIPTORW fileGroupDescW = reinterpret_cast<LPFILEGROUPDESCRIPTORW>(GlobalLock(fileGroupDescHandle));
  if (!fileGroupDescW) {
    ::GlobalFree(fileGroupDescHandle);
    return E_OUTOFMEMORY;
  }

  nsAutoString wideFileName;
  nsresult rv;
  nsCOMPtr<nsIURI> sourceURI;
  rv = GetDownloadDetails(getter_AddRefs(sourceURI),
                          wideFileName);
  if (NS_FAILED(rv))
  {
    ::GlobalFree(fileGroupDescHandle);
    return E_FAIL;
  }

  wcsncpy(fileGroupDescW->fgd[0].cFileName, wideFileName.get(), NS_MAX_FILEDESCRIPTOR - 1);
  fileGroupDescW->fgd[0].cFileName[NS_MAX_FILEDESCRIPTOR - 1] = '\0';
  // one file in the file block
  fileGroupDescW->cItems = 1;
  fileGroupDescW->fgd[0].dwFlags = FD_PROGRESSUI;

  GlobalUnlock(fileGroupDescHandle);
  aSTG.hGlobal = fileGroupDescHandle;
  aSTG.tymed = TYMED_HGLOBAL;

  return S_OK;
}

HRESULT nsDataObj::GetFileContents_IStream(FORMATETC& aFE, STGMEDIUM& aSTG)
{
  IStream *pStream = NULL;

  nsDataObj::CreateStream(&pStream);
  NS_ENSURE_TRUE(pStream, E_FAIL);

  aSTG.tymed = TYMED_ISTREAM;
  aSTG.pstm = pStream;
  aSTG.pUnkForRelease = NULL;

  return S_OK;
}

void nsDataObj::RemoveTempFile(nsITimer* aTimer, void* aClosure)
{
  nsDataObj *timedDataObj = static_cast<nsDataObj *>(aClosure);
  if (timedDataObj->mCachedTempFile) {
    timedDataObj->mCachedTempFile->Remove(false);
    timedDataObj->mCachedTempFile = NULL;
  }
  timedDataObj->Release();
}
