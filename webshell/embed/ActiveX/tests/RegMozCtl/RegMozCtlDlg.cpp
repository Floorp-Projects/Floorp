// RegMozCtlDlg.cpp : implementation file
//

#include "stdafx.h"
#include "RegMozCtl.h"
#include "RegMozCtlDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CRegMozCtlDlg dialog

CRegMozCtlDlg::CRegMozCtlDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CRegMozCtlDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CRegMozCtlDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CRegMozCtlDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRegMozCtlDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CRegMozCtlDlg, CDialog)
	//{{AFX_MSG_MAP(CRegMozCtlDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_REGISTER, OnRegister)
	ON_BN_CLICKED(IDC_UNREGISTER, OnUnregister)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRegMozCtlDlg message handlers

BOOL CRegMozCtlDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CRegMozCtlDlg::OnPaint() 
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
HCURSOR CRegMozCtlDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CRegMozCtlDlg::OnRegister() 
{
	RegisterMozillaControl(TRUE);
}

void CRegMozCtlDlg::OnUnregister() 
{
	RegisterMozillaControl(FALSE);
}

void CRegMozCtlDlg::RegisterMozillaControl(BOOL bRegister)
{
	CFileFind cFind;
	CString szFile;
	CString szPath;

	CString szDir;
	GetCurrentDirectory(1024, szDir.GetBuffer(1024));
	szDir.ReleaseBuffer();

	CRegKey cKey;
	if (cKey.Open(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\SharedDlls")) != ERROR_SUCCESS)
	{
		AfxMessageBox(_T("Can't open registry key \"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\SharedDlls\""));
		return;
	}

	// Iterate through directory registering each DLL as a shared DLL
	BOOL bWorking = cFind.FindFile("*.dll");
	while (bWorking)
	{
		bWorking = cFind.FindNextFile();
		szFile = cFind.GetFileName();
		szPath = szDir + CString(_T("\\")) + szFile;
		if (bRegister)
		{
			cKey.SetValue(1, szPath);
		}
		else
		{
			cKey.DeleteValue(szPath);
		}
	}

	// Now register the mozilla control
	CString szMozCtl = szDir + CString(_T("\\npmozctl.dll"));
	HINSTANCE hMod = LoadLibrary(szMozCtl);
	if (hMod == NULL)
	{
		AfxMessageBox(_T("Can't find npmozctl.dll in current directory"));
	}
	FARPROC pfn = GetProcAddress(hMod, bRegister ? _T("DllRegisterServer") : _T("DllUnregisterServer"));
	if (pfn)
	{
		pfn();
	}
	FreeLibrary(hMod);

	AfxMessageBox(bRegister ? _T("Register completed") : _T("Unregister completed"));
}

