/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "DropTarget.h"

#ifndef CFSTR_SHELLURL
#define CFSTR_SHELLURL _T("UniformResourceLocator")
#endif

static const UINT g_cfURL = RegisterClipboardFormat(CFSTR_SHELLURL);

CDropTarget::CDropTarget()
{
	m_pOwner = NULL;
}


CDropTarget::~CDropTarget()
{
}


void CDropTarget::SetOwner(CMozillaBrowser *pOwner)
{
	m_pOwner = pOwner;
}

///////////////////////////////////////////////////////////////////////////////
// IDropTarget implementation

HRESULT STDMETHODCALLTYPE CDropTarget::DragEnter(/* [unique][in] */ IDataObject __RPC_FAR *pDataObj, /* [in] */ DWORD grfKeyState, /* [in] */ POINTL pt, /* [out][in] */ DWORD __RPC_FAR *pdwEffect)
{
	if (pdwEffect == NULL || pDataObj == NULL)
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}

	if (m_spDataObject != NULL)
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	// TODO process Internet Shortcuts (*.URL) files

	// Test if the data object supports types we can handle
	BOOL bSupported = FALSE;
	FORMATETC formatetc;
	memset(&formatetc, 0, sizeof(formatetc));
	formatetc.dwAspect = DVASPECT_CONTENT;
	formatetc.lindex = -1;
	formatetc.tymed = TYMED_HGLOBAL;
	formatetc.cfFormat = g_cfURL;
	if (pDataObj->QueryGetData(&formatetc) == S_OK)
	{
		bSupported = TRUE;
	}

	if (bSupported)
	{
		m_spDataObject = pDataObj;
		*pdwEffect = DROPEFFECT_LINK;
	}
	else
	{
		*pdwEffect = DROPEFFECT_NONE;
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CDropTarget::DragOver(/* [in] */ DWORD grfKeyState, /* [in] */ POINTL pt, /* [out][in] */ DWORD __RPC_FAR *pdwEffect)
{
	if (pdwEffect == NULL)
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}
	*pdwEffect = m_spDataObject ? DROPEFFECT_LINK : DROPEFFECT_NONE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CDropTarget::DragLeave(void)
{
	if (m_spDataObject)
	{
		m_spDataObject.Release();
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CDropTarget::Drop(/* [unique][in] */ IDataObject __RPC_FAR *pDataObj, /* [in] */ DWORD grfKeyState, /* [in] */ POINTL pt, /* [out][in] */ DWORD __RPC_FAR *pdwEffect)
{
	if (pdwEffect == NULL)
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}
	if (m_spDataObject == NULL)
	{
		*pdwEffect = DROPEFFECT_NONE;
		return S_OK;
	}

	*pdwEffect = DROPEFFECT_LINK;

	// TODO process Internet Shortcuts (*.URL) files

	// Get the URL from the data object
	FORMATETC formatetc;
	STGMEDIUM stgmedium;
	memset(&formatetc, 0, sizeof(formatetc));
	memset(&stgmedium, 0, sizeof(formatetc));

	formatetc.dwAspect = DVASPECT_CONTENT;
	formatetc.lindex = -1;
	formatetc.tymed = TYMED_HGLOBAL;
	formatetc.cfFormat = g_cfURL;

	if (m_spDataObject->GetData(&formatetc, &stgmedium) == S_OK)
	{
		NG_ASSERT(stgmedium.tymed == TYMED_HGLOBAL);
		NG_ASSERT(stgmedium.hGlobal);
		char *pszURL = (char *) GlobalLock(stgmedium.hGlobal);
		NG_TRACE("URL \"%s\" dragged over control\n", pszURL);
		// Browse to the URL
		if (m_pOwner)
		{
			USES_CONVERSION;
			BSTR bstrURL = SysAllocString(A2OLE(pszURL));
			m_pOwner->Navigate(bstrURL, NULL, NULL, NULL, NULL);
			SysFreeString(bstrURL);
		}
		GlobalUnlock(stgmedium.hGlobal);
		ReleaseStgMedium(&stgmedium);
	}

	m_spDataObject.Release();

	return S_OK;
}

