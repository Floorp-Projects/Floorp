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


//we need openfilename stuff... MMP
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <stdio.h>

#include "resources.h"
#include "jsconsres.h"
#include "JSConsole.h"

#include "nsGlobalVariables.h"
#include "nsIWebWidget.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsUnitConversion.h"
#include "nsVoidArray.h"
#include "nsCRT.h"
#include "prthread.h"
#include "prprf.h"
#include "nsRepository.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include "nsViewsCID.h"
#include "nsString.h"
#include "plevent.h"
#include <malloc.h>
#include "nsIScriptContext.h"
#include "nsDocLoader.h"

// JSConsole window
JSConsole *gConsole = NULL;

#define SAMPLES_BASE_URL "resource:/res/samples"
#define START_URL SAMPLES_BASE_URL "/test0.html"

static char* class1Name = "Viewer";
static char* class2Name = "PrintPreview";

static HANDLE gInstance, gPrevInstance;
static char* startURL;
static nsVoidArray* gWindows;

// Temporary Netlib stuff...
/* XXX: Don't include net.h... */
extern "C" {
extern int  NET_PollSockets();
};

//----------------------------------------------------------------------

class DocObserver : public nsIDocumentObserver {
public:
  DocObserver(nsIWebWidget* aWebWidget) {
    NS_INIT_REFCNT();
    mWebWidget = aWebWidget;
    NS_ADDREF(aWebWidget);
  }

  NS_DECL_ISUPPORTS;

  NS_IMETHOD SetTitle(const nsString& aTitle);

  virtual void BeginUpdate() { }
  virtual void EndUpdate() { }
  virtual void ContentChanged(nsIContent* aContent,
                              nsISubContent* aSubContent,
                              PRInt32 aChangeType) { }
  virtual void ContentAppended(nsIContent* aContainer) { }
  virtual void ContentInserted(nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer) { }
  virtual void ContentReplaced(nsIContent* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aNewChild,
                               PRInt32 aIndexInContainer) { }
  virtual void ContentWillBeRemoved(nsIContent* aContainer,
                                    nsIContent* aChild,
                                    PRInt32 aIndexInContainer) { }
  virtual void ContentHasBeenRemoved(nsIContent* aContainer,
                                     nsIContent* aChild,
                                     PRInt32 aIndexInContainer) { }
  virtual void StyleSheetAdded(nsIStyleSheet* aStyleSheet) { }


protected:
  ~DocObserver() {
    NS_RELEASE(mWebWidget);
  }

  nsIWebWidget* mWebWidget;
};

struct WindowData {
  HWND window;
  nsIWebWidget* ww;
  DocObserver* observer;

  WindowData() {
    window = nsnull;
    ww = nsnull;
  }
};

static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENTOBSERVER_IID);

NS_IMPL_ISUPPORTS(DocObserver, kIDocumentObserverIID);

// Pass title information through to all of the web widgets that
// belong to this document.
NS_IMETHODIMP DocObserver::SetTitle(const nsString& aTitle)
{
  PRInt32 i, n = gWindows->Count();
  for (i = 0; i < n; i++) {
    WindowData* wd = (WindowData*) gWindows->ElementAt(i);
    if (wd->ww == mWebWidget) {
      char* cp = aTitle.ToNewCString();
      ::SetWindowText(wd->window, cp);
      delete cp;
    }
  } 
  return NS_OK;
}

static DocObserver* NewObserver(nsIWebWidget* ww)
{
  nsISupports* oldContainer;
  nsresult rv = ww->GetContainer(&oldContainer);
  if (NS_OK == rv) {
    if (nsnull == oldContainer) {
      DocObserver* it = new DocObserver(ww);
      NS_ADDREF(it);
      ww->SetContainer(it);
      return it;
    }
    else {
      NS_RELEASE(oldContainer);
    }
  }
  return nsnull;
}

//----------------------------------------------------------------------



/*
*
* BEGIN PURIFY METHODS
*
*/



PRBool  DoPurifyTest()
{
#ifdef NS_WIN32
  char*               name = "MOZ_PURIFY_TEST";
  char                buffer[256];
  int                 result;

  result = GetEnvironmentVariable(name, buffer, 256);
  return PRBool(result != 0);
#endif
  return PR_FALSE;
}


void AddTestDocs(nsDocLoader* aDocLoader)
{
  char url[500];
  for (int docnum = 0; docnum < 9; docnum++)
  {
    PR_snprintf(url, 500, "%s/test%d.html", SAMPLES_BASE_URL, docnum);
    aDocLoader->AddURL(url);
  }
}

