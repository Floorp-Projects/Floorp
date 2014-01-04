/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <shlobj.h>

#include "nsDataObjCollection.h"
#include "nsClipboard.h"
#include "IEnumFE.h"

#include <ole2.h>

// {25589C3E-1FAC-47b9-BF43-CAEA89B79533}
const IID IID_IDataObjCollection =
  {0x25589c3e, 0x1fac, 0x47b9, {0xbf, 0x43, 0xca, 0xea, 0x89, 0xb7, 0x95, 0x33}};

/*
 * Class nsDataObjCollection
 */

nsDataObjCollection::nsDataObjCollection()
  : m_cRef(0), mIsAsyncMode(FALSE), mIsInOperation(FALSE)
{
  m_enumFE = new CEnumFormatEtc();
  m_enumFE->AddRef();
}

nsDataObjCollection::~nsDataObjCollection()
{
  mDataFlavors.Clear();
  mDataObjects.Clear();

  m_enumFE->Release();
}


// IUnknown interface methods - see iunknown.h for documentation
STDMETHODIMP nsDataObjCollection::QueryInterface(REFIID riid, void** ppv)
{
  *ppv=nullptr;

  if ( (IID_IUnknown == riid) || (IID_IDataObject  == riid) ) {
    *ppv = static_cast<IDataObject*>(this); 
    AddRef();
    return NOERROR;
  }

  if ( IID_IDataObjCollection  == riid ) {
    *ppv = static_cast<nsIDataObjCollection*>(this); 
    AddRef();
    return NOERROR;
  }

  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) nsDataObjCollection::AddRef()
{
  return ++m_cRef;
}

STDMETHODIMP_(ULONG) nsDataObjCollection::Release()
{
  if (0 != --m_cRef)
    return m_cRef;

  delete this;

  return 0;
}

BOOL nsDataObjCollection::FormatsMatch(const FORMATETC& source,
                                       const FORMATETC& target) const
{
  if ((source.cfFormat == target.cfFormat) &&
      (source.dwAspect & target.dwAspect)  &&
      (source.tymed    & target.tymed)) {
    return TRUE;
  } else {
    return FALSE;
  }
}

// IDataObject methods
STDMETHODIMP nsDataObjCollection::GetData(LPFORMATETC pFE, LPSTGMEDIUM pSTM)
{
  static CLIPFORMAT fileDescriptorFlavorA =
                               ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORA);
  static CLIPFORMAT fileDescriptorFlavorW =
                               ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW);
  static CLIPFORMAT fileFlavor = ::RegisterClipboardFormat(CFSTR_FILECONTENTS);

  switch (pFE->cfFormat) {
  case CF_TEXT:
  case CF_UNICODETEXT:
    return GetText(pFE, pSTM);
  case CF_HDROP:
    return GetFile(pFE, pSTM);
  default:
    if (pFE->cfFormat == fileDescriptorFlavorA ||
        pFE->cfFormat == fileDescriptorFlavorW) {
      return GetFileDescriptors(pFE, pSTM);
    }
    if (pFE->cfFormat == fileFlavor) {
      return GetFileContents(pFE, pSTM);
    }
  }
  return GetFirstSupporting(pFE, pSTM);
}

STDMETHODIMP nsDataObjCollection::GetDataHere(LPFORMATETC pFE, LPSTGMEDIUM pSTM)
{
  return E_FAIL;
}

// Other objects querying to see if we support a particular format
STDMETHODIMP nsDataObjCollection::QueryGetData(LPFORMATETC pFE)
{
  UINT format = nsClipboard::GetFormat(MULTI_MIME);

  if (format == pFE->cfFormat) {
    return S_OK;
  }

  for (uint32_t i = 0; i < mDataObjects.Length(); ++i) {
    IDataObject * dataObj = mDataObjects.ElementAt(i);
    if (S_OK == dataObj->QueryGetData(pFE)) {
      return S_OK;
    }
  }

  return DV_E_FORMATETC;
}

STDMETHODIMP nsDataObjCollection::GetCanonicalFormatEtc(LPFORMATETC pFEIn,
                                                        LPFORMATETC pFEOut)
{
  return E_NOTIMPL;
}

