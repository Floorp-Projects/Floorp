// MIMETypeChooseDlg.cpp : implementation file
//

#include "stdafx.h"
#include "XPIPackager.h"
#include "MIMETypeChooseDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMIMETypeChooseDlg dialog


CMIMETypeChooseDlg::CMIMETypeChooseDlg(CString& a_MIMEType, CString& a_Suffix, CString& a_SuffixDescription, CString& a_Plugin,
                                       CStringArray* a_Array, CWnd* pParent /*=NULL*/)
: CDialog(CMIMETypeChooseDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CMIMETypeChooseDlg)
	m_MIMEType_Value = _T("");
	m_Suffix_Value = _T("");
	m_Suffix_Description_Value = _T("");
	//}}AFX_DATA_INIT
  // set members
  m_PluginList = a_Array;
  m_MIMEType_Value = a_MIMEType;
  m_Suffix_Value = a_Suffix;
  m_Suffix_Description_Value = a_SuffixDescription;
  m_Plugin = a_Plugin;
}


void CMIMETypeChooseDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMIMETypeChooseDlg)
	DDX_Control(pDX, IDC_PLUGIN_LIST, m_Plugin_List_Control);
	DDX_Text(pDX, IDC_MIMETYPE, m_MIMEType_Value);
	DDX_Text(pDX, IDC_SUFFIX, m_Suffix_Value);
	DDX_Text(pDX, IDC_SUFFIX_DESCRIPTION, m_Suffix_Description_Value);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CMIMETypeChooseDlg, CDialog)
	//{{AFX_MSG_MAP(CMIMETypeChooseDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMIMETypeChooseDlg message handlers

BOOL CMIMETypeChooseDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
  CString temp;
  for(int i = 0; i < m_PluginList->GetSize(); i++)
  {
    temp = m_PluginList->ElementAt(i);
    m_Plugin_List_Control.AddString(temp);
  }
	m_Plugin_List_Control.SelectString(-1, m_Plugin);
  UpdateData(false);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CMIMETypeChooseDlg::OnOK() 
{
	// TODO: Add extra validation here
  UpdateData(true);
  if(m_Plugin_List_Control.GetCurSel() == LB_ERR )
  {
    CString tMessage = "You must select a Plugin File from the List in order to add a MIME Type.";
    MessageBox(tMessage, "Must Choose A Plugin File!", MB_OK + MB_ICONEXCLAMATION);
    return;
  }
  m_Plugin_List_Control.GetText(m_Plugin_List_Control.GetCurSel(), m_Plugin);

	CDialog::OnOK();
}
