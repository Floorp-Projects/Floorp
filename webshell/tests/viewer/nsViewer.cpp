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

/*
 * Only cross-platform code should reside in this file.
 * winmain.cpp (Win32) and main.cpp (Unix) contain the platform specific code.
 */

#include <stdio.h>
#ifdef XP_MAC
#include <stdlib.h>
#else
#include <malloc.h>
#endif

//#define VIEWER_PLUGINS

#define NS_IMPL_IDS
#include "nsViewer.h"
#include "resources.h"          // #defines ID's for menu items
#include "nsIWidget.h"
#include "nsGlobalVariables.h"
#include "nsIWebShell.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIURL.h"
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
#include "prenv.h"
#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMDocument.h"
#include "nsDocLoader.h"
#include "nsIFileWidget.h"
#include "nsIDOMDocument.h"
#include "nsEditorMode.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsISizeOfHandler.h"
#include "nsIViewManager.h"
#include "nsGfxCIID.h"
#include "nsIDeviceContext.h"
#include "nsINetService.h"
#include "nsINetContainerApplication.h"
#include "nsIButton.h"
#include "nsITextWidget.h"
#include "nsIPref.h"
#include "nsIImageObserver.h"
#include "nsFont.h"
#include "nsIFontMetrics.h"
#include "nsIImageRequest.h"
#include "nsITimer.h"
#include "nsIDocumentViewer.h"
#include "nsIStyleSet.h"
#include "nsIStyleContext.h"
#include "nsIBrowserWindow.h"

#ifdef VIEWER_PLUGINS
#include "nsPluginsCID.h"
#include "nsplugin.h"
#include "nsIPluginHost.h"
#endif

// Class ID's
static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
static NS_DEFINE_IID(kCWindowCID, NS_WINDOW_CID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kCAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
static NS_DEFINE_IID(kCWindowIID, NS_WINDOW_CID);
static NS_DEFINE_IID(kCScrollbarIID, NS_VERTSCROLLBAR_CID);
static NS_DEFINE_IID(kCHScrollbarIID, NS_HORZSCROLLBAR_CID);
static NS_DEFINE_IID(kCButtonCID, NS_BUTTON_CID);
static NS_DEFINE_IID(kCComboBoxCID, NS_COMBOBOX_CID);
static NS_DEFINE_IID(kCListBoxCID, NS_LISTBOX_CID);
static NS_DEFINE_IID(kCRadioButtonCID, NS_RADIOBUTTON_CID);
static NS_DEFINE_IID(kCTextAreaCID, NS_TEXTAREA_CID);
static NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kCCheckButtonIID, NS_CHECKBUTTON_CID);
static NS_DEFINE_IID(kCChildIID, NS_CHILD_CID);
static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID);
static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID);
static NS_DEFINE_IID(kCRegionIID, NS_REGION_CID);
static NS_DEFINE_IID(kCViewManagerCID, NS_VIEW_MANAGER_CID);
static NS_DEFINE_IID(kCViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kCScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kCDocumentLoaderCID, NS_DOCUMENTLOADER_CID);
static NS_DEFINE_IID(kBrowserWindowCID, NS_BROWSER_WINDOW_CID);
static NS_DEFINE_IID(kThrobberCID, NS_THROBBER_CID);

#ifdef VIEWER_PLUGINS
static NS_DEFINE_IID(kCPluginHostCID, NS_PLUGIN_HOST_CID);
#endif

// IID's
static NS_DEFINE_IID(kIButtonIID, NS_IBUTTON_IID);
static NS_DEFINE_IID(kIBrowserWindowIID, NS_IBROWSER_WINDOW_IID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kIDocumentLoaderIID, NS_IDOCUMENTLOADER_IID);
static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);
static NS_DEFINE_IID(kIChildWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIThrobberIID, NS_ITHROBBER_IID);

#ifdef VIEWER_PLUGINS
static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIPluginHostIID, NS_IPLUGINHOST_IID);
#endif

#define VIEWER_UI
#undef INSET_WEBWIDGET

#ifdef VIEWER_UI
#define THROBBER_WIDTH 32
#define THROBBER_HEIGHT 32
#define BUTTON_WIDTH 90
#else
#define THROBBER_WIDTH 0
#define THROBBER_HEIGHT 0
#define BUTTON_WIDTH 0
#endif

#define BUTTON_HEIGHT THROBBER_HEIGHT
#define THROB_NUM 14
#define THROBBER_AT "resource:/res/throbber/anims%02d.gif"

#ifdef INSET_WEBWIDGET
#define WEBWIDGET_LEFT_INSET 5
#define WEBWIDGET_RIGHT_INSET 5
#define WEBWIDGET_TOP_INSET 5
#define WEBWIDGET_BOTTOM_INSET 5
#else
#define WEBWIDGET_LEFT_INSET 0
#define WEBWIDGET_RIGHT_INSET 0
#define WEBWIDGET_TOP_INSET 0
#define WEBWIDGET_BOTTOM_INSET 0
#endif

#define SAMPLES_BASE_URL "resource:/res/samples"
#define DEFAULT_DOC "/test0.html"

nsViewer* gTheViewer = nsnull;
WindowData * gMainWindowData = nsnull;
nsIAppShell *gAppShell= nsnull;
nsIPref *gPrefs;
#define MAX_RL 15
static char* gRLList[MAX_RL];
static int gRLPos = 0;
static char* startURL;
static nsVoidArray* gWindows;
static PRBool gDoPurify;          // run in Purify auto mode
static PRBool gDoQuantify;        // run in Quantify auto mode
static PRBool gLoadTestFromFile;  // run in auto mode by pulling URLs from a file (gInputFileName)
static PRInt32 gDelay=1;          // if running in an auto mode, this is the delay between URL loads
static PRInt32 gRepeatCount=1;    // if running in an auto mode, this is the number of times to cycle through the input
static PRInt32 gNumSamples=9;     // if running in an auto mode that uses the samples, this is the last sample to load
static char gInputFileName[MAXPATHLEN];

#ifdef VIEWER_PLUGINS
nsIPluginManager *gPluginManager = nsnull;
nsIPluginHost *gPluginHost = nsnull;
#endif

// Temporary Netlib stuff...
/* XXX: Don't include net.h... */
extern "C" {
extern int  NET_PollSockets();
};

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);

static nsIPresShell*
GetPresShell(nsIWebShell* aWebWidget)
{
  nsIPresShell* shell = nsnull;
  if (nsnull != aWebWidget) {
    nsIContentViewer* cv = nsnull;
    aWebWidget->GetContentViewer(cv);
    if (nsnull != cv) {
      nsIDocumentViewer* docv = nsnull;
      cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
      if (nsnull != docv) {
        nsIPresContext* cx;
        docv->GetPresContext(cx);
        if (nsnull != cx) {
          shell = cx->GetShell();
          NS_RELEASE(cx);
        }
        NS_RELEASE(docv);
      }
      NS_RELEASE(cv);
    }
  }
  return shell;
}

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENT_OBSERVER_IID);
static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);


