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

#include "stdafx.h"
#include <string.h>
#include <string>

#include "MozillaControl.h"
#include "MozillaBrowser.h"
#include "IEHtmlDocument.h"

static const TCHAR *c_szInvalidArg = _T("Invalid parameter");
static const TCHAR *c_szUninitialized = _T("Method called while control is uninitialized");

#define RETURN_ERROR(message, hr) \
	SetErrorInfo(message, hr); \
	return hr;

#define RETURN_E_INVALIDARG() \
	RETURN_ERROR(c_szInvalidArg, E_INVALIDARG);

#define RETURN_E_UNEXPECTED() \
	RETURN_ERROR(c_szUninitialized, E_UNEXPECTED);


extern "C" void NS_SetupRegistry();

static const std::string c_szPrefsHomePage = "browser.startup.homepage";
static const std::string c_szDefaultPage   = "resource:/res/MozillaControl.html";

class CMozDir
{
	TCHAR m_szOldDir[1024];
public:
	CMozDir()
	{
		GetCurrentDirectory(1024, m_szOldDir);
		CRegKey cKey;
		if (cKey.Open(HKEY_LOCAL_MACHINE, _T("Software\\Mozilla")) == ERROR_SUCCESS)
		{
			TCHAR szCurDir[1024];
			DWORD dwSize = sizeof(szCurDir) / sizeof(szCurDir[0]);
			if (cKey.QueryValue(szCurDir, _T("MozillaDir"), &dwSize) == ERROR_SUCCESS)
			{
				SetCurrentDirectory(szCurDir);
			}
			cKey.Close();
		}
	}
	~CMozDir()
	{
		// Restore old working directory
		SetCurrentDirectory(m_szOldDir);
	}
};

BOOL CMozillaBrowser::m_bRegistryInitialized = FALSE;

/////////////////////////////////////////////////////////////////////////////
// CMozillaBrowser

CMozillaBrowser::CMozillaBrowser()
{
	NG_TRACE_METHOD(CMozillaBrowser::CMozillaBrowser);

	// ATL flags ensures the control opens with a window
	m_bWindowOnly = TRUE;
	m_bWndLess = FALSE;

	// Initialize layout interfaces
    m_pIWebShell = nsnull;
#ifdef USE_NGPREF
	m_pIPref = nsnull;
#endif

	// Ready state of control
	m_nBrowserReadyState = READYSTATE_UNINITIALIZED;
	
	// Create the container that handles some things for us
	m_pWebShellContainer = NULL;

	// Controls starts off unbusy
	m_bBusy = FALSE;

	// Control starts off without being a drop target
	m_bDropTarget = FALSE;

 	// the IHTMLDOcument, lazy allocation.
 	m_pDocument = NULL;

	// Change the current directory to the Mozilla dist so that registration
	// works properly

	// Register components
	if (!m_bRegistryInitialized)
	{
		NS_SetupRegistry();
		m_bRegistryInitialized = TRUE;
	}

	// Create the Event Queue for the UI thread...
	//
	// If an event queue already exists for the thread, then 
	// CreateThreadEventQueue(...) will fail...
	nsresult rv;
	nsIEventQueueService* eventQService = NULL;

	rv = nsServiceManager::GetService(kEventQueueServiceCID,
								kIEventQueueServiceIID,
								(nsISupports **)&eventQService);
	if (NS_SUCCEEDED(rv)) {
		rv = eventQService->CreateThreadEventQueue();
		nsServiceManager::ReleaseService(kEventQueueServiceCID, eventQService);
	}

}


CMozillaBrowser::~CMozillaBrowser()
{
	// XXX: Do not call DestroyThreadEventQueue(...) for now...
	NG_TRACE_METHOD(CMozillaBrowser::~CMozillaBrowser);
}


STDMETHODIMP CMozillaBrowser::InterfaceSupportsErrorInfo(REFIID riid)
{
	static const IID* arr[] = 
	{
		&IID_IWebBrowser,
		&IID_IWebBrowser2,
		&IID_IWebBrowserApp
	};
	for (int i=0;i<sizeof(arr)/sizeof(arr[0]);i++)
	{
		if (InlineIsEqualGUID(*arr[i],riid))
			return S_OK;
	}
	return S_FALSE;
}


int CMozillaBrowser::MessageBox(LPCTSTR lpszText, LPCTSTR lpszCaption, UINT nType)
{
	// Let the doc host display it's own message box if it can
	CIPtr(IDocHostShowUI) spIDocHostShowUI = m_spClientSite;
	if (spIDocHostShowUI)
	{
		USES_CONVERSION;
		LRESULT lResult = 0;
		HRESULT hr = spIDocHostShowUI->ShowMessage(m_hWnd,
						T2OLE(lpszText), T2OLE(lpszCaption), nType, NULL, 0, &lResult);
		if (hr == S_OK)
		{
			return lResult;
		}
	}

	// Do the default message box
	return CWindow::MessageBox(lpszText, lpszCaption, nType);
}


HRESULT CMozillaBrowser::SetErrorInfo(LPCTSTR lpszDesc, HRESULT hr)
{
	USES_CONVERSION;
	return AtlSetErrorInfo(GetObjectCLSID(), T2OLE(lpszDesc), 0, NULL, GUID_NULL, hr, NULL);
}


///////////////////////////////////////////////////////////////////////////////
// Message handlers


// Handle WM_CREATE windows message
LRESULT CMozillaBrowser::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	NG_TRACE_METHOD(CMozillaBrowser::OnCreate);

	// Clip the child windows out of paint operations
	SetWindowLong(GWL_STYLE, GetWindowLong(GWL_STYLE) | WS_CLIPCHILDREN);

    // Create the NGLayout WebShell
    CreateWebShell();

	// TODO create and register a drop target

	// Control is ready
	m_nBrowserReadyState = READYSTATE_LOADED;

	// Load browser helpers
	LoadBrowserHelpers();

	// Browse to a default page - if in design mode
    BOOL bUserMode = FALSE;
    if (SUCCEEDED(GetAmbientUserMode(bUserMode)))
    {
		if (!bUserMode)
		{
			USES_CONVERSION;
			Navigate(A2OLE(c_szDefaultPage.c_str()), NULL, NULL, NULL, NULL);
		}
	}

	return 0;
}


