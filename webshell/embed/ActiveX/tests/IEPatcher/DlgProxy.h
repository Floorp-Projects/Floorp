// DlgProxy.h : header file
//

#if !defined(AFX_DLGPROXY_H__A603167E_3B36_11D2_B44D_00600819607E__INCLUDED_)
#define AFX_DLGPROXY_H__A603167E_3B36_11D2_B44D_00600819607E__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class CIEPatcherDlg;

/////////////////////////////////////////////////////////////////////////////
// CIEPatcherDlgAutoProxy command target

class CIEPatcherDlgAutoProxy : public CCmdTarget
{
	DECLARE_DYNCREATE(CIEPatcherDlgAutoProxy)

	CIEPatcherDlgAutoProxy();           // protected constructor used by dynamic creation

// Attributes
public:
	CIEPatcherDlg* m_pDialog;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CIEPatcherDlgAutoProxy)
	public:
	virtual void OnFinalRelease();
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CIEPatcherDlgAutoProxy();

	// Generated message map functions
	//{{AFX_MSG(CIEPatcherDlgAutoProxy)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
	DECLARE_OLECREATE(CIEPatcherDlgAutoProxy)

	// Generated OLE dispatch map functions
	//{{AFX_DISPATCH(CIEPatcherDlgAutoProxy)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_DISPATCH
	DECLARE_DISPATCH_MAP()
	DECLARE_INTERFACE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DLGPROXY_H__A603167E_3B36_11D2_B44D_00600819607E__INCLUDED_)