DocObserver::DocObserver(nsIWidget* aWindow, nsIWebShell* aWebWidget) {
  NS_INIT_REFCNT();
  mWebWidget = aWebWidget;
  NS_ADDREF(aWebWidget);

  mWindowWidget = aWindow;
  NS_ADDREF(aWindow);

  NSRepository::CreateInstance(kCDocumentLoaderCID, nsnull, kIDocumentLoaderIID, (void**)&mDocLoader);
  
  mScriptContext = nsnull;
  mScriptGlobal = nsnull;
}

DocObserver::~DocObserver() {
  NS_RELEASE(mWindowWidget);
  NS_RELEASE(mWebWidget);
  NS_RELEASE(mDocLoader);
  NS_IF_RELEASE(mScriptContext);
  NS_IF_RELEASE(mScriptGlobal);
}


NS_IMPL_ADDREF(DocObserver);
NS_IMPL_RELEASE(DocObserver);

nsresult
DocObserver::QueryInterface(const nsIID& aIID,
                            void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDocumentObserverIID)) {
    *aInstancePtrResult = (void*) ((nsIDocumentObserver*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIStreamObserverIID)) {
    *aInstancePtrResult = (void*) ((nsIStreamObserver*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptContextOwnerIID)) {
    *aInstancePtrResult = (void*)(nsIScriptContextOwner*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)((nsIDocumentObserver*)this));
    AddRef();
    return NS_OK;
  }

#ifdef VIEWER_PLUGINS
  //XXX wow, what a hack... MMP
  if (nsnull != gPluginManager)
    return gPluginManager->QueryInterface(aIID, aInstancePtrResult);
#endif

  return NS_NOINTERFACE;
}

