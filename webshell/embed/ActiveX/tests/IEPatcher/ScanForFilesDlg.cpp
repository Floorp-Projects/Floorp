// ScanForFilesDlg.cpp : implementation file
//

#include "stdafx.h"
#include "IEPatcher.h"
#include "ScanForFilesDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CScanForFilesDlg dialog


CScanForFilesDlg::CScanForFilesDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CScanForFilesDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CScanForFilesDlg)
	m_sFilePattern = _T("");
	//}}AFX_DATA_INIT
}


void CScanForFilesDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CScanForFilesDlg)
	DDX_Text(pDX, IDC_FILEPATTERN, m_sFilePattern);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CScanForFilesDlg, CDialog)
	//{{AFX_MSG_MAP(CScanForFilesDlg)
	ON_BN_CLICKED(IDC_SELECTFILE, OnSelectFile)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CScanForFilesDlg message handlers

void CScanForFilesDlg::OnOK() 
{
	UpdateData();
	CDialog::OnOK();
}

void CScanForFilesDlg::OnSelectFile() 
{
	CFileDialog dlg(TRUE, NULL, _T("*.*"));
	
	if (dlg.DoModal() == IDOK)
	{
		m_sFilePattern = dlg.GetPathName();
		UpdateData(FALSE);
	}
}
