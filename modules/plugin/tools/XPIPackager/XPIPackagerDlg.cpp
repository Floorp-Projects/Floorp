// XPIPackagerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "XPIPackager.h"
#include "XPIPackagerDlg.h"
#include "FileChooserDlg.h"
#include "MIMETypeChooseDlg.h"
#include "ZipArchive.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define COMPANY_DEFAULT_VALUE "My Plugin Company"
#define PLUGIN_DESC_DEFAULT_VALUE "My Exemplary Plugin Mine All Mine"
#define DOMAIN_DEFAULT_VALUE "www.MyPluginCompany.com"

#define ZIP_COMMAND "Y:\\workspace\\XPIinstaller\\Debug\\zip.exe -q "
#define QUOTE "\""
#define SPACE " "
#define INSTALL_JS_FILE "install.js"

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CXPIPackagerDlg dialog

CXPIPackagerDlg::CXPIPackagerDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CXPIPackagerDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CXPIPackagerDlg)
	m_Domain_Name_Value = _T("");
	m_PLID_Value = _T("");
	m_Plugin_Version_Value = _T("");
	m_Product_Name_Value = _T("");
	m_Archive_Name_Value = _T("");
	m_Component_Size_Value = 0;
	m_Company_Name_Value = _T("");
	m_Plugin_Size_Value = 0;
	m_Plugin_Description_Value = _T("");
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CXPIPackagerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CXPIPackagerDlg)
	DDX_Control(pDX, IDC_MIME_LIST, m_MIME_List_Control);
	DDX_Control(pDX, IDC_COMPONENT_LIST, m_Component_List_Control);
	DDX_Control(pDX, IDC_PLUGIN_LIST, m_Plugin_List_Control);
	DDX_Text(pDX, IDC_DOMAIN_NAME, m_Domain_Name_Value);
	DDX_Text(pDX, IDC_PLID, m_PLID_Value);
	DDX_Text(pDX, IDC_PLUGIN_VERSION, m_Plugin_Version_Value);
	DDX_Text(pDX, IDC_PRODUCT_NAME, m_Product_Name_Value);
	DDX_Text(pDX, IDC_ARCHIVE_NAME, m_Archive_Name_Value);
	DDX_Text(pDX, IDC_COMPONENT_SIZE, m_Component_Size_Value);
	DDX_Text(pDX, IDC_COMPANY_NAME, m_Company_Name_Value);
	DDX_Text(pDX, IDC_PLUGIN_SIZE, m_Plugin_Size_Value);
	DDX_Text(pDX, IDC_PLUGIN_DESCRIPTION, m_Plugin_Description_Value);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CXPIPackagerDlg, CDialog)
	//{{AFX_MSG_MAP(CXPIPackagerDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDOK, OnCreate)
	ON_BN_CLICKED(IDC_AUTO_PLID, OnAutoPLID)
	ON_BN_CLICKED(IDC_BIG_HELP, OnBigHelp)
	ON_BN_CLICKED(IDC_PLID_HELP, OnPLIDHelp)
	ON_BN_CLICKED(IDC_COMPONENT_BROWSE, OnComponentBrowse)
	ON_BN_CLICKED(IDC_COMPONENT_REMOVE, OnComponentRemove)
	ON_BN_CLICKED(IDC_PLUGIN_BROWSE, OnPluginBrowse)
	ON_BN_CLICKED(IDC_PLUGIN_REMOVE, OnPluginRemove)
	ON_BN_CLICKED(IDC_MIMETYPE_ADD, OnMIMETypeAdd)
	ON_BN_CLICKED(IDC_MIMETYPE_REMOVE, OnMIMETypeRemove)
	ON_NOTIFY(NM_DBLCLK, IDC_PLUGIN_LIST, OnDblclkPluginList)
	ON_NOTIFY(NM_DBLCLK, IDC_COMPONENT_LIST, OnDblclkComponentList)
	ON_NOTIFY(NM_DBLCLK, IDC_MIME_LIST, OnDblclkMimeList)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CXPIPackagerDlg message handlers

