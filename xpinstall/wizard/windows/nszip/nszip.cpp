/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Troy Chevalier <troy@netscape.com>
 *     Sean Su <ssu@netscape.com>
 */

// nszip.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "nszip.h"

#include "mainfrm.h"
#include "nszipdoc.h"
#include "nszipvw.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNsZipApp

BEGIN_MESSAGE_MAP(CNsZipApp, CWinApp)
	//{{AFX_MSG_MAP(CNsZipApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNsZipApp construction

CNsZipApp::CNsZipApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CNsZipApp object

CNsZipApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CNsZipApp initialization

void CNsZipApp::ProcessCmdLine()
{
	// Make a copy of the command line since we will change it while parsing
	CString		strCmdLine(m_lpCmdLine);

	// Get the name of the archive
	LPSTR	lpArchive = strtok((LPSTR)(LPCSTR)strCmdLine, " ");

	if (lpArchive) {
		char			szPath[MAX_PATH], szExtension[_MAX_EXT];
		LPSTR			lpFiles;
		CDocTemplate*	pTemplate;
		CNsZipDoc*		pDoc;

//		ASSERT(m_templateList.GetCount() == 1);
//		pTemplate = (CDocTemplate*)m_templateList.GetHead();

    POSITION pos = GetFirstDocTemplatePosition();

    pTemplate = (CDocTemplate*)GetNextDocTemplate(pos);
    ASSERT(pTemplate);
		ASSERT(pTemplate->IsKindOf(RUNTIME_CLASS(CDocTemplate)));

		// Create a new document
		VERIFY(pDoc = (CNsZipDoc*)pTemplate->CreateNewDocument());

		// We need a fully qualified pathname
		::GetFullPathName(lpArchive, sizeof(szPath), szPath, NULL);

		// Make sure it ends with .EXE
		_splitpath(szPath, NULL, NULL, NULL, szExtension);
		if (stricmp(szExtension, ".exe") != 0)
			strcat(szPath, ".exe");

		// Now go ahead and create the archive
		pDoc->OnNewDocument(szPath);

		// Process the files. These can be wildcards
		while (lpFiles = strtok(NULL, " "))
			pDoc->AddFiles(lpFiles);		
//		while (lpFiles = strtok(lpArchive, " "))
//			pDoc->AddFiles(lpFiles);		
	
		pDoc->OnSaveDocument(szPath);
		delete pDoc;
	}
}

BOOL CNsZipApp::InitInstance()
{
	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

	Enable3dControls();

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.
	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CNsZipDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(CNsZipView));
	AddDocTemplate(pDocTemplate);

	if (m_lpCmdLine[0] != '\0') {
		ProcessCmdLine();
		return FALSE;
	}

	// create a new (empty) document
	OnFileNew();

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

// Implementation
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// App command to run the dialog
void CNsZipApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////
// CNsZipApp commands

int APIENTRY
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	return 0;  
}
