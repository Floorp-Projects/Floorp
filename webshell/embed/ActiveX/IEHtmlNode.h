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
#ifndef IEHTMLNODE_H
#define IEHTMLNODE_H

class CIEHtmlNode :	public CComObjectRootEx<CComSingleThreadModel>
{
protected:
	nsIDOMNode *m_pIDOMNode;
	IDispatch  *m_pIDispParent;

public:
	CIEHtmlNode();
protected:
	virtual ~CIEHtmlNode();

public:
	virtual HRESULT SetDOMNode(nsIDOMNode *pIDOMNode);
	virtual HRESULT GetDOMNode(nsIDOMNode **pIDOMNode);
	virtual HRESULT GetDOMElement(nsIDOMElement **pIDOMElement);
	virtual HRESULT SetParentNode(IDispatch *pIDispParent);
	virtual HRESULT GetIDispatch(IDispatch **pDispatch);
};

typedef CComObject<CIEHtmlNode> CIEHtmlNodeInstance;

#endif