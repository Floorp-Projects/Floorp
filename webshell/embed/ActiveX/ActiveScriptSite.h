/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#ifndef ACTIVESCRIPTSITE_H
#define ACTIVESCRIPTSITE_H


class CActiveScriptSite :	public CComObjectRootEx<CComSingleThreadModel>,
							public IActiveScriptSite
{
	// Pointer to owned script engine
	CComQIPtr<IActiveScript, &IID_IActiveScript> m_spIActiveScript;

	// List of registered, named objects
	CNamedObjectList m_cObjectList;

public:
	CActiveScriptSite();
	virtual ~CActiveScriptSite();

BEGIN_COM_MAP(CActiveScriptSite)
	COM_INTERFACE_ENTRY(IActiveScriptSite)
END_COM_MAP()

	// Attach to the specified script engine	
	virtual HRESULT Attach(CLSID clsidScriptEngine);
	// Attach to the VBScript engine
	virtual HRESULT AttachVBScript();
	// Attach to the JScript engine
	virtual HRESULT AttachJScript();
	// Detach from the script engine
	virtual HRESULT Detach();

	// Parse the specified script
	virtual HRESULT ParseScriptText(const tstring &szScript);
	// Add object to script address space
	virtual HRESULT AddNamedObject(const tstring &szName, IUnknown *pObject);
	// Play the script
	virtual HRESULT PlayScript();

	// IActiveScriptSite
    virtual HRESULT STDMETHODCALLTYPE GetLCID(/* [out] */ LCID __RPC_FAR *plcid);
    virtual HRESULT STDMETHODCALLTYPE GetItemInfo(/* [in] */ LPCOLESTR pstrName, /* [in] */ DWORD dwReturnMask, /* [out] */ IUnknown __RPC_FAR *__RPC_FAR *ppiunkItem, /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppti);
    virtual HRESULT STDMETHODCALLTYPE GetDocVersionString(/* [out] */ BSTR __RPC_FAR *pbstrVersion);
    virtual HRESULT STDMETHODCALLTYPE OnScriptTerminate(/* [in] */ const VARIANT __RPC_FAR *pvarResult, /* [in] */ const EXCEPINFO __RPC_FAR *pexcepinfo);
    virtual HRESULT STDMETHODCALLTYPE OnStateChange(/* [in] */ SCRIPTSTATE ssScriptState);
    virtual HRESULT STDMETHODCALLTYPE OnScriptError(/* [in] */ IActiveScriptError __RPC_FAR *pscripterror);
    virtual HRESULT STDMETHODCALLTYPE OnEnterScript(void);
    virtual HRESULT STDMETHODCALLTYPE OnLeaveScript(void);
};

typedef CComObject<CActiveScriptSite> CActiveScriptSiteInstance;

#endif