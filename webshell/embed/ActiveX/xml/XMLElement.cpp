// XMLElement.cpp : Implementation of CXMLElement
#include "stdafx.h"
#include "Activexml.h"
#include "XMLElement.h"


CXMLElement::CXMLElement()
{
	m_nType = 0;
}


CXMLElement::~CXMLElement()
{
}


HRESULT CXMLElement::SetParent(IXMLElement *pParent)
{
	// Note: parent is not refcounted
	m_pParent = pParent;
	return S_OK;
}

HRESULT CXMLElement::ReleaseAll()
{
	// Release all children
	m_cChildren.clear();
	return S_OK;
}


/////////////////////////////////////////////////////////////////////////////
// CXMLElement

HRESULT STDMETHODCALLTYPE CXMLElement::get_tagName(/* [out][retval] */ BSTR __RPC_FAR *p)
{
	if (p == NULL)
	{
		return E_INVALIDARG;
	}
	USES_CONVERSION;
	*p = SysAllocString(A2OLE(m_szTagName.c_str()));
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLElement::put_tagName(/* [in] */ BSTR p)
{
	if (p == NULL)
	{
		return E_INVALIDARG;
	}
	USES_CONVERSION;
	m_szTagName = OLE2A(p);
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLElement::get_parent(/* [out][retval] */ IXMLElement __RPC_FAR *__RPC_FAR *ppParent)
{
	if (ppParent == NULL)
	{
		return E_INVALIDARG;
	}

	*ppParent = NULL;
	if (m_pParent)
	{
		return m_pParent->QueryInterface(IID_IXMLElement, (void **) ppParent);
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLElement::setAttribute(/* [in] */ BSTR strPropertyName, /* [in] */ VARIANT PropertyValue)
{
	if (strPropertyName == NULL || PropertyValue.vt != VT_BSTR)
	{
		return E_INVALIDARG;
	}

	USES_CONVERSION;
	std::string szPropertyName = OLE2A(strPropertyName);
	std::string szPropertyValue = OLE2A(PropertyValue.bstrVal);
	m_cAttributes[szPropertyName] = szPropertyValue;

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLElement::getAttribute(/* [in] */ BSTR strPropertyName, /* [out][retval] */ VARIANT __RPC_FAR *PropertyValue)
{
	if (strPropertyName == NULL || PropertyValue == NULL)
	{
		return E_INVALIDARG;
	}

	USES_CONVERSION;
	std::string szPropertyName = OLE2A(strPropertyName);
	StringMap::iterator i = m_cAttributes.find(szPropertyName);
	if (i == m_cAttributes.end())
	{
		return S_FALSE;
	}

	PropertyValue->vt = VT_BSTR;
	PropertyValue->bstrVal = SysAllocString(A2OLE((*i).second.c_str()));
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLElement::removeAttribute(/* [in] */ BSTR strPropertyName)
{
	if (strPropertyName == NULL)
	{
		return E_INVALIDARG;
	}

	USES_CONVERSION;
	std::string szPropertyName = OLE2A(strPropertyName);
	StringMap::iterator i = m_cAttributes.find(szPropertyName);
	if (i == m_cAttributes.end())
	{
		return E_INVALIDARG;
	}
	
	m_cAttributes.erase(i);
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLElement::get_children(/* [out][retval] */ IXMLElementCollection __RPC_FAR *__RPC_FAR *pp)
{
	// TODO 
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLElement::get_type(/* [out][retval] */ long __RPC_FAR *plType)
{
	if (plType == NULL)
	{
		return E_INVALIDARG;
	}
	*plType = m_nType;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLElement::get_text(/* [out][retval] */ BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLElement::put_text(/* [in] */ BSTR p)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLElement::addChild(/* [in] */ IXMLElement __RPC_FAR *pChildElem, long lIndex, long lReserved)
{
	if (pChildElem == NULL)
	{
		return E_INVALIDARG;
	}

	//TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CXMLElement::removeChild(/* [in] */ IXMLElement __RPC_FAR *pChildElem)
{
	// TODO
	return E_NOTIMPL;
}

