/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "stdafx.h"
#include "IEHtmlElement.h"
#include "IEHtmlElementCollection.h"

CIEHtmlElementCollection::CIEHtmlElementCollection()
{
}

CIEHtmlElementCollection::~CIEHtmlElementCollection()
{
}


HRESULT CIEHtmlElementCollection::CreateFromParentNode(nsIDOMNode *pParentNode, CIEHtmlElementCollection **pInstance)
{
	if (pInstance == NULL)
	{
		return E_INVALIDARG;
	}
	
	if (pParentNode == nsnull)
	{
		return E_INVALIDARG;
	}

	*pInstance = NULL;

	CIEHtmlElementCollectionInstance *pCollection = NULL;
	CIEHtmlElementCollectionInstance::CreateInstance(&pCollection);

	// Get elements
	nsIDOMNodeList *pIDOMNodeList = nsnull;
	pParentNode->GetChildNodes(&pIDOMNodeList);
	if (pIDOMNodeList)
	{
		PRUint32 aLength = 0;
		pIDOMNodeList->GetLength(&aLength);
		for (PRUint32 i = 0; i < aLength; i++)
		{
			nsIDOMNode *pChildNode = nsnull;
			pIDOMNodeList->Item(i, &pChildNode);

			// Create an equivalent IE element
			CIEHtmlElementInstance *pElement = NULL;
			CIEHtmlElementInstance::CreateInstance(&pElement);
			if (pElement)
			{
				pElement->SetDOMNode(pChildNode);
				pCollection->AddNode(pElement);
			}

			if (pChildNode)
			{
				pChildNode->Release();
			}
		}
		pIDOMNodeList->Release();
	}

	*pInstance = pCollection;

	return S_OK;
}


HRESULT CIEHtmlElementCollection::AddNode(IDispatch *pNode)
{
	if (pNode == NULL)
	{
		return E_INVALIDARG;
	}
	m_cNodeList.push_back(pNode);

	return S_OK;
}


///////////////////////////////////////////////////////////////////////////////
// IHTMLElementCollection methods


HRESULT STDMETHODCALLTYPE CIEHtmlElementCollection::toString(BSTR __RPC_FAR *String)
{
	if (String == NULL)
	{
		return E_INVALIDARG;
	}
	*String = SysAllocString(OLESTR("Element collection"));
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlElementCollection::put_length(long v)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlElementCollection::get_length(long __RPC_FAR *p)
{
	if (p == NULL)
	{
		return E_INVALIDARG;
	}
	*p = m_cNodeList.size();
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlElementCollection::get__newEnum(IUnknown __RPC_FAR *__RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlElementCollection::item(VARIANT name, VARIANT index, IDispatch __RPC_FAR *__RPC_FAR *pdisp)
{
	CComVariant vIndex;
	if (FAILED(vIndex.ChangeType(VT_I4, &index)))
	{
		return E_INVALIDARG;
	}

	int nIndex = vIndex.lVal;
	if (nIndex < 0 || nIndex >= m_cNodeList.size())
	{
		return E_INVALIDARG;
	}

	*pdisp = NULL;

	IDispatch *pNode = m_cNodeList[nIndex];
	if (pNode == NULL)
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	pNode->QueryInterface(IID_IDispatch, (void **) pdisp);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElementCollection::tags(VARIANT tagName, IDispatch __RPC_FAR *__RPC_FAR *pdisp)
{
	*pdisp = NULL;
	return E_NOTIMPL;
}

