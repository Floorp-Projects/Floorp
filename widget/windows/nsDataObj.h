/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSDATAOBJ_H_
#define _NSDATAOBJ_H_

#include <oleidl.h>
#include <shldisp.h>

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsIURI.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"
#include "nsIChannel.h"
#include "nsCOMArray.h"
#include "nsITimer.h"

class nsIThread;
class nsIPrincipal;
class CEnumFormatEtc;
class nsITransferable;

/*
 * This ole registered class is used to facilitate drag-drop of objects which
 * can be adapted by an object derived from CfDragDrop. The CfDragDrop is
 * associated with instances via SetDragDrop().
 */
class nsDataObj : public IDataObject, public IDataObjectAsyncCapability {
  nsCOMPtr<nsIThread> mIOThread;

 public:  // construction, destruction
  explicit nsDataObj(nsIURI* uri = nullptr);

 protected:
  virtual ~nsDataObj();

 public:  // IUnknown methods - see iunknown.h for documentation
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP QueryInterface(REFIID, void**) override;
  STDMETHODIMP_(ULONG) Release() override;

  // support for clipboard
  virtual void AddDataFlavor(const char* aDataFlavor, LPFORMATETC aFE);
  void SetTransferable(nsITransferable* aTransferable);

 public:  // IDataObject methods - these are general comments. see CfDragDrop
          // for overriding behavior
  // Store data in pSTM according to the format specified by pFE, if the
  // format is supported (supported formats are specified in CfDragDrop::
  // GetFormats) and return NOERROR; otherwise return DATA_E_FORMATETC. It
  // is the callers responsibility to free pSTM if NOERROR is returned.
  STDMETHODIMP GetData(LPFORMATETC pFE, LPSTGMEDIUM pSTM) override;

  // Similar to GetData except that the caller allocates the structure
  // referenced by pSTM.
  STDMETHODIMP GetDataHere(LPFORMATETC pFE, LPSTGMEDIUM pSTM) override;

  // Returns S_TRUE if this object supports the format specified by pSTM,
  // S_FALSE otherwise.
  STDMETHODIMP QueryGetData(LPFORMATETC pFE) override;

  // Set pCanonFE to the canonical format of pFE if one exists and return
  // NOERROR, otherwise return DATA_S_SAMEFORMATETC. A canonical format
  // implies an identical rendering.
  STDMETHODIMP GetCanonicalFormatEtc(LPFORMATETC pFE,
                                     LPFORMATETC pCanonFE) final;

  // Set this objects data according to the format specified by pFE and
  // the storage medium specified by pSTM and return NOERROR, if the format
  // is supported. If release is TRUE this object must release the storage
  // associated with pSTM.
  STDMETHODIMP SetData(LPFORMATETC pFE, LPSTGMEDIUM pSTM,
                       BOOL release) override;

  // Set ppEnum to an IEnumFORMATETC object which will iterate all of the
  // data formats that this object supports. direction is either DATADIR_GET
  // or DATADIR_SET.
  STDMETHODIMP EnumFormatEtc(DWORD direction, LPENUMFORMATETC* ppEnum) final;

  // Set up an advisory connection to this object based on the format specified
  // by pFE, flags, and the pAdvise. Set pConn to the established advise
  // connection.
  STDMETHODIMP DAdvise(LPFORMATETC pFE, DWORD flags, LPADVISESINK pAdvise,
                       DWORD* pConn) final;

  // Turn off advising of a previous call to DAdvise which set pConn.
  STDMETHODIMP DUnadvise(DWORD pConn) final;

  // Set ppEnum to an IEnumSTATDATA object which will iterate over the
  // existing objects which have established advisory connections to this
  // object.
  STDMETHODIMP EnumDAdvise(LPENUMSTATDATA* ppEnum) final;

  // IDataObjectAsyncCapability methods
  STDMETHODIMP EndOperation(HRESULT hResult, IBindCtx* pbcReserved,
                            DWORD dwEffects) final;
  STDMETHODIMP GetAsyncMode(BOOL* pfIsOpAsync) final;
  STDMETHODIMP InOperation(BOOL* pfInAsyncOp) final;
  STDMETHODIMP SetAsyncMode(BOOL fDoOpAsync) final;
  STDMETHODIMP StartOperation(IBindCtx* pbcReserved) final;

 private:  // other methods
  // Gets the filename from the kFilePromiseURLMime flavour
  HRESULT GetDownloadDetails(nsIURI** aSourceURI, nsAString& aFilename);

  // help determine the kind of drag
  bool IsFlavourPresent(const char* inFlavour);

 protected:
  HRESULT GetFile(FORMATETC& aFE, STGMEDIUM& aSTG);
  HRESULT GetText(const nsACString& aDF, FORMATETC& aFE, STGMEDIUM& aSTG);

 private:
  HRESULT GetDib(const nsACString& inFlavor, FORMATETC&, STGMEDIUM& aSTG);

  HRESULT DropImage(FORMATETC& aFE, STGMEDIUM& aSTG);
  HRESULT DropFile(FORMATETC& aFE, STGMEDIUM& aSTG);
  HRESULT DropTempFile(FORMATETC& aFE, STGMEDIUM& aSTG);