BOOL CXPIPackagerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
  // Initialize the CListControl Columns
  m_Plugin_List_Control.InsertColumn( 0, "Plugin File", LVCFMT_LEFT, 200, -1 );
  m_Plugin_List_Control.InsertColumn( 1, "PLID", LVCFMT_LEFT, 125, -1 );

  m_Component_List_Control.InsertColumn( 0, "Component File", LVCFMT_LEFT, 200, -1 );
  m_Component_List_Control.InsertColumn( 1, "PLID", LVCFMT_LEFT, 125, -1 );

  m_MIME_List_Control.InsertColumn( 0, "Mime Type", LVCFMT_LEFT, 150, -1 );
  m_MIME_List_Control.InsertColumn( 1, "Suffix", LVCFMT_LEFT, 100, -1 );
  m_MIME_List_Control.InsertColumn( 2, "Suffix Description", LVCFMT_LEFT, 150, -1 );
  m_MIME_List_Control.InsertColumn( 3, "Plugin File", LVCFMT_LEFT, 250, -1 );

  // fill with starter information
  m_Company_Name_Value = COMPANY_DEFAULT_VALUE;
  m_Plugin_Description_Value = PLUGIN_DESC_DEFAULT_VALUE;
  m_Domain_Name_Value = DOMAIN_DEFAULT_VALUE;

  UpdateData(false);
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CXPIPackagerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CXPIPackagerDlg::OnPaint() 
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
HCURSOR CXPIPackagerDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}



void CXPIPackagerDlg::OnCreate() 
{
	// TODO: Add your control notification handler code here
  UpdateData(true);

  if(!m_Archive_Name_Value.GetLength())
  {
    MessageBox("Need a valid name for the final XPI Installer file.", "Invalid File", MB_OK + MB_ICONEXCLAMATION);
    return;
  }

  if(!m_PluginFileList.GetSize())
  {
    MessageBox("You must have at least one PLUGIN file in a XPI Archive.", "Plugin File Missing", MB_OK + MB_ICONEXCLAMATION);
    return;
  }
  
  CString tempFileName = m_PluginFileList.ElementAt(0);
  CString tempArchiveName = m_Archive_Name_Value.Left(tempFileName.ReverseFind('.') + 1);

  CString temp = tempFileName.Left(tempFileName.ReverseFind('\\') + 1);
  temp += m_Archive_Name_Value;

  m_Archive_Name_Value = temp;

  //create new archive *.xpi
  CZipArchive archive;
  archive.Open(m_Archive_Name_Value, CZipArchive::create);

  //add plugin dlls
  int FileCount = m_PluginFileList.GetSize();
  for(int i = 0; i < FileCount; i++)
  {
    tempFileName = m_PluginFileList.GetAt(i);
    archive.AddNewFile(tempFileName, -1, false);
  }

  //add components
  FileCount = m_ComponentFileList.GetSize();
  if( FileCount != 0 )
  {
    for( i = 0; i < FileCount; i++)
    {
      tempFileName = m_ComponentFileList.GetAt(i);
      archive.AddNewFile(tempFileName, -1, false);
    }
  }

  //add install .js
  HRSRC info = ::FindResource(NULL,MAKEINTRESOURCE(IDR_HEADER), "JSCRIPT");
  HGLOBAL resource = ::LoadResource(NULL, info);
  DWORD fileSize = SizeofResource(NULL, info);
  BYTE *mempointer = (BYTE *) GlobalLock(resource);
  CString header;
  memcpy(header.GetBuffer(fileSize), mempointer, fileSize);
  header.ReleaseBuffer();
  int foo = header.GetLength();

  info = ::FindResource(NULL,MAKEINTRESOURCE(IDR_BODY), "JSCRIPT");
  resource = ::LoadResource(NULL, info);
  fileSize = SizeofResource(NULL, info);
  mempointer = (BYTE *) GlobalLock(resource);
  CString body;
  memcpy(body.GetBuffer(fileSize), mempointer, fileSize);
  body.ReleaseBuffer();
  int foo2 = body.GetLength();
  
  CString finalFile;
  GetGuts(header, body, finalFile);

  if (finalFile.GetLength())
  {
    CZipFileHeader header;
    header.m_uMethod = Z_DEFLATED;
    header.SetFileName("install.js");
    header.SetComment("install.js: install script");
    archive.OpenNewFile(header, Z_DEFAULT_COMPRESSION, m_Archive_Name_Value);
    archive.WriteNewFile(finalFile.GetBuffer(0), finalFile.GetLength());
    archive.CloseNewFile();
  }
  //close archive
  archive.Close();
  GlobalUnlock(resource);

  CString tMessage = "The following XPI Archive:\n\n";
  tMessage += m_Archive_Name_Value;
  tMessage += "\n\nwas created successfully!";
  MessageBox(tMessage, "XPI Archive Created!", MB_OK + MB_ICONEXCLAMATION);
  return;
}

