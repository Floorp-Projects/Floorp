// CBrowseDlg.h : header file
//

#if !defined(AFX_CBROWSEDLG_H__5121F5E6_5324_11D2_93E1_000000000000__INCLUDED_)
#define AFX_CBROWSEDLG_H__5121F5E6_5324_11D2_93E1_000000000000__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

/////////////////////////////////////////////////////////////////////////////
// CBrowseDlg dialog

class CBrowseDlg : public CDialog
{
// Construction
public:
	CControlSiteInstance *m_pControlSite;

	CBrowseDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CBrowseDlg)
	enum { IDD = IDD_CBROWSE_DIALOG };
	CString	m_sURL;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBrowseDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CBrowseDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnGo();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CBROWSEDLG_H__5121F5E6_5324_11D2_93E1_000000000000__INCLUDED_)
