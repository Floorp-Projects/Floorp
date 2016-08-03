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

// The SDK shipping with VC11 has renamed IAsyncOperation to
// IDataObjectAsyncCapability.  We try to detect this, and rename this in our
// code too to make sure that we pick the correct name when building.
#ifdef __IDataObjectAsyncCapability_INTERFACE_DEFINED__
#define IAsyncOperation IDataObjectAsyncCapability
#define IID_IAsyncOperation IID_IDataObjectAsyncCapability
#else
// XXX for older version of PSDK where IAsyncOperation and related stuff is not available
// but thisdefine  should be removed when parocles config is updated
#ifndef __IAsyncOperation_INTERFACE_DEFINED__
// IAsyncOperation interface definition
EXTERN_C const IID IID_IAsyncOperation;

MIDL_INTERFACE("3D8B0590-F691-11d2-8EA9-006097DF5BD4")
IAsyncOperation : public IUnknown
{
  virtual HRESULT STDMETHODCALLTYPE SetAsyncMode(BOOL fDoOpAsync) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetAsyncMode(BOOL *pfIsOpAsync) = 0;
  virtual HRESULT STDMETHODCALLTYPE StartOperation(IBindCtx *pbcReserved) = 0;
  virtual HRESULT STDMETHODCALLTYPE InOperation(BOOL *pfInAsyncOp) = 0;
  virtual HRESULT STDMETHODCALLTYPE EndOperation(HRESULT hResult,
                                                 IBindCtx *pbcReserved,
                                                 DWORD dwEffects) = 0;
};
// this is not defined in the old headers for some reason
#ifndef FD_PROGRESSUI
  #define FD_PROGRESSUI 0x4000
#endif

#endif // __IAsyncOperation_INTERFACE_DEFINED__
#endif // __IDataObjectAsyncCapability_INTERFACE_DEFINED__

/* 
 * CFSTR_SHELLURL is deprecated and doesn't have a Unicode version.
 * Therefore we are using CFSTR_INETURL instead of CFSTR_SHELLURL.
 * See http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/programmersguide/shell_basics/shell_basics_programming/transferring/clipboard.asp
 */
#ifndef CFSTR_INETURLA
#define CFSTR_INETURLA    L"UniformResourceLocator"
#endif
#ifndef CFSTR_INETURLW
#define CFSTR_INETURLW    L"UniformResourceLocatorW"
#endif

// For support of MinGW w32api v2.4. 
// When the next version of w32api is released with shlobj.h rev 1.35 
// http://sources.redhat.com/cgi-bin/cvsweb.cgi/src/winsup/w32api/include/shlobj.h?cvsroot=src
// then that can be made the base required version and this code should be removed.
#ifndef CFSTR_FILEDESCRIPTORA
# define CFSTR_FILEDESCRIPTORA   L"FileGroupDescriptor"
#endif
#ifndef CFSTR_FILEDESCRIPTORW
# define CFSTR_FILEDESCRIPTORW   L"FileGroupDescriptorW"
#endif

class CEnumFormatEtc;
class nsITransferable;

/*
 * This ole registered class is used to facilitate drag-drop of objects which
 * can be adapted by an object derived from CfDragDrop. The CfDragDrop is
 * associated with instances via SetDragDrop().
 */
