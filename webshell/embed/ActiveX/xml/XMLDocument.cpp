// XMLDocument.cpp : Implementation of CXMLDocument
#include "stdafx.h"
//#include "Activexml.h"
#include "XMLDocument.h"


CXMLDocument::CXMLDocument()
{
	ATLTRACE(_T("CXMLDocument::CXMLDocument()\n"));
	m_nReadyState = READYSTATE_COMPLETE;
}


CXMLDocument::~CXMLDocument()
{
}


/////////////////////////////////////////////////////////////////////////////
// CXMLDocument


STDMETHODIMP CXMLDocument::InterfaceSupportsErrorInfo(REFIID riid)
{
	static const IID* arr[] = 
	{
		&IID_IXMLDocument
	};
	for (int i=0; i < sizeof(arr) / sizeof(arr[0]); i++)
	{
		if (InlineIsEqualGUID(*arr[i],riid))
			return S_OK;
	}
	return S_FALSE;
}


/////////////////////////////////////////////////////////////////////////////
// IPersistStreamInit implementation


HRESULT STDMETHODCALLTYPE CXMLDocument::Load(/* [in] */ LPSTREAM pStm)
{
	if (pStm == NULL)
	{
		return E_INVALIDARG;
	}

	// Load the XML from the stream
	STATSTG statstg;
	pStm->Stat(&statstg, STATFLAG_NONAME);

	ULONG cbBufSize = statstg.cbSize.LowPart;

	char *pBuffer = new char[cbBufSize];
	if (pBuffer == NULL)
	{
		return E_OUTOFMEMORY;
	}

	memset(pBuffer, 0, cbBufSize);
	pStm->Read(pBuffer, cbBufSize, NULL);

	m_spRoot.Release();
	ParseExpat(pBuffer, cbBufSize, (IXMLDocument *) this, &m_spRoot);

	delete []pBuffer;

	m_nReadyState = READYSTATE_LOADED;

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::Save(/* [in] */ LPSTREAM pStm, /* [in] */ BOOL fClearDirty)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::GetSizeMax(/* [out] */ ULARGE_INTEGER __RPC_FAR *pCbSize)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::InitNew(void)
{
	return S_OK;
}


/////////////////////////////////////////////////////////////////////////////
// IPersistMoniker implementation


HRESULT STDMETHODCALLTYPE CXMLDocument::GetClassID(/* [out] */ CLSID __RPC_FAR *pClassID)
{
	if (pClassID == NULL)
	{
		return E_INVALIDARG;
	}
	*pClassID = CLSID_MozXMLDocument;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::IsDirty(void)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::Load(/* [in] */ BOOL fFullyAvailable, /* [in] */ IMoniker __RPC_FAR *pimkName, /* [in] */ LPBC pibc, /* [in] */ DWORD grfMode)
{
	if (pimkName == NULL)
	{
		return E_INVALIDARG;
	}

	m_nReadyState = READYSTATE_LOADING;

	// Bind to the stream specified by the moniker
	CComQIPtr<IStream, &IID_IStream> spIStream;
	if (FAILED(pimkName->BindToStorage(pibc, NULL, IID_IStream, (void **) &spIStream)))
	{
		return E_FAIL;
	}

	return Load(spIStream);
}


HRESULT STDMETHODCALLTYPE CXMLDocument::Save(/* [in] */ IMoniker __RPC_FAR *pimkName, /* [in] */ LPBC pbc, /* [in] */ BOOL fRemember)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::SaveCompleted(/* [in] */ IMoniker __RPC_FAR *pimkName, /* [in] */ LPBC pibc)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::GetCurMoniker(/* [out] */ IMoniker __RPC_FAR *__RPC_FAR *ppimkName)
{
	return E_NOTIMPL;
}


/////////////////////////////////////////////////////////////////////////////
// IXMLError implementation

HRESULT STDMETHODCALLTYPE CXMLDocument::GetErrorInfo(XML_ERROR __RPC_FAR *pErrorReturn)
{
	return E_NOTIMPL;
}

/////////////////////////////////////////////////////////////////////////////
// IXMLDocument implementation


HRESULT STDMETHODCALLTYPE CXMLDocument::get_root(/* [out][retval] */ IXMLElement __RPC_FAR *__RPC_FAR *p)
{
	if (p == NULL)
	{
		return E_INVALIDARG;
	}
	*p = NULL;
	if (m_spRoot)
	{
		m_spRoot->QueryInterface(IID_IXMLElement, (void **) p);
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::get_fileSize(/* [out][retval] */ BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::get_fileModifiedDate(/* [out][retval] */ BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::get_fileUpdatedDate(/* [out][retval] */ BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::get_URL(/* [out][retval] */ BSTR __RPC_FAR *p)
{
	if (p == NULL)
	{
		return E_INVALIDARG;
	}

	USES_CONVERSION;
	*p = SysAllocString(A2OLE(m_szURL.c_str()));
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::put_URL(/* [in] */ BSTR p)
{
	if (p == NULL)
	{
		return E_INVALIDARG;
	}

	USES_CONVERSION;
	m_szURL = OLE2A(p);

	// Destroy old document
	CComQIPtr<IMoniker, &IID_IMoniker> spIMoniker;
	if (FAILED(CreateURLMoniker(NULL, A2W(m_szURL.c_str()), &spIMoniker)))
	{
		return E_FAIL;
	}

	CComQIPtr<IBindCtx, &IID_IBindCtx> spIBindCtx;
	if (FAILED(CreateBindCtx(0, &spIBindCtx)))
	{
		return E_FAIL;
	}

	if (FAILED(Load(TRUE, spIMoniker, spIBindCtx, 0)))
	{
		return E_FAIL;
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::get_mimeType(/* [out][retval] */ BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::get_readyState(/* [out][retval] */ long __RPC_FAR *pl)
{
	if (pl == NULL)
	{
		return E_INVALIDARG;
	}
	*pl = m_nReadyState;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::get_charset(/* [out][retval] */ BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::put_charset(/* [in] */ BSTR p)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::get_version(/* [out][retval] */ BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::get_doctype(/* [out][retval] */ BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::get_dtdURL(/* [out][retval] */ BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::createElement(/* [in] */ VARIANT vType, /* [in][optional] */ VARIANT var1, /* [out][retval] */ IXMLElement __RPC_FAR *__RPC_FAR *ppElem)
{
	if (vType.vt != VT_I4)
	{
		return E_INVALIDARG;
	}
	if (ppElem == NULL)
	{
		return E_INVALIDARG;
	}

	CXMLElementInstance *pInstance = NULL;
	CXMLElementInstance::CreateInstance(&pInstance);
	if (pInstance == NULL)
	{
		return E_OUTOFMEMORY;
	}

	IXMLElement *pElement = NULL;
	if (FAILED(pInstance->QueryInterface(IID_IXMLElement, (void **) &pElement)))
	{
		pInstance->Release();
		return E_NOINTERFACE;
	}

	// Set the element type
	long nType = vType.intVal;
	pInstance->PutType(nType);

	// Set the tag name
	if (var1.vt == VT_BSTR)
	{
		pInstance->put_tagName(var1.bstrVal);
	}
	
	*ppElem = pElement;
	return S_OK;
}


