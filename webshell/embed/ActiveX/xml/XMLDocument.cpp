// XMLDocument.cpp : Implementation of CXMLDocument
#include "stdafx.h"
#include "Activexml.h"
#include "XMLDocument.h"


CXMLDocument::CXMLDocument()
{
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
	return E_NOTIMPL;
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
	return E_NOTIMPL;
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

//	pimkName->BindToStorage(pibc, NULL, iid, pObj)
	return E_NOTIMPL;
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
	return E_NOTIMPL;
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
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLDocument::put_URL(/* [in] */ BSTR p)
{
	return E_NOTIMPL;
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
	
	return pInstance->QueryInterface(IID_IXMLElement, (void **) ppElem);
}


