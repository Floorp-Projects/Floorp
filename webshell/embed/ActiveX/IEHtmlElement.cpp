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

CIEHtmlElement::CIEHtmlElement()
{
}


CIEHtmlElement::~CIEHtmlElement()
{
}


HRESULT CIEHtmlElement::GetIDispatch(IDispatch **pDispatch)
{
	if (pDispatch == NULL)
	{
		return E_INVALIDARG;
	}
	return QueryInterface(IID_IDispatch, (void **) pDispatch);
}

///////////////////////////////////////////////////////////////////////////////
// IHTMLElement implementation

HRESULT STDMETHODCALLTYPE CIEHtmlElement::setAttribute(BSTR strAttributeName, VARIANT AttributeValue, LONG lFlags)
{
	if (strAttributeName == NULL)
	{
		return E_INVALIDARG;
	}

	// Get the name from the BSTR
	USES_CONVERSION;
	nsString szName = OLE2W(strAttributeName);

	// Get the value from the variant
	CComVariant vValue;
	if (FAILED(vValue.ChangeType(VT_BSTR, &AttributeValue)))
	{
		return E_INVALIDARG;
	}
	nsString szValue = OLE2W(vValue.bstrVal);

	nsIDOMElement *pIDOMElement = nsnull;
	if (FAILED(GetDOMElement(&pIDOMElement)))
	{
		return E_UNEXPECTED;
	}

	// Set the attribute
	pIDOMElement->SetAttribute(szName, szValue);
	pIDOMElement->Release();

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::getAttribute(BSTR strAttributeName, LONG lFlags, VARIANT __RPC_FAR *AttributeValue)
{
	if (strAttributeName == NULL)
	{
		return E_INVALIDARG;
	}
	if (AttributeValue == NULL)
	{
		return E_INVALIDARG;
	}
	VariantInit(AttributeValue);

	// Get the name from the BSTR
	USES_CONVERSION;
	nsString szName = OLE2W(strAttributeName);

	nsIDOMElement *pIDOMElement = nsnull;
	if (FAILED(GetDOMElement(&pIDOMElement)))
	{
		return E_UNEXPECTED;
	}

	BOOL bCaseSensitive = (lFlags == VARIANT_TRUE) ? TRUE : FALSE;

	nsString szValue;

	// Get the attribute
	nsresult nr = pIDOMElement->GetAttribute(szName, szValue);
	pIDOMElement->Release();

	if (nr == NS_OK)
	{
		USES_CONVERSION;
		AttributeValue->vt = VT_BSTR;
		AttributeValue->bstrVal = SysAllocString(W2COLE((const PRUnichar *) szValue));
		return S_OK;
	}
	else
	{
		return S_FALSE;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::removeAttribute(BSTR strAttributeName, LONG lFlags, VARIANT_BOOL __RPC_FAR *pfSuccess)
{
	if (strAttributeName == NULL)
	{
		return E_INVALIDARG;
	}

	nsIDOMElement *pIDOMElement = nsnull;
	if (FAILED(GetDOMElement(&pIDOMElement)))
	{
		return E_UNEXPECTED;
	}

	BOOL bCaseSensitive = (lFlags == VARIANT_TRUE) ? TRUE : FALSE;

	// Get the name from the BSTR
	USES_CONVERSION;
	nsString szName = OLE2W(strAttributeName);

	// Remove the attribute
	nsresult nr = pIDOMElement->RemoveAttribute(szName);
	BOOL bRemoved = (nr == NS_OK) ? TRUE : FALSE;
	pIDOMElement->Release();

	if (pfSuccess)
	{
		*pfSuccess = (bRemoved) ? VARIANT_TRUE : VARIANT_FALSE;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_className(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_className(BSTR __RPC_FAR *p)
{
	if (p == NULL)
	{
		return E_INVALIDARG;
	}

	VARIANT vValue;
	VariantInit(&vValue);
	BSTR bstrName = SysAllocString(OLESTR("class"));
	getAttribute(bstrName, FALSE, &vValue);
	SysFreeString(bstrName);

	*p = vValue.bstrVal;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_id(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_id(BSTR __RPC_FAR *p)
{
	if (p == NULL)
	{
		return E_INVALIDARG;
	}

	VARIANT vValue;
	VariantInit(&vValue);
	BSTR bstrName = SysAllocString(OLESTR("id"));
	getAttribute(bstrName, FALSE, &vValue);
	SysFreeString(bstrName);

	*p = vValue.bstrVal;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_tagName(BSTR __RPC_FAR *p)
{
	if (p == NULL)
	{
		return E_INVALIDARG;
	}

	nsIDOMElement *pIDOMElement = nsnull;
	if (FAILED(GetDOMElement(&pIDOMElement)))
	{
		return E_UNEXPECTED;
	}

	nsString szTagName;
	pIDOMElement->GetTagName(szTagName);
	pIDOMElement->Release();

	USES_CONVERSION;
	*p = SysAllocString(W2COLE((const PRUnichar *) szTagName));
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_parentElement(IHTMLElement __RPC_FAR *__RPC_FAR *p)
{
	if (p == NULL)
	{
		return E_INVALIDARG;
	}

	*p = NULL;
	if (m_pIDispParent)
	{
		m_pIDispParent->QueryInterface(IID_IHTMLElement, (void **) p);
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_style(IHTMLStyle __RPC_FAR *__RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onhelp(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onhelp(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onclick(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onclick(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_ondblclick(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_ondblclick(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onkeydown(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onkeydown(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onkeyup(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onkeyup(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onkeypress(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onkeypress(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onmouseout(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onmouseout(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onmouseover(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onmouseover(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onmousemove(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onmousemove(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onmousedown(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onmousedown(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onmouseup(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onmouseup(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_document(IDispatch __RPC_FAR *__RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_title(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_title(BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_language(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_language(BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onselectstart(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onselectstart(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::scrollIntoView(VARIANT varargStart)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::contains(IHTMLElement __RPC_FAR *pChild, VARIANT_BOOL __RPC_FAR *pfResult)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_sourceIndex(long __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_recordNumber(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_lang(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_lang(BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_offsetLeft(long __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_offsetTop(long __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_offsetWidth(long __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_offsetHeight(long __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_offsetParent(IHTMLElement __RPC_FAR *__RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_innerHTML(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_innerHTML(BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_innerText(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_innerText(BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_outerHTML(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_outerHTML(BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_outerText(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_outerText(BSTR __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::insertAdjacentHTML(BSTR where, BSTR html)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::insertAdjacentText(BSTR where, BSTR text)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_parentTextEdit(IHTMLElement __RPC_FAR *__RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_isTextEdit(VARIANT_BOOL __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::click(void)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_filters(IHTMLFiltersCollection __RPC_FAR *__RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_ondragstart(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_ondragstart(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::toString(BSTR __RPC_FAR *String)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onbeforeupdate(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onbeforeupdate(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onafterupdate(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onafterupdate(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onerrorupdate(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onerrorupdate(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onrowexit(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onrowexit(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onrowenter(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onrowenter(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_ondatasetchanged(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_ondatasetchanged(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_ondataavailable(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_ondataavailable(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_ondatasetcomplete(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_ondatasetcomplete(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::put_onfilterchange(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_onfilterchange(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_children(IDispatch __RPC_FAR *__RPC_FAR *p)
{
	// Validate parameters
	if (p == NULL)
	{
		return E_INVALIDARG;
	}

	*p = NULL;

	// Create a collection representing the children of this node
	CIEHtmlElementCollectionInstance *pCollection = NULL;
	CIEHtmlElementCollection::CreateFromParentNode(this, (CIEHtmlElementCollection **) &pCollection);
	if (pCollection)
	{
		pCollection->AddRef();
		*p = pCollection;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElement::get_all(IDispatch __RPC_FAR *__RPC_FAR *p)
{
	// Validate parameters
	if (p == NULL)
	{
		return E_INVALIDARG;
	}

	*p = NULL;

	// TODO get ALL contained elements, not just the immediate children

	CIEHtmlElementCollectionInstance *pCollection = NULL;
	CIEHtmlElementCollection::CreateFromParentNode(this, (CIEHtmlElementCollection **) &pCollection);
	if (pCollection)
	{
		pCollection->AddRef();
		*p = pCollection;
	}

	return S_OK;
}