/* --- FILLER.JS ---
var PLUGIN_FILE         = [%s];
var PLUGIN_FILE_PLID    = [%s];
var COMPONENT_FILE      = [%s];
var COMPONENT_FILE_PLID = [%s];
// (DLL files)
var PLUGIN_SIZE         = %d;
// (XPI files)
var COMPONENT_SIZE      = %d;
var SOFTWARE_NAME       = "%s";
var PLID    = "%s";
var VERSION = "%s";
var MIMETYPE           = [%s];
var SUFFIX             = [%s];
var SUFFIX_DESCRIPTION = [%s];
var COMPANY_NAME       = "%s";
var PLUGIN_DESCRIPTION = "%s";
*/

#define BUFFER_SIZE 5000

void
CXPIPackagerDlg::GetGuts(const CString &header, const CString &body, CString &result)
{
  CString filler;

  HRSRC info = ::FindResource(NULL,MAKEINTRESOURCE(IDR_FILLER), "JSCRIPT");
  HGLOBAL resource = ::LoadResource(NULL, info);
  DWORD fileSize = SizeofResource(NULL, info);
  BYTE *mempointer = (BYTE *) GlobalLock(resource);
  memcpy(filler.GetBuffer(fileSize), mempointer, fileSize);
  filler.ReleaseBuffer();


  CFile t_File;
  CString t_FileName;
  CString t_FullFileName;

  // build the plugin parameter
  CString t_PluginParameter;
  CString t_PluginPLIDParameter;
  int FileCount = m_PluginFileList.GetSize();
  for(int i = 0; i < FileCount; i++)
  {
    t_FullFileName = m_PluginFileList.GetAt(i);
    if(t_File.Open(t_FullFileName, CFile::modeRead))
    {
      t_FileName = t_File.GetFileName();
    }
    t_File.Close();
    t_PluginParameter += "\"" + t_FileName + "\"";
    t_PluginPLIDParameter += "\"" + m_PluginPLIDList.ElementAt(i) + "\"";
    if(i != (FileCount - 1))
    {
      t_PluginParameter += ", ";
      t_PluginPLIDParameter += ", ";
    }
  }

  // build the component parameter
  CString t_ComponentParameter;
  CString t_ComponentPLIDParameter;
  FileCount = m_ComponentFileList.GetSize();
  for( i = 0; i < FileCount; i++)
  {
    t_FullFileName = m_ComponentFileList.GetAt(i);
    if(t_File.Open(t_FullFileName, CFile::modeRead))
    {
      t_FileName = t_File.GetFileName();
    }
    t_File.Close();
    t_ComponentParameter += "\"" + t_FileName + "\"";
    t_ComponentPLIDParameter += "\"" + m_ComponentPLIDList.ElementAt(i) + "\"";
    if(i != (FileCount - 1))
    {
      t_ComponentParameter += ", ";
      t_ComponentPLIDParameter += ", ";
    }
  }
  if(!t_ComponentParameter.GetLength())
    t_ComponentParameter = "\"\"";
  if(!t_ComponentPLIDParameter.GetLength())
    t_ComponentPLIDParameter = "\"\"";

  // build the MIME parameters
  CString t_MIMETypeParameter;
  CString t_SuffixParameter;
  CString t_SuffixDescriptionParameter;
  FileCount = m_MIMETypeList.GetSize();
  for( i = 0; i < FileCount; i++)
  {
    t_MIMETypeParameter          += "\"" + m_MIMETypeList.ElementAt(i) + "\"";
    t_SuffixParameter            += "\"" + m_SuffixList.ElementAt(i) + "\"";
    t_SuffixDescriptionParameter += "\"" + m_SuffixDescriptionList.ElementAt(i) + "\"";
    if(i != (FileCount - 1))
    {
      t_MIMETypeParameter          += ", ";;
      t_SuffixParameter            += ", ";;
      t_SuffixDescriptionParameter += ", ";;
    }
  }
  if(!t_MIMETypeParameter.GetLength())
    t_MIMETypeParameter = "\"\"";
  if(!t_SuffixParameter.GetLength())
    t_SuffixParameter = "\"\"";
  if(!t_SuffixDescriptionParameter.GetLength())
    t_SuffixDescriptionParameter = "\"\"";
  
  CString buffer;

  sprintf(buffer.GetBuffer(filler.GetLength() + BUFFER_SIZE), filler, t_PluginParameter, t_PluginPLIDParameter,
    t_ComponentParameter, t_ComponentPLIDParameter, m_Plugin_Size_Value, m_Component_Size_Value, 
    m_Product_Name_Value, m_PLID_Value, m_Plugin_Version_Value, t_MIMETypeParameter, t_SuffixParameter,
    t_SuffixDescriptionParameter, m_Company_Name_Value, m_Plugin_Description_Value);

  buffer.ReleaseBuffer();
  result += header;
  result += buffer;
  result += body;
}