/*
*
* END PURIFY METHODS
*
*/





static nsresult ShowPrintPreview(nsIWebWidget* ww, PRIntn aColumns);

void DestroyConsole()
{
  if (gConsole) {
    gConsole->SetNotification(NULL);
    delete gConsole;
    gConsole = NULL;
  }
}

static void Destroy(WindowData* wd)
{
  DestroyConsole();
  if (nsnull != wd) {
    if (nsnull != wd->ww) {
      NS_RELEASE(wd->ww);
    }
    if (nsnull != wd->observer) {
      NS_RELEASE(wd->observer);
    }
    gWindows->RemoveElement(wd);
    delete wd;
  }
}

static void DestroyAllWindows()
{
  if (nsnull != gWindows) {
    PRInt32 n = gWindows->Count();
    for (PRInt32 i = 0; i < n; i++) {
      WindowData* wd = (WindowData*) gWindows->ElementAt(i);
      ::DestroyWindow(wd->window);
    }
    gWindows->Clear();
  }
}

#define FILE_PROTOCOL "file://"

// Displays the Open common dialog box and lets the user specify an HTML file
// to open. Then passes the filename along to the Web widget.
static void
OpenHTMLFile(WindowData* wd)
{
  char          szFile[_MAX_PATH];
  OPENFILENAME  ofn;

  // Prompt the user for a filename
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.hwndOwner = wd->window;
  ofn.hInstance = gInstance;
  ofn.lpstrFilter = "HTML Files\0*.html;*.htm\0All Files\0*.*\0\0";
  ofn.lpstrCustomFilter = NULL;
  ofn.nFilterIndex = 1;
  szFile[0] = '\0';
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFileTitle = NULL;
  ofn.lpstrInitialDir = NULL;  // use current directory as initial directory
  ofn.lpstrTitle = NULL;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NONETWORKBUTTON;
  ofn.lpstrDefExt = NULL;
  ofn.lpTemplateName = NULL;

  if (GetOpenFileName(&ofn)) {
    PRInt32 len = strlen(szFile);
    char*   lpszFileURL = (char*)malloc(len + sizeof(FILE_PROTOCOL));

    // Translate '\' to '/'
    for (PRInt32 i = 0; i < len; i++) {
      if (szFile[i] == '\\') {
        szFile[i] = '/';
      }
    }

    // Build the file URL
    wsprintf(lpszFileURL, "%s%s", FILE_PROTOCOL, szFile);

    // Ask the Web widget to load the file URL
    wd->ww->LoadURL(lpszFileURL);
    free(lpszFileURL);
  }
}

