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

#include <math.h>
#include <fpieee.h>
#include <float.h>

#include <signal.h>

void __cdecl fperr(int sig)
{
	CString sError;
	sError.Format("FP Error %08x", sig);
	AfxMessageBox(sError);
}

TCHAR *aURLs[] =
{
	_T("http://www.mozilla.org"),
	_T("http://www.yahoo.com"),
	_T("http://www.netscape.com"),
	_T("http://www.microsoft.com")
};

CBrowseDlg *CBrowseDlg::m_pBrowseDlg = NULL;

/////////////////////////////////////////////////////////////////////////////
// CBrowseDlg dialog

CBrowseDlg::CBrowseDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CBrowseDlg::IDD, pParent)
{
	signal(SIGFPE, fperr);
	double x = 0.0;
	double y = 1.0/x;

	//{{AFX_DATA_INIT(CBrowseDlg)
	m_szTestDescription = _T("");
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	
	m_pBrowseDlg = this;
	m_pControlSite = NULL;
	m_clsid = CLSID_NULL;

	CWinApp *pApp = AfxGetApp();
	m_szTestURL = pApp->GetProfileString(SECTION_TEST, KEY_TESTURL, KEY_TESTURL_DEFAULTVALUE);
}

void CBrowseDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CBrowseDlg)
	DDX_Control(pDX, IDC_RUNTEST, m_btnRunTest);
	DDX_Control(pDX, IDC_URL, m_cmbURLs);
	DDX_Control(pDX, IDC_TESTLIST, m_tcTests);
	DDX_Control(pDX, IDC_LISTMESSAGES, m_lbMessages);
	DDX_Text(pDX, IDC_TESTDESCRIPTION, m_szTestDescription);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CBrowseDlg, CDialog)
	//{{AFX_MSG_MAP(CBrowseDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_GO, OnGo)
	ON_BN_CLICKED(IDC_RUNTEST, OnRuntest)
	ON_BN_CLICKED(IDC_BACKWARD, OnBackward)
	ON_BN_CLICKED(IDC_FORWARD, OnForward)
	ON_NOTIFY(TVN_SELCHANGED, IDC_TESTLIST, OnSelchangedTestlist)
	ON_NOTIFY(NM_DBLCLK, IDC_TESTLIST, OnDblclkTestlist)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

#define IL_CLOSEDFOLDER	0
#define IL_OPENFOLDER	1
#define IL_TEST			2
#define IL_TESTFAILED	3
#define IL_TESTPASSED	4

/////////////////////////////////////////////////////////////////////////////
// CBrowseDlg message handlers