void CXPIPackagerDlg::OnAutoPLID() 
{
  // TODO: Add your control notification handler code here
  UpdateData(true);
  CString temp;
  temp = "@";
  temp += m_Domain_Name_Value;
  temp += "/";
  temp += m_Product_Name_Value;
  temp += ",version=";
  temp += m_Plugin_Version_Value;
  
  m_PLID_Value = temp;

  UpdateData(false);	
}

void CXPIPackagerDlg::OnBigHelp() 
{
	// TODO: Add your control notification handler code here
  CString tempString = "http://devedge.netscape.com/viewsource/2002/xpinstall-guidelines/";
  ShellExecute(NULL,"open",tempString,"","",SW_SHOWDEFAULT);
}

void CXPIPackagerDlg::OnPLIDHelp() 
{
	// TODO: Add your control notification handler code here
  CString tempString = "http://www.mozilla.org/projects/plugins/plugin-identifier.html";
  ShellExecute(NULL,"open",tempString,"","",SW_SHOWDEFAULT);
}


long CXPIPackagerDlg::CalculateTotalFileSize(const CStringArray* t_Array) 
{
	// TODO: Add your control notification handler code here
  CFile t_File;
  CString t_FileName;
	int t_NumFiles = t_Array->GetSize();

  long t_Size = 0;

  for(int i = 0; i < t_NumFiles; i++)
  {
    t_FileName = t_Array->GetAt(i);
    if(t_File.Open(t_FileName, CFile::modeRead))
    {
      CFileStatus t_FileStatus;
      if(t_File.GetStatus(t_FileStatus))
        t_Size += t_FileStatus.m_size;
    }
    t_File.Close();
  }
  return t_Size;
}

void CXPIPackagerDlg::OnComponentBrowse() 
{
	// TODO: Add your control notification handler code here
  UpdateData(true);
  CString t_PLID;
  CString t_ComponentName;
  CString t_ComponentFileName;
  CFileChooserDlg dlg(CXPIPackagerApp::XPT_TYPE, m_PLID_Value, t_ComponentFileName, m_Domain_Name_Value, m_Product_Name_Value, m_Plugin_Version_Value, NULL);

  if(IDOK == dlg.DoModal())
  {
    t_ComponentFileName = dlg.GetFileName();
    t_PLID = dlg.GetPLIDForFileName();

    CFile t_File;
    if(t_File.Open(t_ComponentFileName, CFile::modeRead))
    {
      CFileStatus t_FileStatus;
      if(t_File.GetStatus(t_FileStatus))
      {
        t_ComponentName = t_File.GetFileName();

        m_ComponentList.InsertAt(0, t_ComponentName);
        m_ComponentFileList.InsertAt(0, t_ComponentFileName);
        m_ComponentPLIDList.InsertAt(0, t_PLID);

        m_Component_List_Control.InsertItem( 0, "", -1);
        m_Component_List_Control.SetItemText( 0, 0, t_ComponentFileName);
        m_Component_List_Control.SetItemText( 0, 1, t_PLID);

        // re-adjust plugin total size
        t_File.Close();
        m_ComponentTotalSize = CalculateTotalFileSize(&m_ComponentFileList);
        m_Component_Size_Value = m_ComponentTotalSize;
      }
    }
  }
  UpdateData(false);
}