long PASCAL
WndProc(HWND hWnd, UINT msg, WPARAM param, LPARAM lparam)
{
  HMENU hMenu;

  if (msg == WM_CREATE) {
    LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lparam;
    WindowData* w = (WindowData*) lpcs->lpCreateParams;
    SetWindowLong(hWnd, GWL_USERDATA, (LONG) w);
  }

  WindowData* wd = (WindowData*) GetWindowLong(hWnd, GWL_USERDATA);
  switch (msg) {
  case WM_COMMAND:
    hMenu = GetMenu(hWnd);
    switch (LOWORD(param)) {
    case VIEWER_EXIT:
      DestroyAllWindows();
      return 0;
    case PREVIEW_CLOSE:
      ::DestroyWindow(hWnd);
      return 0;

    case VIEWER_FILE_OPEN:
      OpenHTMLFile(wd);
      break;

    case VIEWER_DEMO0:
    case VIEWER_DEMO1:
    case VIEWER_DEMO2:
    case VIEWER_DEMO3:
    case VIEWER_DEMO4:
    case VIEWER_DEMO5:
    case VIEWER_DEMO6:
    case VIEWER_DEMO7:
    case VIEWER_DEMO8:
      if ((nsnull != wd) && (nsnull != wd->ww)) {
        PRIntn ix = LOWORD(param) - VIEWER_DEMO0;
        char* url = new char[500];
        PR_snprintf(url, 500, "%s/test%d.html", SAMPLES_BASE_URL, ix);
        wd->ww->LoadURL(url);
        delete url;
      }
      break;

    case VIEWER_VISUAL_DEBUGGING:
      if ((nsnull != wd) && (nsnull != wd->ww)) {
        wd->ww->ShowFrameBorders(PRBool(!wd->ww->GetShowFrameBorders()));
      }
      break;
    case VIEWER_DUMP_CONTENT:
      if ((nsnull != wd) && (nsnull != wd->ww)) {
        wd->ww->DumpContent();
      }
      break;
    case VIEWER_DUMP_FRAMES:
      if ((nsnull != wd) && (nsnull != wd->ww)) {
        wd->ww->DumpFrames();
      }
      break;
    case VIEWER_DUMP_VIEWS:
      if ((nsnull != wd) && (nsnull != wd->ww)) {
        wd->ww->DumpViews();
      }
      break;
    case VIEWER_DUMP_STYLE:
      if ((nsnull != wd) && (nsnull != wd->ww)) {
        wd->ww->DumpStyle();
      }
      break;

    case VIEWER_APPEND_CONTENT:
      if ((nsnull != wd) && (nsnull != wd->ww)) {
        wd->ww->HackAppendContent();
      }
      break;
    case VIEWER_ONE_COLUMN:
    case VIEWER_TWO_COLUMN:
    case VIEWER_THREE_COLUMN:
      if ((nsnull != wd) && (nsnull != wd->ww)) {
        ShowPrintPreview(wd->ww, LOWORD(param) - VIEWER_ONE_COLUMN + 1);
      }
      break;
    case JS_CONSOLE:
      if (!gConsole) {

        // load the accelerator table for the console
        if (!JSConsole::sAccelTable) {
          JSConsole::sAccelTable = LoadAccelerators(gInstance, 
                                                    MAKEINTRESOURCE(ACCELERATOR_TABLE));
        }

        nsIScriptContext *context = nsnull;
        if (NS_OK == wd->ww->GetScriptContext(&context)) {
          // create the console
          gConsole = JSConsole::CreateConsole();
          gConsole->SetContext(context);
          // lifetime of the context is still unclear at this point.
          // Anyway, as long as the web widget is alive the context is alive.
          // Maybe the context shouldn't even be RefCounted
          context->Release();
          gConsole->SetNotification(DestroyConsole);
        }
        else {
          MessageBox(hWnd, "Unable to load JavaScript", "Viewer Error", MB_ICONSTOP);
        }
      }
      break;

    default:
      break;
    }
    break;

  case WM_SIZE:
    if ((nsnull != wd) && (nsnull != wd->ww)) {
      RECT r;
      ::GetClientRect(hWnd, &r);
      nsRect rr(0, 0, PRInt32(r.right - r.left), PRInt32(r.bottom - r.top));
      wd->ww->SetBounds(rr);
    }
    return 0;

  case WM_DESTROY:
    Destroy(wd);
    if (gWindows->Count() == 0) {
      PostQuitMessage(0);
    }
    return 0;

  case WM_CLOSE:
    // Let windows destroy the window
    break;

  default:
    break;
  }
  return DefWindowProc(hWnd, msg, param, lparam);
}

static WindowData* CreateTopLevel(const char* clazz, const char* title,
                                  int aWidth, int aHeight)
{
  // Create a simple top level window
  WindowData* wd = new WindowData();
  HWND window = ::CreateWindowEx(WS_EX_CLIENTEDGE,
                                 clazz, title,
                                 WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
                                 CW_USEDEFAULT, CW_USEDEFAULT,
                                 aWidth, aHeight,
                                 HWND_DESKTOP,
                                 NULL,
                                 gInstance,
                                 wd);
  wd->window = window;
  gWindows->AppendElement(wd);
  ::ShowWindow(window, SW_SHOW);
  ::UpdateWindow(window);
  return wd;
}

static nsresult ShowPrintPreview(nsIWebWidget* web, PRIntn aColumns)
{
  if (nsnull != web) {
    nsIDocument* doc = web->GetDocument();
    if (nsnull != doc) {
      nsIPresContext* cx;
      nsresult rv = NS_NewPrintPreviewContext(&cx);
      if (NS_OK != rv) {
        return rv;
      }

      WindowData* wd = CreateTopLevel(class2Name, "Print Preview", 500, 300);

      RECT r;
      ::GetClientRect(wd->window, &r);
      nsRect rr(0, 0, PRInt32(r.right - r.left), PRInt32(r.bottom - r.top));
      rv = NS_NewWebWidget(&wd->ww);
      rv = wd->ww->Init(wd->window, rr, doc, cx);
      wd->ww->Show();
      wd->observer = NewObserver(wd->ww);

      NS_RELEASE(cx);
      NS_RELEASE(doc);
    }
  }
  return NS_OK;
}

void ReleaseMemory()
{
  nsGlobalVariables::Release();
  delete gWindows;
}

