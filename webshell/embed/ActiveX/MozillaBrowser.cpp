// MozillaBrowser.cpp : Implementation of CMozillaBrowser
#include "stdafx.h"
#include "MozillaControl.h"
#include "MozillaBrowser.h"

/////////////////////////////////////////////////////////////////////////////
// CMozillaBrowser

extern "C" void NS_SetupRegistry();

CMozillaBrowser::CMozillaBrowser()
{
	m_bWindowOnly = TRUE;
	m_bWndLess = FALSE;

    m_pIWebShell = nsnull;

	// Create the container that handles some things for us
	m_pWebShellContainer = NULL;

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
		&IID_IMozillaBrowser,
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
    /* Destroy NGLayout... */
    if (m_pIWebShell != nsnull)
	{
		m_pIWebShell->Destroy();
        NS_RELEASE(m_pIWebShell);
		m_pWebShellContainer->Release();
		m_pWebShellContainer = NULL;
	}
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

	rv = NSRepository::CreateInstance(kWebShellCID, nsnull,
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
	m_pWebShellContainer = new CWebShellContainer;
	m_pWebShellContainer->AddRef();

	m_pIWebShell->SetContainer((nsIWebShellContainer*) m_pWebShellContainer);
//	m_pIWebShell->SetObserver((nsIStreamObserver*)this);
//	m_pIWebShell->SetPrefs(aPrefs);
	m_pIWebShell->Show();

	// TODO
	// -- remove
	// Use the IWebBrowser::Navigate() method
	USES_CONVERSION;
	LPOLESTR pszUrl = T2OLE(_T("http://www.mozilla.org"));
	BSTR bstrUrl = ::SysAllocString(pszUrl);
	Navigate(bstrUrl, NULL, NULL, NULL, NULL);
	::SysFreeString(bstrUrl);
	// -- remove

	return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// IWebBrowser

HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoBack(void)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	if (m_pIWebShell->CanBack())
	{
		m_pIWebShell->Back();
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoForward(void)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	if (m_pIWebShell->CanForward())
	{
		m_pIWebShell->Forward();
	}

	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoHome(void)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO find and navigate to the home page somehow
	USES_CONVERSION;
	Navigate(T2OLE(_T("http://home.netscape.com")), NULL, NULL, NULL, NULL);
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoSearch(void)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO find and navigate to the search page somehow
	
	USES_CONVERSION;
	Navigate(T2OLE(_T("http://search.netscape.com")), NULL, NULL, NULL, NULL);
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Navigate(BSTR URL, VARIANT __RPC_FAR *Flags, VARIANT __RPC_FAR *TargetFrameName, VARIANT __RPC_FAR *PostData, VARIANT __RPC_FAR *Headers)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// Extract the URL parameter
	nsString sUrl;
	if (URL == NULL)
	{
//		ASSERT(0);
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
	if (TargetFrameName)
	{
		USES_CONVERSION;
		CComVariant vTargetFrame;
		if (VariantChangeType(TargetFrameName, &vTargetFrame, 0, VT_BSTR) != S_OK)
		{
			return E_INVALIDARG;
		}
		sTargetFrame = nsString(OLE2A(vTargetFrame.bstrVal));
	}

	// Extract the post data parameter
	nsString sPostData;
	if (PostData)
	{
		USES_CONVERSION;
		CComVariant vPostData;
		if (VariantChangeType(PostData, &vPostData, 0, VT_BSTR) != S_OK)
		{
			return E_INVALIDARG;
		}
		sPostData = nsString(OLE2A(vPostData.bstrVal));
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
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// Reload the page
	CComVariant vRefreshType(REFRESH_NORMAL);
	return Refresh2(&vRefreshType);
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Refresh2(VARIANT __RPC_FAR *Level)
{
    if (m_pIWebShell == nsnull)
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
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	m_pIWebShell->Stop();
	
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Application(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Parent(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Container(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Document(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_TopLevelContainer(VARIANT_BOOL __RPC_FAR *pBool)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Type(BSTR __RPC_FAR *Type)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Left(long __RPC_FAR *pl)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Left(long Left)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Top(long __RPC_FAR *pl)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Top(long Top)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Width(long __RPC_FAR *pl)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Width(long Width)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Height(long __RPC_FAR *pl)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Height(long Height)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_LocationName(BSTR __RPC_FAR *LocationName)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_LocationURL(BSTR __RPC_FAR *LocationURL)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Busy(VARIANT_BOOL __RPC_FAR *pBool)
{
    if (m_pIWebShell == nsnull)
	{
		return E_UNEXPECTED;
	}

	// TODO

	return E_NOTIMPL;
}