STDMETHODIMP nsDataObjCollection::SetData(LPFORMATETC pFE,
                                          LPSTGMEDIUM pSTM,
                                          BOOL fRelease)
{
  // Set arbitrary data formats on the first object in the collection and let
  // it handle the heavy lifting
  if (mDataObjects.Length() == 0)
    return E_FAIL;
  return mDataObjects.ElementAt(0)->SetData(pFE, pSTM, fRelease);
}

STDMETHODIMP nsDataObjCollection::EnumFormatEtc(DWORD dwDir,
                                                LPENUMFORMATETC *ppEnum)
{
  if (dwDir == DATADIR_GET) {
    // Clone addref's the new enumerator.
    m_enumFE->Clone(ppEnum);
    if (!(*ppEnum))
      return E_FAIL;
    (*ppEnum)->Reset();
    return S_OK;
  }

  return E_NOTIMPL;
}

STDMETHODIMP nsDataObjCollection::DAdvise(LPFORMATETC pFE,
                                          DWORD dwFlags,
                                          LPADVISESINK pIAdviseSink,
                                          DWORD* pdwConn)
{
  return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP nsDataObjCollection::DUnadvise(DWORD dwConn)
{
  return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP nsDataObjCollection::EnumDAdvise(LPENUMSTATDATA *ppEnum)
{
  return OLE_E_ADVISENOTSUPPORTED;
}

// GetData and SetData helper functions
HRESULT nsDataObjCollection::AddSetFormat(FORMATETC& aFE)
{
  return S_OK;
}

HRESULT nsDataObjCollection::AddGetFormat(FORMATETC& aFE)
{
  return S_OK;
}

// Registers a DataFlavor/FE pair
void nsDataObjCollection::AddDataFlavor(const char * aDataFlavor,
                                        LPFORMATETC aFE)
{
  // Add the FormatEtc to our list if it's not already there.  We don't care
  // about the internal aDataFlavor because nsDataObj handles that.
  IEnumFORMATETC * ifEtc;
  FORMATETC fEtc;
  ULONG num;
  if (S_OK != this->EnumFormatEtc(DATADIR_GET, &ifEtc))
    return;
  while (S_OK == ifEtc->Next(1, &fEtc, &num)) {
    NS_ASSERTION(1 == num,
         "Bit off more than we can chew in nsDataObjCollection::AddDataFlavor");
    if (FormatsMatch(fEtc, *aFE)) {
      ifEtc->Release();
      return;
    }
  } // If we didn't find a matching format, add this one
  ifEtc->Release();
  m_enumFE->AddFormatEtc(aFE);
}

// We accept ownership of the nsDataObj which we free on destruction
void nsDataObjCollection::AddDataObject(IDataObject * aDataObj)
{
  nsDataObj* dataObj = reinterpret_cast<nsDataObj*>(aDataObj);
  mDataObjects.AppendElement(dataObj);
}

// Methods for getting data
HRESULT nsDataObjCollection::GetFile(LPFORMATETC pFE, LPSTGMEDIUM pSTM)
{
  STGMEDIUM workingmedium;
  FORMATETC fe = *pFE;
  HGLOBAL hGlobalMemory;
  HRESULT hr;
  // Make enough space for the header and the trailing null
  uint32_t buffersize = sizeof(DROPFILES) + sizeof(char16_t);
  uint32_t alloclen = 0;
  char16_t* realbuffer;
  nsAutoString filename;
  
  hGlobalMemory = GlobalAlloc(GHND, buffersize);

  for (uint32_t i = 0; i < mDataObjects.Length(); ++i) {
    nsDataObj* dataObj = mDataObjects.ElementAt(i);
    hr = dataObj->GetData(&fe, &workingmedium);
    if (hr != S_OK) {
      switch (hr) {
      case DV_E_FORMATETC:
        continue;
      default:
        return hr;
      }
    }
    // Now we need to pull out the filename
    char16_t* buffer = (char16_t*)GlobalLock(workingmedium.hGlobal);
    if (buffer == nullptr)
      return E_FAIL;
    buffer += sizeof(DROPFILES)/sizeof(char16_t);
    filename = buffer;
    GlobalUnlock(workingmedium.hGlobal);
    ReleaseStgMedium(&workingmedium);
    // Now put the filename into our buffer
    alloclen = (filename.Length() + 1) * sizeof(char16_t);
    hGlobalMemory = ::GlobalReAlloc(hGlobalMemory, buffersize + alloclen, GHND);
    if (hGlobalMemory == nullptr)
      return E_FAIL;
    realbuffer = (char16_t*)((char*)GlobalLock(hGlobalMemory) + buffersize);
    if (!realbuffer)
      return E_FAIL;
    realbuffer--; // Overwrite the preceding null
    memcpy(realbuffer, filename.get(), alloclen);
    GlobalUnlock(hGlobalMemory);
    buffersize += alloclen;
  }
  // We get the last null (on the double null terminator) for free since we used
  // the zero memory flag when we allocated.  All we need to do is fill the
  // DROPFILES structure
  DROPFILES* df = (DROPFILES*)GlobalLock(hGlobalMemory);
  if (!df)
    return E_FAIL;
  df->pFiles = sizeof(DROPFILES); //Offset to start of file name string
  df->fNC    = 0;
  df->pt.x   = 0;
  df->pt.y   = 0;
  df->fWide  = TRUE; // utf-16 chars
  GlobalUnlock(hGlobalMemory);
  // Finally fill out the STGMEDIUM struct
  pSTM->tymed = TYMED_HGLOBAL;
  pSTM->pUnkForRelease = nullptr; // Caller gets to free the data
  pSTM->hGlobal = hGlobalMemory;
  return S_OK;
}

HRESULT nsDataObjCollection::GetText(LPFORMATETC pFE, LPSTGMEDIUM pSTM)
{
  STGMEDIUM workingmedium;
  FORMATETC fe = *pFE;
  HGLOBAL hGlobalMemory;
  HRESULT hr;
  uint32_t buffersize = 1;
  uint32_t alloclen = 0;

  hGlobalMemory = GlobalAlloc(GHND, buffersize);

  if (pFE->cfFormat == CF_TEXT) {
    nsAutoCString text;
    for (uint32_t i = 0; i < mDataObjects.Length(); ++i) {
      nsDataObj* dataObj = mDataObjects.ElementAt(i);
      hr = dataObj->GetData(&fe, &workingmedium);
      if (hr != S_OK) {
        switch (hr) {
        case DV_E_FORMATETC:
          continue;
        default:
          return hr;
        }
      }
      // Now we need to pull out the text
      char* buffer = (char*)GlobalLock(workingmedium.hGlobal);
      if (buffer == nullptr)
        return E_FAIL;
      text = buffer;
      GlobalUnlock(workingmedium.hGlobal);
      ReleaseStgMedium(&workingmedium);
      // Now put the text into our buffer
      alloclen = text.Length();
      hGlobalMemory = ::GlobalReAlloc(hGlobalMemory, buffersize + alloclen,
                                      GHND);
      if (hGlobalMemory == nullptr)
        return E_FAIL;
      buffer = ((char*)GlobalLock(hGlobalMemory) + buffersize);
      if (!buffer)
        return E_FAIL;
      buffer--; // Overwrite the preceding null
      memcpy(buffer, text.get(), alloclen);
      GlobalUnlock(hGlobalMemory);
      buffersize += alloclen;
    }
    pSTM->tymed = TYMED_HGLOBAL;
    pSTM->pUnkForRelease = nullptr; // Caller gets to free the data
    pSTM->hGlobal = hGlobalMemory;
    return S_OK;
  }
  if (pFE->cfFormat == CF_UNICODETEXT) {
    buffersize = sizeof(char16_t);
    nsAutoString text;
    for (uint32_t i = 0; i < mDataObjects.Length(); ++i) {
      nsDataObj* dataObj = mDataObjects.ElementAt(i);
      hr = dataObj->GetData(&fe, &workingmedium);
      if (hr != S_OK) {
        switch (hr) {
        case DV_E_FORMATETC:
          continue;
        default:
          return hr;
        }
      }
      // Now we need to pull out the text
      char16_t* buffer = (char16_t*)GlobalLock(workingmedium.hGlobal);
      if (buffer == nullptr)
        return E_FAIL;
      text = buffer;
      GlobalUnlock(workingmedium.hGlobal);
      ReleaseStgMedium(&workingmedium);
      // Now put the text into our buffer
      alloclen = text.Length() * sizeof(char16_t);
      hGlobalMemory = ::GlobalReAlloc(hGlobalMemory, buffersize + alloclen,
                                      GHND);
      if (hGlobalMemory == nullptr)
        return E_FAIL;
      buffer = (char16_t*)((char*)GlobalLock(hGlobalMemory) + buffersize);
      if (!buffer)
        return E_FAIL;
      buffer--; // Overwrite the preceding null
      memcpy(buffer, text.get(), alloclen);
      GlobalUnlock(hGlobalMemory);
      buffersize += alloclen;
    }
    pSTM->tymed = TYMED_HGLOBAL;
    pSTM->pUnkForRelease = nullptr; // Caller gets to free the data
    pSTM->hGlobal = hGlobalMemory;
    return S_OK;
  }

  return E_FAIL;
}

HRESULT nsDataObjCollection::GetFileDescriptors(LPFORMATETC pFE,
                                                LPSTGMEDIUM pSTM)
{
  STGMEDIUM workingmedium;
  FORMATETC fe = *pFE;
  HGLOBAL hGlobalMemory;
  HRESULT hr;
  uint32_t buffersize = sizeof(FILEGROUPDESCRIPTOR);
  uint32_t alloclen = sizeof(FILEDESCRIPTOR);

  hGlobalMemory = GlobalAlloc(GHND, buffersize);

  for (uint32_t i = 0; i < mDataObjects.Length(); ++i) {
    nsDataObj* dataObj = mDataObjects.ElementAt(i);
    hr = dataObj->GetData(&fe, &workingmedium);
    if (hr != S_OK) {
      switch (hr) {
      case DV_E_FORMATETC:
        continue;
      default:
        return hr;
      }
    }
    // Now we need to pull out the filedescriptor
    FILEDESCRIPTOR* buffer =
     (FILEDESCRIPTOR*)((char*)GlobalLock(workingmedium.hGlobal) + sizeof(UINT));
    if (buffer == nullptr)
      return E_FAIL;
    hGlobalMemory = ::GlobalReAlloc(hGlobalMemory, buffersize + alloclen, GHND);
    if (hGlobalMemory == nullptr)
      return E_FAIL;
    FILEGROUPDESCRIPTOR* realbuffer =
                                (FILEGROUPDESCRIPTOR*)GlobalLock(hGlobalMemory);
    if (!realbuffer)
      return E_FAIL;
    FILEDESCRIPTOR* copyloc = (FILEDESCRIPTOR*)((char*)realbuffer + buffersize);
    memcpy(copyloc, buffer, sizeof(FILEDESCRIPTOR));
    realbuffer->cItems++;
    GlobalUnlock(hGlobalMemory);
    GlobalUnlock(workingmedium.hGlobal);
    ReleaseStgMedium(&workingmedium);
    buffersize += alloclen;
  }
  pSTM->tymed = TYMED_HGLOBAL;
  pSTM->pUnkForRelease = nullptr; // Caller gets to free the data
  pSTM->hGlobal = hGlobalMemory;
  return S_OK;
}

HRESULT nsDataObjCollection::GetFileContents(LPFORMATETC pFE, LPSTGMEDIUM pSTM)
{
  ULONG num = 0;
  ULONG numwanted = (pFE->lindex == -1) ? 0 : pFE->lindex;
  FORMATETC fEtc = *pFE;
  fEtc.lindex = -1;  // We're lying to the data object so it thinks it's alone

  // The key for this data type is to figure out which data object the index
  // corresponds to and then just pass it along
  for (uint32_t i = 0; i < mDataObjects.Length(); ++i) {
    nsDataObj* dataObj = mDataObjects.ElementAt(i);
    if (dataObj->QueryGetData(&fEtc) != S_OK)
      continue;
    if (num == numwanted)
      return dataObj->GetData(pFE, pSTM);
    numwanted++;
  }
  return DV_E_LINDEX;
}

HRESULT nsDataObjCollection::GetFirstSupporting(LPFORMATETC pFE,
                                                LPSTGMEDIUM pSTM)
{
  // There is no way to pass more than one of this, so just find the first data
  // object that supports it and pass it along
  for (uint32_t i = 0; i < mDataObjects.Length(); ++i) {
    if (mDataObjects.ElementAt(i)->QueryGetData(pFE) == S_OK)
      return mDataObjects.ElementAt(i)->GetData(pFE, pSTM);
  }
  return DV_E_FORMATETC;
}
