/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef PROPERTYBAG_H
#define PROPERTYBAG_H


class CPropertyBag :	public CComObjectRootEx<CComSingleThreadModel>,
						public IPropertyBag
{
	// List of properties in the bag
	PropertyList m_PropertyList;

public:
	CPropertyBag();
	virtual ~CPropertyBag();

BEGIN_COM_MAP(CPropertyBag)
	COM_INTERFACE_ENTRY(IPropertyBag)
END_COM_MAP()

	// IPropertyBag methods
	virtual /* [local] */ HRESULT STDMETHODCALLTYPE Read(/* [in] */ LPCOLESTR pszPropName, /* [out][in] */ VARIANT __RPC_FAR *pVar, /* [in] */ IErrorLog __RPC_FAR *pErrorLog);
	virtual HRESULT STDMETHODCALLTYPE Write(/* [in] */ LPCOLESTR pszPropName, /* [in] */ VARIANT __RPC_FAR *pVar);
};

typedef CComObject<CPropertyBag> CPropertyBagInstance;

#endif