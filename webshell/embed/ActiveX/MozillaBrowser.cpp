// MozillaBrowser.cpp : Implementation of CMozillaBrowser
#include "stdafx.h"
#include "MozillaControl.h"
#include "MozillaBrowser.h"


#define NS_DEFAULT_PREFS			"prefs.js"
#define NS_DEFAULT_PREFS_HOMEPAGE	"browser.startup.homepage"

/////////////////////////////////////////////////////////////////////////////
// CMozillaBrowser

extern "C" void NS_SetupRegistry();

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
    /* Destroy layout... */
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
    /* Pass resize information down to the WebShell... */
    if (m_pIWebShell)
	{
		m_pIWebShell->SetBounds(0, 0, LOWORD(lParam), HIWORD(lParam));
    }
	return 0;
}


BOOL CMozillaBrowser::IsValid()
{
	if (m_pIWebShell == nsnull)
	{
		return FALSE;
	}

	return TRUE;
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
		return E_UNEXPECTED;
	}

	RECT rcLocation;
	GetClientRect(&rcLocation);

	nsresult rv;

	// Load preferences
#ifdef USE_NGPREF
	rv = nsRepository::CreateInstance(kPrefCID, NULL, kIPrefIID, (void **) &m_pIPref);
	if (NS_OK != rv) {
		return E_FAIL;
	}
	m_pIPref->Startup(NS_DEFAULT_PREFS);
#endif
	
	rv = nsRepository::CreateInstance(kWebShellCID, nsnull,
									kIWebShellIID,
									(void**)&m_pIWebShell);
	if (NS_OK != rv)
	{
		return E_FAIL;
	}

	nsRect r;
	r.x = 0;
	r.y = 0;
	r.width  = rcLocation.right  - rcLocation.left;
	r.height = rcLocation.bottom - rcLocation.top;

	PRBool aAllowPlugins = PR_FALSE;

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
		return E_UNEXPECTED;
	}

	USES_CONVERSION;

	// Find the home page stored in prefs
	TCHAR * sUrl = _T("http://home.netscape.com");
#ifdef USE_NGPREF
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
		return E_UNEXPECTED;
	}

	// Extract the URL parameter
	nsString sUrl;
	if (URL == NULL)
	{
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
	nsString sPostData;
	if (PostData && PostData->vt == VT_BSTR)
	{
		USES_CONVERSION;
		sPostData = nsString(OLE2A(PostData->bstrVal));
	}


	// TODO Extract the headers parameter
	PRBool bModifyHistory = PR_TRUE;

	// Check the navigation flags
	if (lFlags & navOpenInNewWindow)
	{
		// TODO open in new window?
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

	// Create post data from string
	nsIPostData *pIPostData = nsnull;
//	NS_NewPostData(PR_FALSE, sPostData, &pIPostData);

	// TODO find the correct target frame

	// Load the URL
	m_pIWebShell->LoadURL(sUrl, pIPostData, bModifyHistory);
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Refresh(void)
{
    if (!IsValid())
	{
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
		return E_UNEXPECTED;
	}

	// TODO check specified type

	nsReloadType type = nsReload;
	m_pIWebShell->Reload(type);
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Stop()
{
    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	m_pIWebShell->Stop();
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Application(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Parent(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Container(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Document(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_TopLevelContainer(VARIANT_BOOL __RPC_FAR *pBool)
{
    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Type(BSTR __RPC_FAR *Type)
{
    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Left(long __RPC_FAR *pl)
{
    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	if (pl == NULL)
	{
		return E_INVALIDARG;
	}
	*pl = 0; // TODO
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Left(long Left)
{
    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	// TODO
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Top(long __RPC_FAR *pl)
{
    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	if (pl == NULL)
	{
		return E_INVALIDARG;
	}
	*pl = 0; // TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Top(long Top)
{
    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	// TODO
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Width(long __RPC_FAR *pl)
{
    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	if (pl == NULL)
	{
		return E_INVALIDARG;
	}
	*pl = 0; // TODO
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Width(long Width)
{
    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	// TODO
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
	*pl = 0; // TODO
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Height(long Height)
{
    if (!IsValid())
	{
		return E_UNEXPECTED;
	}

	// TODO
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_LocationName(BSTR __RPC_FAR *LocationName)
{
    if (!IsValid())
	{
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
		return E_UNEXPECTED;
	}

	if (LocationURL == NULL)
	{
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
		return E_UNEXPECTED;
	}

	if (!NgIsValidAddress(pBool, sizeof(*pBool)))
	{
		return E_INVALIDARG;
	}

	*pBool = (m_bBusy) ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}
