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

// MozillaBrowser.cpp : Implementation of CMozillaBrowser
#include "stdafx.h"

#include <string.h>
#include <string>

#include "MozillaControl.h"
#include "MozillaBrowser.h"

extern "C" void NS_SetupRegistry();

static const std::string c_szPrefsFile     = "prefs.js";
static const std::string c_szPrefsHomePage = "browser.startup.homepage";
static const std::string c_szDefaultPage   = "resource://res/MozillaControl.html";


/////////////////////////////////////////////////////////////////////////////
// CMozillaBrowser

CMozillaBrowser::CMozillaBrowser()
{
	NG_TRACE(_T("CMozillaBrowser::CMozillaBrowser\n"));

	// ATL flags ensures the control opens with a window
	m_bWindowOnly = TRUE;
	m_bWndLess = FALSE;

	// Initialize layout interfaces
    m_pIWebShell = nsnull;
#ifdef USE_NGPREF
	m_pIPref = nsnull;
#endif

	// Create the container that handles some things for us
	m_pWebShellContainer = NULL;

	// Controls starts off unbusy
	m_bBusy = FALSE;

	// Initialise the events library if it hasn't been already
	PL_InitializeEventsLib("");

	// Register components
	NS_SetupRegistry();
}


CMozillaBrowser::~CMozillaBrowser()
{
	NG_TRACE(_T("CMozillaBrowser::~CMozillaBrowser\n"));
}


STDMETHODIMP CMozillaBrowser::InterfaceSupportsErrorInfo(REFIID riid)
{
	static const IID* arr[] = 
	{
		&IID_IWebBrowser,
	};
	for (int i=0;i<sizeof(arr)/sizeof(arr[0]);i++)
	{
		if (InlineIsEqualGUID(*arr[i],riid))
			return S_OK;
	}
	return S_FALSE;
}


LRESULT CMozillaBrowser::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	NG_TRACE(_T("CMozillaBrowser::OnCreate\n"));

	// Clip the child windows out of paint operations
	SetWindowLong(GWL_STYLE, GetWindowLong(GWL_STYLE) | WS_CLIPCHILDREN);

    // Create the NGLayout WebShell
    CreateWebShell();

	// Browse to a default page
	USES_CONVERSION;
	Navigate(A2OLE(c_szDefaultPage.c_str()), NULL, NULL, NULL, NULL);

	return 0;
}


LRESULT CMozillaBrowser::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	NG_TRACE(_T("CMozillaBrowser::OnDestroy\n"));

    // Destroy layout...
    if (m_pIWebShell != nsnull)
	{
		m_pIWebShell->Destroy();
        NS_RELEASE(m_pIWebShell);
	}

	if (m_pWebShellContainer)
	{
		m_pWebShellContainer->Release();
		m_pWebShellContainer = NULL;
	}

#ifdef USE_NGPREF
	if (m_pIPref)
	{
		m_pIPref->Shutdown();
		NS_RELEASE(m_pIPref);
	}
#endif

	return 0;
}


LRESULT CMozillaBrowser::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	NG_TRACE(_T("CMozillaBrowser::OnSize\n"));

    // Pass resize information down to the WebShell...
    if (m_pIWebShell)
	{
		m_pIWebShell->SetBounds(0, 0, LOWORD(lParam), HIWORD(lParam));
    }
	return 0;
}


LRESULT CMozillaBrowser::OnPrint(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	MessageBox(_T("Control doesn't print yet!"), _T("Control Message"), MB_OK);
	// TODO print the current page
	return 0;
}


BOOL CMozillaBrowser::IsValid()
{
	return (m_pIWebShell == nsnull) ? FALSE : TRUE;
}


