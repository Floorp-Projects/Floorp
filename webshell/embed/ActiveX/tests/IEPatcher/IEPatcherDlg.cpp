// IEPatcherDlg.cpp : implementation file
//

#include "stdafx.h"
#include "IEPatcher.h"
#include "IEPatcherDlg.h"
#include "DlgProxy.h"

#include <sys/types.h> 
#include <sys/stat.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const CLSID CLSID_WebBrowser_V1 = { 0xEAB22AC3, 0x30C1, 0x11CF, { 0xA7, 0xEB, 0x00, 0x00, 0xC0, 0x5B, 0xAE, 0x0B } };
const CLSID CLSID_WebBrowser = { 0x8856F961, 0x340A, 0x11D0, { 0xA9, 0x6B, 0x00, 0xC0, 0x4F, 0xD7, 0x05, 0xA2 } };
const CLSID CLSID_InternetExplorer = { 0x0002DF01, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
const CLSID CLSID_MozillaBrowser = {0x1339B54C,0x3453,0x11D2,{0x93,0xB9,0x00,0x00,0x00,0x00,0x00,0x00}};
const IID IID_IWebBrowser = { 0xEAB22AC1, 0x30C1, 0x11CF, { 0xA7, 0xEB, 0x00, 0x00, 0xC0, 0x5B, 0xAE, 0x0B } };
const IID IID_IWebBrowser2 = { 0xD30C1661, 0xCDAF, 0x11d0, { 0x8A, 0x3E, 0x00, 0xC0, 0x4F, 0xC9, 0xE2, 0x6E } };
const IID IID_IWebBrowserApp = { 0x0002DF05, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

/////////////////////////////////////////////////////////////////////////////
// CIEPatcherDlg dialog

IMPLEMENT_DYNAMIC(CIEPatcherDlg, CDialog);

CIEPatcherDlg::CIEPatcherDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CIEPatcherDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CIEPatcherDlg)
	m_sFile = _T("");
	m_sDestinationFile = _T("");
	m_bPatchFile = FALSE;
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pAutoProxy = NULL;
}

CIEPatcherDlg::~CIEPatcherDlg()
{
	// If there is an automation proxy for this dialog, set
	//  its back pointer to this dialog to NULL, so it knows
	//  the dialog has been deleted.
	if (m_pAutoProxy != NULL)
		m_pAutoProxy->m_pDialog = NULL;
}

void CIEPatcherDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CIEPatcherDlg)
	DDX_Control(pDX, IDC_PICKDESTINATION, m_btnPickDestination);
	DDX_Control(pDX, IDC_SCAN, m_btnScan);
	DDX_Control(pDX, IDC_DESTINATION_FILENAME, m_edtDestinationFile);
	DDX_Control(pDX, IDC_PROGRESS, m_lstProgress);
	DDX_Text(pDX, IDC_FILENAME, m_sFile);
	DDX_Text(pDX, IDC_DESTINATION_FILENAME, m_sDestinationFile);
	DDX_Check(pDX, IDC_PATCHFILE, m_bPatchFile);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CIEPatcherDlg, CDialog)
	//{{AFX_MSG_MAP(CIEPatcherDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_SCAN, OnScan)
	ON_BN_CLICKED(IDC_PATCHFILE, OnPatchFile)
	ON_BN_CLICKED(IDC_PICKSOURCE, OnPicksource)
	ON_BN_CLICKED(IDC_PICKDESTINATION, OnPickdestination)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CIEPatcherDlg message handlers

BOOL CIEPatcherDlg::OnInitDialog()
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

void CIEPatcherDlg::OnPaint() 
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
HCURSOR CIEPatcherDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

// Automation servers should not exit when a user closes the UI
//  if a controller still holds on to one of its objects.  These
//  message handlers make sure that if the proxy is still in use,
//  then the UI is hidden but the dialog remains around if it
//  is dismissed.

void CIEPatcherDlg::OnClose() 
{
	if (CanExit())
		CDialog::OnClose();
}

void CIEPatcherDlg::OnOK() 
{
	if (CanExit())
		CDialog::OnOK();
}

void CIEPatcherDlg::OnCancel() 
{
	if (CanExit())
		CDialog::OnCancel();
}

BOOL CIEPatcherDlg::CanExit()
{
	// If the proxy object is still around, then the automation
	//  controller is still holding on to this application.  Leave
	//  the dialog around, but hide its UI.
	if (m_pAutoProxy != NULL)
	{
		ShowWindow(SW_HIDE);
		return FALSE;
	}

	return TRUE;
}

void CIEPatcherDlg::OnScan() 
{
	UpdateData();
	if (!m_bPatchFile || m_sDestinationFile.IsEmpty())
	{
		PatchFile(m_sFile);
	}
	else
	{
		PatchFile(m_sFile, m_sDestinationFile);
	}
}


