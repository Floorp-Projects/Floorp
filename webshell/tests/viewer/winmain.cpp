/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// Win32 specific viewer driver

#include <windows.h>
#include <crtdbg.h>
#include "resources.h"
#include "jsconsres.h"
#include "JSConsole.h"
#include "nsViewer.h"
#include "nsGlobalVariables.h"
#include "nsIDocument.h"
#include "nsIURL.h"
#include "nsVoidArray.h"
#include "nsCRT.h"
#include "prenv.h"
#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsITimer.h"

// Debug Robot options
static int gDebugRobotLoads = 5000;
static char gVerifyDir[_MAX_PATH];
static BOOL gVisualDebug = TRUE;

static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);

// DebugRobot call
extern "C" NS_EXPORT int DebugRobot(
   nsVoidArray * workList, nsIWebWidget * ww, int imax, char * verify_dir, void (*yieldProc)(const char *));

// Temporary Netlib stuff...
/* XXX: Don't include net.h... */
extern "C" {
extern int  NET_PollSockets();
};

#define DEBUG_EMPTY "(none)"


class nsWin32Viewer : public nsViewer {
    // From nsViewer
  public:
    virtual void AddMenu(nsIWidget* aMainWindow, PRBool aForPrintPreview);
    virtual void ShowConsole(WindowData* aWindata);
    virtual void DoDebugRobot(WindowData* aWindata);
    virtual void CopySelection(WindowData* aWindata);
    virtual void Destroy(WindowData* wd);
    virtual void CloseConsole();
    virtual void Stop();
    virtual void CrtSetDebug(PRUint32 aNewFlags);
      // Utilities
    virtual void CopyTextContent(WindowData* wd, HWND aHWnd);
};

static HANDLE gInstance, gPrevInstance;

//-----------------------------------------------------------------
// JSConsole support
//-----------------------------------------------------------------

// JSConsole window
JSConsole *gConsole = NULL;

static char* class1Name = "Viewer";
static char* class2Name = "PrintPreview";

void DestroyConsole()
{
 if (gConsole) {
    gConsole->SetNotification(NULL);
    delete gConsole;
    gConsole = NULL;
  }
}

//-----------------------------------------------------------------
// CRT Debug
//-----------------------------------------------------------------

void nsWin32Viewer::CrtSetDebug(PRUint32 aNewFlags)
{
#ifndef MOZ_NO_DEBUG_RTL
  int oldFlags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
  _CrtSetDbgFlag(aNewFlags);
  printf("Note: crt flags: old=%x new=%x\n", oldFlags, aNewFlags);
#endif
}


//-----------------------------------------------------------------
// Debug Robot support
//-----------------------------------------------------------------

void yieldProc(const char * str)
{
  // Process messages
  MSG msg;
  while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
    GetMessage(&msg, NULL, 0, 0);
    if (!JSConsole::sAccelTable ||
        !gConsole ||
        !gConsole->GetMainWindow() ||
        !TranslateAccelerator(gConsole->GetMainWindow(), JSConsole::sAccelTable, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      /* Pump Netlib... */
      NET_PollSockets();
    }
  }
}

/* Debug Robot Dialog options */

BOOL CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam,LPARAM lParam)
{
   BOOL translated = FALSE;
   HWND hwnd;
   switch (msg)
   {
      case WM_INITDIALOG:
         {
            SetDlgItemInt(hDlg,IDC_PAGE_LOADS,5000,FALSE);
            char * text = PR_GetEnv("VERIFY_PARSER");
            SetDlgItemText(hDlg,IDC_VERIFICATION_DIRECTORY,text ? text : DEBUG_EMPTY);
            hwnd = GetDlgItem(hDlg,IDC_UPDATE_DISPLAY);
            SendMessage(hwnd,BM_SETCHECK,TRUE,0);
         }
         return FALSE;
      case WM_COMMAND:
         switch (LOWORD(wParam))
         {
            case IDOK:
               gDebugRobotLoads = GetDlgItemInt(hDlg,IDC_PAGE_LOADS,&translated,FALSE);
               GetDlgItemText(hDlg, IDC_VERIFICATION_DIRECTORY, gVerifyDir, sizeof(gVerifyDir));
               if (!strcmp(gVerifyDir,DEBUG_EMPTY))
                  gVerifyDir[0] = '\0';
               hwnd = GetDlgItem(hDlg,IDC_UPDATE_DISPLAY);
               gVisualDebug = (BOOL)SendMessage(hwnd,BM_GETCHECK,0,0);
               EndDialog(hDlg,IDOK);
               break;
            case IDCANCEL:
               EndDialog(hDlg,IDCANCEL);
               break;
         }
         break;
      default:
         return FALSE;
   }
   return TRUE;
}


BOOL CreateRobotDialog(HWND hParent)
{
   BOOL result = (DialogBox(gInstance,MAKEINTRESOURCE(IDD_DEBUGROBOT),hParent,(DLGPROC)DlgProc) == IDOK);
   return result;
}


void AddViewerMenu(HINSTANCE hInstance, nsIWidget* aWidget, LPCTSTR lpMenuName)
{
  HMENU menu = ::LoadMenu(hInstance,lpMenuName);
  HWND hwnd = aWidget->GetNativeData(NS_NATIVE_WIDGET);
  ::SetMenu(hwnd, menu);
}

void nsWin32Viewer::AddMenu(nsIWidget* aMainWindow, PRBool aForPrintPreview)
{
   AddViewerMenu(gInstance, aMainWindow,
                 aForPrintPreview ? class2Name : class1Name);
}

//-----------------------------------------------------------------
// nsWin32Viewer Implementation
//-----------------------------------------------------------------