NS_IMETHODIMP
DocObserver::SetTitle(const nsString& aTitle)
{
  gTheViewer->mTitle = aTitle;
  gTheViewer->mWD->windowWidget->SetTitle(aTitle);
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::GetTitle(nsString& aResult)
{
  aResult = gTheViewer->mTitle;
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::WillLoadURL(nsIWebShell* aShell, const nsString& aURL)
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::BeginLoadURL(nsIWebShell* aShell, const nsString& aURL)
{
  gTheViewer->mThrobber->Start();
  gTheViewer->mLocation->SetText(aURL);
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::EndLoadURL(nsIWebShell* aShell, const nsString& aURL)
{
  gTheViewer->mThrobber->Stop();
  return NS_OK;
}

#if 0
nsresult
DocObserver::LoadURL(const nsString& aURLSpec, 
                     const char* aCommand,
                     nsIViewerContainer* aContainer,
                     nsIPostData* aPostData,
                     nsISupports* aExtraInfo,
                     nsIStreamObserver* anObserver)
{
  mURL = aURLSpec;
  for (int i = 0; i < MAX_RL; i++) {
     if (gRLList[i]) {
        PL_strfree(gRLList[i]);
        gRLList[i] = 0;
     }
  }
  gRLPos = 0;

    mViewer->ResetRelatedLinks();
    if (mViewer->mRelatedLinks) {
       char * pStr = aURLSpec.ToNewCString();
       mViewer->mRelatedLinks->SetRLWindowURL(mViewer->mRLWindow, pStr);
       if (pStr)
          free(pStr);
       mViewer->mRelatedLinks->WindowItems(mViewer->mRLWindow);
    }

  return mDocLoader->LoadURL(aURLSpec, aCommand, aContainer,
                             aPostData, aExtraInfo, anObserver);
}
#endif

NS_IMETHODIMP
DocObserver::BeginUpdate()
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::EndUpdate()
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::BeginLoad()
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::EndLoad()
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::BeginReflow(nsIPresShell* aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::EndReflow(nsIPresShell* aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::ContentChanged(nsIContent* aContent,
                            nsISupports* aSubContent)
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::ContentAppended(nsIContent* aContainer)
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::ContentInserted(nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer)
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::ContentReplaced(nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer)
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::ContentWillBeRemoved(nsIContent* aContainer,
                                  nsIContent* aChild,
                                  PRInt32 aIndexInContainer)
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::ContentHasBeenRemoved(nsIContent* aContainer,
                                   nsIContent* aChild,
                                   PRInt32 aIndexInContainer)
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::StyleSheetAdded(nsIStyleSheet* aStyleSheet)
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax,
                        const nsString& aMsg)
{
  if (nsnull != aURL) {
    nsAutoString url;
    aURL->ToString(url);
    fputs(url, stdout);
  }
  printf(": progress %d", aProgress);
  if (0 != aProgressMax) {
    printf(" (out of %d)", aProgressMax);
  }
  fputs("\n", stdout);
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  if (nsnull != aURL) {
    nsAutoString url;
    aURL->ToString(url);
    fputs(url, stdout);
  }
  fputs(": start\n", stdout);
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::OnStopBinding(nsIURL* aURL, PRInt32 status, const nsString& aMsg)
{
  if (nsnull != aURL) {
    nsAutoString url;
    aURL->ToString(url);
    fputs(url, stdout);
  }
  fputs(": stop\n", stdout);

  if (nsnull != gTheViewer->mThrobber) {
    gTheViewer->mThrobber->Stop();
  }
  return NS_OK;
}

static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);

nsresult 
DocObserver::GetScriptContext(nsIScriptContext **aContext)
{
  NS_PRECONDITION(nsnull != aContext, "null arg");
  nsresult res = NS_OK;

  if (nsnull == mScriptGlobal) {
    res = NS_NewScriptGlobalObject(&mScriptGlobal);
    if (NS_OK != res) {
      return res;
    }

    res = NS_CreateContext(mScriptGlobal, &mScriptContext);
    if (NS_OK != res) {
      return res;
    }

    nsIPresShell* shell = GetPresShell(mWebWidget);
    if (nsnull != shell) {
      nsIDocument* doc = shell->GetDocument();
      if (nsnull != doc) {
        nsIDOMDocument *domdoc = nsnull;
        doc->QueryInterface(kIDOMDocumentIID, (void**) &domdoc);
        if (nsnull != domdoc) {
          mScriptGlobal->SetNewDocument(domdoc);
          NS_RELEASE(domdoc);
        }
        NS_RELEASE(doc);
      }
      NS_RELEASE(shell);
    }
  }

  *aContext = mScriptContext;
  NS_ADDREF(mScriptContext);

  return res;
}

nsresult 
DocObserver::ReleaseScriptContext(nsIScriptContext *aContext)
{
  NS_IF_RELEASE(aContext);

  return NS_OK;
}

static DocObserver* NewObserver(nsIWidget* aWindow, nsIWebShell* aWebShell)
{
  nsIWebShellContainer* oldContainer;
  nsresult rv = aWebShell->GetContainer(oldContainer);
  if (NS_OK == rv) {
    if (nsnull == oldContainer) {
      DocObserver* it = new DocObserver(aWindow, aWebShell);
      NS_ADDREF(it);
      aWebShell->SetContainer((nsIWebShellContainer*) it);
      return it;
    }
  }
  return nsnull;
}

//----------------------------------------------------------------------

void
nsViewer::Layout(WindowData* aWindowData, int aWidth, int aHeight)
{
  if (aWindowData->observer) {
    nsRect rr(0, BUTTON_HEIGHT,
              aWidth,
              aHeight - BUTTON_HEIGHT);

    // position location bar (it's stretchy)
    if (mLocation) {
      if (mThrobber)
        mLocation->Resize(2*BUTTON_WIDTH, 0, rr.width - (2*BUTTON_WIDTH + THROBBER_WIDTH),
                          BUTTON_HEIGHT, PR_TRUE);
      else
        mLocation->Resize(2*BUTTON_WIDTH, 0, rr.width - 2*BUTTON_WIDTH,
                          BUTTON_HEIGHT, PR_TRUE);
    }

    if (mThrobber) {
        mThrobber->MoveTo(rr.width - THROBBER_WIDTH, 0);
    }

    // inset the web widget
    rr.x += WEBWIDGET_LEFT_INSET;
    rr.y += WEBWIDGET_TOP_INSET;
    rr.width -= WEBWIDGET_LEFT_INSET + WEBWIDGET_RIGHT_INSET;
    rr.height -= WEBWIDGET_TOP_INSET + WEBWIDGET_BOTTOM_INSET;
    aWindowData->observer->mWebWidget->SetBounds(rr);
  }
}

nsEventStatus PR_CALLBACK HandleEventApp(nsGUIEvent *aEvent)
{ 
    nsEventStatus result = nsEventStatus_eIgnore;
    switch(aEvent->message) {
      case NS_SIZE:
        {
          struct WindowData *wd = gTheViewer->FindWindowData(aEvent->widget);
          nsSizeEvent* sizeEvent = (nsSizeEvent*)aEvent;  
          if (wd->mViewer) {
            wd->mViewer->Layout(wd, sizeEvent->windowSize->width,
                                sizeEvent->windowSize->height);
          }
        }
        return nsEventStatus_eConsumeNoDefault;

      case NS_DESTROY:
        {
          struct WindowData *wd = gTheViewer->FindWindowData(aEvent->widget);
          gTheViewer->Destroy(wd);

          if (gWindows->Count() == 0) {
            gTheViewer->Stop();
     
          }
        }
        return nsEventStatus_eConsumeDoDefault;
 

      default:
        break;
    }
    return(gTheViewer->DispatchMenuItem(aEvent));
}

void ReleaseMemory()
{
  nsGlobalVariables::Release();
  delete gWindows;
}

void PrintHelpInfo(char **argv)
{
  fprintf(stderr, "Usage: %s [-p][-q][-md #][-f filename][-d #] [starting url]\n", argv[0]);
  fprintf(stderr, "\t-p[#]   -- run purify, optionally with a # that says which sample to stop at.  For example, -p2 says to run samples 0, 1, and 2.\n");
  fprintf(stderr, "\t-q   -- run quantify\n");
  fprintf(stderr, "\t-md # -- set the crt debug flags to #\n");
  fprintf(stderr, "\t-d # -- set the delay between URL loads to # (in milliseconds)\n");
  fprintf(stderr, "\t-r # -- set the repeat count, which is the number of times the URLs will be loaded in batch mode.\n");
  fprintf(stderr, "\t-f filename -- read a list of URLs from <filename>\n");
}

void nsViewer::ProcessArguments(int argc, char **argv)
{
 int i;
 for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (strncmp(argv[i], "-p", 2) == 0) {
        gDoPurify = PR_TRUE;
        char *optionalSampleStopIndex = &(argv[i][2]);
        if (nsnull!=*optionalSampleStopIndex)
        {
          if (1!=sscanf(optionalSampleStopIndex, "%d", &gNumSamples))
          {
            PrintHelpInfo(argv);
            exit(-1);
          }
        }
      }
      else if (strcmp(argv[i], "-q") == 0) {
        gDoQuantify = PR_TRUE;
      }
      else if (strcmp(argv[i], "-f") == 0) {
        gLoadTestFromFile = PR_TRUE;
        i++;
        if (i>=argc || nsnull==argv[i] || nsnull==*(argv[i]))
        {
          PrintHelpInfo(argv);
          exit(-1);
        }
        strcpy(gInputFileName, argv[i]);
      }
      else if (strcmp(argv[i], "-d") == 0) {
        i++;
        if (i>=argc || 1!=sscanf(argv[i], "%d", &gDelay))
        {
          PrintHelpInfo(argv);
          exit(-1);
        }
      }
      else if (strcmp(argv[i], "-md") == 0) {
        if (i == argc - 1) {
          PrintHelpInfo(argv);
          exit(-1);
        }
        PRUint32 newFlags = atoi(argv[++i]);
        CrtSetDebug(newFlags);
      }
      else if (strcmp(argv[i], "-r") == 0) {
        i++;
        if (i>=argc || 1!=sscanf(argv[i], "%d", &gRepeatCount))
        {
          PrintHelpInfo(argv);
          exit(-1);
        }
      }
      else {
        PrintHelpInfo(argv);
        exit(-1);
      }
    }
    else
      break;
  }
  if (i < argc) {
    startURL = argv[i];
  }
}

//----------------------------------------------------------------------
// nsViewer Implementation
//----------------------------------------------------------------------

static NS_DEFINE_IID(kINetContainerApplicationIID, NS_INETCONTAINERAPPLICATION_IID);

NS_IMPL_ISUPPORTS(nsViewer, kINetContainerApplicationIID);

/*
 * Purify methods
 */

void nsViewer::AddTestDocs(nsDocLoader* aDocLoader)
{
  char url[500];
  for (int docnum = 0; docnum < gNumSamples; docnum++)
  {
    PR_snprintf(url, 500, "%s/test%d.html", GetBaseURL(), docnum);
    aDocLoader->AddURL(url);
  }
}

/*
 * SelfTest methods
 */

// Load a bunch of test url's from a file
void nsViewer::AddTestDocsFromFile(nsDocLoader* aDocLoader, char* aFileName)
{
#ifdef XP_PC
  FILE* fp = fopen(aFileName, "rb");
#else
  FILE* fp = fopen(aFileName, "r");
#endif

  for (;;) {
    char linebuf[2000];
    char* cp = fgets(linebuf, sizeof(linebuf), fp);
    if (nsnull == cp) {
      break;
    }
    if (linebuf[0] == '#') {
      continue;
    }

    // strip crlf's from the line
    int len = strlen(linebuf);
    if (0 != len) {
      if (('\n' == linebuf[len-1]) || ('\r' == linebuf[len-1])) {
        linebuf[--len] = 0;
      }
    }
    if (0 != len) {
      if (('\n' == linebuf[len-1]) || ('\r' == linebuf[len-1])) {
        linebuf[--len] = 0;
      }
    }

    // Add non-empty lines to the test list
    if (0 != len) {
      aDocLoader->AddURL(linebuf);
    }
  }

  fclose(fp);
}

void nsViewer::DestroyAllWindows()
{
  if (nsnull != gWindows) {
    PRInt32 n = gWindows->Count();
    for (PRInt32 i = 0; i < n; i++) {
      WindowData* wd = (WindowData*) gWindows->ElementAt(i);
      wd->windowWidget->Destroy();
    }
    gWindows->Clear();
  }
}

struct WindowData* nsViewer::FindWindowData(nsIWidget* aWidget)
{
  PRInt32 i, n = gWindows->Count();
  for (i = 0; i < n; i++) {
    WindowData* wd = (WindowData*) gWindows->ElementAt(i);
    if (wd->windowWidget == aWidget) {
      return(wd);
    }
  }
  return(nsnull);
}

#define FILE_PROTOCOL "file://"

// Displays the Open common dialog box and lets the user specify an HTML file
// to open.
PRBool nsViewer::GetFileNameFromFileSelector(nsIWidget* aParentWindow, nsString* aFileName)
{
  PRBool selectedFileName = PR_FALSE;
  nsIFileWidget *fileWidget;
  nsString title("Open HTML");

  NSRepository::CreateInstance(kCFileWidgetCID, nsnull, kIFileWidgetIID, (void**)&fileWidget);
  nsString titles[] = {"all files","html" };
  nsString filters[] = {"*.*", "*.html"};
  fileWidget->SetFilterList(2, titles, filters);
  fileWidget->Create(aParentWindow,
                     title,
                     eMode_load,
                     nsnull,
                     nsnull);

  PRUint32 result = fileWidget->Show();
  if (result) {
    fileWidget->GetFile(*aFileName);
    selectedFileName = PR_TRUE;
  }
 
  NS_RELEASE(fileWidget);
  return(selectedFileName);
}

// Displays the Open common dialog box and lets the user specify an HTML file
// to open. Then passes the filename along to the Web widget.
void nsViewer::OpenHTMLFile(WindowData* wd)
{
  nsString      fileName;
  char          szFile[MAXPATHLEN];

  if (GetFileNameFromFileSelector(wd->windowWidget, &fileName)) {
    

    fileName.ToCString(szFile, MAXPATHLEN);
    PRInt32 len = strlen(szFile);
    char*   lpszFileURL = (char*)malloc(len + sizeof(FILE_PROTOCOL));
    
    // Translate '\' to '/'
    for (PRInt32 i = 0; i < len; i++) {
      if (szFile[i] == '\\') {
        szFile[i] = '/';
      }
    }

    // Build the file URL
    PR_snprintf(lpszFileURL, MAXPATHLEN, "%s%s", FILE_PROTOCOL, szFile);

    // Ask the Web widget to load the file URL
    GoTo(lpszFileURL);
    free(lpszFileURL);
  }
}

// Selects all the Content
void nsViewer::SelectAll(WindowData* wd)
{
  if (wd->observer != nsnull) {
#ifdef XXX_fix_me
    nsIDocument* doc = wd->observer->mWebWidget->GetDocument();
    if (doc != nsnull) {
      doc->SelectAll();
      nsIFrame::ShowFrameBorders(PR_FALSE);
      /*PRInt32 numShells = doc->GetNumberOfShells();
      for (PRInt32 i=0;i<numShells;i++) {
        nsIPresShell   * shell   = doc->GetShellAt(i);
        //nsIViewManager * viewMgr = shell->GetViewManager();
        nsIFrame       * frame   = shell->GetRootFrame();
        nsRect    rect;
        nsIView * view;
        nsPoint   pnt;
        frame->GetOffsetFromView(pnt, view);
        frame->GetRect(rect);
        rect.x = pnt.x;
        rect.y = pnt.y;
        if (view != nsnull) {
          nsIViewManager * viewMgr = view->GetViewManager();
          if (viewMgr != nsnull) {
            viewMgr->UpdateView(view, rect, 0);
          }
        }
        NS_IF_RELEASE(shell);
        //NS_IF_RELEASE(frame);
      }*/

      NS_IF_RELEASE(doc);
    }
#endif
  }
}

/**
 * Create a top level window
 */

WindowData* nsViewer::CreateTopLevel(const char* title,
                                  int aWidth, int aHeight)
{
  WindowData* wd = new WindowData();
  nsIWidget* window = nsnull;
  NSRepository::CreateInstance(kCWindowCID, nsnull, kIWidgetIID, (void**)&window);
  nsRect rect(100, 100, aWidth, aHeight);

  window->Create((nsIWidget*)NULL, rect, HandleEventApp, nsnull, nsnull, (nsWidgetInitData *)gAppShell->GetNativeData(NS_NATIVE_SHELL));
  window->SetTitle(title);
  wd->windowWidget = window; 
  gWindows->AppendElement(wd);
  window->Show(PR_TRUE);
 
  return wd;
}


nsresult nsViewer::ShowPrintPreview(nsIWebShell* web, PRIntn aColumns)
{
#if XXX_fix_me
  if (nsnull != web) {
    nsIDocument* doc = web->GetDocument();
    if (nsnull != doc) {
      nsIPresContext* cx;
      nsIDeviceContext* dx;

      WindowData* wd = CreateTopLevel("Print Preview", 500, 300);

      static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
      static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

      nsresult rv = NSRepository::CreateInstance(kDeviceContextCID, nsnull, kDeviceContextIID, (void **)&dx);

      if (NS_OK == rv) {
        dx->Init(wd->windowWidget->GetNativeData(NS_NATIVE_WIDGET));
        dx->SetDevUnitsToAppUnits(dx->GetDevUnitsToTwips());
        dx->SetAppUnitsToDevUnits(dx->GetTwipsToDevUnits());
        dx->SetGamma(1.7f);

        NS_ADDREF(dx);
      }

      rv = NS_NewPrintPreviewContext(&cx);

      if (NS_OK != rv) {
        return rv;
      }

      cx->Init(dx);
  
      nsIWebShell* ww;
      nsRect bounds;
      wd->windowWidget->GetBounds(bounds);
      nsRect rr(0, 0, bounds.width, bounds.height);

      rv = NS_NewWebWidget(&ww);
      AddMenu(wd->windowWidget, PR_TRUE);

      // XXX There is a chicken-and-egg bug here: the Init method goes
      // ahead and reflows the document before we have added the
      // observer; adding the observer sets the link-handler. The
      // link-handler is needed to do the initial reflow properly. YIKES!

      rv = ww->Init(wd->windowWidget->GetNativeData(NS_NATIVE_WIDGET),
                    rr, doc, cx);
      wd->observer = NewObserver(wd->windowWidget, ww);
      ww->Show();

      NS_RELEASE(dx);
      NS_RELEASE(cx);
      NS_RELEASE(doc);
      NS_RELEASE(ww);
    }
  }
#endif
  return NS_OK;
}

void nsViewer::ExitViewer()
{
  DestroyAllWindows();
  Stop();
}

nsEventStatus nsViewer::DispatchMenuItem(nsGUIEvent *aEvent)
{
  nsEventStatus result = nsEventStatus_eIgnore;
  if (aEvent->message == NS_MENU_SELECTED) {
    nsMenuEvent* menuEvent = (nsMenuEvent*)aEvent;
    WindowData* wd = FindWindowData(aEvent->widget);
    return ProcessMenu(menuEvent->menuItem, wd);
  }
  return result;
}

nsEventStatus nsViewer::DispatchMenuItem(PRUint32 aId)
{
  return ProcessMenu(aId, gMainWindowData);
}

nsEventStatus nsViewer::ProcessMenu(PRUint32 aId, WindowData* wd)
{
  nsEventStatus result = nsEventStatus_eIgnore;

  switch(aId) {
    case VIEWER_EXIT:
      ExitViewer();
      return nsEventStatus_eConsumeNoDefault;

    case PREVIEW_CLOSE:
      wd->windowWidget->Destroy();
      return nsEventStatus_eConsumeNoDefault;

    case VIEWER_FILE_OPEN:
      OpenHTMLFile(wd);
      break;

    case VIEWER_EDIT_CUT:
      break;

    case VIEWER_EDIT_COPY:
      CopySelection(wd);
      break;

    case VIEWER_EDIT_PASTE:
      break;

    case VIEWER_EDIT_SELECTALL:
      SelectAll(wd);
      break;

    case VIEWER_EDIT_FINDINPAGE:
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
    case VIEWER_DEMO9: 
      if ((nsnull != wd) && (nsnull != wd->observer)) {
        PRIntn ix = aId - VIEWER_DEMO0;
        char* url = new char[500];
        PR_snprintf(url, 500, "%s/test%d.html", GetBaseURL(), ix);
        wd->mViewer->GoTo(url);
        delete url;
      }
      break;

    case VIEWER_VISUAL_DEBUGGING:
      if ((nsnull != wd) && (nsnull != wd->observer)) {
        gTheViewer->ToggleFrameBorders(wd->observer->mWebWidget);
      }
      break;

    case VIEWER_DUMP_CONTENT:
      if ((nsnull != wd) && (nsnull != wd->observer)) {
        gTheViewer->DumpContent(wd->observer->mWebWidget);
        gTheViewer->DumpShells(wd->observer->mWebWidget);
      }
      break;
    case VIEWER_DUMP_FRAMES:
      if ((nsnull != wd) && (nsnull != wd->observer)) {
        gTheViewer->DumpFrames(wd->observer->mWebWidget);
      }
      break;
    case VIEWER_DUMP_VIEWS:
      if ((nsnull != wd) && (nsnull != wd->observer)) {
        gTheViewer->DumpViews(wd->observer->mWebWidget);
      }
      break;
    case VIEWER_DUMP_STYLE_SHEETS:
      if ((nsnull != wd) && (nsnull != wd->observer)) {
        gTheViewer->DumpStyleSheets(wd->observer->mWebWidget);
      }
      break;
    case VIEWER_DUMP_STYLE_CONTEXTS:
      if ((nsnull != wd) && (nsnull != wd->observer)) {
        gTheViewer->DumpStyleContexts(wd->observer->mWebWidget);
      }
      break;

    case VIEWER_TOP100:
      DoSiteWalker(wd);
      break;

    case VIEWER_SHOW_CONTENT_SIZE:
      if ((nsnull != wd) && (nsnull != wd->observer)) {
        gTheViewer->ShowContentSize(wd->observer->mWebWidget);
      }
      break;

    case VIEWER_SHOW_FRAME_SIZE:
      if ((nsnull != wd) && (nsnull != wd->observer)) {
        gTheViewer->ShowFrameSize(wd->observer->mWebWidget);
      }
      break;

    case VIEWER_SHOW_STYLE_SIZE:
      if ((nsnull != wd) && (nsnull != wd->observer)) {
        gTheViewer->ShowStyleSize(wd->observer->mWebWidget);
      }
      break;

#if XXX_fix_me
    case VIEWER_SHOW_CONTENT_QUALITY:
      if ((nsnull != wd) && (nsnull != wd->observer)) {
        nsIPresContext *px = wd->observer->mWebWidget->GetPresContext();
        nsIPresShell   *ps = px->GetShell();
        nsIViewManager *vm = ps->GetViewManager();

        vm->ShowQuality(!vm->GetShowQuality());

        NS_RELEASE(vm);
        NS_RELEASE(ps);
        NS_RELEASE(px);
      }
      break;
#endif

    case VIEWER_ONE_COLUMN:
    case VIEWER_TWO_COLUMN:
    case VIEWER_THREE_COLUMN:
      if ((nsnull != wd) && (nsnull != wd->observer)) {
//XXX_fix_me        ShowPrintPreview(wd->observer->mWebWidget, aId - VIEWER_ONE_COLUMN + 1);
      }
      break;

    case JS_CONSOLE:
        ShowConsole(wd);
      break;

    case EDITOR_MODE:
      if ((nsnull != wd) && (nsnull != wd->observer)) {
#if XXX_fix_me
        nsIDOMDocument* domDoc = nsnull;
        if (NS_OK == wd->observer->mWebWidget->GetDOMDocument(&domDoc)) {
          nsInitEditorMode(domDoc);
          domDoc->Release();
        }
#endif
      }
      break;

    case VIEWER_DEBUGSAVE:
      DoDebugSave(wd);
      break;

    case VIEWER_RL_BASE:
    case VIEWER_RL_BASE+1:
    case VIEWER_RL_BASE+2:
    case VIEWER_RL_BASE+3:
    case VIEWER_RL_BASE+4:
    case VIEWER_RL_BASE+5:
    case VIEWER_RL_BASE+6:
    case VIEWER_RL_BASE+7:
    case VIEWER_RL_BASE+8:
    case VIEWER_RL_BASE+9:
    case VIEWER_RL_BASE+10:
      if (wd) {
         wd->mViewer->GoTo(gRLList[aId-VIEWER_RL_BASE]);
      }
      break;

  }

  return(result);
}

void nsViewer::CleanupViewer(nsDocLoader* aDocLoader) 
{
  NS_IF_RELEASE(aDocLoader);
  ReleaseMemory();
  NS_ShutdownINetService();
}

nsEventStatus PR_CALLBACK HandleBackEvent(nsGUIEvent *aEvent)
{
  switch(aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_UP:
    gTheViewer->Back();
    break;
  }
  return nsEventStatus_eIgnore;
}

void
nsViewer::Back()
{
  if (nsnull != mWD && nsnull != mWD->observer) {
    mWD->observer->mWebWidget->Back(mWD->observer);
  }
}

nsEventStatus PR_CALLBACK HandleForwardEvent(nsGUIEvent *aEvent)
{
  switch(aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_UP:
    gTheViewer->Forward();
    break;
  }
  return nsEventStatus_eIgnore;
}

void
nsViewer::Forward()
{
  if (nsnull != mWD && nsnull != mWD->observer) {
    mWD->observer->mWebWidget->Forward(mWD->observer);
  }
}

nsEventStatus PR_CALLBACK HandleLocationEvent(nsGUIEvent *aEvent)
{
  switch (aEvent->message) {
  case NS_KEY_UP:
    if (NS_VK_RETURN == ((nsKeyEvent*)aEvent)->keyCode) {
      nsAutoString text;
      gTheViewer->mLocation->GetText(text, 1000);
      gTheViewer->GoTo(text);
    }
    break;
  }

  return nsEventStatus_eIgnore;
}

nsresult
nsViewer::GoTo(const nsString& aURLSpec, 
               const char* aCommand,
               nsIWebShell* aWebWidget,
               nsIPostData* aPostData,
               nsISupports* aExtraInfo)
{
  nsresult rv = NS_ERROR_ILLEGAL_VALUE;
  if (nsnull != mWD && nsnull != mWD->observer) {
    rv = mWD->observer->mWebWidget->LoadURL(aURLSpec, mWD->observer, aPostData);
  }
  return rv;
}

static nsIRelatedLinks * gRelatedLinks = 0;
static void DumpRLValues(void* pdata, RL_Window win)
{
   nsViewer * pIViewer = (nsViewer *)pdata;
   if (pIViewer) {
      RL_Item nextItem = gRelatedLinks->WindowItems(win);
      do {
         char * pname = gRelatedLinks->ItemName(nextItem);
         char * purl = gRelatedLinks->ItemUrl(nextItem);
         if (pname) {
            if (gRLPos < MAX_RL)
               gRLList[gRLPos++] = PL_strdup(purl);
            pIViewer->AddRelatedLink(pname, purl);
         }
   
      } while ((nextItem = gRelatedLinks->NextItem(nextItem))!=0);
   }
}

extern "C" void NS_SetupRegistry();

nsDocLoader* nsViewer::SetupViewer(nsIWidget **aMainWindow, int argc, char **argv)
{
  NS_SetupRegistry();
  
  gWindows = new nsVoidArray();

  nsresult res;
  res=NSRepository::CreateInstance(kCAppShellCID, nsnull, kIAppShellIID, (void**)&gAppShell);
  if (NS_OK==res)
  {
    gAppShell->Create(&argc, argv);
    gAppShell->SetDispatchListener(this);
  }
  else
  {
    fprintf(stderr, "Couldn't create an instance of AppShell\n");
    return(nsnull);
  }

  res=NSRepository::CreateInstance(kPrefCID, NULL, kIPrefIID, (void **) &gPrefs);
  if (NS_OK==res)
  {
    gPrefs->Startup("prefs.js");
  }

  NS_InitINetService(this);

  for (int i=0; i<MAX_RL; i++)
     gRLList[i] = nsnull;
  mRelatedLinks = NS_NewRelatedLinks();
  gRelatedLinks = mRelatedLinks;
  if (mRelatedLinks) {
     mRLWindow = mRelatedLinks->MakeRLWindowWithCallback(DumpRLValues, this);
  }

#ifdef VIEWER_PLUGINS
  res=NSRepository::CreateInstance(kCPluginHostCID, nsnull, kIPluginManagerIID, (void**)&gPluginManager);
  if (NS_OK==res)
  {
    if (NS_OK == gPluginManager->QueryInterface(kIPluginHostIID, (void **)&gPluginHost))
    {
      gPluginHost->Init();
      gPluginHost->LoadPlugins();
    }
  }
  else
  {
    fprintf(stderr, "Couldn't create an instance of PluginManager\n");
    return(nsnull);
  }
#endif

  // Create a browser window and have it load the default url
  nsIBrowserWindow* bw = nsnull;
  res = NSRepository::CreateInstance(kBrowserWindowCID, nsnull,
                                     kIBrowserWindowIID,
                                     (void**) &bw);
  bw->Init(gAppShell, nsRect(0, 0, 620, 400), ~0);
  bw->Show();
  {
    char* start = startURL ? startURL : GetDefaultStartURL();
    bw->LoadURL(start);
  }
  
#if 0
    // Create a top level window for the WebWidget
  WindowData* wd = CreateTopLevel("Raptor HTML Viewer", 620, 400);
  *aMainWindow = wd->windowWidget;
  gMainWindowData = wd;

    // Attach a menu to the top level window
  AddMenu(wd->windowWidget, PR_FALSE);

  nsRect bounds;
  wd->windowWidget->GetBounds(bounds);

#ifdef VIEWER_UI
    // Add the back button
  nsIButton* back;
  nsRect rect(0, 0, BUTTON_WIDTH, BUTTON_HEIGHT);  
  NSRepository::CreateInstance(kCButtonCID, nsnull, kIButtonIID,
                               (void**)&back);
  back->Create(wd->windowWidget, rect, HandleBackEvent, NULL);
  back->SetLabel("Back");
  back->Show(PR_TRUE);
  NS_RELEASE(back);

    // Add the forward button
  nsIButton* forward;
  rect.SetRect(BUTTON_WIDTH, 0, BUTTON_WIDTH, BUTTON_HEIGHT);  
  NSRepository::CreateInstance(kCButtonCID, nsnull, kIButtonIID,
                               (void**)&forward);
  forward->Create(wd->windowWidget, rect, HandleForwardEvent, NULL);
  forward->SetLabel("Forward");
  forward->Show(PR_TRUE);
  NS_RELEASE(forward);

    // Add a location box
  rect.SetRect(2*BUTTON_WIDTH, 0,
               bounds.width - 2*BUTTON_WIDTH - THROBBER_WIDTH, BUTTON_HEIGHT);
  NSRepository::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID,
                               (void**)&mLocation);
  mLocation->Create(wd->windowWidget, rect, HandleLocationEvent, NULL);
  mLocation->SetText("");
  mLocation->Show(PR_TRUE);
  mLocation->SetForegroundColor(NS_RGB(255, 0, 0));
  mLocation->SetBackgroundColor(NS_RGB(255, 255, 255));

    // Add a throbber
  rect.SetRect(bounds.width - THROBBER_WIDTH, 0,
               THROBBER_WIDTH, THROBBER_HEIGHT);
  NSRepository::CreateInstance(kThrobberCID, nsnull, kIThrobberIID,
                               (void**)&mThrobber);
  mThrobber->Init(wd->windowWidget, rect);
  mThrobber->Show();

  wd->windowWidget->SetBackgroundColor(NS_RGB(255, 0, 0));
#endif
  wd->mViewer = this;

    // Now embed a web shell in it
  nsresult rv = NSRepository::CreateInstance(kWebShellCID, nsnull,
                                             kIWebShellIID,
                                             (void**)&wd->ww);
  nsRect rr(WEBWIDGET_LEFT_INSET, BUTTON_HEIGHT+WEBWIDGET_TOP_INSET,
            bounds.width - WEBWIDGET_LEFT_INSET - WEBWIDGET_RIGHT_INSET,
            bounds.height - BUTTON_HEIGHT - WEBWIDGET_TOP_INSET -
            WEBWIDGET_BOTTOM_INSET);
  rv = wd->ww->Init(wd->windowWidget->GetNativeData(NS_NATIVE_WIDGET), rr);
  wd->ww->Show();
  wd->observer = NewObserver(wd->windowWidget, wd->ww);
  wd->observer->mViewer = this;
  mWD = wd;


    // Determine if we should run the purify test
  nsDocLoader* dl = nsnull;
  if (gDoPurify) {
    dl = new nsDocLoader(wd->ww, this, gDelay);
    dl->AddRef();

      // Add the documents to the loader
    for (PRInt32 i=0; i<gRepeatCount; i++)
      AddTestDocs(dl);

      // Start the timer
    dl->StartTimedLoading();
  }
  else if (gLoadTestFromFile) {
    dl = new nsDocLoader(wd->ww, this, gDelay);
    dl->AddRef();
    for (PRInt32 i=0; i<gRepeatCount; i++)
      AddTestDocsFromFile(dl, gInputFileName);
    if (0 == gDelay) {
      dl->StartLoading();
    }
    else {
      dl->StartTimedLoading();
    }
  }
  else {
      // Load the starting url if we have one
    char* start = startURL ? startURL : GetDefaultStartURL();
    if (start) {
      gTheViewer->GoTo(start);
    }
    if (gDoQuantify) {
      // Synthesize 20 ResizeReflow commands (+/- 10 pixels) and then
      // exit.
#define kNumReflows 20
      for (PRIntn i = 0; i < kNumReflows; i++) {
        nsRect r;
        wd->observer->mWebWidget->GetBounds(r);
        if (i & 1) {
          r.width -= 10;
        }
        else {
          r.width += 10;
        }
        wd->observer->mWebWidget->SetBounds(r);
      }
      exit(0);
    }
  }
#endif

  return(nsnull);
}

void nsViewer::AfterDispatch()
{
   NET_PollSockets();
}

char* nsViewer::GetBaseURL()
{
  return(SAMPLES_BASE_URL);
}

char* nsViewer::GetDefaultStartURL()
{
static char defaultURL[MAXPATHLEN];

  sprintf(defaultURL, "%s%s", GetBaseURL(), DEFAULT_DOC);
  return(defaultURL);
}

void nsViewer::AddMenu(nsIWidget* aMainWindow, PRBool aForPrintPreview)
{
  printf("Menu not implemented\n");
}

void nsViewer::ShowConsole(WindowData* aWindata)
{
  printf("ShowConsole not implemented\n");
}

void nsViewer::AddRelatedLink(char * name, char * url)
{
  printf("AddRelatedLink not implemented\n");
}

void nsViewer::ResetRelatedLinks()
{
  printf("ResetRelatedLinks not implemented\n");
}

void nsViewer::CloseConsole() 
{
  printf("CloseConsole not implemented\n");
}

void nsViewer::DoDebugRobot(WindowData* aWinData)
{
  printf("DebugRobot not implemented\n");
}

void nsViewer::CopySelection(WindowData* aWindata)
{
  printf("CopySelection not implemented\n");
}

void nsViewer::DoSiteWalker(WindowData* aWinData)
{
  printf("Site Walker not implemented\n");
}

void nsViewer::DoDebugSave(WindowData* aWinData)
{
  printf("DebugSave not implemented\n");
}

void nsViewer::CrtSetDebug(PRUint32 aNewFlags)
{
  printf("CrtSetDebug not implemented\n");
}

void nsViewer::Destroy(WindowData* aWinData)
{
  if (nsnull != aWinData) {
    if (nsnull != aWinData->observer) {
#if XXX_fix_me
      aWinData->observer->mWebWidget->SetLinkHandler(nsnull);
      aWinData->observer->mWebWidget->SetContainer(nsnull); // release the doc observer
#endif
///   NS_RELEASE(aWinData->ww);
    }
    if (nsnull != aWinData->observer) {
      NS_RELEASE(aWinData->observer);
    }
    gWindows->RemoveElement(aWinData);
    delete aWinData;
  }
  if (nsnull != gPrefs) {
    gPrefs->Shutdown();
    gPrefs->Release();
  }
}

nsresult nsViewer::Run()
{
  return(gAppShell->Run());
}

void nsViewer::Stop()
{
  gAppShell->Exit();
}

void SetViewer(nsViewer* aViewer)
{
  gTheViewer = aViewer;
}

NS_IMETHODIMP    
nsViewer::GetAppCodeName(nsString& aAppCodeName)
{
  aAppCodeName.SetString("Mozilla");
  
  return NS_OK;
}
 
NS_IMETHODIMP
nsViewer::GetAppVersion(nsString& aAppVersion)
{
  aAppVersion.SetString("5.0 [en] (Windows;I)");
  
  return NS_OK;
}
 
NS_IMETHODIMP
nsViewer::GetAppName(nsString& aAppName)
{
  aAppName.SetString("Netscape");
  
  return NS_OK;
}
 
NS_IMETHODIMP
nsViewer::GetLanguage(nsString& aLanguage)
{
  aLanguage.SetString("en");
  
  return NS_OK;
}
 
NS_IMETHODIMP    
nsViewer::GetPlatform(nsString& aPlatform)
{
  aPlatform.SetString("Win32");
  
  return NS_OK;
}

void
nsViewer::DumpContent(nsIWebShell* aWebWidget)
{
  nsIPresShell* shell = GetPresShell(aWebWidget);
  if (nsnull != shell) {
    nsIDocument* doc = shell->GetDocument();
    if (nsnull != doc) {
      nsIContent* root = doc->GetRootContent();
      if (nsnull != root) {
        root->List(stdout);
        NS_RELEASE(root);
      }
      NS_RELEASE(doc);
    }
    NS_RELEASE(shell);
  }
  else {
    fputs("null pres shell\n", stdout);
  }
}

void
nsViewer::DumpFrames(nsIWebShell* aWebWidget)
{
  nsIPresShell* shell = GetPresShell(aWebWidget);
  if (nsnull != shell) {
    nsIFrame* root = shell->GetRootFrame();
    if (nsnull != root) {
      root->List(stdout);
    }
    NS_RELEASE(shell);
  }
  else {
    fputs("null pres shell\n", stdout);
  }
}

void
nsViewer::DumpViews(nsIWebShell* aWebWidget)
{
  nsIPresShell* shell = GetPresShell(aWebWidget);
  if (nsnull != shell) {
    nsIViewManager* vm = shell->GetViewManager();
    if (nsnull != vm) {
      nsIView* root = vm->GetRootView();
      if (nsnull != root) {
        root->List(stdout);
        NS_RELEASE(root);
      }
      NS_RELEASE(vm);
    }
    NS_RELEASE(shell);
  }
  else {
    fputs("null pres shell\n", stdout);
  }
}

static void DumpAWebShell(nsIWebShell* aShell, FILE* out, PRInt32 aIndent)
{
  nsAutoString name;
  nsIWebShell* parent;
  PRInt32 i, n;

  for (i = aIndent; --i >= 0; ) fprintf(out, "  ");

  fprintf(out, "%p '", aShell);
  aShell->GetName(name);
  aShell->GetParent(parent);
  fputs(name, out);
  fprintf(out, "' parent=%p <\n", parent);
  NS_IF_RELEASE(parent);

  aIndent++;
  aShell->GetChildCount(n);
  for (i = 0; i < n; i++) {
    nsIWebShell* child;
    aShell->ChildAt(i, child);
    if (nsnull != child) {
      DumpAWebShell(child, out, aIndent);
    }
  }
  aIndent--;
  for (i = aIndent; --i >= 0; ) fprintf(out, "  ");
  fputs(">\n", out);
}

void
nsViewer::DumpShells(nsIWebShell* aShell)
{
  DumpAWebShell(aShell, stdout, 0);
}

void
nsViewer::DumpStyleSheets(nsIWebShell* aWebWidget)
{
  nsIPresShell* shell = GetPresShell(aWebWidget);
  if (nsnull != shell) {
    nsIStyleSet* styleSet = shell->GetStyleSet();
    if (nsnull == styleSet) {
      fputs("null style set\n", stdout);
    } else {
      styleSet->List(stdout);
      NS_RELEASE(styleSet);
    }
    NS_RELEASE(shell);
  }
  else {
    fputs("null pres shell\n", stdout);
  }
}

void
nsViewer::DumpStyleContexts(nsIWebShell* aWebWidget)
{
  nsIPresShell* shell = GetPresShell(aWebWidget);
  if (nsnull != shell) {
    nsIPresContext* cx = shell->GetPresContext();
    nsIStyleSet* styleSet = shell->GetStyleSet();
    if (nsnull == styleSet) {
      fputs("null style set\n", stdout);
    } else {
      nsIFrame* root = shell->GetRootFrame();
      if (nsnull == root) {
        fputs("null root frame\n", stdout);
      } else {
        nsIStyleContext* rootContext;
        root->GetStyleContext(cx, rootContext);
        if (nsnull != rootContext) {
          styleSet->ListContexts(rootContext, stdout);
          NS_RELEASE(rootContext);
        }
        else {
          fputs("null root context", stdout);
        }
      }
      NS_RELEASE(styleSet);
    }
    NS_IF_RELEASE(cx);
    NS_RELEASE(shell);
  } else {
    fputs("null pres shell\n", stdout);
  }
}

void
nsViewer::ToggleFrameBorders(nsIWebShell* aWebWidget)
{
  PRBool showing = nsIFrame::GetShowFrameBorders();
  nsIFrame::ShowFrameBorders(!showing);
  ForceRefresh(aWebWidget);
}

void
nsViewer::ForceRefresh(nsIWebShell* aWebWidget)
{
  nsIPresShell* shell = GetPresShell(aWebWidget);
  if (nsnull != shell) {
    nsIViewManager* vm = shell->GetViewManager();
    if (nsnull != vm) {
      nsIView* root = vm->GetRootView();
      if (nsnull != root) {
        vm->UpdateView(root, (nsIRegion*)nsnull, NS_VMREFRESH_IMMEDIATE);
        NS_RELEASE(root);
      }
      NS_RELEASE(vm);
    }
    NS_RELEASE(shell);
  }
}

void
nsViewer::ShowContentSize(nsIWebShell* aWebWidget)
{
  nsISizeOfHandler* szh;
  if (NS_OK != NS_NewSizeOfHandler(&szh)) {
    return;
  }

  nsIPresShell* shell = GetPresShell(aWebWidget);
  if (nsnull != shell) {
    nsIDocument* doc = shell->GetDocument();
    if (nsnull != doc) {
      nsIContent* content = doc->GetRootContent();
      if (nsnull != content) {
        content->SizeOf(szh);
        PRUint32 totalSize;
        szh->GetSize(totalSize);
        printf("Content model size is approximately %d bytes\n", totalSize);
        NS_RELEASE(content);
      }
      NS_RELEASE(doc);
    }
    NS_RELEASE(shell);
  }
  NS_RELEASE(szh);
}

void
nsViewer::ShowFrameSize(nsIWebShell* aWebWidget)
{
  nsIPresShell* shell0 = GetPresShell(aWebWidget);
  if (nsnull != shell0) {
    nsIDocument* doc = shell0->GetDocument();
    if (nsnull != doc) {
      PRInt32 i, shells = doc->GetNumberOfShells();
      for (i = 0; i < shells; i++) {
        nsIPresShell* shell = doc->GetShellAt(i);
        if (nsnull != shell) {
          nsISizeOfHandler* szh;
          if (NS_OK != NS_NewSizeOfHandler(&szh)) {
            return;
          }
          nsIFrame* root;
          root = shell->GetRootFrame();
          if (nsnull != root) {
            root->SizeOf(szh);
            PRUint32 totalSize;
            szh->GetSize(totalSize);
            printf("Frame model for shell=%p size is approximately %d bytes\n",
                   shell, totalSize);
          }
          NS_RELEASE(szh);
          NS_RELEASE(shell);
        }
      }
      NS_RELEASE(doc);
    }
    NS_RELEASE(shell0);
  }
}

void
nsViewer::ShowStyleSize(nsIWebShell* aWebWidget)
{
}
