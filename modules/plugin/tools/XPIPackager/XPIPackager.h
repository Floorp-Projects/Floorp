// XPIPackager.h : main header file for the XPIPACKAGER application
//

#if !defined(AFX_XPIPACKAGER_H__B014FADE_A3A9_4663_8F39_F03905B8EDCA__INCLUDED_)
#define AFX_XPIPACKAGER_H__B014FADE_A3A9_4663_8F39_F03905B8EDCA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CXPIPackagerApp:
// See XPIPackager.cpp for the implementation of this class
//

class CXPIPackagerApp : public CWinApp
{
public:
  enum FILETYPE{DLL_TYPE = 0, XPT_TYPE, NUM_TYPES};
	CXPIPackagerApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CXPIPackagerApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CXPIPackagerApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_XPIPACKAGER_H__B014FADE_A3A9_4663_8F39_F03905B8EDCA__INCLUDED_)