BOOL CIEPatcherDlg::PatchFile(const CString & sFileFrom, const CString & sFileTo)
{
	m_lstProgress.ResetContent();

	// Turn on patching if possible
	BOOL bPatchOnly = TRUE;
	if (!sFileTo.IsEmpty())
	{
		if (sFileTo == sFileFrom)
		{
			m_lstProgress.AddString("Warning: Patching disabled for safety reasons, source and destination names must differ");
		}
		else
		{
			bPatchOnly = FALSE;
		}
	}


	CString sProcessing;
	sProcessing.Format("Processing file \"%s\"", sFileFrom);
	m_lstProgress.AddString(sProcessing);

	// Allocate a memory buffer to slurp up the whole file
	struct _stat srcStat;
	_stat(sFileFrom, &srcStat);

	char *pszFile = new char[srcStat.st_size / sizeof(char)];
	if (pszFile == NULL)
	{
		m_lstProgress.AddString("Error: Could not allocate buffer for file");
	}
	else
	{
		FILE *fSrc = fopen(sFileFrom, "rb");
		if (fSrc == NULL)
		{
			m_lstProgress.AddString("Error: Could not open file");
		}
		else
		{
			size_t sizeSrc = srcStat.st_size;
			size_t sizeRead = 0;

			// Dumb but effective
			sizeRead = fread(pszFile, 1, sizeSrc, fSrc);
			fclose(fSrc);

			if (sizeRead != sizeSrc)
			{
				m_lstProgress.AddString("Error: Could not read all of file");
			}
			else
			{
				BOOL bPatchApplied = FALSE;

				m_lstProgress.AddString("Scanning for IE...");

				// Scan through buffer, one byte at a time doing a memcmp
				for (size_t i = 0; i < sizeSrc - sizeof(CLSID); i++)
				{
					if (memcmp(&pszFile[i], &CLSID_WebBrowser_V1, sizeof(CLSID)) == 0)
					{
						m_lstProgress.AddString("Found CLSID_WebBrowser_V1");
						if (!bPatchOnly)
						{
							m_lstProgress.AddString("Patching with CLSID_MozillaBrowser");
							memcpy(&pszFile[i], &CLSID_MozillaBrowser, sizeof(CLSID));
							bPatchApplied = TRUE;
						}
					}
					else if (memcmp(&pszFile[i], &CLSID_WebBrowser, sizeof(CLSID)) == 0)
					{
						m_lstProgress.AddString("Found CLSID_WebBrowser");
						if (!bPatchOnly)
						{
							m_lstProgress.AddString("Patching with CLSID_MozillaBrowser");
							memcpy(&pszFile[i], &CLSID_MozillaBrowser, sizeof(CLSID));
							m_lstProgress.AddString("Patching with CLSID_MozillaBrowser");
						}
					}
					else if (memcmp(&pszFile[i], &IID_IWebBrowser, sizeof(CLSID)) == 0)
					{
						m_lstProgress.AddString("Found IID_IWebBrowser");
					}
					else if (memcmp(&pszFile[i], &CLSID_InternetExplorer, sizeof(CLSID)) == 0)
					{
						m_lstProgress.AddString("Warning: Found CLSID_InternetExplorer, patching might not work");
					}
					else if (memcmp(&pszFile[i], &IID_IWebBrowser2, sizeof(CLSID)) == 0)
					{
						m_lstProgress.AddString("Warning: Found IID_IWebBrowser2, patching might not work");
					}
					else if (memcmp(&pszFile[i], &IID_IWebBrowserApp, sizeof(CLSID)) == 0)
					{
						m_lstProgress.AddString("Warning: Found IID_IWebBrowserApp, patching might not work");
					}
				}

				m_lstProgress.AddString("Scan completed");

				// Write out the patch file
				if (!bPatchOnly)
				{
					if (!bPatchApplied)
					{
						m_lstProgress.AddString("Error: Nothing was found to patch");
					}
					else
					{
						CString sMessage;
						sMessage.Format("Saving patched file \"%s\"", sFileTo);
						m_lstProgress.AddString(sMessage);
						FILE *fDest = fopen(sFileTo, "wb");
						if (fDest == NULL)
						{
							m_lstProgress.AddString("Error: Could not open destination file");
						}
						else
						{
							fwrite(pszFile, 1, sizeSrc, fDest);
							fclose(fDest);

							m_lstProgress.AddString("File saved");
						}
					}
				}
			}
		}

		delete []pszFile;
	}

	m_lstProgress.AddString("Processing completed");

	return FALSE;
}

void CIEPatcherDlg::OnPatchFile() 
{
	UpdateData();
	m_edtDestinationFile.EnableWindow(m_bPatchFile);
	m_btnPickDestination.EnableWindow(m_bPatchFile);
	m_btnScan.SetWindowText(m_bPatchFile ? "&Patch" : "&Scan");
}

void CIEPatcherDlg::OnPicksource() 
{
	CFileDialog	dlgChooser(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, _T("Application Files (*.exe;*.dll)|*.exe; *.dll|All Files (*.*)|*.*||"));

	if (dlgChooser.DoModal() == IDOK)
	{
		m_sFile = dlgChooser.GetPathName();
		UpdateData(FALSE);
	}
}

void CIEPatcherDlg::OnPickdestination() 
{
	CFileDialog	dlgChooser(FALSE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, _T("Application Files (*.exe;*.dll)|*.exe; *.dll|All Files (*.*)|*.*||"));

	if (dlgChooser.DoModal() == IDOK)
	{
		m_sDestinationFile = dlgChooser.GetPathName();
		UpdateData(FALSE);
	}
}