// Handle WM_DESTROY window message
LRESULT CMozillaBrowser::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	NG_TRACE_METHOD(CMozillaBrowser::OnDestroy);

	// Unload browser helpers
	UnloadBrowserHelpers();

	// TODO unregister drop target

 	// Destroy the htmldoc
 	if (m_pDocument != NULL)
 	{
 		m_pDocument->Release();
 		m_pDocument = NULL;
 	}

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
		NS_RELEASE(m_pIPref);
	}
#endif

	return 0;
}


// Handle WM_SIZE windows message
LRESULT CMozillaBrowser::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	NG_TRACE_METHOD(CMozillaBrowser::OnSize);

    // Pass resize information down to the WebShell...
    if (m_pIWebShell)
	{
		m_pIWebShell->SetBounds(0, 0, LOWORD(lParam), HIWORD(lParam));
    }
	return 0;
}


// Handle WM_PAINT windows message (and IViewObject::Draw) 
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


// Handle ID_PAGESETUP command
LRESULT CMozillaBrowser::OnPageSetup(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	MessageBox(_T("No page setup yet!"), _T("Control Message"), MB_OK);
	// TODO show the page setup dialog
	return 0;
}


// Handle ID_PRINT command
LRESULT CMozillaBrowser::OnPrint(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	// Print the contents
	if (m_pIWebShell)
	{
		nsIContentViewer *pContentViewer = nsnull;
		m_pIWebShell->GetContentViewer(&pContentViewer);
		if (nsnull != pContentViewer)
		{
			pContentViewer->Print();
			NS_RELEASE(pContentViewer);
		}
	}
	return 0;
}


