// XPIPackagerDlg.h : header file
//

#if !defined(AFX_XPIPACKAGERDLG_H__E235AD70_02F3_47CB_85DD_2F9F9E023FCE__INCLUDED_)
#define AFX_XPIPACKAGERDLG_H__E235AD70_02F3_47CB_85DD_2F9F9E023FCE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CXPIPackagerDlg dialog

class CXPIPackagerDlg : public CDialog
{
// Construction
public:
	CXPIPackagerDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CXPIPackagerDlg)
	enum { IDD = IDD_XPIPACKAGER_DIALOG };
	CListCtrl	m_MIME_List_Control;
	CListCtrl	m_Component_List_Control;
	CListCtrl	m_Plugin_List_Control;
	CString	m_Domain_Name_Value;
	CString	m_PLID_Value;
	CString	m_Plugin_Version_Value;
	CString	m_Product_Name_Value;
	CString	m_Archive_Name_Value;
	long	m_Component_Size_Value;
	CString	m_Company_Name_Value;
	long	m_Plugin_Size_Value;
	CString	m_Plugin_Description_Value;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CXPIPackagerDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CXPIPackagerDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnCreate();
	afx_msg void OnAutoPLID();
	afx_msg void OnBigHelp();
	afx_msg void OnPLIDHelp();
	afx_msg void OnComponentBrowse();
	afx_msg void OnComponentRemove();
	afx_msg void OnPluginBrowse();
	afx_msg void OnPluginRemove();
	afx_msg void OnMIMETypeAdd();
	afx_msg void OnMIMETypeRemove();
	afx_msg void OnDblclkPluginList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkComponentList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkMimeList(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
  void GetGuts(const CString &header, const CString &body, CString &result);
  long CalculateTotalFileSize(const CStringArray* t_Array);

  CStringArray m_PluginList;
  CStringArray m_PluginFileList;
  CStringArray m_PluginPLIDList;

  CStringArray m_ComponentList;
  CStringArray m_ComponentFileList;
  CStringArray m_ComponentPLIDList;

  CStringArray m_MIMETypeList;
  CStringArray m_SuffixList;
  CStringArray m_SuffixDescriptionList;
  CStringArray m_MIMEPluginList;

  long m_PluginTotalSize;
  long m_ComponentTotalSize;

  CString m_Archive_Name;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_XPIPACKAGERDLG_H__E235AD70_02F3_47CB_85DD_2F9F9E023FCE__INCLUDED_)
