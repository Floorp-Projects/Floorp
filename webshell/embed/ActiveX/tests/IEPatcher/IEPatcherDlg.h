// IEPatcherDlg.h : header file
//

#if !defined(AFX_IEPATCHERDLG_H__A603167C_3B36_11D2_B44D_00600819607E__INCLUDED_)
#define AFX_IEPATCHERDLG_H__A603167C_3B36_11D2_B44D_00600819607E__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class CIEPatcherDlgAutoProxy;

/////////////////////////////////////////////////////////////////////////////
// CIEPatcherDlg dialog

class CIEPatcherDlg : public CDialog
{
	DECLARE_DYNAMIC(CIEPatcherDlg);
	friend class CIEPatcherDlgAutoProxy;

// Construction
public:
	BOOL PatchFile(const CString &sFileFrom, const CString &sFileTo = "");
	CIEPatcherDlg(CWnd* pParent = NULL);	// standard constructor
	virtual ~CIEPatcherDlg();

// Dialog Data
	//{{AFX_DATA(CIEPatcherDlg)
	enum { IDD = IDD_IEPATCHER_DIALOG };
	CButton	m_btnPickDestination;
	CButton	m_btnScan;
	CEdit	m_edtDestinationFile;
	CListBox	m_lstProgress;
	CString	m_sFile;
	CString	m_sDestinationFile;
	BOOL	m_bPatchFile;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CIEPatcherDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CIEPatcherDlgAutoProxy* m_pAutoProxy;
	HICON m_hIcon;

	BOOL CanExit();

	// Generated message map functions
	//{{AFX_MSG(CIEPatcherDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnClose();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnScan();
	afx_msg void OnPatchFile();
	afx_msg void OnPicksource();
	afx_msg void OnPickdestination();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_IEPATCHERDLG_H__A603167C_3B36_11D2_B44D_00600819607E__INCLUDED_)