// Handle ID_VIEWSOURCE command
LRESULT CMozillaBrowser::OnViewSource(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{	
	// Get the url from the web shell
	const PRUnichar *pszUrl = nsnull;
	PRInt32 aHistoryIndex;
	m_pIWebShell->GetHistoryIndex(aHistoryIndex);
	m_pIWebShell->GetURL(aHistoryIndex, &pszUrl);

	nsString strUrl(pszUrl);
	nsString strTemp(nsString("view-source:") + strUrl);
	strUrl = strTemp;

	CIPtr(IDispatch) spDispNew;
	VARIANT_BOOL bCancel = VARIANT_FALSE;
	Fire_NewWindow2(&spDispNew, &bCancel);

	if ((bCancel == VARIANT_FALSE) && spDispNew)
	{
		CIPtr(IWebBrowser2) spOther = spDispNew;;
		if (spOther)
		{
			// tack in the viewsource command
			BSTR bstrURL = SysAllocString(strUrl.GetUnicode());
			CComVariant vURL(bstrURL);
			VARIANT vNull;
			vNull.vt = VT_NULL;

			spOther->Navigate2(&vURL, &vNull, &vNull, &vNull, &vNull);			

			// when and if we can get the container we should
			// be able to tell the other windows container not to show toolbars, menus, etc.
			// we would also be able to show the window.  one fix would be to 
			// change the navigate method to set a flag if it's viewing source, and to 
			// have it's document complete method tell it's container to hide toolbars and menu.
			/*
			IDispatch *pOtherContainer = NULL;
			pOther->get_Container(&pOtherContainer);
			if(pOtherContainer != NULL)
			{
				DWebBrowserEvents2 *pOtherEventSink;
				if(SUCCEEDED(pOtherContainer->QueryInterface(IID_IDWebBrowserEvents2,(void**)&pOtherEventSink)))
				{
					__asm int 3

					pOtherEventSink->Release();
				}

				pOtherContainer->Release();
			}
			*/

			SysFreeString(bstrURL);
		}
	}

	bHandled = TRUE;
	return 0;
}


// Test if the browser is in a valid state
BOOL CMozillaBrowser::IsValid()
{
	return (m_pIWebShell == nsnull) ? FALSE : TRUE;
}


/////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);

#ifdef USE_NGPREF
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
#endif /* USE_NGPREF */


// Create and initialise the web shell
HRESULT CMozillaBrowser::CreateWebShell() 
{
	NG_TRACE_METHOD(CMozillaBrowser::CreateWebShell);

	if (m_pIWebShell != nsnull)
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	RECT rcLocation;
	GetClientRect(&rcLocation);
	if (IsRectEmpty(&rcLocation))
	{
		rcLocation.bottom++;
		rcLocation.top++;
	}

	nsresult rv;

#ifdef USE_NGPREF
	// Load preferences
	rv = nsServiceManager::GetService(kPrefCID, 
									nsIPref::GetIID(), 
									(nsISupports **)&prefs);
	if (NS_OK != rv)
	{
		NG_ASSERT(0);
		m_sErrorMessage = _T("Error - could not create preference object");
		return E_FAIL;
	}
#endif
	
	// Create the web shell object
	rv = nsComponentManager::CreateInstance(kWebShellCID, nsnull,
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

	PRBool aAllowPlugins = PR_TRUE;
	PRBool aIsSunkenBorder = PR_FALSE;

	rv = m_pIWebShell->Init(m_hWnd, 
					r.x, r.y,
					r.width, r.height,
					nsScrollPreference_kAuto,
					aAllowPlugins,
					aIsSunkenBorder);
	NG_ASSERT(rv == NS_OK);

	// Create the container object
	m_pWebShellContainer = new CWebShellContainer(this);
	m_pWebShellContainer->AddRef();

	m_pIWebShell->SetContainer((nsIWebShellContainer*) m_pWebShellContainer);
	m_pIWebShell->SetObserver((nsIStreamObserver*) m_pWebShellContainer);
	m_pIWebShell->SetDocLoaderObserver((nsIDocumentLoaderObserver*) m_pWebShellContainer);
	m_pIWebShell->SetWebShellType(nsWebShellContent);

#ifdef USE_NGPREF
	m_pIWebShell->SetPrefs(m_pIPref);
#endif

	m_pIWebShell->Show();

	return S_OK;
}


// Return the root DOM document
HRESULT CMozillaBrowser::GetDOMDocument(nsIDOMDocument **pDocument)
{
	if (pDocument == NULL)
	{
		NG_ASSERT(0);
		return E_INVALIDARG;
	}

	*pDocument = nsnull;

	if (m_pIWebShell == nsnull)
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}
	
	nsIContentViewer * pCViewer = nsnull;
	
	m_pIWebShell->GetContentViewer(&pCViewer);
	if (nsnull != pCViewer)
	{
		nsIDocumentViewer * pDViewer = nsnull;
		if (pCViewer->QueryInterface(kIDocumentViewerIID, (void**) &pDViewer) == NS_OK)
		{
			nsIDocument * pDoc = nsnull;
			pDViewer->GetDocument(pDoc);
			if (pDoc != nsnull)
			{
				if (pDoc->QueryInterface(kIDOMDocumentIID, (void**) pDocument) == NS_OK)
				{
				}
				NS_RELEASE(pDoc);
			}
			NS_RELEASE(pDViewer);
		}
		NS_RELEASE(pCViewer);
	}

	return S_OK;
}


static const tstring c_szHelpKey = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Browser Helper Objects");

// Load any browser helpers
HRESULT CMozillaBrowser::LoadBrowserHelpers()
{
	NG_TRACE_METHOD(CMozillaBrowser::LoadBrowserHelpers);

	// IE loads browser helper objects from a branch of the registry
	// Search the branch looking for objects to load with the control.

	CRegKey cKey;
	if (cKey.Open(HKEY_LOCAL_MACHINE, c_szHelpKey.c_str(), KEY_ENUMERATE_SUB_KEYS) != ERROR_SUCCESS)
	{
		NG_TRACE(_T("No browser helper key found\n"));
		return S_FALSE;
	}

	std::vector<CLSID> cClassList;

	DWORD nKey = 0;
	LONG nResult = ERROR_SUCCESS;
	while (nResult == ERROR_SUCCESS)
	{
		TCHAR szCLSID[50];
		DWORD dwCLSID = sizeof(szCLSID) / sizeof(TCHAR);
		FILETIME cLastWrite;
		
		// Read next subkey
		nResult = RegEnumKeyEx(cKey, nKey++, szCLSID, &dwCLSID, NULL, NULL, NULL, &cLastWrite);
		if (nResult != ERROR_SUCCESS)
		{
			break;
		}

		NG_TRACE(_T("Reading helper object \"%s\"\n"), szCLSID);

		// Turn the key into a CLSID
		USES_CONVERSION;
		CLSID clsid;
		if (CLSIDFromString(T2OLE(szCLSID), &clsid) != NOERROR)
		{
			continue;
		}

		cClassList.push_back(clsid);
	}

	// Empty list?
	if (cClassList.empty())
	{
		NG_TRACE(_T("No browser helper objects found\n"));
		return S_FALSE;
	}

	// Create each object in turn
	for (std::vector<CLSID>::const_iterator i = cClassList.begin(); i != cClassList.end(); i++)
	{
		CLSID clsid = *i;
		HRESULT hr;
		CComQIPtr<IObjectWithSite, &IID_IObjectWithSite> cpObjectWithSite;

		hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, IID_IObjectWithSite, (LPVOID *) &cpObjectWithSite);
		if (FAILED(hr))
		{
			NG_TRACE(_T("Registered browser helper object cannot be created\n"));;
		}

		// Set the object to point at the browser
		cpObjectWithSite->SetSite((IWebBrowser2 *) this);

		// Store in the list
		CComUnkPtr cpUnk = cpObjectWithSite;
		m_cBrowserHelperList.push_back(cpUnk);
	}
		
	return S_OK;
}

// Release browser helpers
HRESULT CMozillaBrowser::UnloadBrowserHelpers()
{
	NG_TRACE_METHOD(CMozillaBrowser::UnloadBrowserHelpers);

	for (ObjectList::const_iterator i = m_cBrowserHelperList.begin(); i != m_cBrowserHelperList.end(); i++)
	{
		CComUnkPtr cpUnk = *i;
		CComQIPtr<IObjectWithSite, &IID_IObjectWithSite> cpObjectWithSite = cpUnk;
		if (cpObjectWithSite)
		{
			cpObjectWithSite->SetSite(NULL);
		}
	}
	m_cBrowserHelperList.clear();

	return S_OK;
}


/////////////////////////////////////////////////////////////////////////////
// IOleObject overrides

// This is an almost verbatim copy of the standard ATL implementation of
// IOleObject::InPlaceActivate but with a few lines commented out.

