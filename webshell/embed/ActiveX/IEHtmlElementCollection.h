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
#ifndef IEHTMLNODECOLLECTION_H
#define IEHTMLNODECOLLECTION_H

#include "IEHtmlNode.h"

class CIEHtmlElement;

class CIEHtmlElementCollection : public CComObjectRootEx<CComSingleThreadModel>,
							  public IDispatchImpl<IHTMLElementCollection, &IID_IHTMLElementCollection, &LIBID_MSHTML>
{
	std::vector< CComQIPtr<IDispatch, &IID_IDispatch> > m_cNodeList;

public:
	CIEHtmlElementCollection();
protected:
	virtual ~CIEHtmlElementCollection();

	CIEHtmlNode *m_pParent;

public:
	// Adds a node to the collection
	virtual HRESULT AddNode(IDispatch *pNode);

	// Sets the parent node of this collection
	virtual HRESULT SetParentNode(CIEHtmlNode *pParent);

	// Helper method creates a collection from a parent node
	static HRESULT CreateFromParentNode(CIEHtmlNode *pParentNode, CIEHtmlElementCollection **pInstance);

BEGIN_COM_MAP(CIEHtmlElementCollection)
	COM_INTERFACE_ENTRY_IID(IID_IDispatch, IHTMLElementCollection)
	COM_INTERFACE_ENTRY_IID(IID_IHTMLElementCollection, IHTMLElementCollection)
END_COM_MAP()

	// IHTMLElementCollection methods
	virtual HRESULT STDMETHODCALLTYPE toString(BSTR __RPC_FAR *String);
	virtual HRESULT STDMETHODCALLTYPE put_length(long v);
	virtual HRESULT STDMETHODCALLTYPE get_length(long __RPC_FAR *p);
	virtual HRESULT STDMETHODCALLTYPE get__newEnum(IUnknown __RPC_FAR *__RPC_FAR *p);
	virtual HRESULT STDMETHODCALLTYPE item(VARIANT name, VARIANT index, IDispatch __RPC_FAR *__RPC_FAR *pdisp);
	virtual HRESULT STDMETHODCALLTYPE tags(VARIANT tagName, IDispatch __RPC_FAR *__RPC_FAR *pdisp);
};

typedef CComObject<CIEHtmlElementCollection> CIEHtmlElementCollectionInstance;

#endif