class nsDataObj : public IDataObject,
                  public IAsyncOperation
{

protected:
  nsCOMPtr<nsIThread> mIOThread;

  public: // construction, destruction
    nsDataObj(nsIURI *uri = nullptr);
    virtual ~nsDataObj();

	public: // IUnknown methods - see iunknown.h for documentation
		STDMETHODIMP_(ULONG) AddRef        ();
		STDMETHODIMP 			QueryInterface(REFIID, void**);
		STDMETHODIMP_(ULONG) Release       ();

    // support for clipboard
    virtual void AddDataFlavor(const char* aDataFlavor, LPFORMATETC aFE);
    void SetTransferable(nsITransferable * aTransferable);

		// Return the registered OLE class ID of this object's CfDataObj.
		CLSID GetClassID() const;

	public: // IDataObject methods - these are general comments. see CfDragDrop
			  // for overriding behavior

		// Store data in pSTM according to the format specified by pFE, if the
		// format is supported (supported formats are specified in CfDragDrop::
		// GetFormats) and return NOERROR; otherwise return DATA_E_FORMATETC. It
		// is the callers responsibility to free pSTM if NOERROR is returned.
		STDMETHODIMP GetData	(LPFORMATETC pFE, LPSTGMEDIUM pSTM);

		// Similar to GetData except that the caller allocates the structure
		// referenced by pSTM.
		STDMETHODIMP GetDataHere (LPFORMATETC pFE, LPSTGMEDIUM pSTM);

		// Returns S_TRUE if this object supports the format specified by pSTM,
		// S_FALSE otherwise.
		STDMETHODIMP QueryGetData (LPFORMATETC pFE);

		// Set pCanonFE to the canonical format of pFE if one exists and return
		// NOERROR, otherwise return DATA_S_SAMEFORMATETC. A canonical format
		// implies an identical rendering.
		STDMETHODIMP GetCanonicalFormatEtc (LPFORMATETC pFE, LPFORMATETC pCanonFE);

		// Set this objects data according to the format specified by pFE and
		// the storage medium specified by pSTM and return NOERROR, if the format
		// is supported. If release is TRUE this object must release the storage
		// associated with pSTM.
		STDMETHODIMP SetData	(LPFORMATETC pFE, LPSTGMEDIUM pSTM, BOOL release);

		// Set ppEnum to an IEnumFORMATETC object which will iterate all of the
		// data formats that this object supports. direction is either DATADIR_GET
		// or DATADIR_SET.
		STDMETHODIMP EnumFormatEtc	(DWORD direction, LPENUMFORMATETC* ppEnum);

		// Set up an advisory connection to this object based on the format specified
		// by pFE, flags, and the pAdvise. Set pConn to the established advise
		// connection.
		STDMETHODIMP DAdvise	(LPFORMATETC pFE, DWORD flags, LPADVISESINK pAdvise,
									 DWORD* pConn);

		// Turn off advising of a previous call to DAdvise which set pConn.
		STDMETHODIMP DUnadvise (DWORD pConn);

		// Set ppEnum to an IEnumSTATDATA object which will iterate over the
		// existing objects which have established advisory connections to this
      // object.
		STDMETHODIMP EnumDAdvise (LPENUMSTATDATA *ppEnum);

    // IAsyncOperation methods
    STDMETHOD(EndOperation)(HRESULT hResult, IBindCtx *pbcReserved, DWORD dwEffects);
    STDMETHOD(GetAsyncMode)(BOOL *pfIsOpAsync);
    STDMETHOD(InOperation)(BOOL *pfInAsyncOp);
    STDMETHOD(SetAsyncMode)(BOOL fDoOpAsync);
    STDMETHOD(StartOperation)(IBindCtx *pbcReserved);

	public: // other methods

    // Gets the filename from the kFilePromiseURLMime flavour
    HRESULT GetDownloadDetails(nsIURI **aSourceURI,
                               nsAString &aFilename);

	protected:
    // help determine the kind of drag
    bool IsFlavourPresent(const char *inFlavour);

    virtual HRESULT AddSetFormat(FORMATETC&  FE);
    virtual HRESULT AddGetFormat(FORMATETC&  FE);

    virtual HRESULT GetFile ( FORMATETC& aFE, STGMEDIUM& aSTG );
    virtual HRESULT GetText ( const nsACString& aDF, FORMATETC& aFE, STGMEDIUM & aSTG );
    virtual HRESULT GetDib ( const nsACString& inFlavor, FORMATETC &, STGMEDIUM & aSTG );
    virtual HRESULT GetMetafilePict(FORMATETC&  FE, STGMEDIUM&  STM);
        
    virtual HRESULT DropImage( FORMATETC& aFE, STGMEDIUM& aSTG );
    virtual HRESULT DropFile( FORMATETC& aFE, STGMEDIUM& aSTG );
    virtual HRESULT DropTempFile( FORMATETC& aFE, STGMEDIUM& aSTG );

    virtual HRESULT GetUniformResourceLocator ( FORMATETC& aFE, STGMEDIUM& aSTG, bool aIsUnicode ) ;
    virtual HRESULT ExtractUniformResourceLocatorA ( FORMATETC& aFE, STGMEDIUM& aSTG ) ;
    virtual HRESULT ExtractUniformResourceLocatorW ( FORMATETC& aFE, STGMEDIUM& aSTG ) ;
    virtual HRESULT GetFileDescriptor ( FORMATETC& aFE, STGMEDIUM& aSTG, bool aIsUnicode ) ;
    virtual HRESULT GetFileContents ( FORMATETC& aFE, STGMEDIUM& aSTG ) ;
    virtual HRESULT GetPreferredDropEffect ( FORMATETC& aFE, STGMEDIUM& aSTG );

    virtual HRESULT SetBitmap(FORMATETC&  FE, STGMEDIUM&  STM);
    virtual HRESULT SetDib   (FORMATETC&  FE, STGMEDIUM&  STM);
    virtual HRESULT SetText  (FORMATETC&  FE, STGMEDIUM&  STM);
    virtual HRESULT SetMetafilePict(FORMATETC&  FE, STGMEDIUM&  STM);

      // Provide the structures needed for an internet shortcut by the shell
    virtual HRESULT GetFileDescriptorInternetShortcutA ( FORMATETC& aFE, STGMEDIUM& aSTG ) ;
    virtual HRESULT GetFileDescriptorInternetShortcutW ( FORMATETC& aFE, STGMEDIUM& aSTG ) ;
    virtual HRESULT GetFileContentsInternetShortcut ( FORMATETC& aFE, STGMEDIUM& aSTG ) ;

    // IStream implementation
    virtual HRESULT GetFileDescriptor_IStreamA ( FORMATETC& aFE, STGMEDIUM& aSTG ) ;
    virtual HRESULT GetFileDescriptor_IStreamW ( FORMATETC& aFE, STGMEDIUM& aSTG ) ;
    virtual HRESULT GetFileContents_IStream ( FORMATETC& aFE, STGMEDIUM& aSTG ) ;

    nsresult ExtractShortcutURL ( nsString & outURL ) ;
    nsresult ExtractShortcutTitle ( nsString & outTitle ) ;

      // munge our HTML data to win32's CF_HTML spec. Will null terminate
    nsresult BuildPlatformHTML ( const char* inOurHTML, char** outPlatformHTML ) ;

    // Used for the SourceURL part of CF_HTML
    nsCString mSourceURL;

    BOOL FormatsMatch(const FORMATETC& source, const FORMATETC& target) const;

		ULONG        m_cRef;              // the reference count

    nsTArray<nsCString> mDataFlavors;

    nsITransferable  * mTransferable; // nsDataObj owns and ref counts nsITransferable, 
                                      // the nsITransferable does know anything about the nsDataObj

    CEnumFormatEtc   * m_enumFE;      // Ownership Rules: 
                                      // nsDataObj owns and ref counts CEnumFormatEtc,

    nsCOMPtr<nsIFile> mCachedTempFile;

    BOOL mIsAsyncMode;
    BOOL mIsInOperation;
    ///////////////////////////////////////////////////////////////////////////////
    // CStream class implementation
    // this class is used in Drag and drop with download sample
    // called from IDataObject::GetData
    class CStream : public IStream, public nsIStreamListener
    {
      nsCOMPtr<nsIChannel> mChannel;
      nsTArray<uint8_t> mChannelData;
      bool mChannelRead;
      nsresult mChannelResult;
      uint32_t mStreamRead;

    protected:
      virtual ~CStream();
      nsresult WaitForCompletion();

    public:
      CStream();
      nsresult Init(nsIURI *pSourceURI,
                    uint32_t aContentPolicyType,
                    nsIPrincipal* aRequestingPrincipal);

      NS_DECL_ISUPPORTS
      NS_DECL_NSIREQUESTOBSERVER
      NS_DECL_NSISTREAMLISTENER

      // IUnknown
      STDMETHOD(QueryInterface)(REFIID refiid, void** ppvResult);

      // IStream  
      STDMETHOD(Clone)(IStream** ppStream);
      STDMETHOD(Commit)(DWORD dwFrags);
      STDMETHOD(CopyTo)(IStream* pDestStream, ULARGE_INTEGER nBytesToCopy, ULARGE_INTEGER* nBytesRead, ULARGE_INTEGER* nBytesWritten);
      STDMETHOD(LockRegion)(ULARGE_INTEGER nStart, ULARGE_INTEGER nBytes, DWORD dwFlags);
      STDMETHOD(Read)(void* pvBuffer, ULONG nBytesToRead, ULONG* nBytesRead);
      STDMETHOD(Revert)(void);
      STDMETHOD(Seek)(LARGE_INTEGER nMove, DWORD dwOrigin, ULARGE_INTEGER* nNewPos);
      STDMETHOD(SetSize)(ULARGE_INTEGER nNewSize);
      STDMETHOD(Stat)(STATSTG* statstg, DWORD dwFlags);
      STDMETHOD(UnlockRegion)(ULARGE_INTEGER nStart, ULARGE_INTEGER nBytes, DWORD dwFlags);
      STDMETHOD(Write)(const void* pvBuffer, ULONG nBytesToRead, ULONG* nBytesRead);
    };

    HRESULT CreateStream(IStream **outStream);

  private:

    // Drag and drop helper data for implementing drag and drop image support
    typedef struct {
      FORMATETC   fe;
      STGMEDIUM   stgm;
    } DATAENTRY, *LPDATAENTRY;

    nsTArray <LPDATAENTRY> mDataEntryList;
    nsCOMPtr<nsITimer> mTimer;

    bool LookupArbitraryFormat(FORMATETC *aFormat, LPDATAENTRY *aDataEntry, BOOL aAddorUpdate);
    bool CopyMediumData(STGMEDIUM *aMediumDst, STGMEDIUM *aMediumSrc, LPFORMATETC aFormat, BOOL aSetData);
    static void RemoveTempFile(nsITimer* aTimer, void* aClosure);
};


#endif  // _NSDATAOBJ_H_