void CXPIPackagerDlg::OnComponentRemove() 
{
	// TODO: Add your control notification handler code here
  UpdateData(true);
	int t_CurrentSelection = m_Component_List_Control.GetSelectionMark( );
	if(t_CurrentSelection == -1)
	  return;

	m_Component_List_Control.DeleteItem(t_CurrentSelection);
  m_ComponentList.RemoveAt(t_CurrentSelection);
  m_ComponentFileList.RemoveAt(t_CurrentSelection);
  m_ComponentPLIDList.RemoveAt(t_CurrentSelection);
  // re-adjust size
  m_Component_Size_Value = CalculateTotalFileSize(&m_ComponentFileList);
  UpdateData(false);
}

void CXPIPackagerDlg::OnPluginBrowse() 
{
	// TODO: Add your control notification handler code here
  UpdateData(true);

  CString t_PLID;
  CString t_PluginName;
  CString t_PluginFileName;

  CFileChooserDlg dlg(CXPIPackagerApp::DLL_TYPE, m_PLID_Value, t_PluginFileName, m_Domain_Name_Value, m_Product_Name_Value, m_Plugin_Version_Value, NULL);

  if(IDOK == dlg.DoModal())
  {
    t_PluginFileName = dlg.GetFileName();
    t_PLID = dlg.GetPLIDForFileName();

    CFile t_File;
    if(t_File.Open(t_PluginFileName, CFile::modeRead))
    {
      CFileStatus t_FileStatus;
      if(t_File.GetStatus(t_FileStatus))
      {
        t_PluginName = t_File.GetFileName();

        m_PluginList.InsertAt(0, t_PluginName);
        m_PluginFileList.InsertAt(0, t_PluginFileName);
        m_PluginPLIDList.InsertAt(0, t_PLID);

        m_Plugin_List_Control.InsertItem( 0, "", -1);
        m_Plugin_List_Control.SetItemText( 0, 0, t_PluginFileName);
        m_Plugin_List_Control.SetItemText( 0, 1, t_PLID);

        // re-adjust plugin total size
        t_File.Close();
        m_PluginTotalSize = CalculateTotalFileSize(&m_PluginFileList);
        m_Plugin_Size_Value = m_PluginTotalSize;
      }
    }
  }
  UpdateData(false);
}

void CXPIPackagerDlg::OnPluginRemove() 
{
	// TODO: Add your control notification handler code here
  UpdateData(true);
	int t_CurrentSelection = m_Plugin_List_Control.GetSelectionMark( );
	if(t_CurrentSelection == -1)
	  return;

	m_Plugin_List_Control.DeleteItem(t_CurrentSelection);
  m_PluginList.RemoveAt(t_CurrentSelection);
  m_PluginFileList.RemoveAt(t_CurrentSelection);
  m_PluginPLIDList.RemoveAt(t_CurrentSelection);
  // re-adjust size
  m_Plugin_Size_Value = CalculateTotalFileSize(&m_PluginFileList);
  UpdateData(false);
}


