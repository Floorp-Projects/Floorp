#if !defined(AFX_SCANFORFILESDLG_H__2CAAFC02_6C47_11D2_A1F5_000000000000__INCLUDED_)
#define AFX_SCANFORFILESDLG_H__2CAAFC02_6C47_11D2_A1F5_000000000000__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ScanForFilesDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CScanForFilesDlg dialog

class CScanForFilesDlg : public CDialog
{
// Construction
public:
	CScanForFilesDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CScanForFilesDlg)
	enum { IDD = IDD_SCANFORFILES };
	CString	m_sFilePattern;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CScanForFilesDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CScanForFilesDlg)
	virtual void OnOK();
	afx_msg void OnSelectFile();
	afx_msg void OnSelectFolder();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SCANFORFILESDLG_H__2CAAFC02_6C47_11D2_A1F5_000000000000__INCLUDED_)
