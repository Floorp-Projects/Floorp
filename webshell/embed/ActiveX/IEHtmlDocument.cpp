/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL") you may not use this file except in
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
#include "IEHtmlDocument.h"
#include "IEHtmlElementCollection.h"


CIEHtmlDocument::CIEHtmlDocument()
{
}


CIEHtmlDocument::~CIEHtmlDocument()
{
}


HRESULT CIEHtmlDocument::GetIDispatch(IDispatch **pDispatch)
{
	if (pDispatch == NULL)
	{
		return E_INVALIDARG;
	}
	return QueryInterface(IID_IDispatch, (void **) pDispatch);
}


///////////////////////////////////////////////////////////////////////////////
// IHTMLDocument methods

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_Script(IDispatch __RPC_FAR *__RPC_FAR *p)
{
	return E_NOTIMPL;
}

///////////////////////////////////////////////////////////////////////////////
// IHTMLDocument2 methods

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_all(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
	// Validate parameters
	if (p == NULL)
	{
		return E_INVALIDARG;
	}

	*p = NULL;

	// TODO get all elements
	CIEHtmlElementCollectionInstance *pCollection = NULL;
	CIEHtmlElementCollection::CreateFromParentNode(this, (CIEHtmlElementCollection **) &pCollection);
	if (pCollection)
	{
		pCollection->AddRef();
		*p = pCollection;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_body(IHTMLElement __RPC_FAR *__RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_activeElement(IHTMLElement __RPC_FAR *__RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_images(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_applets(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_links(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_forms(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_anchors(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_title(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_title(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_scripts(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_designMode(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_designMode(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_selection(IHTMLSelectionObject __RPC_FAR *__RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_readyState(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_frames(IHTMLFramesCollection2 __RPC_FAR *__RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_embeds(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_plugins(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_alinkColor(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_alinkColor(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_bgColor(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_bgColor(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_fgColor(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_fgColor(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_linkColor(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_linkColor(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_vlinkColor(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_vlinkColor(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_referrer(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_location(IHTMLLocation __RPC_FAR *__RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_lastModified(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_URL(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_URL(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_domain(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_domain(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_cookie(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_cookie(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_expando(VARIANT_BOOL v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_expando(VARIANT_BOOL __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_charset(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_charset(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_defaultCharset(BSTR v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_defaultCharset(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_mimeType(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_fileSize(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_fileCreatedDate(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_fileModifiedDate(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_fileUpdatedDate(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_security(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_protocol(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_nameProp(BSTR __RPC_FAR *p)
{
	*p = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::write(SAFEARRAY __RPC_FAR * psarray)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::writeln(SAFEARRAY __RPC_FAR * psarray)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::open(BSTR url, VARIANT name, VARIANT features, VARIANT replace, IDispatch __RPC_FAR *__RPC_FAR *pomWindowResult)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::close(void)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::clear(void)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::queryCommandSupported(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::queryCommandEnabled(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::queryCommandState(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::queryCommandIndeterm(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::queryCommandText(BSTR cmdID, BSTR __RPC_FAR *pcmdText)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::queryCommandValue(BSTR cmdID, VARIANT __RPC_FAR *pcmdValue)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::execCommand(BSTR cmdID, VARIANT_BOOL showUI, VARIANT value, VARIANT_BOOL __RPC_FAR *pfRet)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::execCommandShowHelp(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::createElement(BSTR eTag, IHTMLElement __RPC_FAR *__RPC_FAR *newElem)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onhelp(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onhelp(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onclick(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onclick(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_ondblclick(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_ondblclick(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onkeyup(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onkeyup(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onkeydown(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onkeydown(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onkeypress(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onkeypress(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onmouseup(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onmouseup(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onmousedown(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onmousedown(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onmousemove(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onmousemove(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onmouseout(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onmouseout(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onmouseover(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onmouseover(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onreadystatechange(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onreadystatechange(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onafterupdate(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onafterupdate(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onrowexit(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onrowexit(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onrowenter(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onrowenter(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_ondragstart(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_ondragstart(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onselectstart(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onselectstart(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::elementFromPoint(long x, long y, IHTMLElement __RPC_FAR *__RPC_FAR *elementHit)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_parentWindow(IHTMLWindow2 __RPC_FAR *__RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_styleSheets(IHTMLStyleSheetsCollection __RPC_FAR *__RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onbeforeupdate(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onbeforeupdate(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onerrorupdate(VARIANT v)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onerrorupdate(VARIANT __RPC_FAR *p)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::toString(BSTR __RPC_FAR *String)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::createStyleSheet(BSTR bstrHref, long lIndex, IHTMLStyleSheet __RPC_FAR *__RPC_FAR *ppnewStyleSheet)
{
	return E_NOTIMPL;
}