HRESULT CMozillaBrowser::InPlaceActivate(LONG iVerb, const RECT* prcPosRect)
{
	HRESULT hr;

	if (m_spClientSite == NULL)
		return S_OK;

	CComPtr<IOleInPlaceObject> pIPO;
	ControlQueryInterface(IID_IOleInPlaceObject, (void**)&pIPO);
	_ASSERTE(pIPO != NULL);
	if (prcPosRect != NULL)
		pIPO->SetObjectRects(prcPosRect, prcPosRect);

	if (!m_bNegotiatedWnd)
	{
		if (!m_bWindowOnly)
			// Try for windowless site
			hr = m_spClientSite->QueryInterface(IID_IOleInPlaceSiteWindowless, (void **)&m_spInPlaceSite);

		if (m_spInPlaceSite)
		{
			m_bInPlaceSiteEx = TRUE;
			m_bWndLess = SUCCEEDED(m_spInPlaceSite->CanWindowlessActivate());
			m_bWasOnceWindowless = TRUE;
		}
		else
		{
			m_spClientSite->QueryInterface(IID_IOleInPlaceSiteEx, (void **)&m_spInPlaceSite);
			if (m_spInPlaceSite)
				m_bInPlaceSiteEx = TRUE;
			else
				hr = m_spClientSite->QueryInterface(IID_IOleInPlaceSite, (void **)&m_spInPlaceSite);
		}
	}

	_ASSERTE(m_spInPlaceSite);
	if (!m_spInPlaceSite)
		return E_FAIL;

	m_bNegotiatedWnd = TRUE;

	if (!m_bInPlaceActive)
	{

		BOOL bNoRedraw = FALSE;
		if (m_bWndLess)
			m_spInPlaceSite->OnInPlaceActivateEx(&bNoRedraw, ACTIVATE_WINDOWLESS);
		else
		{
			if (m_bInPlaceSiteEx)
				m_spInPlaceSite->OnInPlaceActivateEx(&bNoRedraw, 0);
			else
			{
				HRESULT hr = m_spInPlaceSite->CanInPlaceActivate();
				if (FAILED(hr))
					return hr;
				m_spInPlaceSite->OnInPlaceActivate();
			}
		}
	}

	m_bInPlaceActive = TRUE;

	// get location in the parent window,
	// as well as some information about the parent
	//
	OLEINPLACEFRAMEINFO frameInfo;
	RECT rcPos, rcClip;
	CComPtr<IOleInPlaceFrame> spInPlaceFrame;
	CComPtr<IOleInPlaceUIWindow> spInPlaceUIWindow;
	frameInfo.cb = sizeof(OLEINPLACEFRAMEINFO);
	HWND hwndParent;
	if (m_spInPlaceSite->GetWindow(&hwndParent) == S_OK)
	{
		m_spInPlaceSite->GetWindowContext(&spInPlaceFrame,
			&spInPlaceUIWindow, &rcPos, &rcClip, &frameInfo);

		if (!m_bWndLess)
		{
			if (m_hWndCD)
			{
				::ShowWindow(m_hWndCD, SW_SHOW);
				::SetFocus(m_hWndCD);
			}
			else
			{
				HWND h = CreateControlWindow(hwndParent, rcPos);
				_ASSERTE(h == m_hWndCD);
			}
		}

		pIPO->SetObjectRects(&rcPos, &rcClip);
	}

	CComPtr<IOleInPlaceActiveObject> spActiveObject;
	ControlQueryInterface(IID_IOleInPlaceActiveObject, (void**)&spActiveObject);

	// Gone active by now, take care of UIACTIVATE
	if (DoesVerbUIActivate(iVerb))
	{
		if (!m_bUIActive)
		{
			m_bUIActive = TRUE;
			hr = m_spInPlaceSite->OnUIActivate();
			if (FAILED(hr))
				return hr;

			SetControlFocus(TRUE);
			// set ourselves up in the host.
			//
			if (spActiveObject)
			{
				if (spInPlaceFrame)
					spInPlaceFrame->SetActiveObject(spActiveObject, NULL);
				if (spInPlaceUIWindow)
					spInPlaceUIWindow->SetActiveObject(spActiveObject, NULL);
			}

// These lines are deliberately commented out to demonstrate what breaks certain
// control containers.

//			if (spInPlaceFrame)
//				spInPlaceFrame->SetBorderSpace(NULL);
//			if (spInPlaceUIWindow)
//				spInPlaceUIWindow->SetBorderSpace(NULL);
		}
	}

	m_spClientSite->ShowObject();

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GetClientSite(IOleClientSite **ppClientSite)
{
	NG_ASSERT(ppClientSite);

	// This fixes a problem in the base class which asserts if the client
	// site has not been set when this method is called. 

	HRESULT hRes = E_POINTER;
	if (ppClientSite != NULL)
	{
		*ppClientSite = NULL;
		if (m_spClientSite == NULL)
		{
			return S_OK;
		}
	}

	return IOleObjectImpl<CMozillaBrowser>::GetClientSite(ppClientSite);
}


/////////////////////////////////////////////////////////////////////////////
// IWebBrowser

HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoBack(void)
{
	NG_TRACE_METHOD(CMozillaBrowser::GoBack);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (m_pIWebShell->CanBack() == NS_OK)
	{
		m_pIWebShell->Back();
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoForward(void)
{
	NG_TRACE_METHOD(CMozillaBrowser::GoForward);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (m_pIWebShell->CanForward() == NS_OK)
	{
		m_pIWebShell->Forward();
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoHome(void)
{
	NG_TRACE_METHOD(CMozillaBrowser::GoHome);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	USES_CONVERSION;

	TCHAR * sUrl = _T("http://home.netscape.com");
#ifdef USE_NGPREF
	// Find the home page stored in prefs
	if (m_pIPref)
	{
		char *szBuffer;
		nsresult rv;
		rv = m_pIPref->CopyCharPref(c_szPrefsHomePage, &szBuffer);
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
	NG_TRACE_METHOD(CMozillaBrowser::GoSearch);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
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
	NG_TRACE_METHOD(CMozillaBrowser::Navigate);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	// Extract the URL parameter
 	nsString sCommand("view");
	nsString sUrl;
	if (URL == NULL)
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}
	else
	{
		USES_CONVERSION;
		sUrl = OLE2A(URL);
	}

	// Check for a view-source op - this is a bit kludgy
	if (sUrl.Compare(L"view-source:", PR_TRUE, 12) == 0)
 	{
 		sUrl.Left(sCommand, 11);
 		sUrl.Cut(0,12);
 	}

	// Extract the launch flags parameter
	LONG lFlags = 0;
	if (Flags &&
		Flags->vt != VT_ERROR &&
		Flags->vt != VT_EMPTY &&
		Flags->vt != VT_NULL)
	{
		CComVariant vFlags;
		if (vFlags.ChangeType(VT_I4, Flags) != S_OK)
		{
			NG_ASSERT(0);
			RETURN_E_INVALIDARG();
		}
		lFlags = vFlags.lVal;
	}


	// Extract the target frame parameter
	nsString sTargetFrame;
	if (TargetFrameName &&
		TargetFrameName->vt == VT_BSTR)
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
		CIPtr(IDispatch) spDispNew;
		VARIANT_BOOL bCancel = VARIANT_FALSE;
		
		// Test if the event sink can give us a new window to navigate into
		Fire_NewWindow2(&spDispNew, &bCancel);

		lFlags &= ~(navOpenInNewWindow);
		if ((bCancel == VARIANT_FALSE) && spDispNew)
		{
			CIPtr(IWebBrowser2) spOther = spDispNew;;
			if (spOther)
			{
				CComVariant vURL(URL);
				CComVariant vFlags(lFlags);
				return spOther->Navigate2(&vURL, &vFlags, TargetFrameName, PostData, Headers);
			}
		}

		// Can't open a new window without client suppot
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
	char *tmpCommand = sCommand.ToNewCString();
	m_pIWebShell->LoadURL(sUrl.GetUnicode(), tmpCommand, pIPostData, bModifyHistory);
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Refresh(void)
{
	NG_TRACE_METHOD(CMozillaBrowser::Refresh);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	// Reload the page
	CComVariant vRefreshType(REFRESH_NORMAL);
	return Refresh2(&vRefreshType);
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Refresh2(VARIANT __RPC_FAR *Level)
{
	NG_TRACE_METHOD(CMozillaBrowser::Refresh2);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
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
			RETURN_E_UNEXPECTED();
		}
		iRefreshLevel = (OLECMDID_REFRESHFLAG) vLevelAsInt.iVal;
	}

	// Turn the IE refresh type into the nearest NG equivalent
#ifdef NECKO
	nsLoadFlags type = nsIChannel::LOAD_NORMAL;
#else
	nsURLReloadType type = nsURLReload;
#endif
	switch (iRefreshLevel & OLECMDIDF_REFRESH_LEVELMASK)
	{
	case OLECMDIDF_REFRESH_NORMAL:
	case OLECMDIDF_REFRESH_IFEXPIRED:
	case OLECMDIDF_REFRESH_CONTINUE:
	case OLECMDIDF_REFRESH_NO_CACHE:
	case OLECMDIDF_REFRESH_RELOAD:
#ifdef NECKO
	    type = nsIChannel::LOAD_NORMAL;
#else
		type = nsURLReload;
#endif
		break;
	case OLECMDIDF_REFRESH_COMPLETELY:
#ifdef NECKO
        type = nsIHTTPChannel::BYPASS_CACHE | nsIHTTPChannel::BYPASS_PROXY;
#else
		type = nsURLReloadBypassCacheAndProxy;
#endif
		break;
	default:
		// No idea what refresh type this is supposed to be
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	m_pIWebShell->Reload(type);
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Stop()
{
	NG_TRACE_METHOD(CMozillaBrowser::Stop);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	m_pIWebShell->Stop();
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Application(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_Application);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (!NgIsValidAddress(ppDisp, sizeof(IDispatch *)))
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}

	// Return a pointer to this controls dispatch interface
	*ppDisp = (IDispatch *) this;
	(*ppDisp)->AddRef();
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Parent(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_Parent);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (!NgIsValidAddress(ppDisp, sizeof(IDispatch *)))
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
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
	NG_TRACE_METHOD(CMozillaBrowser::get_Container);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (!NgIsValidAddress(ppDisp, sizeof(IDispatch *)))
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}

	*ppDisp = NULL;
	RETURN_E_UNEXPECTED();
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Document(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_Document);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (!NgIsValidAddress(ppDisp, sizeof(IDispatch *)))
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	*ppDisp = NULL;

	// Get hold of the DOM document
	nsIDOMDocument *pIDOMDocument = nsnull;
	if (FAILED(GetDOMDocument(&pIDOMDocument)) || pIDOMDocument == nsnull)
	{
		RETURN_E_UNEXPECTED();
	}

	if (m_pDocument == NULL)
 	{	
 		CIEHtmlDocumentInstance::CreateInstance(&m_pDocument);
  		if (m_pDocument == NULL)
 		{
			RETURN_ERROR(_T("Refresh2: called while browser is invalid"), E_OUTOFMEMORY);
 		}
 		
		// addref it so it doesn't go away on us.
 		m_pDocument->AddRef();
 
 		// give it a pointer to us.  note that we shouldn't be addref'd by this call, or it would be 
 		// a circular reference.
 		m_pDocument->SetParent(this);
	}

	m_pDocument->QueryInterface(IID_IDispatch, (void **) ppDisp);
	m_pDocument->SetDOMNode(pIDOMDocument);
	pIDOMDocument->Release();

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_TopLevelContainer(VARIANT_BOOL __RPC_FAR *pBool)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_TopLevelContainer);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (!NgIsValidAddress(pBool, sizeof(VARIANT_BOOL)))
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}

	*pBool = VARIANT_TRUE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Type(BSTR __RPC_FAR *Type)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_Type);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	RETURN_ERROR(_T("get_Type: failed"), E_FAIL);
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Left(long __RPC_FAR *pl)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_Left);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (pl == NULL)
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}
	*pl = 0;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Left(long Left)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_Left);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Top(long __RPC_FAR *pl)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_Top);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (pl == NULL)
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}
	*pl = 0;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Top(long Top)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_Top);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Width(long __RPC_FAR *pl)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_Width);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (pl == NULL)
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}
	*pl = 0;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Width(long Width)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_Width);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Height(long __RPC_FAR *pl)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_Height);

    if (!IsValid())
	{
		RETURN_E_UNEXPECTED();
	}

	if (pl == NULL)
	{
		RETURN_E_INVALIDARG();
	}
	*pl = 0;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Height(long Height)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_Height);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_LocationName(BSTR __RPC_FAR *LocationName)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_LocationName);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (LocationName == NULL)
	{
		RETURN_E_INVALIDARG();
	}

	// Get the url from the web shell
	const PRUnichar *pszLocationName = nsnull;
	m_pIWebShell->GetTitle(&pszLocationName);
	if (pszLocationName == nsnull)
	{
		RETURN_E_UNEXPECTED();
	}

	// Convert the string to a BSTR
	USES_CONVERSION;
	LPOLESTR pszConvertedLocationName = W2OLE(const_cast<PRUnichar *>(pszLocationName));
	*LocationName = SysAllocString(pszConvertedLocationName);

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_LocationURL(BSTR __RPC_FAR *LocationURL)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_LocationURL);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (LocationURL == NULL)
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}

	// Get the url from the web shell
	const PRUnichar *pszUrl = nsnull;
	PRInt32 aHistoryIndex;
	m_pIWebShell->GetHistoryIndex(aHistoryIndex);
	m_pIWebShell->GetURL(aHistoryIndex, &pszUrl);
	if (pszUrl == nsnull)
	{
		return E_FAIL;
	}

	// Convert the string to a BSTR
	USES_CONVERSION;
	LPOLESTR pszConvertedUrl = W2OLE(const_cast<PRUnichar *>(pszUrl));
	*LocationURL = SysAllocString(pszConvertedUrl);

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Busy(VARIANT_BOOL __RPC_FAR *pBool)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_Busy);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (!NgIsValidAddress(pBool, sizeof(*pBool)))
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}

	*pBool = (m_bBusy) ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}


