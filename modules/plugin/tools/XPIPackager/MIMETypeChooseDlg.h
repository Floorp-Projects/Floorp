#if !defined(AFX_MIMETYPECHOOSEDLG_H__DD9847ED_1BC0_42AA_BC61_FA4F65DEDB8E__INCLUDED_)
#define AFX_MIMETYPECHOOSEDLG_H__DD9847ED_1BC0_42AA_BC61_FA4F65DEDB8E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MIMETypeChooseDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CMIMETypeChooseDlg dialog

class CMIMETypeChooseDlg : public CDialog
{
// Construction
public:
	CMIMETypeChooseDlg(CString& a_MIMEType, CString& a_Suffix, CString& a_SuffixDescription, CString& a_Plugin,
                     CStringArray* a_Array, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CMIMETypeChooseDlg)
	enum { IDD = IDD_MIMETYPE_CHOOSER };
	CListBox	m_Plugin_List_Control;
	CString	m_MIMEType_Value;
	CString	m_Suffix_Value;
	CString	m_Suffix_Description_Value;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMIMETypeChooseDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CMIMETypeChooseDlg)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
  CStringArray* m_PluginList;
  CString m_Plugin;

public:
  CString GetMIMEType(){return m_MIMEType_Value;}
  CString GetSuffix(){return m_Suffix_Value;}
  CString GetSuffixDescription(){return m_Suffix_Description_Value;}
  CString GetPlugin(){return m_Plugin;}
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MIMETYPECHOOSEDLG_H__DD9847ED_1BC0_42AA_BC61_FA4F65DEDB8E__INCLUDED_)