  HRESULT GetUniformResourceLocator(FORMATETC& aFE, STGMEDIUM& aSTG,
                                    bool aIsUnicode);
  HRESULT ExtractUniformResourceLocatorA(FORMATETC& aFE, STGMEDIUM& aSTG);
  HRESULT ExtractUniformResourceLocatorW(FORMATETC& aFE, STGMEDIUM& aSTG);
  HRESULT GetFileDescriptor(FORMATETC& aFE, STGMEDIUM& aSTG, bool aIsUnicode);

 protected:
  HRESULT GetFileContents(FORMATETC& aFE, STGMEDIUM& aSTG);

 private:
  HRESULT GetPreferredDropEffect(FORMATETC& aFE, STGMEDIUM& aSTG);

  // Provide the structures needed for an internet shortcut by the shell
  HRESULT GetFileDescriptorInternetShortcutA(FORMATETC& aFE, STGMEDIUM& aSTG);
  HRESULT GetFileDescriptorInternetShortcutW(FORMATETC& aFE, STGMEDIUM& aSTG);
  HRESULT GetFileContentsInternetShortcut(FORMATETC& aFE, STGMEDIUM& aSTG);

  // IStream implementation
  HRESULT GetFileDescriptor_IStreamA(FORMATETC& aFE, STGMEDIUM& aSTG);
  HRESULT GetFileDescriptor_IStreamW(FORMATETC& aFE, STGMEDIUM& aSTG);
  HRESULT GetFileContents_IStream(FORMATETC& aFE, STGMEDIUM& aSTG);

  nsresult ExtractShortcutURL(nsString& outURL);
  nsresult ExtractShortcutTitle(nsString& outTitle);

  // munge our HTML data to win32's CF_HTML spec. Will null terminate
  nsresult BuildPlatformHTML(const char* inOurHTML, char** outPlatformHTML);

  // Used for the SourceURL part of CF_HTML
  nsCString mSourceURL;

 protected:
  BOOL FormatsMatch(const FORMATETC& source, const FORMATETC& target) const;

  ULONG m_cRef;  // the reference count

 private:
  nsTArray<nsCString> mDataFlavors;

  nsITransferable* mTransferable;  // nsDataObj owns and ref counts
                                   // nsITransferable, the nsITransferable does
                                   // know anything about the nsDataObj

 protected:
  CEnumFormatEtc* m_enumFE;  // Ownership Rules:
                             // nsDataObj owns and ref counts CEnumFormatEtc,

 private:
  nsCOMPtr<nsIFile> mCachedTempFile;

  BOOL mIsAsyncMode;
  BOOL mIsInOperation;
  ///////////////////////////////////////////////////////////////////////////////
  // CStream class implementation
  // this class is used in Drag and drop with download sample
  // called from IDataObject::GetData
  class CStream final : public IStream, public nsIStreamListener {
    nsCOMPtr<nsIChannel> mChannel;
    FallibleTArray<uint8_t> mChannelData;
    bool mChannelRead;
    nsresult mChannelResult;
    uint32_t mStreamRead;

    virtual ~CStream();
    nsresult WaitForCompletion();

   public:
    CStream();
    nsresult Init(nsIURI* pSourceURI, uint32_t aContentPolicyType,
                  nsIPrincipal* aRequestingPrincipal);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID refiid, void** ppvResult) override;

    // IStream
    STDMETHODIMP Clone(IStream** ppStream) final;
    STDMETHODIMP Commit(DWORD dwFrags) final;
    STDMETHODIMP CopyTo(IStream* pDestStream, ULARGE_INTEGER nBytesToCopy,
                        ULARGE_INTEGER* nBytesRead,
                        ULARGE_INTEGER* nBytesWritten) final;
    STDMETHODIMP LockRegion(ULARGE_INTEGER nStart, ULARGE_INTEGER nBytes,
                            DWORD dwFlags) final;
    STDMETHODIMP Read(void* pvBuffer, ULONG nBytesToRead,
                      ULONG* nBytesRead) final;
    STDMETHODIMP Revert(void) final;
    STDMETHODIMP Seek(LARGE_INTEGER nMove, DWORD dwOrigin,
                      ULARGE_INTEGER* nNewPos) final;
    STDMETHODIMP SetSize(ULARGE_INTEGER nNewSize) final;
    STDMETHODIMP Stat(STATSTG* statstg, DWORD dwFlags) final;
    STDMETHODIMP UnlockRegion(ULARGE_INTEGER nStart, ULARGE_INTEGER nBytes,
                              DWORD dwFlags) final;
    STDMETHODIMP Write(const void* pvBuffer, ULONG nBytesToRead,
                       ULONG* nBytesRead) final;
  };

  HRESULT CreateStream(IStream** outStream);

 private:
  // Drag and drop helper data for implementing drag and drop image support
  typedef struct {
    FORMATETC fe;
    STGMEDIUM stgm;
  } DATAENTRY, *LPDATAENTRY;

  nsTArray<LPDATAENTRY> mDataEntryList;
  nsCOMPtr<nsITimer> mTimer;

  bool LookupArbitraryFormat(FORMATETC* aFormat, LPDATAENTRY* aDataEntry,
                             BOOL aAddorUpdate);
  bool CopyMediumData(STGMEDIUM* aMediumDst, STGMEDIUM* aMediumSrc,
                      LPFORMATETC aFormat, BOOL aSetData);
};

#endif  // _NSDATAOBJ_H_
