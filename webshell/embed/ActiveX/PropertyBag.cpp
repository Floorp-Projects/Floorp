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

#include "StdAfx.h"

#include "PropertyBag.h"


CPropertyBag::CPropertyBag()
{
}


CPropertyBag::~CPropertyBag()
{
}


///////////////////////////////////////////////////////////////////////////////
// IPropertyBag implementation

HRESULT STDMETHODCALLTYPE CPropertyBag::Read(/* [in] */ LPCOLESTR pszPropName, /* [out][in] */ VARIANT __RPC_FAR *pVar, /* [in] */ IErrorLog __RPC_FAR *pErrorLog)
{
	if (pszPropName == NULL)
	{
		return E_INVALIDARG;
	}
	if (pVar == NULL)
	{
		return E_INVALIDARG;
	}

	VariantInit(pVar);
	PropertyList::iterator i;
	for (i = m_PropertyList.begin(); i != m_PropertyList.end(); i++)
	{
		// Is the property already in the list?
		if (wcsicmp((*i).szName, pszPropName) == 0)
		{
			// Copy the new value
			VariantCopy(pVar, &(*i).vValue);
			return S_OK;
		}
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CPropertyBag::Write(/* [in] */ LPCOLESTR pszPropName, /* [in] */ VARIANT __RPC_FAR *pVar)
{
	if (pszPropName == NULL)
	{
		return E_INVALIDARG;
	}
	if (pVar == NULL)
	{
		return E_INVALIDARG;
	}

	PropertyList::iterator i;
	for (i = m_PropertyList.begin(); i != m_PropertyList.end(); i++)
	{
		// Is the property already in the list?
		if (wcsicmp((*i).szName, pszPropName) == 0)
		{
			// Copy the new value
			(*i).vValue = CComVariant(*pVar);
			return S_OK;
		}
	}

	Property p;
	p.szName = CComBSTR(pszPropName);
	p.vValue = *pVar;

	m_PropertyList.push_back(p);
	return S_OK;
}