void CXPIPackagerDlg::OnMIMETypeAdd() 
{
	// TODO: Add your control notification handler code here
  UpdateData(true);
  if(!m_PluginList.GetSize())
  {
    CString tMessage = "No Plugin File is present in the archive yet.  A Plugin File is required to a MIME Type for the Plugin.";
    MessageBox(tMessage, "Error: No Plugin File!", MB_OK + MB_ICONEXCLAMATION);
    return;
  }
  CString temp;
  CMIMETypeChooseDlg dlg(temp, temp, temp, temp, &m_PluginList, NULL);

  if(IDOK == dlg.DoModal())
  {
    m_MIMETypeList.InsertAt(0, dlg.GetMIMEType());
    m_SuffixList.InsertAt(0, dlg.GetSuffix());
    m_SuffixDescriptionList.InsertAt(0, dlg.GetSuffixDescription());
    m_MIMEPluginList.InsertAt(0, dlg.GetPlugin());

    m_MIME_List_Control.InsertItem( 0, "", -1);
    m_MIME_List_Control.SetItemText( 0, 0, dlg.GetMIMEType());
    m_MIME_List_Control.SetItemText( 0, 1, dlg.GetSuffix());
    m_MIME_List_Control.SetItemText( 0, 2, dlg.GetSuffixDescription());
    m_MIME_List_Control.SetItemText( 0, 3, dlg.GetPlugin());
  }
  UpdateData(false);
}

void CXPIPackagerDlg::OnMIMETypeRemove() 
{
	// TODO: Add your control notification handler code here
  UpdateData(true);
	int t_CurrentSelection = m_MIME_List_Control.GetSelectionMark( );
	if(t_CurrentSelection == -1)
	  return;

	m_MIME_List_Control.DeleteItem(t_CurrentSelection);

  m_MIMETypeList.RemoveAt(t_CurrentSelection);
  m_SuffixList.RemoveAt(t_CurrentSelection);
  m_SuffixDescriptionList.RemoveAt(t_CurrentSelection);
  m_MIMEPluginList.RemoveAt(t_CurrentSelection);

  UpdateData(false);
}

void CXPIPackagerDlg::OnDblclkPluginList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here
  UpdateData(true);
	int t_CurrentSelection = m_Plugin_List_Control.GetSelectionMark( );
	if(t_CurrentSelection == -1)
	  return;

  CString t_PLID = m_PluginPLIDList.ElementAt(t_CurrentSelection);
  CString t_PluginName;
  CString t_PluginFileName = m_PluginFileList.ElementAt(t_CurrentSelection);

  CFileChooserDlg dlg(CXPIPackagerApp::DLL_TYPE, t_PLID, t_PluginFileName, m_Domain_Name_Value, m_Product_Name_Value, m_Plugin_Version_Value, NULL);

  if(IDOK == dlg.DoModal())
  {
    t_PluginFileName = dlg.GetFileName();
    t_PLID = dlg.GetPLIDForFileName();

    CFile t_File;
    if(t_File.Open(t_PluginFileName, CFile::modeRead))
    {
      CFileStatus t_FileStatus;
      if(t_File.GetStatus(t_FileStatus))
      {
        t_PluginName = t_File.GetFileName();

        m_PluginList.RemoveAt(t_CurrentSelection);
        m_PluginList.InsertAt(t_CurrentSelection, t_PluginName);
        m_PluginFileList.RemoveAt(t_CurrentSelection);
        m_PluginFileList.InsertAt(t_CurrentSelection, t_PluginFileName);
        m_PluginPLIDList.RemoveAt(t_CurrentSelection);
        m_PluginPLIDList.InsertAt(t_CurrentSelection, t_PLID);

        //m_Plugin_List_Control.InsertItem( 0, "", -1);
        m_Plugin_List_Control.SetItemText( t_CurrentSelection, 0, t_PluginFileName);
        m_Plugin_List_Control.SetItemText( t_CurrentSelection, 1, t_PLID);

        // re-adjust plugin total size
        t_File.Close();
        m_PluginTotalSize = CalculateTotalFileSize(&m_PluginFileList);
        m_Plugin_Size_Value = m_PluginTotalSize;
      }
    }
  }
  UpdateData(false);
	
	*pResult = 0;
}

