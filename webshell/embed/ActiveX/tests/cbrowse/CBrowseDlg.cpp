// CBrowseDlg.cpp : implementation file
//

#include "stdafx.h"
#include "cbrowse.h"
#include "CBrowseDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static const CLSID clsidMozilla =
{ 0x1339B54C, 0x3453, 0x11D2, { 0x93, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

/////////////////////////////////////////////////////////////////////////////
// CBrowseDlg dialog

CBrowseDlg::CBrowseDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CBrowseDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CBrowseDlg)
	m_sURL = _T("");
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	
	m_sURL = _T("http://www.mozilla.com");
	m_pControlSite = NULL;
}

void CBrowseDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CBrowseDlg)
	DDX_Text(pDX, IDC_URL, m_sURL);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CBrowseDlg, CDialog)
	//{{AFX_MSG_MAP(CBrowseDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_GO, OnGo)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBrowseDlg message handlers

BOOL CBrowseDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// Get the position of the browser marker
	CRect rcMarker;
	GetDlgItem(IDC_BROWSER_MARKER)->GetWindowRect(&rcMarker);
	ScreenToClient(rcMarker);

	CControlSiteInstance::CreateInstance(&m_pControlSite);

	if (m_pControlSite)
	{
		m_pControlSite->Attach(clsidMozilla, GetSafeHwnd(), rcMarker, NULL);
	}

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CBrowseDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CBrowseDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CBrowseDlg::OnGo() 
{
	UpdateData();

	// If there's a browser control, use it to navigate to the specified URL

	if (m_pControlSite)
	{
		IUnknown *pIUnkBrowser = NULL;
		
		m_pControlSite->GetControlUnknown(&pIUnkBrowser);
		if (pIUnkBrowser)
		{
			IWebBrowser *pIWebBrowser = NULL;
			pIUnkBrowser->QueryInterface(IID_IWebBrowser, (void **) &pIWebBrowser);
			if (pIWebBrowser)
			{
				BSTR bstrURL = m_sURL.AllocSysString();
				pIWebBrowser->Navigate(bstrURL, NULL, NULL, NULL, NULL);
				::SysFreeString(bstrURL);
				pIWebBrowser->Release();
			}
			pIUnkBrowser->Release();
		}
	}
}
