// MozillaBrowser.cpp : Implementation of CMozillaBrowser
#include "stdafx.h"

#include <string.h>

#include "MozillaControl.h"
#include "MozillaBrowser.h"


#define NS_DEFAULT_PREFS			"prefs.js"
#define NS_DEFAULT_PREFS_HOMEPAGE	"browser.startup.homepage"

extern "C" void NS_SetupRegistry();


/////////////////////////////////////////////////////////////////////////////
// CMozillaBrowser

CMozillaBrowser::CMozillaBrowser()
{
	m_bWindowOnly = TRUE;
	m_bWndLess = FALSE;

	// Initialize layout interfaces
    m_pIWebShell = nsnull;
#ifdef USE_NGPREF
	m_pIPref = nsnull;
#endif

	// Create the container that handles some things for us
	m_pWebShellContainer = NULL;

	m_bBusy = FALSE;

	PL_InitializeEventsLib("");
	// Register everything
	NS_SetupRegistry();
}


CMozillaBrowser::~CMozillaBrowser()
{
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
	// Clip the child windows out of paint operations
	SetWindowLong(GWL_STYLE, GetWindowLong(GWL_STYLE) | WS_CLIPCHILDREN);

    // Create the NGLayout WebShell
    CreateWebShell();

	return 0;
}


LRESULT CMozillaBrowser::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
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
    // Pass resize information down to the WebShell...
    if (m_pIWebShell)
	{
		m_pIWebShell->SetBounds(0, 0, LOWORD(lParam), HIWORD(lParam));
    }
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
		DrawText(di.hdcDraw, _T("Mozilla Browser (no window)"), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}
	return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);

HRESULT CMozillaBrowser::CreateWebShell() 
{
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
	if (NS_OK != rv) {
		return E_FAIL;
	}
	m_pIPref->Startup(NS_DEFAULT_PREFS);
#endif
	
	// Create the web shell object
	rv = nsRepository::CreateInstance(kWebShellCID, nsnull,
									kIWebShellIID,
									(void**)&m_pIWebShell);
	if (NS_OK != rv)
	{
		NG_ASSERT(0);
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
		memset(szBuffer, 0, sizeof(szBuffer);
		rv = m_pIPref->GetCharPref(NS_DEFAULT_PREFS_HOMEPAGE, szBuffer, sizeof(szBuffer));
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
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	// TODO find and navigate to the search page stored in prefs
	//      and not this hard coded address

	TCHAR * sUrl = _T("http://search.netscape.com");
	
	USES_CONVERSION;
	Navigate(T2OLE(sUrl), NULL, NULL, NULL, NULL);
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Navigate(BSTR URL, VARIANT __RPC_FAR *Flags, VARIANT __RPC_FAR *TargetFrameName, VARIANT __RPC_FAR *PostData, VARIANT __RPC_FAR *Headers)
{
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
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}
	return E_FAIL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Left(long __RPC_FAR *pl)
{
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
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Top(long __RPC_FAR *pl)
{
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
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Width(long __RPC_FAR *pl)
{
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
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Height(long __RPC_FAR *pl)
{
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
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_LocationName(BSTR __RPC_FAR *LocationName)
{
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
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	NG_ASSERT_POINTER(pHWND, HWND);
	if (pHWND == NULL)
	{
	}
	pHWND = NULL;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_FullName(BSTR __RPC_FAR *FullName)
{
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
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_StatusBar(VARIANT_BOOL __RPC_FAR *pBool)
{
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
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_StatusText(BSTR __RPC_FAR *StatusText)
{
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
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_ToolBar(int __RPC_FAR *Value)
{
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
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::ExecWB(OLECMDID cmdID, OLECMDEXECOPT cmdexecopt, VARIANT __RPC_FAR *pvaIn, VARIANT __RPC_FAR *pvaOut)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::ShowBrowserBar(VARIANT __RPC_FAR *pvaClsid, VARIANT __RPC_FAR *pvarShow, VARIANT __RPC_FAR *pvarSize)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_ReadyState(READYSTATE __RPC_FAR *plReadyState)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Offline(VARIANT_BOOL __RPC_FAR *pbOffline)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Offline(VARIANT_BOOL bOffline)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Silent(VARIANT_BOOL __RPC_FAR *pbSilent)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Silent(VARIANT_BOOL bSilent)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_RegisterAsBrowser(VARIANT_BOOL __RPC_FAR *pbRegister)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_RegisterAsBrowser(VARIANT_BOOL bRegister)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_RegisterAsDropTarget(VARIANT_BOOL __RPC_FAR *pbRegister)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_RegisterAsDropTarget(VARIANT_BOOL bRegister)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_TheaterMode(VARIANT_BOOL __RPC_FAR *pbRegister)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_TheaterMode(VARIANT_BOOL bRegister)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_AddressBar(VARIANT_BOOL __RPC_FAR *Value)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_AddressBar(VARIANT_BOOL Value)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Resizable(VARIANT_BOOL __RPC_FAR *Value)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Resizable(VARIANT_BOOL Value)
{
    if (!IsValid())
	{
		NG_ASSERT(0);
		return E_UNEXPECTED;
	}

	return E_NOTIMPL;
}