BOOL CBrowseDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// Image list
	m_cImageList.Create(16, 16, ILC_COLOR | ILC_MASK, 0, 10);
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_CLOSEDFOLDER));
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_OPENFOLDER));
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_TEST));
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_TESTFAILED));
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_TESTPASSED));
	m_tcTests.SetImageList(&m_cImageList, TVSIL_NORMAL);
	for (int i = 0; i < nTestSets; i++)
	{
		TestSet *pTestSet = &aTestSets[i];
		HTREEITEM hParent = m_tcTests.InsertItem(pTestSet->szName, IL_CLOSEDFOLDER, IL_CLOSEDFOLDER);
		m_tcTests.SetItemData(hParent, (DWORD) pTestSet);

		for (int j = 0; j < pTestSet->nTests; j++)
		{
			Test *pTest = &pTestSet->aTests[j];
			HTREEITEM hTest = m_tcTests.InsertItem(pTest->szName, IL_TEST, IL_TEST, hParent);
			if (hTest)
			{
				m_tcTests.SetItemData(hTest, (DWORD) pTest);
			}
		}
	}

	// Set up some URLs. The first couple are internal
	m_cmbURLs.AddString(m_szTestURL);
	for (i = 0; i < sizeof(aURLs) / sizeof(aURLs[0]); i++)
	{
		m_cmbURLs.AddString(aURLs[i]);
	}
	m_cmbURLs.SetCurSel(0);

	// Get the position of the browser marker
	CRect rcMarker;
	GetDlgItem(IDC_BROWSER_MARKER)->GetWindowRect(&rcMarker);
	ScreenToClient(rcMarker);

	GetDlgItem(IDC_BROWSER_MARKER)->DestroyWindow();

	CControlSiteInstance::CreateInstance(&m_pControlSite);
	if (m_pControlSite)
	{
		PropertyList pl;
		m_pControlSite->Create(m_clsid, pl);
		m_pControlSite->Attach(GetSafeHwnd(), rcMarker, NULL);
		m_pControlSite->SetPosition(rcMarker);
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
		CPaintDC dc(this);

		m_pControlSite->Draw(dc.m_hDC);
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CBrowseDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

HRESULT CBrowseDlg::GetWebBrowser(IWebBrowser **pWebBrowser)
{
	if (pWebBrowser == NULL)
	{
		return E_INVALIDARG;
	}

	*pWebBrowser = NULL;

	if (m_pControlSite)
	{
		IUnknown *pIUnkBrowser = NULL;
		m_pControlSite->GetControlUnknown(&pIUnkBrowser);
		if (pIUnkBrowser)
		{
			pIUnkBrowser->QueryInterface(IID_IWebBrowser, (void **) pWebBrowser);
			if (*pWebBrowser)
			{
				return S_OK;
			}
			pIUnkBrowser->Release();
		}
	}

	return E_FAIL;
}

void CBrowseDlg::OnGo() 
{
	UpdateData();

	IWebBrowser *pIWebBrowser = NULL;
	if (SUCCEEDED(GetWebBrowser(&pIWebBrowser)))
	{
		int nItem = m_cmbURLs.GetCurSel();
		CString szURL = (nItem == 0) ? m_szTestURL : aURLs[nItem - 1];

		BSTR bstrURL = szURL.AllocSysString();
		pIWebBrowser->Navigate(bstrURL, NULL, NULL, NULL, NULL);
		::SysFreeString(bstrURL);
		pIWebBrowser->Release();
	}
}

void CBrowseDlg::OnBackward() 
{
	IWebBrowser *pIWebBrowser = NULL;
	if (SUCCEEDED(GetWebBrowser(&pIWebBrowser)))
	{
		pIWebBrowser->GoBack();
		pIWebBrowser->Release();
	}
}

void CBrowseDlg::OnForward() 
{
	IWebBrowser *pIWebBrowser = NULL;
	if (SUCCEEDED(GetWebBrowser(&pIWebBrowser)))
	{
		pIWebBrowser->GoForward();
		pIWebBrowser->Release();
	}
}

void CBrowseDlg::RunTestSet(TestSet *pTestSet)
{
	ASSERT(pTestSet);
	if (pTestSet == NULL)
	{
		return;
	}

	for (int j = 0; j < pTestSet->nTests; j++)
	{
		Test *pTest = &pTestSet->aTests[j];
		RunTest(pTest);
	}
}


TestResult CBrowseDlg::RunTest(Test *pTest)
{
	ASSERT(pTest);
	TestResult nResult = trFailed;

	CString szMsg;
	szMsg.Format(_T("Running test \"%s\""), pTest->szName);
	OutputString(szMsg);

	if (pTest && pTest->pfn)
	{
		BrowserInfo cInfo;

		cInfo.clsid = m_clsid;
		cInfo.pControlSite = m_pControlSite;
		cInfo.pIUnknown = NULL;
		cInfo.pBrowseDlg = this;
		cInfo.szTestURL = m_szTestURL;
		if (cInfo.pControlSite)
		{
			cInfo.pControlSite->GetControlUnknown(&cInfo.pIUnknown);
		}
		nResult = pTest->pfn(cInfo);
		pTest->nLastResult = nResult;
		if (cInfo.pIUnknown)
		{
			cInfo.pIUnknown->Release();
		}
	}

	switch (nResult)
	{
	case trFailed:
		OutputString(_T("Test failed"));
		break;
	case trPassed:
		OutputString(_T("Test passed"));
		break;
	case trPartial:
		OutputString(_T("Test partial"));
		break;
	default:
		break;
	}

	return nResult;
}

void CBrowseDlg::OutputString(const TCHAR *szMessage, ...)
{
	if (m_pBrowseDlg == NULL)
	{
		return;
	}

	TCHAR szBuffer[256];

	va_list cArgs;
	va_start(cArgs, szMessage);
	_vstprintf(szBuffer, szMessage, cArgs);
	va_end(cArgs);

	CString szOutput;
	szOutput.Format(_T("%s"), szBuffer);

	m_lbMessages.AddString(szOutput);
	m_lbMessages.SetTopIndex(m_lbMessages.GetCount() - 1);
}

void CBrowseDlg::UpdateTest(HTREEITEM hItem, TestResult nResult)
{
	if (nResult == trPassed)
	{
		m_tcTests.SetItemImage(hItem, IL_TESTPASSED, IL_TESTPASSED);
	}
	else if (nResult == trFailed)
	{
		m_tcTests.SetItemImage(hItem, IL_TESTFAILED, IL_TESTFAILED);
	}
	else if (nResult == trPartial)
	{
		// TODO
	}
}

void CBrowseDlg::UpdateTestSet(HTREEITEM hItem)
{
	// Examine the results
	HTREEITEM hTest = m_tcTests.GetNextItem(hItem, TVGN_CHILD);
	while (hTest)
	{
		Test *pTest = (Test *) m_tcTests.GetItemData(hTest);
		UpdateTest(hTest, pTest->nLastResult);
		hTest = m_tcTests.GetNextItem(hTest, TVGN_NEXT);
	}
}

void CBrowseDlg::OnRuntest() 
{
	HTREEITEM hItem = m_tcTests.GetNextItem(NULL, TVGN_FIRSTVISIBLE);
	while (hItem)
	{
		UINT nState = m_tcTests.GetItemState(hItem, TVIS_SELECTED);
		if (!(nState & TVIS_SELECTED))
		{
			hItem = m_tcTests.GetNextItem(hItem, TVGN_NEXTVISIBLE);
			continue;
		}

		if (m_tcTests.ItemHasChildren(hItem))
		{
			// Run complete set of tests
			TestSet *pTestSet = (TestSet *) m_tcTests.GetItemData(hItem);
			RunTestSet(pTestSet);
			UpdateTestSet(hItem);
		}
		else
		{
			// Find the test
			Test *pTest = (Test *) m_tcTests.GetItemData(hItem);
			TestResult nResult = RunTest(pTest);
			UpdateTest(hItem, nResult);
		}

		hItem = m_tcTests.GetNextItem(hItem, TVGN_NEXTVISIBLE);
	}
}


void CBrowseDlg::OnSelchangedTestlist(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;

	BOOL bItemSelected = FALSE;
	m_szTestDescription.Empty();

	HTREEITEM hItem = m_tcTests.GetNextItem(NULL, TVGN_FIRSTVISIBLE);
	while (hItem)
	{
		UINT nState;

		nState = m_tcTests.GetItemState(hItem, TVIS_SELECTED);
		if (nState & TVIS_SELECTED)
		{
			bItemSelected = TRUE;
			if (m_tcTests.ItemHasChildren(hItem))
			{
				TestSet *pTestSet = (TestSet *) m_tcTests.GetItemData(hItem);
				m_szTestDescription = pTestSet->szDesc;
			}
			else
			{
				Test *pTest = (Test *) m_tcTests.GetItemData(hItem);
				m_szTestDescription = pTest->szDesc;
			}
		}

		hItem = m_tcTests.GetNextItem(hItem, TVGN_NEXTVISIBLE);
	}

	UpdateData(FALSE);
	m_btnRunTest.EnableWindow(bItemSelected);

	*pResult = 0;
}


void CBrowseDlg::OnDblclkTestlist(NMHDR* pNMHDR, LRESULT* pResult) 
{
	OnRuntest();
	*pResult = 0;
}
