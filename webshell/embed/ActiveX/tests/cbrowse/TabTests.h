#if !defined(AFX_TABTESTS_H__33D0A351_12E1_11D3_9407_000000000000__INCLUDED_)
#define AFX_TABTESTS_H__33D0A351_12E1_11D3_9407_000000000000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TabTests.h : header file
//

class CBrowseDlg;

/////////////////////////////////////////////////////////////////////////////
// CTabTests dialog

class CTabTests : public CPropertyPage
{
	DECLARE_DYNCREATE(CTabTests)

// Construction
public:
	CTabTests();
	~CTabTests();

	CBrowseDlg *m_pBrowseDlg;

// Dialog Data
	//{{AFX_DATA(CTabTests)
	enum { IDD = IDD_TAB_TESTS };
	CButton	m_btnRunTest;
	CTreeCtrl	m_tcTests;
	CString	m_szTestDescription;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CTabTests)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CTabTests)
	afx_msg void OnRunTest();
	afx_msg void OnSelchangedTestlist(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkTestlist(NMHDR* pNMHDR, LRESULT* pResult);
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TABTESTS_H__33D0A351_12E1_11D3_9407_000000000000__INCLUDED_)
