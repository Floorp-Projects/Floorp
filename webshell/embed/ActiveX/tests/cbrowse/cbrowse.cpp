// cbrowse.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "cbrowse.h"
#include "CBrowseDlg.h"
#include "PickerDlg.h"
#include <initguid.h>
#include "Cbrowse_i.c"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBrowseApp

BEGIN_MESSAGE_MAP(CBrowseApp, CWinApp)
	//{{AFX_MSG_MAP(CBrowseApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBrowseApp construction

CBrowseApp::CBrowseApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CBrowseApp object

CBrowseApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CBrowseApp initialization

BOOL CBrowseApp::InitInstance()
{
	// Initialize OLE libraries
	if (!AfxOleInit())
	{
		return FALSE;
	}
/*	if (!InitATL())
		return FALSE; */

	AfxEnableControlContainer();
/*	_Module.RegisterClassObjects(CLSCTX_LOCAL_SERVER, 
		REGCLS_MULTIPLEUSE);
*/

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	int nResponse;

	CPickerDlg pickerDlg;
	nResponse = pickerDlg.DoModal();
	if (nResponse != IDOK)
	{
		return FALSE;
	}

	CBrowseDlg dlg;

	dlg.m_clsid = pickerDlg.m_clsid;

	m_pMainWnd = &dlg;
	nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

	
CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
END_OBJECT_MAP()
/*
LONG CCbrowseModule::Unlock()
{
	AfxOleUnlockApp();
	return 0;
}

LONG CCbrowseModule::Lock()
{
	AfxOleLockApp();
	return 1;
}

*/
int CBrowseApp::ExitInstance()
{
/*	if (m_bATLInited)
	{
		_Module.RevokeClassObjects();
		_Module.Term();
	}
*/
	return CWinApp::ExitInstance();

}

/*
BOOL CBrowseApp::InitATL()
{
	m_bATLInited = TRUE;

	_Module.Init(ObjectMap, AfxGetInstanceHandle());
	_Module.dwThreadID = GetCurrentThreadId();

	return TRUE;

}
*/