#define WIDGET_DLL "raptorwidget.dll"
#define GFXWIN_DLL "raptorgfxwin.dll"
#define VIEW_DLL   "raptorview.dll"

int PASCAL
WinMain(HANDLE instance, HANDLE prevInstance, LPSTR cmdParam, int nCmdShow)
{
  PL_InitializeEventsLib("");

  gInstance = instance;
  gPrevInstance = prevInstance;
  gWindows = new nsVoidArray();

  static NS_DEFINE_IID(kCWindowIID, NS_WINDOW_CID);
  static NS_DEFINE_IID(kCScrollbarIID, NS_VERTSCROLLBAR_CID);
  static NS_DEFINE_IID(kCButtonIID, NS_BUTTON_CID);
  static NS_DEFINE_IID(kCComboBoxCID, NS_COMBOBOX_CID);
  static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
  static NS_DEFINE_IID(kCListBoxCID, NS_LISTBOX_CID);
  static NS_DEFINE_IID(kCRadioButtonCID, NS_RADIOBUTTON_CID);
  static NS_DEFINE_IID(kCTextAreaCID, NS_TEXTAREA_CID);
  static NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);
  static NS_DEFINE_IID(kCCheckButtonIID, NS_CHECKBUTTON_CID);
  static NS_DEFINE_IID(kCChildIID, NS_CHILD_CID);

  NSRepository::RegisterFactory(kCWindowIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCScrollbarIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCButtonIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCComboBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCFileWidgetCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCListBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCRadioButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCTextAreaCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCTextFieldCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCCheckButtonIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCChildIID, WIDGET_DLL, PR_FALSE, PR_FALSE);

  static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID);
  static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID);
  static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID);
  static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID);

  NSRepository::RegisterFactory(kCRenderingContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCDeviceContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCFontMetricsIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCImageIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);

  static NS_DEFINE_IID(kCViewManagerCID, NS_VIEW_MANAGER_CID);
  static NS_DEFINE_IID(kCViewCID, NS_VIEW_CID);
  static NS_DEFINE_IID(kCScrollingViewCID, NS_SCROLLING_VIEW_CID);

  NSRepository::RegisterFactory(kCViewManagerCID, VIEW_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCViewCID, VIEW_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCScrollingViewCID, VIEW_DLL, PR_FALSE, PR_FALSE);

  // initialize the toolkit (widget library).
  // This may look weak and in fact it is...but hey...
  //XXX move it somewhere else please
  //NS_InitToolkit(PR_GetCurrentThread());

  if (!prevInstance) {
    WNDCLASS wndClass;
    wndClass.style = 0;
    wndClass.lpfnWndProc = WndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = gInstance;
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH) GetStockObject(LTGRAY_BRUSH);
    wndClass.lpszMenuName = class1Name;
    wndClass.lpszClassName = class1Name;
    RegisterClass(&wndClass);

    wndClass.lpszMenuName = class2Name;
    wndClass.lpszClassName = class2Name;
    RegisterClass(&wndClass);
  }

  // Create our first top level window
  WindowData* wd = CreateTopLevel(class1Name, "Raptor HTML Viewer", 620, 400);

  // Now embed the web widget in it
  RECT r;
  ::GetClientRect(wd->window, &r);
  nsresult rv = NS_NewWebWidget(&wd->ww);
  nsRect rr(0, 0, PRInt32(r.right - r.left), PRInt32(r.bottom - r.top));
  rv = wd->ww->Init(wd->window, rr);
  wd->ww->Show();
  wd->observer = NewObserver(wd->ww);


  // Determine if we should run the purify test
  PRBool  purify = DoPurifyTest();
  nsDocLoader* dl = nsnull;
  if (purify)
  {
    dl = new nsDocLoader(wd->ww);

    // Add the documents to the loader
    AddTestDocs(dl);

    // Start the timer
    dl->StartTimedLoading();
  }
  else
  {
    // Load the starting url if we have one
    wd->ww->LoadURL(startURL ? startURL : START_URL);
  }

  // Process messages
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
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
  if (dl != nsnull)
    delete dl;
  ReleaseMemory();
  if (!prevInstance) {
    UnregisterClass(class1Name, gInstance);
    UnregisterClass(class2Name, gInstance);
  }
  return msg.wParam;
}

void main(int argc, char **argv)
{
  if (argc > 1) {
    startURL = argv[1];
  }
  WinMain(GetModuleHandle(NULL), NULL, 0, SW_SHOW);
}