/////////////////////////////////////////////////////////////////////////////
// IWebBrowserApp

HRESULT STDMETHODCALLTYPE CMozillaBrowser::Quit(void)
{
	NG_TRACE_METHOD(CMozillaBrowser::Quit);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	// TODO fire quit event
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::ClientToWindow(int __RPC_FAR *pcx, int __RPC_FAR *pcy)
{
	NG_TRACE_METHOD(CMozillaBrowser::ClientToWindow);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	// TODO convert points to be relative to browser
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::PutProperty(BSTR szProperty, VARIANT vtValue)
{
	NG_TRACE_METHOD(CMozillaBrowser::PutProperty);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (szProperty == NULL)
	{
		RETURN_E_INVALIDARG();
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
	NG_TRACE_METHOD(CMozillaBrowser::GetProperty);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	NG_ASSERT(Property);
	NG_ASSERT_POINTER(pvtValue, VARIANT);
	
	if (Property == NULL || pvtValue == NULL)
	{
		RETURN_E_INVALIDARG();
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
	NG_TRACE_METHOD(CMozillaBrowser::get_Name);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	NG_ASSERT_POINTER(Name, BSTR);
	if (Name == NULL)
	{
		RETURN_E_INVALIDARG();
	}
	*Name = SysAllocString(L""); // TODO get Mozilla's executable name
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_HWND(long __RPC_FAR *pHWND)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_HWND);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	NG_ASSERT_POINTER(pHWND, HWND);
	if (pHWND == NULL)
	{
		RETURN_E_INVALIDARG();
	}

	*pHWND = NULL;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_FullName(BSTR __RPC_FAR *FullName)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_FullName);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	NG_ASSERT_POINTER(FullName, BSTR);
	if (FullName == NULL)
	{
		RETURN_E_INVALIDARG();
	}
	*FullName = SysAllocString(L""); // TODO get Mozilla's executable name
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Path(BSTR __RPC_FAR *Path)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_Path);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	NG_ASSERT_POINTER(Path, BSTR);
	if (Path == NULL)
	{
		RETURN_E_INVALIDARG();
	}
	*Path = SysAllocString(L""); // TODO get Mozilla's path
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Visible(VARIANT_BOOL __RPC_FAR *pBool)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_Visible);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	NG_ASSERT_POINTER(pBool, int);
	if (pBool == NULL)
	{
		RETURN_E_INVALIDARG();
	}
	*pBool = VARIANT_FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Visible(VARIANT_BOOL Value)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_Visible);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_StatusBar(VARIANT_BOOL __RPC_FAR *pBool)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_StatusBar);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	NG_ASSERT_POINTER(pBool, int);
	if (pBool == NULL)
	{
		RETURN_E_INVALIDARG();
	}
	*pBool = VARIANT_FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_StatusBar(VARIANT_BOOL Value)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_StatusBar);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_StatusText(BSTR __RPC_FAR *StatusText)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_StatusText);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	NG_ASSERT_POINTER(StatusText, BSTR);
	if (StatusText == NULL)
	{
		RETURN_E_INVALIDARG();
	}
	*StatusText = SysAllocString(L""); // TODO
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_StatusText(BSTR StatusText)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_StatusText);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_ToolBar(int __RPC_FAR *Value)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_ToolBar);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	NG_ASSERT_POINTER(Value, int);
	if (Value == NULL)
	{
		RETURN_E_INVALIDARG();
	}
	*Value = FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_ToolBar(int Value)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_ToolBar);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	// No toolbar in control!
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_MenuBar(VARIANT_BOOL __RPC_FAR *Value)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_MenuBar);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	NG_ASSERT_POINTER(Value, int);
	if (Value == NULL)
	{
		RETURN_E_INVALIDARG();
	}
	*Value = VARIANT_FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_MenuBar(VARIANT_BOOL Value)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_MenuBar);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	// No menu in control!
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_FullScreen(VARIANT_BOOL __RPC_FAR *pbFullScreen)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_FullScreen);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	NG_ASSERT_POINTER(pbFullScreen, VARIANT_BOOL);
	if (pbFullScreen == NULL)
	{
		RETURN_E_INVALIDARG();
	}
	*pbFullScreen = VARIANT_FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_FullScreen(VARIANT_BOOL bFullScreen)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_FullScreen);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	// No fullscreen mode in control!
	return S_OK;
}



