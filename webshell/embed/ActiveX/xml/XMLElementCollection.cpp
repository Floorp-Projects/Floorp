// XMLElementCollection.cpp : Implementation of CXMLElementCollection
#include "stdafx.h"
//#include "Activexml.h"
#include "XMLElementCollection.h"

CXMLElementCollection::CXMLElementCollection()
{
}


CXMLElementCollection::~CXMLElementCollection()
{
}


HRESULT CXMLElementCollection::Add(IXMLElement *pElement)
{
	if (pElement == NULL)
	{
		return E_INVALIDARG;
	}

	m_cElements.push_back( CComQIPtr<IXMLElement, &IID_IXMLElement>(pElement));
	return S_OK;
}


/////////////////////////////////////////////////////////////////////////////
// CXMLElementCollection

HRESULT STDMETHODCALLTYPE CXMLElementCollection::put_length(/* [in] */ long v)
{
	// Why does MS define a method that has no purpose?
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLElementCollection::get_length(/* [out][retval] */ long __RPC_FAR *p)
{
	if (p == NULL)
	{
		return E_INVALIDARG;
	}
	*p = m_cElements.size();
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CXMLElementCollection::get__newEnum(/* [out][retval] */ IUnknown __RPC_FAR *__RPC_FAR *ppUnk)
{
	return E_NOTIMPL;
}


// Perhaps the most overly complicated method ever...
HRESULT STDMETHODCALLTYPE CXMLElementCollection::item(/* [in][optional] */ VARIANT var1, /* [in][optional] */ VARIANT var2, /* [out][retval] */ IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
	if (ppDisp == NULL)
	{
		return E_INVALIDARG;
	}

	*ppDisp;

	CComVariant vIndex;

	// If var1 is a number, the caller wants the element at the specified index
	
	if (vIndex.ChangeType(VT_I4, &var1) == S_OK)
	{
		long nIndex = vIndex.intVal;
		if (nIndex < 0 || nIndex >= m_cElements.size())
		{
			return E_INVALIDARG;
		}
		// Get the element at the specified index
		m_cElements[nIndex]->QueryInterface(IID_IDispatch, (void **) ppDisp);
		return S_OK;
	}

	// If var1 is a string, the caller wants a collection of all elements with
	// the matching tagname, unless var2 contains an index or if there is only
	// one in which case just the element is returned.
	
	CComVariant vName;
	if (FAILED(vName.ChangeType(VT_BSTR, &var1)))
	{
		return E_INVALIDARG;
	}

	// Compile a list of elements matching the name
	ElementList cElements;
	ElementList::iterator i;

	for (i = m_cElements.begin(); i != m_cElements.end(); i++)
	{
		CComQIPtr<IXMLElement, &IID_IXMLElement> spElement;
		BSTR bstrTagName = NULL;
		(*i)->get_tagName(&bstrTagName);
		if (bstrTagName)
		{
			if (wcscmp(bstrTagName, vName.bstrVal) == 0)
			{
				cElements.push_back(*i);
			}
			SysFreeString(bstrTagName);
		}
	}

	// Are there any matching elements?
	if (cElements.empty())
	{
		return S_OK;
	}

	// Does var2 contain an index?
	if (var2.vt == VT_I4)
	{
		long nIndex = var2.vt;
		if (nIndex < 0 || nIndex >= cElements.size())
		{
			return E_INVALIDARG;
		}

		// Get the element at the specified index
		cElements[nIndex]->QueryInterface(IID_IDispatch, (void **) ppDisp);
		return S_OK;
	}

	// Is there more than one element?
	if (cElements.size() > 1)
	{
		// Create another collection
		CXMLElementCollectionInstance *pCollection = NULL;
		CXMLElementCollectionInstance::CreateInstance(&pCollection);
		if (pCollection == NULL)
		{
			return E_OUTOFMEMORY;
		}

		if (FAILED(pCollection->QueryInterface(IID_IDispatch, (void **) ppDisp)))
		{
			pCollection->Release();
			return E_FAIL;
		}

		// Add elements to the collection
		for (i = cElements.begin(); i != cElements.end(); i++)
		{
			pCollection->Add(*i);
		}

		return S_OK;
	}

	// Return the pointer to the element
	if (FAILED(cElements[0]->QueryInterface(IID_IDispatch, (void **) ppDisp)))
	{
		return E_FAIL;
	}

	return S_OK;
}