void nsWin32Viewer::ShowConsole(WindowData* aWinData)
{
    HWND hWnd = aWinData->windowWidget->GetNativeData(NS_NATIVE_WIDGET);
    if (!gConsole) {

      // load the accelerator table for the console
      if (!JSConsole::sAccelTable) {
        JSConsole::sAccelTable = LoadAccelerators(gInstance,
                                                  MAKEINTRESOURCE(ACCELERATOR_TABLE));
      }
      
      nsIScriptContextOwner *owner = nsnull;
      nsIScriptContext *context = nsnull;        
      if (NS_OK == aWinData->observer->mWebWidget->QueryInterface(kIScriptContextOwnerIID, (void **)&owner)) {
        if (NS_OK == owner->GetScriptContext(&context)) {

          // create the console
          gConsole = JSConsole::CreateConsole();
          gConsole->SetContext(context);
          // lifetime of the context is still unclear at this point.
          // Anyway, as long as the web widget is alive the context is alive.
          // Maybe the context shouldn't even be RefCounted
          context->Release();
          gConsole->SetNotification(DestroyConsole);
        }
        
        NS_RELEASE(owner);
      }
      else {
        MessageBox(hWnd, "Unable to load JavaScript", "Viewer Error", MB_ICONSTOP);
      }
    }
}

void nsWin32Viewer::CloseConsole()
{
  DestroyConsole();
}

void nsWin32Viewer::DoDebugRobot(WindowData* aWindata)
{
 if ((nsnull != aWindata) && (nsnull != aWindata->observer)) {
             if (CreateRobotDialog(aWindata->windowWidget->GetNativeData(NS_NATIVE_WIDGET)))
             {
                nsIDocument* doc = aWindata->observer->mWebWidget->GetDocument();
                if (nsnull!=doc) {
                   const char * str = doc->GetDocumentURL()->GetSpec();
                   nsVoidArray * gWorkList = new nsVoidArray();
                   gWorkList->AppendElement(new nsString(str));
                   DebugRobot( 
                      gWorkList, 
                      gVisualDebug ? aWindata->observer->mWebWidget : nsnull, 
                      gDebugRobotLoads, 
                      PL_strdup(gVerifyDir),
                      yieldProc);
                }
             }
          }
}


// Selects all the Content
void nsWin32Viewer::CopyTextContent(WindowData* wd, HWND aHWnd)
{
  HGLOBAL     hGlobalMemory;
  PSTR        pGlobalMemory;

  if (wd->observer != nsnull) {
    nsIDocument* doc = wd->observer->mWebWidget->GetDocument();
    if (doc != nsnull) {
      // Get Text from Selection
      nsString text;
      doc->GetSelectionText(text);

      // Copy text to Global Memory Area
      hGlobalMemory = (HGLOBAL)GlobalAlloc(GHND, text.Length()+1);
      if (hGlobalMemory != NULL) {
        pGlobalMemory = (PSTR) GlobalLock(hGlobalMemory);
        char * str = text.ToNewCString();
        char * s   = str;
        for (int i=0;i<text.Length();i++) {
          *pGlobalMemory++ = *s++;
        }
        delete str;

        // Put data on Clipboard
        GlobalUnlock(hGlobalMemory);
        OpenClipboard(aHWnd);
        EmptyClipboard();
        SetClipboardData(CF_TEXT, hGlobalMemory);
        CloseClipboard();
      }

      NS_IF_RELEASE(doc);
    }
  }
}

void nsWin32Viewer::CopySelection(WindowData* aWindata)
{
  CopyTextContent(aWindata, aWindata->windowWidget->GetNativeData(NS_NATIVE_WIDGET));
}

void nsWin32Viewer::Stop()
{
  PostQuitMessage(0);
}

void nsWin32Viewer::Destroy(WindowData* wd)
{
  CloseConsole();
  nsViewer::Destroy(wd);
}

static nsITimer* gNetTimer;

static void
PollNet(nsITimer *aTimer, void *aClosure)
{
  NET_PollSockets();
  NS_IF_RELEASE(gNetTimer);
  if (NS_OK == NS_NewTimer(&gNetTimer)) {
    gNetTimer->Init(PollNet, nsnull, 1000 / 50);
  }
}

int PASCAL
RunViewer(HANDLE instance, HANDLE prevInstance, LPSTR cmdParam, int nCmdShow, nsWin32Viewer* aViewer)
{
  gInstance = instance;
  gPrevInstance = prevInstance;

  SetViewer(aViewer);

  nsIWidget *mainWindow = nsnull;
  nsDocLoader* dl = aViewer->SetupViewer(&mainWindow, 0, 0);
 
  // Process messages
  MSG msg;
  PollNet(0, 0);
  while (::GetMessage(&msg, NULL, 0, 0)) {
    if (!JSConsole::sAccelTable ||
        !gConsole ||
        !gConsole->GetMainWindow() ||
        !TranslateAccelerator(gConsole->GetMainWindow(), JSConsole::sAccelTable, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      NET_PollSockets();
    }
  }

  aViewer->CleanupViewer(dl);

  return msg.wParam;
}


//------------------------------------------------------------------
// Win32 Main
//------------------------------------------------------------------

void main(int argc, char **argv)
{
  nsWin32Viewer* viewer = new nsWin32Viewer();
  viewer->ProcessArguments(argc, argv);
  RunViewer(GetModuleHandle(NULL), NULL, 0, SW_SHOW, viewer);
}

int PASCAL
WinMain(HANDLE instance, HANDLE prevInstance, LPSTR cmdParam, int nCmdShow)
{
  nsWin32Viewer* viewer = new nsWin32Viewer();
  return(RunViewer(instance, prevInstance, cmdParam, nCmdShow, viewer));
}