void CXPIPackagerDlg::OnDblclkComponentList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here
  UpdateData(true);
	int t_CurrentSelection = m_Component_List_Control.GetSelectionMark( );
	if(t_CurrentSelection == -1)
	  return;

  CString t_PLID = m_ComponentPLIDList.ElementAt(t_CurrentSelection);
  CString t_ComponentName;
  CString t_ComponentFileName = m_ComponentFileList.ElementAt(t_CurrentSelection);

  CFileChooserDlg dlg(CXPIPackagerApp::XPT_TYPE, t_PLID, t_ComponentFileName, m_Domain_Name_Value, m_Product_Name_Value, m_Plugin_Version_Value, NULL);

  if(IDOK == dlg.DoModal())
  {
    t_ComponentFileName = dlg.GetFileName();
    t_PLID = dlg.GetPLIDForFileName();

    CFile t_File;
    if(t_File.Open(t_ComponentFileName, CFile::modeRead))
    {
      CFileStatus t_FileStatus;
      if(t_File.GetStatus(t_FileStatus))
      {
        t_ComponentName = t_File.GetFileName();

        m_ComponentList.RemoveAt(t_CurrentSelection);
        m_ComponentList.InsertAt(t_CurrentSelection, t_ComponentName);
        m_ComponentFileList.RemoveAt(t_CurrentSelection);
        m_ComponentFileList.InsertAt(t_CurrentSelection, t_ComponentFileName);
        m_ComponentPLIDList.RemoveAt(t_CurrentSelection);
        m_ComponentPLIDList.InsertAt(t_CurrentSelection, t_PLID);

        //m_Plugin_List_Control.InsertItem( 0, "", -1);
        m_Component_List_Control.SetItemText( t_CurrentSelection, 0, t_ComponentFileName);
        m_Component_List_Control.SetItemText( t_CurrentSelection, 1, t_PLID);

        // re-adjust plugin total size
        t_File.Close();
        m_ComponentTotalSize = CalculateTotalFileSize(&m_ComponentFileList);
        m_Component_Size_Value = m_ComponentTotalSize;
      }
    }
  }
  UpdateData(false);
	
	*pResult = 0;
}

void CXPIPackagerDlg::OnDblclkMimeList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here
  UpdateData(true);
	int t_CurrentSelection = m_MIME_List_Control.GetSelectionMark( );
	if(t_CurrentSelection == -1)
	  return;

  if(!m_PluginList.GetSize())
  {
    CString tMessage = "No Plugin File is present in the archive yet.  A Plugin File is required to a MIME Type for the Plugin.";
    MessageBox(tMessage, "Error: No Plugin File!", MB_OK + MB_ICONEXCLAMATION);
    return;
  }

  CString t_MIMEType = m_MIMETypeList.ElementAt(t_CurrentSelection);
  CString t_Suffix = m_SuffixList.ElementAt(t_CurrentSelection);
  CString t_SuffixDescription = m_SuffixDescriptionList.ElementAt(t_CurrentSelection);
  CString t_Plugin = m_MIMEPluginList.ElementAt(t_CurrentSelection);

  CMIMETypeChooseDlg dlg(t_MIMEType, t_Suffix, t_SuffixDescription, t_Plugin, &m_PluginList, NULL);

  if(IDOK == dlg.DoModal())
  {
    m_MIMETypeList.RemoveAt(t_CurrentSelection);
    m_SuffixList.RemoveAt(t_CurrentSelection);
    m_SuffixDescriptionList.RemoveAt(t_CurrentSelection);
    m_MIMEPluginList.RemoveAt(t_CurrentSelection);

    m_MIMETypeList.InsertAt(t_CurrentSelection, dlg.GetMIMEType());
    m_SuffixList.InsertAt(t_CurrentSelection, dlg.GetSuffix());
    m_SuffixDescriptionList.InsertAt(t_CurrentSelection, dlg.GetSuffixDescription());
    m_MIMEPluginList.InsertAt(t_CurrentSelection, dlg.GetPlugin());

    //m_MIME_List_Control.InsertItem( t_CurrentSelection, "", -1);
    m_MIME_List_Control.SetItemText( t_CurrentSelection, 0, dlg.GetMIMEType());
    m_MIME_List_Control.SetItemText( t_CurrentSelection, 1, dlg.GetSuffix());
    m_MIME_List_Control.SetItemText( t_CurrentSelection, 2, dlg.GetSuffixDescription());
    m_MIME_List_Control.SetItemText( t_CurrentSelection, 3, dlg.GetPlugin());
  }
  UpdateData(false);

	*pResult = 0;
}