/////////////////////////////////////////////////////////////////////////////
// IWebBrowser2

HRESULT STDMETHODCALLTYPE CMozillaBrowser::Navigate2(VARIANT __RPC_FAR *URL, VARIANT __RPC_FAR *Flags, VARIANT __RPC_FAR *TargetFrameName, VARIANT __RPC_FAR *PostData, VARIANT __RPC_FAR *Headers)
{
	NG_TRACE_METHOD(CMozillaBrowser::Navigate2);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (URL == NULL)
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}

	CComVariant vURLAsString;
	if (vURLAsString.ChangeType(VT_BSTR, URL) != S_OK)
	{
		RETURN_E_INVALIDARG();
	}

	return Navigate(vURLAsString.bstrVal, Flags, TargetFrameName, PostData, Headers);
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::QueryStatusWB(OLECMDID cmdID, OLECMDF __RPC_FAR *pcmdf)
{
	NG_TRACE_METHOD(CMozillaBrowser::QueryStatusWB);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (pcmdf == NULL)
	{
		RETURN_E_INVALIDARG();
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
	NG_TRACE_METHOD(CMozillaBrowser::ExecWB);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	// Call through to IOleCommandTarget::Exec
	HRESULT hr;
	hr = Exec(NULL, cmdID, cmdexecopt, pvaIn, pvaOut);
	return hr;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::ShowBrowserBar(VARIANT __RPC_FAR *pvaClsid, VARIANT __RPC_FAR *pvarShow, VARIANT __RPC_FAR *pvarSize)
{
	NG_TRACE_METHOD(CMozillaBrowser::ShowBrowserBar);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_ReadyState(READYSTATE __RPC_FAR *plReadyState)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_ReadyState);

	if (plReadyState == NULL)
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}

	*plReadyState = m_nBrowserReadyState;

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Offline(VARIANT_BOOL __RPC_FAR *pbOffline)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_Offline);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (pbOffline == NULL)
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}

	*pbOffline = VARIANT_FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Offline(VARIANT_BOOL bOffline)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_Offline);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Silent(VARIANT_BOOL __RPC_FAR *pbSilent)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_Silent);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (pbSilent == NULL)
	{
		RETURN_E_INVALIDARG();
	}

	*pbSilent = VARIANT_FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Silent(VARIANT_BOOL bSilent)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_Silent);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	// IGNORE
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_RegisterAsBrowser(VARIANT_BOOL __RPC_FAR *pbRegister)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_RegisterAsBrowser);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (pbRegister == NULL)
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}

	*pbRegister = VARIANT_FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_RegisterAsBrowser(VARIANT_BOOL bRegister)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_RegisterAsBrowser);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_RegisterAsDropTarget(VARIANT_BOOL __RPC_FAR *pbRegister)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_RegisterAsDropTarget);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (pbRegister == NULL)
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}

	*pbRegister = m_bDropTarget ? VARIANT_TRUE : VARIANT_FALSE;
	return S_OK;
}



