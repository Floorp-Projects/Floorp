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
#ifndef IEHTMLDOCUMENT_H
#define IEHTMLDOCUMENT_H

class CIEHtmlDocument :	public CComObjectRootEx<CComSingleThreadModel>,
						public IDispatchImpl<IHTMLDocument2, &IID_IHTMLDocument2, &LIBID_MSHTML>
{
public:
	CIEHtmlDocument();
protected:
	virtual ~CIEHtmlDocument();

public:

BEGIN_COM_MAP(CIEHtmlDocument)
	COM_INTERFACE_ENTRY_IID(IID_IDispatch, IHTMLDocument2)
	COM_INTERFACE_ENTRY_IID(IID_IHTMLDocument, IHTMLDocument2)
	COM_INTERFACE_ENTRY_IID(IID_IHTMLDocument2, IHTMLDocument2)
END_COM_MAP()

	// IHTMLDocument methods
	virtual HRESULT STDMETHODCALLTYPE get_Script(IDispatch __RPC_FAR *__RPC_FAR *p);

	// IHTMLDocument2 methods
	virtual  HRESULT STDMETHODCALLTYPE get_all(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_body(IHTMLElement __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_activeElement(IHTMLElement __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_images(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_applets(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_links(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_forms(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_anchors(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_title(BSTR v);
	virtual  HRESULT STDMETHODCALLTYPE get_title(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_scripts(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_designMode(BSTR v);
	virtual  HRESULT STDMETHODCALLTYPE get_designMode(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_selection(IHTMLSelectionObject __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_readyState(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_frames(IHTMLFramesCollection2 __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_embeds(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_plugins(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_alinkColor(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_alinkColor(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_bgColor(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_bgColor(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_fgColor(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_fgColor(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_linkColor(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_linkColor(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_vlinkColor(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_vlinkColor(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_referrer(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_location(IHTMLLocation __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_lastModified(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_URL(BSTR v);
	virtual  HRESULT STDMETHODCALLTYPE get_URL(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_domain(BSTR v);
	virtual  HRESULT STDMETHODCALLTYPE get_domain(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_cookie(BSTR v);
	virtual  HRESULT STDMETHODCALLTYPE get_cookie(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_expando(VARIANT_BOOL v);
	virtual  HRESULT STDMETHODCALLTYPE get_expando(VARIANT_BOOL __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_charset(BSTR v);
	virtual  HRESULT STDMETHODCALLTYPE get_charset(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_defaultCharset(BSTR v);
	virtual  HRESULT STDMETHODCALLTYPE get_defaultCharset(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_mimeType(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_fileSize(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_fileCreatedDate(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_fileModifiedDate(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_fileUpdatedDate(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_security(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_protocol(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_nameProp(BSTR __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE write(SAFEARRAY __RPC_FAR * psarray);
	virtual  HRESULT STDMETHODCALLTYPE writeln(SAFEARRAY __RPC_FAR * psarray);
	virtual  HRESULT STDMETHODCALLTYPE open(BSTR url, VARIANT name, VARIANT features, VARIANT replace, IDispatch __RPC_FAR *__RPC_FAR *pomWindowResult);
	virtual  HRESULT STDMETHODCALLTYPE close(void);
	virtual  HRESULT STDMETHODCALLTYPE clear(void);
	virtual  HRESULT STDMETHODCALLTYPE queryCommandSupported(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
	virtual  HRESULT STDMETHODCALLTYPE queryCommandEnabled(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
	virtual  HRESULT STDMETHODCALLTYPE queryCommandState(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
	virtual  HRESULT STDMETHODCALLTYPE queryCommandIndeterm(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
	virtual  HRESULT STDMETHODCALLTYPE queryCommandText(BSTR cmdID, BSTR __RPC_FAR *pcmdText);
	virtual  HRESULT STDMETHODCALLTYPE queryCommandValue(BSTR cmdID, VARIANT __RPC_FAR *pcmdValue);
	virtual  HRESULT STDMETHODCALLTYPE execCommand(BSTR cmdID, VARIANT_BOOL showUI, VARIANT value, VARIANT_BOOL __RPC_FAR *pfRet);
	virtual  HRESULT STDMETHODCALLTYPE execCommandShowHelp(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
	virtual  HRESULT STDMETHODCALLTYPE createElement(BSTR eTag, IHTMLElement __RPC_FAR *__RPC_FAR *newElem);
	virtual  HRESULT STDMETHODCALLTYPE put_onhelp(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onhelp(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onclick(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onclick(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_ondblclick(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_ondblclick(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onkeyup(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onkeyup(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onkeydown(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onkeydown(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onkeypress(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onkeypress(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onmouseup(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onmouseup(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onmousedown(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onmousedown(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onmousemove(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onmousemove(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onmouseout(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onmouseout(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onmouseover(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onmouseover(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onreadystatechange(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onreadystatechange(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onafterupdate(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onafterupdate(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onrowexit(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onrowexit(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onrowenter(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onrowenter(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_ondragstart(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_ondragstart(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onselectstart(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onselectstart(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE elementFromPoint(long x, long y, IHTMLElement __RPC_FAR *__RPC_FAR *elementHit);
	virtual  HRESULT STDMETHODCALLTYPE get_parentWindow(IHTMLWindow2 __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE get_styleSheets(IHTMLStyleSheetsCollection __RPC_FAR *__RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onbeforeupdate(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onbeforeupdate(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE put_onerrorupdate(VARIANT v);
	virtual  HRESULT STDMETHODCALLTYPE get_onerrorupdate(VARIANT __RPC_FAR *p);
	virtual  HRESULT STDMETHODCALLTYPE toString(BSTR __RPC_FAR *String);
	virtual  HRESULT STDMETHODCALLTYPE createStyleSheet(BSTR bstrHref, long lIndex, IHTMLStyleSheet __RPC_FAR *__RPC_FAR *ppnewStyleSheet);
};

typedef CComObject<CIEHtmlDocument> CIEHtmlDocumentInstance;

#endif