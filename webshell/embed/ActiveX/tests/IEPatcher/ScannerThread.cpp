// ScannerThread.cpp : implementation file
//

#include "stdafx.h"
#include "iepatcher.h"
#include "ScannerThread.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CScannerThread

IMPLEMENT_DYNCREATE(CScannerThread, CWinThread)

CScannerThread::CScannerThread()
{
}

CScannerThread::~CScannerThread()
{
}

BOOL CScannerThread::InitInstance()
{
	m_cScannerWnd.Create(AfxRegisterWndClass(0), _T("Scanner"), 0L, CRect(0, 0, 0, 0), AfxGetMainWnd(), 1);
	return TRUE;
}

int CScannerThread::ExitInstance()
{
	// TODO:  perform any per-thread cleanup here
	return CWinThread::ExitInstance();
}

BEGIN_MESSAGE_MAP(CScannerThread, CWinThread)
	//{{AFX_MSG_MAP(CScannerThread)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CScannerThread message handlers
