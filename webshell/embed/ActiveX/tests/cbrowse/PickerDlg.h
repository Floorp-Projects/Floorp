#if !defined(AFX_PICKERDLG_H__648FC652_D34B_11D2_A255_000000000000__INCLUDED_)
#define AFX_PICKERDLG_H__648FC652_D34B_11D2_A255_000000000000__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// PickerDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CPickerDlg dialog

class CPickerDlg : public CDialog
{
// Construction
public:
	CPickerDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CPickerDlg)
	enum { IDD = IDD_PICKBROWSER };
	CListBox	m_lbPicker;
	//}}AFX_DATA

	CLSID m_clsid;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPickerDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CPickerDlg)
	afx_msg void OnOk();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PICKERDLG_H__648FC652_D34B_11D2_A255_000000000000__INCLUDED_)
