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

#include "ActiveScriptSite.h"


CActiveScriptSite::CActiveScriptSite()
{
}

CActiveScriptSite::~CActiveScriptSite()
{
	Detach();
}

HRESULT CActiveScriptSite::Attach(CLSID clsidScriptEngine)
{
	// Detach to anything already attached to
	Detach();

	HRESULT hr = m_spIActiveScript.CoCreateInstance(clsidScriptEngine);
	if (FAILED(hr))
	{
		return hr;
	}

	m_spIActiveScript->SetScriptSite(this);

	return S_OK;
}

HRESULT CActiveScriptSite::Detach()
{
	if (m_spIActiveScript)
	{
		m_spIActiveScript->Close();
		m_spIActiveScript.Release();
	}

	return S_OK;
}

HRESULT CActiveScriptSite::AttachVBScript()
{
	static const CLSID CLSID_VBScript =
	{ 0xB54F3741, 0x5B07, 0x11CF, { 0xA4, 0xB0, 0x00, 0xAA, 0x00, 0x4A, 0x55, 0xE8} };
	
	return Attach(CLSID_VBScript);
}

HRESULT CActiveScriptSite::AttachJScript()
{
	static const CLSID CLSID_JScript =
	{ 0xF414C260, 0x6AC0, 0x11CF, { 0xB6, 0xD1, 0x00, 0xAA, 0x00, 0xBB, 0xBB, 0x58} };

	return Attach(CLSID_JScript);
}

HRESULT CActiveScriptSite::AddNamedObject(const tstring &szName, IUnknown *pObject)
{
	// TODO check for objects of the same name already

	// Add object to the list
	CNamedObject cObject;
	cObject.szName = szName;
	cObject.spObject = pObject;
	m_cObjectList.push_back(cObject);

	return S_OK;
}

HRESULT CActiveScriptSite::ParseScriptText(const tstring &szScript)
{
	if (m_spIActiveScript == NULL)
	{
		return E_UNEXPECTED;
	}

	CIPtr(IActiveScriptParse) spIActiveScriptParse = m_spIActiveScript;
	if (spIActiveScriptParse)
	{
		USES_CONVERSION;

		CComVariant vResult;
		DWORD dwCookie = 1; // TODO
		DWORD dwFlags = 0;
		EXCEPINFO cExcepInfo;

		spIActiveScriptParse->ParseScriptText(
			T2OLE(szScript.c_str()),
			NULL, NULL, NULL, dwCookie, 0, dwFlags,
			&vResult, &cExcepInfo);
	}
	else
	{
		// TODO persist from IStream
		return E_UNEXPECTED;
	}

	return S_OK;
}

HRESULT CActiveScriptSite::PlayScript()
{
	if (m_spIActiveScript == NULL)
	{
		return E_UNEXPECTED;
	}

	m_spIActiveScript->SetScriptState(SCRIPTSTATE_STARTED);

	return S_OK;
}


///////////////////////////////////////////////////////////////////////////////
// IActiveScriptSite implementation

HRESULT STDMETHODCALLTYPE CActiveScriptSite::GetLCID(/* [out] */ LCID __RPC_FAR *plcid)
{
	// Use the system defined locale
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CActiveScriptSite::GetItemInfo(/* [in] */ LPCOLESTR pstrName, /* [in] */ DWORD dwReturnMask, /* [out] */ IUnknown __RPC_FAR *__RPC_FAR *ppiunkItem, /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppti)
{
//	/* [in] */ LPCOLESTR pstrName,
//	/* [in] */ DWORD dwReturnMask,
//	/* [out] */ IUnknown __RPC_FAR *__RPC_FAR *ppiunkItem,
//	/* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppti)
	
	return TYPE_E_ELEMENTNOTFOUND;
}


HRESULT STDMETHODCALLTYPE CActiveScriptSite::GetDocVersionString(/* [out] */ BSTR __RPC_FAR *pbstrVersion)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CActiveScriptSite::OnScriptTerminate(/* [in] */ const VARIANT __RPC_FAR *pvarResult, /* [in] */ const EXCEPINFO __RPC_FAR *pexcepinfo)
{
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CActiveScriptSite::OnStateChange(/* [in] */ SCRIPTSTATE ssScriptState)
{
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CActiveScriptSite::OnScriptError(/* [in] */ IActiveScriptError __RPC_FAR *pscripterror)
{
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CActiveScriptSite::OnEnterScript(void)
{
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CActiveScriptSite::OnLeaveScript(void)
{
	return S_OK;
}