static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
	::RevokeDragDrop(hwnd);
	return TRUE;
}
 

HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_RegisterAsDropTarget(VARIANT_BOOL bRegister)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_RegisterAsDropTarget);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	// Register the window as a drop target
	if (bRegister == VARIANT_TRUE)
	{
		if (!m_bDropTarget)
		{
			CDropTargetInstance *pDropTarget = NULL;
			CDropTargetInstance::CreateInstance(&pDropTarget);
			if (pDropTarget)
			{
				pDropTarget->AddRef();

				// Ask the site if it wants to replace this drop target for another one
				CIPtr(IDropTarget) spDropTarget;
				CIPtr(IDocHostUIHandler) spDocHostUIHandler = m_spClientSite;
				if (spDocHostUIHandler)
				{
					if (spDocHostUIHandler->GetDropTarget(pDropTarget, &spDropTarget) != S_OK)
					{
						spDropTarget = pDropTarget;
					}
				}
				if (spDropTarget)
				{
					m_bDropTarget = TRUE;
					::RegisterDragDrop(m_hWnd, spDropTarget);
				}

				pDropTarget->Release();
			}

			// Now revoke any child window drop targets and pray they aren't
			// reset by the layout engine.

			::EnumChildWindows(m_hWnd, EnumChildProc, (LPARAM) this);
		}
	}
	else
	{
		if (m_bDropTarget)
		{
			m_bDropTarget = FALSE;
			::RevokeDragDrop(m_hWnd);
		}
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_TheaterMode(VARIANT_BOOL __RPC_FAR *pbRegister)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_TheaterMode);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (pbRegister == NULL)
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}

	// No theatermode!
	*pbRegister = VARIANT_FALSE;

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_TheaterMode(VARIANT_BOOL bRegister)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_TheaterMode);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_AddressBar(VARIANT_BOOL __RPC_FAR *Value)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_AddressBar);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (Value == NULL)
	{
		RETURN_E_INVALIDARG();
	}

	*Value = VARIANT_FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_AddressBar(VARIANT_BOOL Value)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_AddressBar);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Resizable(VARIANT_BOOL __RPC_FAR *Value)
{
	NG_TRACE_METHOD(CMozillaBrowser::get_Resizable);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	if (Value == NULL)
	{
		NG_ASSERT(0);
		RETURN_E_INVALIDARG();
	}

	*Value = VARIANT_FALSE;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Resizable(VARIANT_BOOL Value)
{
	NG_TRACE_METHOD(CMozillaBrowser::put_Resizable);

    if (!IsValid())
	{
		NG_ASSERT(0);
		RETURN_E_UNEXPECTED();
	}

	return S_OK;
}

///////////////////////////////////////////////////////////////////////////////
// IOleCommandTarget implementation

// IOleCommandTarget is a (bloody awful and complicated) generic mechanism
// for querying for and executing commands on an object. It is used in IE
// for printing, so that a client prints a page of HTML by calling Exec
// (or ExecWB) on the control using the command OLECMDID_PRINT.

// To keep things open, all supported commands are in the table below which
// can be extended when and if necessary.

static const GUID CGID_IWebBrowser =
{ 0xED016940L, 0xBD5B, 0x11cf, {0xBA, 0x4E, 0x00, 0xC0, 0x4F, 0xD7, 0x08, 0x16} };

#define HTMLID_FIND 1
#define HTMLID_VIEWSOURCE 2
#define HTMLID_OPTIONS 3

struct OleCommandInfo
{
	ULONG    nCmdID;
	const GUID    *pCmdGUID;
	ULONG    nWindowsCmdID;
	wchar_t *szVerbText;
	wchar_t *szStatusText;
};

struct OleExecData
{
	const GUID *pguidCmdGroup;
	DWORD nCmdID;
	DWORD nCmdexecopt;
	VARIANT *pvaIn;
	VARIANT *pvaOut;
};

static OleCommandInfo s_aSupportedCommands[] =
{
	{ OLECMDID_PRINT, NULL, ID_PRINT, L"Print", L"Print the page" },
	{ OLECMDID_SAVEAS, NULL, 0, L"SaveAs", L"Save the page" },
	{ OLECMDID_PAGESETUP, NULL, ID_PAGESETUP, L"Page Setup", L"Page Setup" },
	{ OLECMDID_PROPERTIES, NULL, 0, L"Properties", L"Show page properties" },
	{ OLECMDID_CUT, NULL, 0, L"Cut", L"Cut selection" },
	{ OLECMDID_COPY, NULL, 0, L"Copy", L"Copy selection" },
	{ OLECMDID_PASTE, NULL, 0, L"Paste", L"Paste as selection" },
	{ OLECMDID_UNDO, NULL, 0, L"Undo", L"Undo" },
	{ OLECMDID_REDO, NULL, 0, L"Redo", L"Redo" },
	{ OLECMDID_SELECTALL, NULL, 0, L"SelectAll", L"Select all" },
	{ OLECMDID_REFRESH, NULL, 0, L"Refresh", L"Refresh" },
	{ OLECMDID_STOP, NULL, 0, L"Stop", L"Stop" },
	{ OLECMDID_ONUNLOAD, NULL, 0, L"OnUnload", L"OnUnload" },

	// Unsupported IE 4.x command group
	{ HTMLID_FIND, &CGID_IWebBrowser, 0, L"Find", L"Find" },
	{ HTMLID_VIEWSOURCE, &CGID_IWebBrowser, 0, L"ViewSource", L"View Source" },
	{ HTMLID_OPTIONS, &CGID_IWebBrowser, 0, L"Options", L"Options" },
};


HRESULT STDMETHODCALLTYPE CMozillaBrowser::QueryStatus(const GUID __RPC_FAR *pguidCmdGroup, ULONG cCmds, OLECMD __RPC_FAR prgCmds[], OLECMDTEXT __RPC_FAR *pCmdText)
{
	if (prgCmds == NULL)
	{
		return E_INVALIDARG;
	}

	BOOL bCmdGroupFound = FALSE;
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
			OleCommandInfo *pCI = &s_aSupportedCommands[nSupported];

			if (pguidCmdGroup && pCI->pCmdGUID && memcmp(pguidCmdGroup, pCI->pCmdGUID, sizeof(GUID)) == 0)
			{
				continue;
			}
			bCmdGroupFound = TRUE;

			if (pCI->nCmdID != prgCmds[nCmd].cmdID)
			{
				continue;
			}

			// Command is supported so flag it and possibly enable it
			prgCmds[nCmd].cmdf = OLECMDF_SUPPORTED;
			if (pCI->nWindowsCmdID != 0)
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
					pszTextToCopy = pCI->szVerbText;
				}
				else if (pCmdText->cmdtextf & OLECMDTEXTF_STATUS)
				{
					pszTextToCopy = pCI->szStatusText;
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
	
	// Was the command group found?
	if (!bCmdGroupFound)
	{
		OLECMDERR_E_UNKNOWNGROUP;
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Exec(const GUID __RPC_FAR *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT __RPC_FAR *pvaIn, VARIANT __RPC_FAR *pvaOut)
{
	BOOL bCmdGroupFound = FALSE;

	// Search the support command list
	int nSupportedCount = sizeof(s_aSupportedCommands) / sizeof(s_aSupportedCommands[0]);
	for (int nSupported = 0; nSupported < nSupportedCount; nSupported++)
	{
		OleCommandInfo *pCI = &s_aSupportedCommands[nSupported];

		if (pguidCmdGroup && pCI->pCmdGUID && memcmp(pguidCmdGroup, pCI->pCmdGUID, sizeof(GUID)) == 0)
		{
			continue;
		}
		bCmdGroupFound = TRUE;

		if (pCI->nCmdID != nCmdID)
		{
			continue;
		}

		// Command is supported but not implemented
		if (pCI->nWindowsCmdID == 0)
		{
			continue;
		}

		// Send ourselves a WM_COMMAND windows message with the associated
		// identifier and exec data
		OleExecData cData;
		cData.pguidCmdGroup = pguidCmdGroup;
		cData.nCmdID = nCmdID;
		cData.nCmdexecopt = nCmdexecopt;
		cData.pvaIn = pvaIn;
		cData.pvaOut = pvaOut;
		SendMessage(WM_COMMAND, LOWORD(pCI->nWindowsCmdID), (LPARAM) &cData);

		return S_OK;
	}

	// Was the command group found?
	if (!bCmdGroupFound)
	{
		OLECMDERR_E_UNKNOWNGROUP;
	}

	return OLECMDERR_E_NOTSUPPORTED;
}

