// FileChooserDlg.cpp : implementation file
//

#include "stdafx.h"
#include "XPIPackager.h"
#include "FileChooserDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CFileChooserDlg dialog


CFileChooserDlg::CFileChooserDlg(int a_FileType, CString& a_PLID, CString& a_FileName, const CString& a_Domain_Name,
                  const CString& a_Product_Name, const CString& a_Version,
                  CWnd* pParent /*=NULL*/)
	: CDialog(CFileChooserDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CFileChooserDlg)
	m_PLID_Value = _T("");
	m_FileName_Value = _T("");
	//}}AFX_DATA_INIT

  // set members
  m_PLID = a_PLID;
  m_FileName = a_FileName;
  m_Domain_Name = a_Domain_Name;
  m_Product_Name = a_Product_Name;
  m_Version = a_Version;

  m_FileType = a_FileType;
}


void CFileChooserDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFileChooserDlg)
	DDX_Text(pDX, IDC_PLID, m_PLID_Value);
	DDX_Text(pDX, IDC_FILENAME, m_FileName_Value);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CFileChooserDlg, CDialog)
	//{{AFX_MSG_MAP(CFileChooserDlg)
	ON_BN_CLICKED(IDC_PLID_HELP, OnPLIDHelp)
	ON_BN_CLICKED(IDC_FILENAME_BROWSE, OnFilenameBrowse)
	ON_BN_CLICKED(IDC_AUTO_PLID, OnAutoPLID)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFileChooserDlg message handlers

void CFileChooserDlg::OnPLIDHelp() 
{
	// TODO: Add your control notification handler code here
  CString tempString = "http://www.mozilla.org/projects/plugins/plugin-identifier.html";
  ShellExecute(NULL,"open",tempString,"","",SW_SHOWDEFAULT);
}


static char BASED_CODE szFilter[] = "DLL Files (*.dll)|*.dll|Executeable Files (*.exe)|*.exe|XPT Files (*.xpt)|*.xpt|";
static char BASED_CODE szFilter2[] = "XPT Files (*.xpt)|*.xpt|DLL Files (*.dll)|*.dll|Executeable Files (*.exe)|*.exe|";

void CFileChooserDlg::OnFilenameBrowse() 
{
	// TODO: Add your control notification handler code here
	// TODO: Add your control notification handler code here
  UpdateData(true);
  CString temp;
  if(m_FileType == CXPIPackagerApp::XPT_TYPE)
    temp = szFilter2;
  else
    temp = szFilter;

  CFileDialog dlg(true, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, temp);
  
  if(IDOK == dlg.DoModal())
  {
    CFile t_File;
    CString t_PluginFileName = dlg.GetPathName();
    if(t_File.Open(t_PluginFileName, CFile::modeRead))
    {
      CFileStatus t_FileStatus;
      if(t_File.GetStatus(t_FileStatus))
      {
        m_FileName_Value = t_PluginFileName;
        m_FileName = t_PluginFileName;// set the member
        t_File.Close();
      }
    }
    UpdateData(false);
  }	
}

void CFileChooserDlg::OnAutoPLID() 
{
	// TODO: Add your control notification handler code here
  UpdateData(true);
  CString temp;
  temp = "@";
  temp += m_Domain_Name;
  temp += "/";
  temp += m_Product_Name;
  temp += ",version=";
  temp += m_Version;
  
  m_PLID_Value = temp;
  m_PLID = temp;// set the member

  UpdateData(false);	
}

BOOL CFileChooserDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
  UpdateData(true);
  
  m_PLID_Value = m_PLID;
  m_FileName_Value = m_FileName;

  UpdateData(false);	
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CFileChooserDlg::OnOK() 
{
	// TODO: Add extra validation here
  UpdateData(true);
	m_PLID = m_PLID_Value;
	m_FileName = m_FileName_Value;

  CFile t_File;
  if(! t_File.Open(m_FileName, CFile::modeRead))
  {
    CString tMessage = m_FileName;
    tMessage += " is not a valid File or File Name";
    MessageBox(tMessage, "Not a valid File!", MB_OK + MB_ICONEXCLAMATION);
    return;
  }
  t_File.Close();

	CDialog::OnOK();
}
