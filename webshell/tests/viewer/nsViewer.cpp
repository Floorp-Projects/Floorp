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
#include <malloc.h>

#include "nsViewer.h"
#include "resources.h"          // #defines ID's for menu items
#include "nsIWidget.h"
#include "nsGlobalVariables.h"
#include "nsIWebWidget.h"
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
#include "nsDocLoader.h"
#include "nsIFileWidget.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsISizeOfHandler.h"

static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
static NS_DEFINE_IID(kCWindowCID, NS_WINDOW_CID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kCAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
static NS_DEFINE_IID(kCWindowIID, NS_WINDOW_CID);
static NS_DEFINE_IID(kCScrollbarIID, NS_VERTSCROLLBAR_CID);
static NS_DEFINE_IID(kCHScrollbarIID, NS_HORZSCROLLBAR_CID);
static NS_DEFINE_IID(kCButtonIID, NS_BUTTON_CID);
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
static NS_DEFINE_IID(kCWebWidgetCID, NS_WEBWIDGET_CID);


#define SAMPLES_BASE_URL "resource:/res/samples"
#define START_URL SAMPLES_BASE_URL "/test0.html"

nsViewer* gTheViewer = nsnull;
WindowData * gMainWindowData = nsnull;
nsIAppShell *gAppShell= nsnull;
static char* startURL;
static nsVoidArray* gWindows;
static PRBool gDoPurify;          // run in Purify auto mode
static PRBool gDoQuantify;        // run in Quantify auto mode
static PRBool gLoadTestFromFile;  // run in auto mode by pulling URLs from a file (gInputFileName)
static PRInt32 gDelay=1;          // if running in an auto mode, this is the delay between URL loads
static PRInt32 gRepeatCount=1;    // if running in an auto mode, this is the number of times to cycle through the input
static PRInt32 gNumSamples=9;     // if running in an auto mode that uses the samples, this is the last sample to load
static char gInputFileName[MAXPATHLEN];


// Temporary Netlib stuff...
/* XXX: Don't include net.h... */
extern "C" {
extern int  NET_PollSockets();
};

//----------------------------------------------------------------------

void
WindowData::ShowContentSize()
{
  if (nsnull == ww) {
    return;
  }
  nsISizeOfHandler* szh;
  if (NS_OK != NS_NewSizeOfHandler(&szh)) {
    return;
  }

  nsIDocument* doc;
  doc = ww->GetDocument();
  if (nsnull != doc) {
    nsIContent* content;
    content = doc->GetRootContent();
    if (nsnull != content) {
      content->SizeOf(szh);
      PRUint32 totalSize;
      szh->GetSize(totalSize);
      printf("Content model size is approximately %d bytes\n", totalSize);
      NS_RELEASE(content);
    }
    NS_RELEASE(doc);
  }
  NS_RELEASE(szh);
}

void
WindowData::ShowFrameSize()
{
  if (nsnull == ww) {
    return;
  }

  nsIDocument* doc;

  doc = ww->GetDocument();
  if (nsnull != doc ){
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
}

void
WindowData::ShowStyleSize()
{
  if (nsnull == ww) {
    return;
  }
}

//----------------------------------------------------------------------

struct OnLinkClickEvent : public PLEvent {
  OnLinkClickEvent(DocObserver* aHandler, const nsString& aURLSpec,
                   const nsString& aTargetSpec, nsIPostData* aPostData = 0);
  ~OnLinkClickEvent();

  void HandleEvent();

  DocObserver* mHandler;
  nsString*    mURLSpec;
  nsString*    mTargetSpec;
  nsIPostData *mPostData;
};

static void PR_CALLBACK HandleEvent(OnLinkClickEvent* aEvent)
{
  aEvent->HandleEvent();
}

static void PR_CALLBACK DestroyEvent(OnLinkClickEvent* aEvent)
{
  delete aEvent;
}

OnLinkClickEvent::OnLinkClickEvent(DocObserver* aHandler,
                                   const nsString& aURLSpec,
                                   const nsString& aTargetSpec,
                                   nsIPostData* aPostData)
{
  mHandler = aHandler;
  NS_ADDREF(aHandler);
  mURLSpec = new nsString(aURLSpec);
  mTargetSpec = new nsString(aTargetSpec);
  mPostData = nsnull;
  if (aPostData) {
    NS_NewPostData(aPostData, &mPostData);
  }

#ifdef XP_PC
  PL_InitEvent(this, nsnull,
               (PLHandleEventProc) ::HandleEvent,
               (PLDestroyEventProc) ::DestroyEvent);


  PLEventQueue* eventQueue = PL_GetMainEventQueue();
  PL_PostEvent(eventQueue, this);
#endif
}

OnLinkClickEvent::~OnLinkClickEvent()
{
  NS_IF_RELEASE(mHandler);
  if (nsnull != mURLSpec) delete mURLSpec;
  if (nsnull != mTargetSpec) delete mTargetSpec;
  if (nsnull != mPostData) delete mPostData;
}

void OnLinkClickEvent::HandleEvent()
{
  mHandler->HandleLinkClickEvent(*mURLSpec, *mTargetSpec, mPostData);
}

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENT_OBSERVER_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMNOTIFICATION_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

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
  if (aIID.Equals(kIStreamListenerIID)) {
    *aInstancePtrResult = (void*) ((nsIStreamListener*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)((nsIDocumentObserver*)this));
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

// Pass title information through to all of the web widgets that
// belong to this document.
NS_IMETHODIMP DocObserver::SetTitle(const nsString& aTitle)
{
  PRInt32 i, n = gWindows->Count();
  for (i = 0; i < n; i++) {
    WindowData* wd = (WindowData*) gWindows->ElementAt(i);
    if (wd->ww == mWebWidget) {
      wd->windowWidget->SetTitle(aTitle);
    }
  }
  return NS_OK;
}

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
DocObserver::GetBindInfo(void)
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::OnProgress(PRInt32 aProgress, PRInt32 aProgressMax,
                        const nsString& aMsg)
{
  fputs("[progress ", stdout);
  fputs(mURL, stdout);
  printf(" %d %d ", aProgress, aProgressMax);
  fputs(aMsg, stdout);
  fputs("]\n", stdout);
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::OnStartBinding(const char *aContentType)
{
  fputs("Loading ", stdout);
  fputs(mURL, stdout);
  fputs("\n", stdout);
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::OnDataAvailable(nsIInputStream *pIStream, PRInt32 length)
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::OnStopBinding(PRInt32 status, const nsString& aMsg)
{
  fputs("Done loading ", stdout);
  fputs(mURL, stdout);
  fputs("\n", stdout);
  return NS_OK;
}

void
DocObserver::LoadURL(const char* aURL)
{
  mURL = aURL;
  mWebWidget->LoadURL(aURL, (nsIStreamListener*) this);
}

NS_IMETHODIMP
DocObserver::Init(nsIWebWidget* aWidget)
{
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::GetWebWidget(nsIWebWidget** aResult)
{
  NS_IF_ADDREF(mWebWidget);
  *aResult = mWebWidget;
  return NS_OK;
}

NS_IMETHODIMP
DocObserver::OnLinkClick(nsIFrame* aFrame, 
                         const nsString& aURLSpec,
                         const nsString& aTargetSpec,
                         nsIPostData* aPostData)
{
  new OnLinkClickEvent(this, aURLSpec, aTargetSpec, aPostData);
  return NS_OK;
}

void
DocObserver::HandleLinkClickEvent(const nsString& aURLSpec,
                                  const nsString& aTargetSpec,
                                  nsIPostData* aPostData)
{
  if (nsnull != mWebWidget) {
    mWebWidget->LoadURL(aURLSpec, (nsIStreamListener*)this, aPostData);
  }
}

NS_IMETHODIMP
DocObserver::OnOverLink(nsIFrame* aFrame, 
                        const nsString& aURLSpec,
                        const nsString& aTargetSpec)
{
  if (!aURLSpec.Equals(mOverURL) || !aTargetSpec.Equals(mOverTarget)) {
fputs("Was '", stdout); fputs(mOverURL, stdout); fputs("' '", stdout); fputs(mOverTarget, stdout); fputs("'\n", stdout); 
    fputs("Over link '", stdout);
    fputs(aURLSpec, stdout);
    fputs("' '", stdout);
    fputs(aTargetSpec, stdout);
    fputs("'\n", stdout);
    mOverURL = aURLSpec;
    mOverTarget = aTargetSpec;
  }
  return NS_OK;
}

NS_IMETHODIMP
DocObserver:: GetLinkState(const nsString& aURLSpec, nsLinkState& aState)
{
  aState = eLinkState_Unvisited;
#ifdef NS_DEBUG
  if (aURLSpec.Equals("http://visited/")) {
    aState = eLinkState_Visited;
  }
  else if (aURLSpec.Equals("http://out-of-date/")) {
    aState = eLinkState_OutOfDate;
  }
  else if (aURLSpec.Equals("http://active/")) {
    aState = eLinkState_Active;
  }
  else if (aURLSpec.Equals("http://hover/")) {
    aState = eLinkState_Hover;
  }
#endif
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
      ww->SetLinkHandler((nsILinkHandler*) it);
      ww->SetContainer((nsIDocumentObserver*) it);
      return it;
    }
    else {
      NS_RELEASE(oldContainer);
    }
  }
  return nsnull;
}

//----------------------------------------------------------------------

nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent)
{ 
    nsEventStatus result = nsEventStatus_eIgnore;
    switch(aEvent->message) {
      case NS_SIZE:
        {
          struct WindowData *wd = gTheViewer->FindWindowData(aEvent->widget);
          nsSizeEvent* sizeEvent = (nsSizeEvent*)aEvent;  
          if (wd->ww) {
            nsRect rr(0, 0, sizeEvent->windowSize->width, sizeEvent->windowSize->height);
            wd->ww->SetBounds(rr);
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
          return nsEventStatus_eConsumeDoDefault;
        }
 

      default:
          return(gTheViewer->DispatchMenuItem(aEvent));
        break;
    }

    return(nsEventStatus_eIgnore);
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

/*
 * Purify methods
 */

void nsViewer::AddTestDocs(nsDocLoader* aDocLoader)
{
  char url[500];
  for (int docnum = 0; docnum < gNumSamples; docnum++)
  {
    PR_snprintf(url, 500, "%s/test%d.html", SAMPLES_BASE_URL, docnum);
    aDocLoader->AddURL(url);
  }
}

/*
 * SelfTest methods
 */
void nsViewer::AddTestDocsFromFile(nsDocLoader* aDocLoader, char *aFileName)
{
  /* Steve's table test code.  
     Assumes you have a file in the current working directory called aFileName
     that contains a list of URLs, one per line, of files to load.
   */

  PRFileDesc* input = PR_Open(aFileName, PR_RDONLY, 0);
  if (nsnull==input)
  {
    printf("FAILED TO OPEN %s!", aFileName);
    return;
  }
  // read one line of input and pass it in as a URL
  char *inputString = new char[10000];
  if (nsnull==inputString)
  {
    printf("couldn't allocate buffer, insufficient memory\n");
    exit (-1);
  }
  nsCRT::memset(inputString, 0, 10000);
  PR_Read(input, inputString, 10000);
  PR_Close(input);

  char *nextInput = inputString;
  while (nsnull!=nextInput && nsnull!=*nextInput)
  {
    char * endOfLine = PL_strchr(nextInput, '\n');
    if (nsnull!=nextInput)
    {
      if (nsnull!=endOfLine)
      {
        char save = *endOfLine;
        *endOfLine = nsnull;
      }
      if ('#' != *nextInput)  // use '#' as a comment character
      {
        aDocLoader->AddURL(nextInput);
      }
      if (nsnull!=endOfLine)
      {
        nextInput = endOfLine+1;
      }
      else
        nextInput = nsnull;
    }
  }
  if (nsnull!=inputString)
    delete [] inputString;
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
    wd->observer->LoadURL(lpszFileURL);
    free(lpszFileURL);
  }
}

// Selects all the Content
void nsViewer::SelectAll(WindowData* wd)
{
  if (wd->ww != nsnull) {
    nsIDocument* doc = wd->ww->GetDocument();
    if (doc != nsnull) {
      doc->SelectAll();
      wd->ww->ShowFrameBorders(PR_FALSE);
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

  window->Create((nsIWidget*)NULL, rect, HandleEvent, nsnull, nsnull, (nsWidgetInitData *)gAppShell->GetNativeData(NS_NATIVE_SHELL));
  window->SetTitle(title);
  wd->windowWidget = window; 
  gWindows->AppendElement(wd);
  window->Show(PR_TRUE);
 
  return wd;
}


nsresult nsViewer::ShowPrintPreview(nsIWebWidget* web, PRIntn aColumns)
{
  if (nsnull != web) {
    nsIDocument* doc = web->GetDocument();
    if (nsnull != doc) {
      nsIPresContext* cx;
      nsresult rv = NS_NewPrintPreviewContext(&cx);
      if (NS_OK != rv) {
        return rv;
      }

      WindowData* wd = CreateTopLevel("Print Preview", 500, 300);
  
      nsRect bounds;
      wd->windowWidget->GetBounds(bounds);
      nsRect rr(0, 0, bounds.width, bounds.height);

      rv = NS_NewWebWidget(&wd->ww);
      rv = wd->ww->Init(wd->windowWidget->GetNativeData(NS_NATIVE_WIDGET), rr, doc, cx);
      wd->ww->Show();
      wd->observer = NewObserver(wd->ww);

      NS_RELEASE(cx);
      NS_RELEASE(doc);
    }
  }
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
      if ((nsnull != wd) && (nsnull != wd->ww)) {
        PRIntn ix = aId - VIEWER_DEMO0;
        char* url = new char[500];
        PR_snprintf(url, 500, "%s/test%d.html", SAMPLES_BASE_URL, ix);
        wd->observer->LoadURL(url);
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
    case VIEWER_DUMP_STYLE_SHEETS:
      if ((nsnull != wd) && (nsnull != wd->ww)) {
        wd->ww->DumpStyleSheets();
      }
      break;
    case VIEWER_DUMP_STYLE_CONTEXTS:
      if ((nsnull != wd) && (nsnull != wd->ww)) {
        wd->ww->DumpStyleContexts();
      }
      break;

    case VIEWER_DEBUGROBOT:
      DoDebugRobot(wd);
      break;

    case VIEWER_SHOW_CONTENT_SIZE:
      if (nsnull != wd) {
        wd->ShowContentSize();
      }
      break;

    case VIEWER_SHOW_FRAME_SIZE:
      if (nsnull != wd) {
        wd->ShowFrameSize();
      }
      break;

    case VIEWER_SHOW_STYLE_SIZE:
      if (nsnull != wd) {
        wd->ShowStyleSize();
      }
      break;

    case VIEWER_ONE_COLUMN:
    case VIEWER_TWO_COLUMN:
    case VIEWER_THREE_COLUMN:
      if ((nsnull != wd) && (nsnull != wd->ww)) {
        ShowPrintPreview(wd->ww, aId - VIEWER_ONE_COLUMN + 1);
      }
      break;

    case JS_CONSOLE:
        ShowConsole(wd);
      break;
  }

  return(result);
}

void nsViewer::CleanupViewer(nsDocLoader* aDl) 
{
  if (aDl != nsnull)
    delete aDl;
  ReleaseMemory();
}


nsDocLoader* nsViewer::SetupViewer(nsIWidget **aMainWindow, int argc, char **argv)
{
#ifdef XP_PC
  PL_InitializeEventsLib("");
#endif

  gWindows = new nsVoidArray();

  NSRepository::RegisterFactory(kCWindowIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCScrollbarIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCHScrollbarIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCButtonIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCComboBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCFileWidgetCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCListBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCRadioButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCTextAreaCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCTextFieldCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCCheckButtonIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCChildIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCAppShellCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCRenderingContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCDeviceContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCFontMetricsIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCImageIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCRegionIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCViewManagerCID, VIEW_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCViewCID, VIEW_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCScrollingViewCID, VIEW_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCWebWidgetCID, WEB_DLL, PR_FALSE, PR_FALSE);

  // Create an application shell
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
  
    // Create a top level window for the WebWidget
  WindowData* wd = CreateTopLevel("Raptor HTML Viewer", 620, 400);
  *aMainWindow = wd->windowWidget;
  gMainWindowData = wd;

    // Attach a menu to the top level window
  AddMenu(wd->windowWidget);

    // Now embed the web widget in it
  nsresult rv = NS_NewWebWidget(&wd->ww);
  nsRect bounds;
  wd->windowWidget->GetBounds(bounds);
  nsRect rr(0, 0, bounds.width, bounds.height);
  rv = wd->ww->Init(wd->windowWidget->GetNativeData(NS_NATIVE_WIDGET), rr);
  wd->ww->Show();
  wd->observer = NewObserver(wd->ww);


    // Determine if we should run the purify test
  nsDocLoader* dl = nsnull;
  if (gDoPurify) {
    dl = new nsDocLoader(wd->ww, this, gDelay);

      // Add the documents to the loader
    for (PRInt32 i=0; i<gRepeatCount; i++)
      AddTestDocs(dl);

      // Start the timer
    dl->StartTimedLoading();
  }
  else if (gLoadTestFromFile) {
    dl = new nsDocLoader(wd->ww, this, gDelay);
    for (PRInt32 i=0; i<gRepeatCount; i++)
      AddTestDocsFromFile(dl, gInputFileName);
    dl->StartTimedLoading();
  }
  else {
      // Load the starting url if we have one
    wd->observer->LoadURL(startURL ? startURL : START_URL);
    if (gDoQuantify) {
      // Synthesize 20 ResizeReflow commands (+/- 10 pixels) and then
      // exit.
#define kNumReflows 20
      for (PRIntn i = 0; i < kNumReflows; i++) {
        nsRect r = wd->ww->GetBounds();
        if (i & 1) {
          r.width -= 10;
        }
        else {
          r.width += 10;
        }
        wd->ww->SetBounds(r);
      }
      exit(0);
    }
  }

  return(dl);
}

void nsViewer::AfterDispatch()
{
   NET_PollSockets();
}

void nsViewer::AddMenu(nsIWidget* aMainWindow)
{
  printf("Menu not implemented\n");
}

void nsViewer::ShowConsole(WindowData* aWindata)
{
  printf("ShowConsole not implemented\n");
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

void nsViewer::CrtSetDebug(PRUint32 aNewFlags)
{
  printf("CrtSetDebug not implemented\n");
}

void nsViewer::Destroy(WindowData* aWinData)
{
  if (nsnull != aWinData) {
    if (nsnull != aWinData->ww) {
      aWinData->ww->SetLinkHandler(nsnull);
      aWinData->ww->SetContainer(nsnull); // release the doc observer
      NS_RELEASE(aWinData->ww);
    }
    if (nsnull != aWinData->observer) {
      NS_RELEASE(aWinData->observer);
    }
    gWindows->RemoveElement(aWinData);
    delete aWinData;
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

