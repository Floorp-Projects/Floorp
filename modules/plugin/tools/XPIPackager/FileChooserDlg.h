#if !defined(AFX_FILECHOOSERDLG_H__4F41CBA6_155C_4CC5_8E5E_7BA0A2F36C93__INCLUDED_)
#define AFX_FILECHOOSERDLG_H__4F41CBA6_155C_4CC5_8E5E_7BA0A2F36C93__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// FileChooserDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CFileChooserDlg dialog

class CFileChooserDlg : public CDialog
{
// Construction
public:
	CFileChooserDlg(int a_FileType, CString& a_PLID, CString& a_FileName, const CString& m_Domain_Name,
                  const CString& m_Product_Name, const CString& a_Version,
                  CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CFileChooserDlg)
	enum { IDD = IDD_FILE_CHOOSER };
	CString	m_PLID_Value;
	CString	m_FileName_Value;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFileChooserDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CFileChooserDlg)
	afx_msg void OnPLIDHelp();
	afx_msg void OnFilenameBrowse();
	afx_msg void OnAutoPLID();
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
  CString m_PLID;
  CString m_FileName;
  CString m_Domain_Name;
  CString m_Product_Name;
  CString m_Version;

  int m_FileType;

public:
  CString GetFileName(){return m_FileName;}
  CString GetPLIDForFileName(){return m_PLID;}
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FILECHOOSERDLG_H__4F41CBA6_155C_4CC5_8E5E_7BA0A2F36C93__INCLUDED_)