HRESULT CMozillaBrowser::OnDraw(ATL_DRAWINFO& di)
{
    if (m_pIWebShell == nsnull)
	{
		RECT& rc = *(RECT*)di.prcBounds;
		Rectangle(di.hdcDraw, rc.left, rc.top, rc.right, rc.bottom);
		DrawText(di.hdcDraw, m_sErrorMessage.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}

	return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);

HRESULT CMozillaBrowser::CreateWebShell() 
{
	NG_TRACE(_T("CMozillaBrowser::CreateWebShell\n"));

	if (m_pIWebShell != nsnull)
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	RECT rcLocation;
	GetClientRect(&rcLocation);

	nsresult rv;

#ifdef USE_NGPREF
	// Load preferences
	rv = nsRepository::CreateInstance(kPrefCID, NULL, kIPrefIID, (void **) &m_pIPref);
	if (NS_OK != rv)
	{
		NG_ASSERT(0);
		m_sErrorMessage = _T("Error - could not create preference object");
		return E_FAIL;
	}
	m_pIPref->Startup(c_szPrefsFile);
#endif
	
	// Create the web shell object
	rv = nsRepository::CreateInstance(kWebShellCID, nsnull,
									kIWebShellIID,
									(void**)&m_pIWebShell);
	if (NS_OK != rv)
	{
		NG_ASSERT(0);
		m_sErrorMessage = _T("Error - could not create web shell, check PATH settings");
		return E_FAIL;
	}

	// Initialise the web shell, making it fit the control dimensions

	nsRect r;
	r.x = 0;
	r.y = 0;
	r.width  = rcLocation.right  - rcLocation.left;
	r.height = rcLocation.bottom - rcLocation.top;

	PRBool aAllowPlugins = PR_FALSE; // For the moment

	rv = m_pIWebShell->Init(m_hWnd, 
					r.x, r.y,
					r.width, r.height,
					nsScrollPreference_kAuto, aAllowPlugins);
	NG_ASSERT(rv == NS_OK);

	// Create the container object
	m_pWebShellContainer = new CWebShellContainer(this);
	m_pWebShellContainer->AddRef();

	m_pIWebShell->SetContainer((nsIWebShellContainer*) m_pWebShellContainer);
	m_pIWebShell->SetObserver((nsIStreamObserver*) m_pWebShellContainer);
#ifdef USE_NGPREF
	m_pIWebShell->SetPrefs(m_pIPref);
#endif
	m_pIWebShell->Show();

	return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// IWebBrowser

HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoBack(void)
{
	NG_TRACE(_T("CMozillaBrowser::GoBack\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (m_pIWebShell->CanBack() == NS_OK)
	{
		m_pIWebShell->Back();
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoForward(void)
{
	NG_TRACE(_T("CMozillaBrowser::GoForward\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (m_pIWebShell->CanForward() == NS_OK)
	{
		m_pIWebShell->Forward();
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoHome(void)
{
	NG_TRACE(_T("CMozillaBrowser::GoHome\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	USES_CONVERSION;

	TCHAR * sUrl = _T("http://home.netscape.com");
#ifdef USE_NGPREF
	// Find the home page stored in prefs
	if (m_pIPref)
	{
		char szBuffer[512];
		nsresult rv;
		int nBufLen = sizeof(szBuffer) / sizeof(szBuffer[0]);
		memset(szBuffer, 0, sizeof(szBuffer));
		rv = m_pIPref->GetCharPref(c_szPrefsHomePage, szBuffer, &nBufLen);
		if (rv == NS_OK)
		{
			sUrl = A2T(szBuffer);
		}
	}
#endif

	// Navigate to the home page
	Navigate(T2OLE(sUrl), NULL, NULL, NULL, NULL);
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoSearch(void)
{
	NG_TRACE(_T("CMozillaBrowser::GoSearch\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	USES_CONVERSION;

	TCHAR * sUrl = _T("http://search.netscape.com");
#ifdef USE_NGPREF
	// Find the home page stored in prefs
	if (m_pIPref)
	{
		// TODO find and navigate to the search page stored in prefs
		//      and not this hard coded address
	}
#endif

	Navigate(T2OLE(sUrl), NULL, NULL, NULL, NULL);
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Navigate(BSTR URL, VARIANT __RPC_FAR *Flags, VARIANT __RPC_FAR *TargetFrameName, VARIANT __RPC_FAR *PostData, VARIANT __RPC_FAR *Headers)
{
	NG_TRACE(_T("CMozillaBrowser::Navigate\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	// Extract the URL parameter
	nsString sUrl;
	if (URL == NULL)
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}
	else
	{
		USES_CONVERSION;
		sUrl = OLE2A(URL);
	}

	// Extract the launch flags parameter
	LONG lFlags = 0;
	if (Flags)
	{
		CComVariant vFlags;
		if (VariantChangeType(Flags, &vFlags, 0, VT_I4) != S_OK)
		{
			NG_ASSERT(0);
			return E_INVALIDARG;
		}
		lFlags = vFlags.lVal;
	}


	// Extract the target frame parameter
	nsString sTargetFrame;
	if (TargetFrameName && TargetFrameName->vt == VT_BSTR)
	{
		USES_CONVERSION;
		sTargetFrame = nsString(OLE2A(TargetFrameName->bstrVal));
	}

	// Extract the post data parameter
	nsIPostData *pIPostData = nsnull;
	if (PostData && PostData->vt == VT_BSTR)
	{
		USES_CONVERSION;
		char *szPostData = OLE2A(PostData->bstrVal);
#if 0
		// Create post data from string
		NS_NewPostData(PR_FALSE, szPostData, &pIPostData);
#endif
	}

	// TODO Extract the headers parameter
	PRBool bModifyHistory = PR_TRUE;

	// Check the navigation flags
	if (lFlags & navOpenInNewWindow)
	{
		// Open in new window is a no no
		return E_NOTIMPL;
	}
	if (lFlags & navNoHistory)
	{
		// Disable history
		bModifyHistory = PR_FALSE;
	}
	if (lFlags & navNoReadFromCache)
	{
		// TODO disable read from cache
	}
	if (lFlags & navNoWriteToCache)
	{
		// TODO disable write to cache
	}

	// TODO find the correct target frame

	// Load the URL
	m_pIWebShell->LoadURL(sUrl, pIPostData, bModifyHistory);
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Refresh(void)
{
	NG_TRACE(_T("CMozillaBrowser::Refresh\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	// Reload the page
	CComVariant vRefreshType(REFRESH_NORMAL);
	return Refresh2(&vRefreshType);
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Refresh2(VARIANT __RPC_FAR *Level)
{
	NG_TRACE(_T("CMozillaBrowser::Refresh2\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	NG_ASSERT_NULL_OR_POINTER(Level, VARIANT);

	// Check the requested refresh type
	OLECMDID_REFRESHFLAG iRefreshLevel = OLECMDIDF_REFRESH_NORMAL;
	if (Level)
	{
		CComVariant vLevelAsInt;
		if (vLevelAsInt.ChangeType(VT_I4, Level) != S_OK)
		{
			NG_ASSERT(0);
			return E_INVALIDARG;
		}
		iRefreshLevel = (OLECMDID_REFRESHFLAG) vLevelAsInt.iVal;
	}

	// Turn the IE refresh type into the nearest NG equivalent
	nsURLReloadType type = nsURLReload;
	switch (iRefreshLevel & OLECMDIDF_REFRESH_LEVELMASK)
	{
	case OLECMDIDF_REFRESH_NORMAL:
	case OLECMDIDF_REFRESH_IFEXPIRED:
	case OLECMDIDF_REFRESH_CONTINUE:
	case OLECMDIDF_REFRESH_NO_CACHE:
	case OLECMDIDF_REFRESH_RELOAD:
		type = nsURLReload;
		break;
	case OLECMDIDF_REFRESH_COMPLETELY:
		type = nsURLReloadBypassCacheAndProxy;
		break;
	default:
		// No idea what refresh type this is supposed to be
		NG_ASSERT(0);
		return E_INVALIDARG;
	}

	m_pIWebShell->Reload(type);
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Stop()
{
	NG_TRACE(_T("CMozillaBrowser::Stop\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	m_pIWebShell->Stop();
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Application(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
	NG_TRACE(_T("CMozillaBrowser::get_Application\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (!NgIsValidAddress(ppDisp, sizeof(IDispatch *)))
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}

	// Return a pointer to this controls dispatch interface
	*ppDisp = (IDispatch *) this;
	(*ppDisp)->AddRef();
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Parent(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
	NG_TRACE(_T("CMozillaBrowser::get_Parent\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (!NgIsValidAddress(ppDisp, sizeof(IDispatch *)))
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}

	// Attempt to get the parent object of this control
	HRESULT hr = E_FAIL;
	if (m_spClientSite)
	{
		hr = m_spClientSite->QueryInterface(IID_IDispatch, (void **) ppDisp);
	}

	return (SUCCEEDED(hr)) ? S_OK : E_NOINTERFACE;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Container(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
	NG_TRACE(_T("CMozillaBrowser::get_Container\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (!NgIsValidAddress(ppDisp, sizeof(IDispatch *)))
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}

	*ppDisp = NULL;
	return E_NOINTERFACE;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Document(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
	NG_TRACE(_T("CMozillaBrowser::get_Document\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (!NgIsValidAddress(ppDisp, sizeof(IDispatch *)))
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}

	*ppDisp = NULL;
	return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_TopLevelContainer(VARIANT_BOOL __RPC_FAR *pBool)
{
	NG_TRACE(_T("CMozillaBrowser::get_TopLevelContainer\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (!NgIsValidAddress(pBool, sizeof(VARIANT_BOOL)))
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}

	*pBool = VARIANT_TRUE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Type(BSTR __RPC_FAR *Type)
{
	NG_TRACE(_T("CMozillaBrowser::get_Type\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}
	return E_FAIL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Left(long __RPC_FAR *pl)
{
	NG_TRACE(_T("CMozillaBrowser::get_Left\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (pl == NULL)
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}
	*pl = 0;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Left(long Left)
{
	NG_TRACE(_T("CMozillaBrowser::put_Left\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Top(long __RPC_FAR *pl)
{
	NG_TRACE(_T("CMozillaBrowser::get_Top\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (pl == NULL)
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}
	*pl = 0;
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Top(long Top)
{
	NG_TRACE(_T("CMozillaBrowser::put_Top\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Width(long __RPC_FAR *pl)
{
	NG_TRACE(_T("CMozillaBrowser::get_Width\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (pl == NULL)
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}
	*pl = 0;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Width(long Width)
{
	NG_TRACE(_T("CMozillaBrowser::put_Width\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Height(long __RPC_FAR *pl)
{
	NG_TRACE(_T("CMozillaBrowser::get_Height\n"));

    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	if (pl == NULL)
	{
		return E_INVALIDARG;
	}
	*pl = 0;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Height(long Height)
{
	NG_TRACE(_T("CMozillaBrowser::put_Height\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_LocationName(BSTR __RPC_FAR *LocationName)
{
	NG_TRACE(_T("CMozillaBrowser::get_LocationName\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (LocationName == NULL)
	{
		return E_INVALIDARG;
	}

	// Get the url from the web shell
	PRUnichar *pszLocationName = nsnull;
	m_pIWebShell->GetTitle(&pszLocationName);
	if (pszLocationName == nsnull)
	{
		return E_FAIL;
	}

	// Convert the string to a BSTR
	USES_CONVERSION;
	LPOLESTR pszConvertedLocationName = W2OLE(pszLocationName);
	*LocationName = SysAllocString(pszConvertedLocationName);

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_LocationURL(BSTR __RPC_FAR *LocationURL)
{
	NG_TRACE(_T("CMozillaBrowser::get_LocationURL\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (LocationURL == NULL)
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}

	// Get the url from the web shell
	PRUnichar *pszUrl = nsnull;
	m_pIWebShell->GetURL(0, &pszUrl);
	if (pszUrl == nsnull)
	{
		return E_FAIL;
	}

	// Convert the string to a BSTR
	USES_CONVERSION;
	LPOLESTR pszConvertedUrl = W2OLE(pszUrl);
	*LocationURL = SysAllocString(pszConvertedUrl);

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Busy(VARIANT_BOOL __RPC_FAR *pBool)
{
	NG_TRACE(_T("CMozillaBrowser::get_Busy\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (!NgIsValidAddress(pBool, sizeof(*pBool)))
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}

	*pBool = (m_bBusy) ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}


/////////////////////////////////////////////////////////////////////////////
// IWebBrowserApp

HRESULT STDMETHODCALLTYPE CMozillaBrowser::Quit(void)
{
	NG_TRACE(_T("CMozillaBrowser::Quit\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	// TODO fire quit event
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::ClientToWindow(int __RPC_FAR *pcx, int __RPC_FAR *pcy)
{
	NG_TRACE(_T("CMozillaBrowser::ClientToWindow\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	// TODO convert points to be relative to browser
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::PutProperty(BSTR szProperty, VARIANT vtValue)
{
	NG_TRACE(_T("CMozillaBrowser::PutProperty\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (szProperty == NULL)
	{
		return E_INVALIDARG;
	}
	PropertyList::iterator i;
	for (i = m_PropertyList.begin(); i != m_PropertyList.end(); i++)
	{
		// Is the property already in the list?
		if (wcscmp((*i).szName, szProperty) == 0)
		{
			// Copy the new value
			(*i).vValue = CComVariant(vtValue);
			return S_OK;
		}
	}

	Property p;
	p.szName = CComBSTR(szProperty);
	p.vValue = vtValue;

	m_PropertyList.push_back(p);
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GetProperty(BSTR Property, VARIANT __RPC_FAR *pvtValue)
{
	NG_TRACE(_T("CMozillaBrowser::GetProperty\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	NG_ASSERT(Property);
	NG_ASSERT_POINTER(pvtValue, VARIANT);
	
	if (Property == NULL || pvtValue == NULL)
	{
		return E_INVALIDARG;
	}
	
	VariantInit(pvtValue);
	PropertyList::iterator i;
	for (i = m_PropertyList.begin(); i != m_PropertyList.end(); i++)
	{
		// Is the property already in the list?
		if (wcscmp((*i).szName, Property) == 0)
		{
			// Copy the new value
			VariantCopy(pvtValue, &(*i).vValue);
			return S_OK;
		}
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Name(BSTR __RPC_FAR *Name)
{
	NG_TRACE(_T("CMozillaBrowser::get_Name\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	NG_ASSERT_POINTER(Name, BSTR);
	if (Name == NULL)
	{
		return E_INVALIDARG;
	}
	*Name = SysAllocString(L""); // TODO get Mozilla's executable name
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_HWND(long __RPC_FAR *pHWND)
{
	NG_TRACE(_T("CMozillaBrowser::get_HWND\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	NG_ASSERT_POINTER(pHWND, HWND);
	if (pHWND == NULL)
	{
		return E_INVALIDARG;
	}

	*pHWND = NULL;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_FullName(BSTR __RPC_FAR *FullName)
{
	NG_TRACE(_T("CMozillaBrowser::get_FullName\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	NG_ASSERT_POINTER(FullName, BSTR);
	if (FullName == NULL)
	{
		return E_INVALIDARG;
	}
	*FullName = SysAllocString(L""); // TODO get Mozilla's executable name
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Path(BSTR __RPC_FAR *Path)
{
	NG_TRACE(_T("CMozillaBrowser::get_Path\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	NG_ASSERT_POINTER(Path, BSTR);
	if (Path == NULL)
	{
		return E_INVALIDARG;
	}
	*Path = SysAllocString(L""); // TODO get Mozilla's path
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Visible(VARIANT_BOOL __RPC_FAR *pBool)
{
	NG_TRACE(_T("CMozillaBrowser::get_Visible\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	NG_ASSERT_POINTER(pBool, int);
	if (pBool == NULL)
	{
		return E_INVALIDARG;
	}
	*pBool = VARIANT_FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Visible(VARIANT_BOOL Value)
{
	NG_TRACE(_T("CMozillaBrowser::put_Visible\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_StatusBar(VARIANT_BOOL __RPC_FAR *pBool)
{
	NG_TRACE(_T("CMozillaBrowser::get_StatusBar\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	NG_ASSERT_POINTER(pBool, int);
	if (pBool == NULL)
	{
		return E_INVALIDARG;
	}
	*pBool = VARIANT_FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_StatusBar(VARIANT_BOOL Value)
{
	NG_TRACE(_T("CMozillaBrowser::put_StatusBar\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_StatusText(BSTR __RPC_FAR *StatusText)
{
	NG_TRACE(_T("CMozillaBrowser::get_StatusText\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	NG_ASSERT_POINTER(StatusText, BSTR);
	if (StatusText == NULL)
	{
		return E_INVALIDARG;
	}
	*StatusText = SysAllocString(L""); // TODO
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_StatusText(BSTR StatusText)
{
	NG_TRACE(_T("CMozillaBrowser::put_StatusText\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_ToolBar(int __RPC_FAR *Value)
{
	NG_TRACE(_T("CMozillaBrowser::get_ToolBar\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	NG_ASSERT_POINTER(Value, int);
	if (Value == NULL)
	{
		return E_INVALIDARG;
	}
	*Value = FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_ToolBar(int Value)
{
	NG_TRACE(_T("CMozillaBrowser::put_ToolBar\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	// No toolbar in control!
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_MenuBar(VARIANT_BOOL __RPC_FAR *Value)
{
	NG_TRACE(_T("CMozillaBrowser::get_MenuBar\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	NG_ASSERT_POINTER(Value, int);
	if (Value == NULL)
	{
		return E_INVALIDARG;
	}
	*Value = VARIANT_FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_MenuBar(VARIANT_BOOL Value)
{
	NG_TRACE(_T("CMozillaBrowser::put_MenuBar\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	// No menu in control!
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_FullScreen(VARIANT_BOOL __RPC_FAR *pbFullScreen)
{
	NG_TRACE(_T("CMozillaBrowser::get_FullScreen\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	NG_ASSERT_POINTER(pbFullScreen, VARIANT_BOOL);
	if (pbFullScreen == NULL)
	{
		return E_INVALIDARG;
	}
	*pbFullScreen = VARIANT_FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_FullScreen(VARIANT_BOOL bFullScreen)
{
	NG_TRACE(_T("CMozillaBrowser::put_FullScreen\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	// No fullscreen mode in control!
	return S_OK;
}



/////////////////////////////////////////////////////////////////////////////
// IWebBrowser2

HRESULT STDMETHODCALLTYPE CMozillaBrowser::Navigate2(VARIANT __RPC_FAR *URL, VARIANT __RPC_FAR *Flags, VARIANT __RPC_FAR *TargetFrameName, VARIANT __RPC_FAR *PostData, VARIANT __RPC_FAR *Headers)
{
	NG_TRACE(_T("CMozillaBrowser::Navigate2\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	CComVariant vURLAsString;
	if (vURLAsString.ChangeType(VT_BSTR, URL) != S_OK)
	{
		return E_INVALIDARG;
	}

	return Navigate(vURLAsString.bstrVal, Flags, TargetFrameName, PostData, Headers);
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::QueryStatusWB(OLECMDID cmdID, OLECMDF __RPC_FAR *pcmdf)
{
	NG_TRACE(_T("CMozillaBrowser::QueryStatusWB\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	if (pcmdf == NULL)
	{
		return E_INVALIDARG;
	}

	// Call through to IOleCommandTarget::QueryStatus
	OLECMD cmd;
	HRESULT hr;
	
	cmd.cmdID = cmdID;
	cmd.cmdf = 0;
	hr = QueryStatus(NULL, 1, &cmd, NULL);
	if (SUCCEEDED(hr))
	{
		*pcmdf = (OLECMDF) cmd.cmdf;
	}

	return hr;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::ExecWB(OLECMDID cmdID, OLECMDEXECOPT cmdexecopt, VARIANT __RPC_FAR *pvaIn, VARIANT __RPC_FAR *pvaOut)
{
	NG_TRACE(_T("CMozillaBrowser::ExecWB\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	// Call through to IOleCommandTarget::Exec
	HRESULT hr;
	hr = Exec(NULL, cmdID, cmdexecopt, pvaIn, pvaOut);
	return hr;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::ShowBrowserBar(VARIANT __RPC_FAR *pvaClsid, VARIANT __RPC_FAR *pvarShow, VARIANT __RPC_FAR *pvarSize)
{
	NG_TRACE(_T("CMozillaBrowser::ShowBrowserBar\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_ReadyState(READYSTATE __RPC_FAR *plReadyState)
{
	NG_TRACE(_T("CMozillaBrowser::get_ReadyState\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Offline(VARIANT_BOOL __RPC_FAR *pbOffline)
{
	NG_TRACE(_T("CMozillaBrowser::get_Offline\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Offline(VARIANT_BOOL bOffline)
{
	NG_TRACE(_T("CMozillaBrowser::put_Offline\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Silent(VARIANT_BOOL __RPC_FAR *pbSilent)
{
	NG_TRACE(_T("CMozillaBrowser::get_Silent\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Silent(VARIANT_BOOL bSilent)
{
	NG_TRACE(_T("CMozillaBrowser::put_Silent\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_RegisterAsBrowser(VARIANT_BOOL __RPC_FAR *pbRegister)
{
	NG_TRACE(_T("CMozillaBrowser::get_RegisterAsBrowser\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_RegisterAsBrowser(VARIANT_BOOL bRegister)
{
	NG_TRACE(_T("CMozillaBrowser::put_RegisterAsBrowser\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_RegisterAsDropTarget(VARIANT_BOOL __RPC_FAR *pbRegister)
{
	NG_TRACE(_T("CMozillaBrowser::get_RegisterAsDropTarget\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_RegisterAsDropTarget(VARIANT_BOOL bRegister)
{
	NG_TRACE(_T("CMozillaBrowser::put_RegisterAsDropTarget\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_TheaterMode(VARIANT_BOOL __RPC_FAR *pbRegister)
{
	NG_TRACE(_T("CMozillaBrowser::get_TheaterMode\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_TheaterMode(VARIANT_BOOL bRegister)
{
	NG_TRACE(_T("CMozillaBrowser::put_TheaterMode\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_AddressBar(VARIANT_BOOL __RPC_FAR *Value)
{
	NG_TRACE(_T("CMozillaBrowser::get_AddressBar\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_AddressBar(VARIANT_BOOL Value)
{
	NG_TRACE(_T("CMozillaBrowser::put_AddressBar\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Resizable(VARIANT_BOOL __RPC_FAR *Value)
{
	NG_TRACE(_T("CMozillaBrowser::get_Resizable\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Resizable(VARIANT_BOOL Value)
{
	NG_TRACE(_T("CMozillaBrowser::put_Resizable\n"));

    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}

///////////////////////////////////////////////////////////////////////////////
// IOleCommandTarget implementation

// IOleCommandTarget is a (bloody awful and complicated) generic mechanism
// for querying for and executing commands on an object. It is used in IE
// for printing, so that a client prints a page of HTML by calling Exec
// (or ExecWB) on the control using the command OLECMDID_PRINT.

// To keep things open, all supported commands are in the table below which
// can be extended when and if necessary.

struct OleCommandInfo
{
	ULONG    nCmdID;
	ULONG    nWindowsCmdID;
	wchar_t *szVerbText;
	wchar_t *szStatusText;
};

static OleCommandInfo s_aSupportedCommands[] =
{
	{ OLECMDID_PRINT, ID_PRINT, L"Print", L"Print the page" },
	{ OLECMDID_SAVEAS, 0, L"SaveAs", L"Save the page" },
	{ OLECMDID_PAGESETUP, 0, L"Page Setup", L"Page Setup" },
	{ OLECMDID_PROPERTIES, 0, L"Properties", L"Show page properties" },
	{ OLECMDID_CUT, 0, L"Cut", L"Cut selection" },
	{ OLECMDID_COPY, 0, L"Copy", L"Copy selection" },
	{ OLECMDID_PASTE, 0, L"Paste", L"Paste as selection" },
	{ OLECMDID_UNDO, 0, L"Undo", L"Undo" },
	{ OLECMDID_REDO, 0, L"Redo", L"Redo" },
	{ OLECMDID_SELECTALL, 0, L"SelectAll", L"Select all" },
	{ OLECMDID_REFRESH, 0, L"Refresh", L"Refresh" },
	{ OLECMDID_STOP, 0, L"Stop", L"Stop" },
	{ OLECMDID_ONUNLOAD, 0, L"OnUnload", L"OnUnload" }
};


HRESULT STDMETHODCALLTYPE CMozillaBrowser::QueryStatus(const GUID __RPC_FAR *pguidCmdGroup, ULONG cCmds, OLECMD __RPC_FAR prgCmds[], OLECMDTEXT __RPC_FAR *pCmdText)
{
	// All command groups except the default are ignored
	if (pguidCmdGroup != NULL)
	{
		OLECMDERR_E_UNKNOWNGROUP;
	}

	if (prgCmds == NULL)
	{
		return E_INVALIDARG;
	}

	BOOL bTextSet = FALSE;

	// Iterate through list of commands and flag them as supported/unsupported
	for (ULONG nCmd = 0; nCmd < cCmds; nCmd++)
	{
		// Unsupported by default
		prgCmds[nCmd].cmdf = 0;

		// Search the support command list
		int nSupportedCount = sizeof(s_aSupportedCommands) / sizeof(s_aSupportedCommands[0]);
		for (int nSupported = 0; nSupported < nSupportedCount; nSupported++)
		{
			if (s_aSupportedCommands[nSupported].nCmdID != prgCmds[nCmd].cmdID)
			{
				continue;
			}

			// Command is supported so flag it and possibly enable it
			prgCmds[nCmd].cmdf = OLECMDF_SUPPORTED;
			if (s_aSupportedCommands[nSupported].nWindowsCmdID != 0)
			{
				prgCmds[nCmd].cmdf |= OLECMDF_ENABLED;
			}

			// Copy the status/verb text for the first supported command only
			if (!bTextSet && pCmdText)
			{
				// See what text the caller wants
				wchar_t *pszTextToCopy = NULL;
				if (pCmdText->cmdtextf & OLECMDTEXTF_NAME)
				{
					pszTextToCopy = s_aSupportedCommands[nSupported].szVerbText;
				}
				else if (pCmdText->cmdtextf & OLECMDTEXTF_STATUS)
				{
					pszTextToCopy = s_aSupportedCommands[nSupported].szStatusText;
				}
				
				// Copy the text
				pCmdText->cwActual = 0;
				memset(pCmdText->rgwz, 0, pCmdText->cwBuf * sizeof(wchar_t));
				if (pszTextToCopy)
				{
					// Don't exceed the provided buffer size
					int nTextLen = wcslen(pszTextToCopy);
					if (nTextLen > pCmdText->cwBuf)
					{
						nTextLen = pCmdText->cwBuf;
					}

					wcsncpy(pCmdText->rgwz, pszTextToCopy, nTextLen);
					pCmdText->cwActual = nTextLen;
				}
				
				bTextSet = TRUE;
			}
			break;
		}
	}
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Exec(const GUID __RPC_FAR *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT __RPC_FAR *pvaIn, VARIANT __RPC_FAR *pvaOut)
{
	// All command groups except the default are ignored
	if (pguidCmdGroup != NULL)
	{
		OLECMDERR_E_UNKNOWNGROUP;
	}

	// Search the support command list
	int nSupportedCount = sizeof(s_aSupportedCommands) / sizeof(s_aSupportedCommands[0]);
	for (int nSupported = 0; nSupported < nSupportedCount; nSupported++)
	{
		if (s_aSupportedCommands[nSupported].nCmdID != nCmdID)
		{
			continue;
		}

		// Command is supported but not implemented
		if (s_aSupportedCommands[nSupported].nWindowsCmdID == 0)
		{
			continue;
		}

		// Send ourselves a WM_COMMAND windows message with the associated identifier
		SendMessage(WM_COMMAND, LOWORD(s_aSupportedCommands[nSupported].nWindowsCmdID));

		return S_OK;
	}

	return OLECMDERR_E_NOTSUPPORTED;
}

