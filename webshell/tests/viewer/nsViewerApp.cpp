/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifdef NGPREFS
#define INITGUID
#endif
#include "nsCOMPtr.h"
#include "nsXPBaseWindow.h" 
#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsWidgetsCID.h"
#include "nsIAppShell.h"
#include "nsIPref.h"
#include "nsINetService.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"
#include "nsWebCrawler.h"
#include "prprf.h"
#include "plstr.h"
#include "prenv.h"

// Needed for Dialog GUI
#include "nsIDialog.h"
#include "nsICheckButton.h"
#include "nsILabel.h"
#include "nsIButton.h"
#include "nsITextWidget.h"
#include "nsILookAndFeel.h"
#include "nsColor.h"
#include "nsWidgetSupport.h"


// XXX For font setting below
#include "nsFont.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"

#define DIALOG_FONT      "Helvetica"
#define DIALOG_FONT_SIZE 10


#ifdef XP_PC
#include "JSConsole.h"
#ifdef NGPREFS
#include "ngprefs.h"
#endif
#endif
#ifdef XP_WIN
#include <crtdbg.h>
#endif

#ifdef MOZ_FULLCIRCLE
#include <fullsoft.h>
#endif

/*********************************************
 SilentDownload
*********************************************/
#include "nsSilentDownload.h"
#include "nsIDOMSilentDownload.h"

static nsIDOMSilentDownload *silentDownload = NULL;
static NS_DEFINE_IID(kISilentDownloadIID, NS_IDOMSILENTDOWNLOAD_IID);
static NS_DEFINE_IID(kSilentDownloadCID,  NS_SilentDownload_CID);
/*********************************************/

extern nsresult NS_NewBrowserWindowFactory(nsIFactory** aFactory);
extern nsresult NS_NewXPBaseWindowFactory(nsIFactory** aFactory);
extern "C" void NS_SetupRegistry();

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kBrowserWindowCID, NS_BROWSER_WINDOW_CID);
static NS_DEFINE_IID(kXPBaseWindowCID, NS_XPBASE_WINDOW_CID);

static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_IID(kIBrowserWindowIID, NS_IBROWSER_WINDOW_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIXPBaseWindowIID, NS_IXPBASE_WINDOW_IID);


#define DEFAULT_WIDTH 620
#define DEFAULT_HEIGHT 400


nsViewerApp::nsViewerApp()
{
  NS_INIT_REFCNT(); 

  char * text = PR_GetEnv("NGLAYOUT_HOME");
  mStartURL = text ? text : "resource:/res/samples/test0.html";

  //rickg 20Nov98: For the sake of a good demo, pre-load a decent URL...
//  mStartURL = text ? text : "http://developer.netscape.com/software/communicator/ngl/index.html";

  mDelay = 1;
  mRepeatCount = 1;
  mNumSamples = 14;
  mAllowPlugins = PR_TRUE;
  mIsInitialized = PR_FALSE;
  mWidth = DEFAULT_WIDTH;
  mHeight = DEFAULT_HEIGHT;
}

nsViewerApp::~nsViewerApp()
{
  Destroy();
  if (nsnull != mPrefs) {
    mPrefs->Shutdown();
    NS_RELEASE(mPrefs);
  }
}

NS_IMPL_THREADSAFE_ADDREF(nsViewerApp)
NS_IMPL_THREADSAFE_RELEASE(nsViewerApp)

nsresult
nsViewerApp::QueryInterface(REFNSIID aIID, void** aInstancePtrResult) 
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsISupports* tmp = this;
    *aInstancePtrResult = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
#if defined(NS_DEBUG) 
  /*
   * Check for the debug-only interface indicating thread-safety
   */
  static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);
  if (aIID.Equals(kIsThreadsafeIID)) {
    return NS_OK;
  }
#endif /* NS_DEBUG */

  return NS_NOINTERFACE;
}

void
nsViewerApp::Destroy()
{
  // Close all of our windows
  nsBrowserWindow::CloseAllWindows();

  // Release the crawler
  NS_IF_RELEASE(mCrawler);

  // Only shutdown if Initialize has been called...
  if (PR_TRUE == mIsInitialized) {
    NS_ShutdownINetService();
    mIsInitialized = PR_FALSE;
  }
}

nsresult
nsViewerApp::SetupRegistry()
{
  NS_SetupRegistry();

  // Register our browser window factory
  nsIFactory* bwf;
  NS_NewBrowserWindowFactory(&bwf);
  nsComponentManager::RegisterFactory(kBrowserWindowCID, 0, 0, bwf, PR_FALSE);

  NS_NewXPBaseWindowFactory(&bwf);
  nsComponentManager::RegisterFactory(kXPBaseWindowCID, 0, 0, bwf, PR_FALSE);

  return NS_OK;
}

nsresult
nsViewerApp::Initialize(int argc, char** argv)
{
  nsresult rv;

  rv = SetupRegistry();
  if (NS_OK != rv) {
    return rv;
  }

  // Create the Event Queue for the UI thread...
  rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                    kIEventQueueServiceIID,
                                    (nsISupports **)&mEventQService);
  if (NS_OK != rv) {
     NS_ASSERTION(PR_FALSE, "Could not obtain the event queue service");
     return rv;
  }

  printf("Going to create the event queue\n");
  rv = mEventQService->CreateThreadEventQueue();
  if (NS_OK != rv) {
     NS_ASSERTION(PR_FALSE, "Could not create the event queue for the the thread");
     return rv;
  }

  // Create widget application shell
  rv = nsComponentManager::CreateInstance(kAppShellCID, nsnull, kIAppShellIID,
                                    (void**)&mAppShell);
  if (NS_OK != rv) {
    return rv;
  }
  mAppShell->Create(&argc, argv);
  mAppShell->SetDispatchListener((nsDispatchListener*) this);

  // Load preferences
  rv = nsComponentManager::CreateInstance(kPrefCID, NULL, kIPrefIID,
                                    (void **) &mPrefs);
  if (NS_OK != rv) {
    return rv;
  }
  mPrefs->Startup(nsnull);


  // Load Fullcircle Talkback crash-reporting mechanism.
  // http://www.fullcirclesoftware.com for more details.
  // Private build only.
#ifdef MOZ_FULLCIRCLE
  // This probably needs to be surrounded by a pref, the
  // old 5.0 world used "general.fullcircle_enable".

  {
	  FC_ERROR fcstatus = FC_ERROR_FAILED;
	  fcstatus = FCInitialize();
	  
	  // Print out error status.
	  switch(fcstatus) {
	  case FC_ERROR_OK:
		  printf("Talkback loaded Ok.\n");
		  break;
	  case FC_ERROR_CANT_INITIALIZE:
		  printf("Talkback error: Can't initialize.\n");
		  break;
	  case FC_ERROR_NOT_INITIALIZED:
		  printf("Talkback error: Not initialized.\n");
		  break;
	  case FC_ERROR_ALREADY_INITIALIZED:
		  printf("Talkback error: Already initialized.\n");
		  break;
	  case FC_ERROR_FAILED:
		  printf("Talkback error: Failure.\n");
		  break;
	  case FC_ERROR_OUT_OF_MEMORY:
		  printf("Talkback error: Out of memory.\n");
		  break;
	  case FC_ERROR_INVALID_PARAMETER:
		  printf("Talkback error: Invalid parameter.\n");
		  break;
	  default:
		  printf("Talkback error: Unknown error status.\n");
		  break;
	  }
  }
#endif

  // Setup networking library
  rv = NS_InitINetService();
  if (NS_OK != rv) {
    return rv;
  }

#if 0
  // XXX where should this live
  for (int i=0; i<MAX_RL; i++)
    gRLList[i] = nsnull;
  mRelatedLinks = NS_NewRelatedLinks();
  gRelatedLinks = mRelatedLinks;
  if (mRelatedLinks) {
    mRLWindow = mRelatedLinks->MakeRLWindowWithCallback(DumpRLValues, this);
  }
#endif

    rv = nsComponentManager::CreateInstance(kSilentDownloadCID, 
                                      nsnull,
                                      kISilentDownloadIID,
                                      (void**) &silentDownload);
    if (rv == NS_OK)
    {
        if (silentDownload->Startup() != NS_OK)
        {
            silentDownload->Shutdown();
            NS_RELEASE(silentDownload);
        }
    }

  // Finally process our arguments
  rv = ProcessArguments(argc, argv);

  mIsInitialized = PR_TRUE;
  return rv;
}

nsresult
nsViewerApp::Exit()
{
  if (silentDownload != NULL)
  {
      silentDownload->Shutdown();
      NS_RELEASE(silentDownload);
  }

  Destroy();
  mAppShell->Exit();
  NS_RELEASE(mAppShell);

  if (nsnull != mEventQService) {
    nsServiceManager::ReleaseService(kEventQueueServiceCID, mEventQService);
    mEventQService = nsnull;
  }
  return NS_OK;
}

static void
PrintHelpInfo(char **argv)
{
  fprintf(stderr, "Usage: %s [options] [starting url]\n", argv[0]);
  fprintf(stderr, "-p[#]   -- autload tests 0-#\n");
  fprintf(stderr, "-q -- jiggles window width after page has autoloaded\n");
  fprintf(stderr, "-f filename -- read a list of URLs to autoload from <filename>\n");
  fprintf(stderr, "-d # -- set the delay between autoloads to # (in milliseconds)\n");
  fprintf(stderr, "-np -- no plugins\n");
  fprintf(stderr, "-v -- verbose (debug noise)\n");
  fprintf(stderr, "-r # -- how many times a page is loaded when autoloading\n");
  fprintf(stderr, "-o dirname -- create an output file for the frame dump of each page\n  and put it in <dirname>. <dirname> must include the trailing\n  <slash> character appropriate for your OS\n");
  fprintf(stderr, "-rd dirname -- specify a regression directory whose contents are from\n  a previous -o run to compare against with this run\n");
  fprintf(stderr, "-h # -- the initial height of the viewer window\n");
  fprintf(stderr, "-w # -- the initial width of the viewer window\n");
  fprintf(stderr, "-C -- enable crawler\n");
  fprintf(stderr, "-R filename -- record pages crawled to in <filename>\n");
  fprintf(stderr, "-S domain -- add a domain/host that is safe to crawl (e.g. www.netscape.com)\n");
  fprintf(stderr, "-A domain -- add a domain/host that should be avoided (e.g. microsoft.com)\n");
  fprintf(stderr, "-N pages -- set the max # of pages to crawl\n");
#if defined(NS_DEBUG) && defined(XP_WIN)
  fprintf(stderr, "-md # -- set the crt debug flags to #\n");
#endif
}

static void
AddTestDocsFromFile(nsWebCrawler* aCrawler, const nsString& aFileName)
{
  char cfn[1000];
  aFileName.ToCString(cfn, sizeof(cfn));
#ifdef XP_PC
  FILE* fp = fopen(cfn, "rb");
#else
  FILE* fp = fopen(cfn, "r");
#endif

  if (nsnull==fp)
  {
    fprintf(stderr, "Input file not found: %s\n", cfn);
    exit (-1);
  }
  nsAutoString line;
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
      line = linebuf;
      aCrawler->AddURL(line);
    }
  }

  fclose(fp);
}

NS_IMETHODIMP
nsViewerApp::ProcessArguments(int argc, char** argv)
{
  mCrawler = new nsWebCrawler(this);
  NS_ADDREF(mCrawler);

  int i;
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
#if defined(NS_DEBUG) && defined(XP_WIN)
      if (PL_strcmp(argv[i], "-md") == 0) {
        int old = _CrtSetDbgFlag(0);
        old |= _CRTDBG_CHECK_ALWAYS_DF;
        _CrtSetDbgFlag(old);
      }
      else
#endif
      if (PL_strncmp(argv[i], "-p", 2) == 0) {
        char *optionalSampleStopIndex = &(argv[i][2]);
        if ('\0' != *optionalSampleStopIndex)
        {
          if (1!=sscanf(optionalSampleStopIndex, "%d", &mNumSamples))
          {
            PrintHelpInfo(argv);
            exit(-1);
          }
        }
        mDoPurify = PR_TRUE;
        mCrawler->SetExitOnDone(PR_TRUE);
        mCrawl = PR_TRUE;
      }
      else if (PL_strcmp(argv[i], "-q") == 0) {
        mCrawler->EnableJiggleLayout();
        mCrawler->SetExitOnDone(PR_TRUE);
        mCrawl = PR_TRUE;
      }
      else if (PL_strcmp(argv[i], "-f") == 0) {
        mLoadTestFromFile = PR_TRUE;
        i++;
        if (i>=argc || nsnull==argv[i] || nsnull==*(argv[i]))
        {
          PrintHelpInfo(argv);
          exit(-1);
        }
        mInputFileName = argv[i];
        mCrawler->SetExitOnDone(PR_TRUE);
        mCrawl = PR_TRUE;
      }
      else if (PL_strcmp(argv[i], "-rd") == 0) {
        i++;
        if (i>=argc || nsnull==argv[i] || nsnull==*(argv[i])) {
          PrintHelpInfo(argv);
          exit(-1);
        }
        mCrawler->SetEnableRegression(PR_TRUE);
        mCrawler->SetRegressionDir(argv[i]);
      }
      else if (PL_strcmp(argv[i], "-o") == 0) {
        i++;
        if (i>=argc || nsnull==argv[i] || nsnull==*(argv[i]))
        {
          PrintHelpInfo(argv);
          exit(-1);
        }
        mCrawler->SetOutputDir(argv[i]);
      }
      else if (PL_strcmp(argv[i], "-d") == 0) {
        int delay;
        i++;
        if (i>=argc || 1!=sscanf(argv[i], "%d", &delay))
        {
          PrintHelpInfo(argv);
          exit(-1);
        }
        mCrawler->SetDelay(delay);
      }
      else if (PL_strcmp(argv[i], "-w") == 0) {
        int width;
        i++;
        if (i>=argc || 1!=sscanf(argv[i], "%d", &width))
        {
          PrintHelpInfo(argv);
          exit(-1);
        }
        mWidth = width > 0 ? width : DEFAULT_WIDTH;
      }
      else if (PL_strcmp(argv[i], "-h") == 0) {
        int height;
        i++;
        if (i>=argc || 1!=sscanf(argv[i], "%d", &height))
        {
          PrintHelpInfo(argv);
          exit(-1);
        }
        mHeight = height > 0 ? height : DEFAULT_HEIGHT;
      }
      else if (PL_strcmp(argv[i], "-r") == 0) {
        i++;
        if (i>=argc || 1!=sscanf(argv[i], "%d", &mRepeatCount))
        {
          PrintHelpInfo(argv);
          exit(-1);
        }
      }
      else if (PL_strcmp(argv[i], "-C") == 0) {
        mCrawler->EnableCrawler();
        mCrawler->SetExitOnDone(PR_TRUE);
        mCrawl = PR_TRUE;
      }
      else if (PL_strcmp(argv[i], "-R") == 0) {
        i++;
        if (i>=argc) {
          PrintHelpInfo(argv);
          exit(-1);
        }
        FILE* fp = fopen(argv[i], "w");
        if (nsnull == fp) {
          fprintf(stderr, "can't create '%s'\n", argv[i]);
          exit(-1);
        }
        mCrawler->SetRecordFile(fp);
      }
      else if (PL_strcmp(argv[i], "-S") == 0) {
        i++;
        if (i>=argc) {
          PrintHelpInfo(argv);
          exit(-1);
        }
        mCrawler->AddSafeDomain(argv[i]);
      }
      else if (PL_strcmp(argv[i], "-A") == 0) {
        i++;
        if (i>=argc) {
          PrintHelpInfo(argv);
          exit(-1);
        }
        mCrawler->AddAvoidDomain(argv[i]);
      }
      else if (PL_strcmp(argv[i], "-N") == 0) {
        int pages;
        i++;
        if (i>=argc || 1!=sscanf(argv[i], "%d", &pages)) {
          PrintHelpInfo(argv);
          exit(-1);
        }
        mCrawler->SetMaxPages(pages);
      }
      else if (PL_strcmp(argv[i], "-np") == 0) {
        mAllowPlugins = PR_FALSE;
      }
      else if (PL_strcmp(argv[i], "-v") == 0) {
        mCrawler->SetVerbose(PR_TRUE);
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
    mStartURL = argv[i];
#ifdef XP_UNIX
    if (argv[i][0] == '/') {
      mStartURL.Insert("file:", 0);
    }
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
nsViewerApp::OpenWindow()
{
  // Create browser window
  // XXX Some piece of code needs to properly hold the reference to this
  // browser window. For the time being the reference is released by the
  // browser event handling code during processing of the NS_DESTROY event...
  nsBrowserWindow* bw = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kBrowserWindowCID, nsnull,
                                             kIBrowserWindowIID,
                                             (void**) &bw);
  if (NS_FAILED(rv)) {
    return rv;
  }
  bw->SetApp(this);
  bw->Init(mAppShell, mPrefs, nsRect(0, 0, mWidth, mHeight),
           PRUint32(~0), mAllowPlugins);
  bw->Show();
  nsIBrowserWindow*	bwCurrent;
  mCrawler->GetBrowserWindow(&bwCurrent);
  if (!bwCurrent) {
	  mCrawler->SetBrowserWindow(bw);
  }
  NS_IF_RELEASE(bwCurrent);

  if (mDoPurify) {
    for (PRInt32 i = 0; i < mRepeatCount; i++) {
      for (int docnum = 0; docnum < mNumSamples; docnum++) {
        char url[500];
        PR_snprintf(url, 500, "%s/test%d.html", SAMPLES_BASE_URL, docnum);
        mCrawler->AddURL(url);
      }
    }
    mCrawler->Start();
  }
  else if (mLoadTestFromFile) {
    for (PRInt32 i = 0; i < mRepeatCount; i++) {
      AddTestDocsFromFile(mCrawler, mInputFileName);
    }
    mCrawler->Start();
  }
  else if (mCrawl) {
    mCrawler->AddURL(mStartURL);
    mCrawler->Start();
  }
  else {
    bw->GoTo(mStartURL);
  }
  NS_RELEASE(bw);

  return NS_OK;
}

NS_IMETHODIMP
nsViewerApp::CloseWindow(nsBrowserWindow* aBrowserWindow)
{
  aBrowserWindow->Close();
  nsIBrowserWindow* bw;
  mCrawler->GetBrowserWindow(&bw);
  if (bw == aBrowserWindow) {
    mCrawler->SetBrowserWindow(nsnull);
  }
  NS_IF_RELEASE(bw);
  NS_RELEASE(aBrowserWindow);

  return NS_OK;
}

NS_IMETHODIMP
nsViewerApp::ViewSource(nsString& aURL)
{
  // Create browser window
  // XXX Some piece of code needs to properly hold the reference to this
  // browser window. For the time being the reference is released by the
  // browser event handling code during processing of the NS_DESTROY event...
  nsBrowserWindow* bw = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kBrowserWindowCID, nsnull,
                                             kIBrowserWindowIID,
                                             (void**) &bw);
  if (NS_FAILED(rv)) {
    return rv;
  }
  bw->SetApp(this);
  bw->Init(mAppShell, mPrefs, nsRect(0, 0, 620, 400), PRUint32(~0), mAllowPlugins);
  bw->Show();
  bw->GoTo(aURL.GetUnicode(),"view-source");
  NS_RELEASE(bw);

  return NS_OK;
}

NS_IMETHODIMP
nsViewerApp::OpenWindow(PRUint32 aNewChromeMask, nsIBrowserWindow*& aNewWindow)
{
  // Create browser window
  nsBrowserWindow* bw = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kBrowserWindowCID, nsnull,
                                             kIBrowserWindowIID,
                                             (void**) &bw);
  if (NS_FAILED(rv)) {
    return rv;
  }
  bw->SetApp(this);
  bw->Init(mAppShell, mPrefs, nsRect(0, 0, 620, 400), aNewChromeMask, mAllowPlugins);

  aNewWindow = bw;

  return NS_OK;
}

//----------------------------------------

// nsDispatchListener implementation

void
nsViewerApp::AfterDispatch()
{
}

//----------------------------------------

#include "prenv.h"
#include "resources.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIURL.h"

#ifndef XP_PC
#ifndef XP_MAC
#define _MAX_PATH 512
#endif
#endif

#define DEBUG_EMPTY "(none)"
static PRInt32 gDebugRobotLoads = 5000;
static char    gVerifyDir[_MAX_PATH];
static PRBool  gVisualDebug = PR_TRUE;

// Robot
static nsIDialog      * mRobotDialog = nsnull;
static nsIButton      * mCancelBtn;
static nsIButton      * mStartBtn;
static nsITextWidget  * mVerDirTxt;
static nsITextWidget  * mStopAfterTxt;
static nsICheckButton * mUpdateChkBtn;

// Site
static nsIDialog      * mSiteDialog = nsnull;
static nsIButton      * mSiteCancelBtn;
static nsIButton      * mSitePrevBtn;
static nsIButton      * mSiteNextBtn;
static nsILabel       * mSiteLabel;

static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kButtonCID,      NS_BUTTON_CID);
static NS_DEFINE_IID(kTextFieldCID,   NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kWindowCID,      NS_WINDOW_CID);
static NS_DEFINE_IID(kDialogCID,      NS_DIALOG_CID);
static NS_DEFINE_IID(kCheckButtonCID, NS_CHECKBUTTON_CID);
static NS_DEFINE_IID(kLabelCID,       NS_LABEL_CID);

static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
static NS_DEFINE_IID(kIButtonIID,      NS_IBUTTON_IID);
static NS_DEFINE_IID(kITextWidgetIID,  NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kIWidgetIID,      NS_IWIDGET_IID);
static NS_DEFINE_IID(kIDialogIID,      NS_IDIALOG_IID);
static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
static NS_DEFINE_IID(kILabelIID,       NS_ILABEL_IID);


static void* GetWidgetNativeData(nsISupports* aObject)
{
	void* 			result = nsnull;
	nsIWidget* 	widget;
	if (NS_OK == aObject->QueryInterface(kIWidgetIID,(void**)&widget))
	{
		result = widget->GetNativeData(NS_NATIVE_WIDGET);
		NS_RELEASE(widget);
	}
	return result;
}




#ifdef XP_PC
extern JSConsole *gConsole;
// XXX temporary robot code until it's made XP
extern HINSTANCE gInstance, gPrevInstance;

extern "C" NS_EXPORT int DebugRobot(
  nsVoidArray * workList, nsIWebShell * ww,
  int imax, char * verify_dir,
  void (*yieldProc)(const char *));

void yieldProc(const char * str)
{
  // Process messages
  MSG msg;
  while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
    GetMessage(&msg, NULL, 0, 0);
    if (!JSConsole::sAccelTable ||
        !gConsole ||
        !gConsole->GetMainWindow() ||
        !TranslateAccelerator(gConsole->GetMainWindow(),
                              JSConsole::sAccelTable, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
}
#endif


/**--------------------------------------------------------------------------------
 * HandleRobotEvent
 *--------------------------------------------------------------------------------
 */
static
nsEventStatus PR_CALLBACK HandleRobotEvent(nsGUIEvent *aEvent)
{
  nsEventStatus result = nsEventStatus_eIgnore;
  if (aEvent == nsnull ||  aEvent->widget == nsnull) {
    return result;
  }

  switch(aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_UP: {
      if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetWidgetNativeData(mCancelBtn)) {
        NS_ShowWidget(mRobotDialog,PR_FALSE);
      } else if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetWidgetNativeData(mStartBtn)) {

        nsString str;
        PRUint32 size;

        mStopAfterTxt->GetText(str, 255, size);
        char * cStr = str.ToNewCString();
        sscanf(cStr, "%d", &gDebugRobotLoads);
        if (gDebugRobotLoads <= 0) {
          gDebugRobotLoads = 5000;
        }
        delete[] cStr;

        mVerDirTxt->GetText(str, 255, size);
        str.ToCString(gVerifyDir, (PRInt32)_MAX_PATH);
        if (!strcmp(gVerifyDir,DEBUG_EMPTY)) {
          gVerifyDir[0] = '\0';
        }
        PRBool state = PR_FALSE;
        mUpdateChkBtn->GetState(state);
        gVisualDebug = state ? PR_TRUE: PR_FALSE;

      } 

      } break;
    
    case NS_PAINT: 
#ifndef XP_UNIX
        // paint the background
      if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == mRobotDialog ) {
          nsIRenderingContext *drawCtx = ((nsPaintEvent*)aEvent)->renderingContext;
          drawCtx->SetColor(aEvent->widget->GetBackgroundColor());
          drawCtx->FillRect(*(((nsPaintEvent*)aEvent)->rect));

          return nsEventStatus_eIgnore;
      }
#endif
      break;

    default:
      result = nsEventStatus_eIgnore;
  } //switch

  return result;
}

//--------------------------------------------
//
//--------------------------------------------
static
PRBool CreateRobotDialog(nsIWidget * aParent)
{

  PRBool result = PR_TRUE;

  if (mRobotDialog != nsnull) {
    NS_ShowWidget(mRobotDialog,PR_TRUE);
    NS_SetFocusToWidget(mStartBtn);
    return PR_TRUE;
  }

   nsILabel * label;

  nsIDeviceContext* dc = aParent->GetDeviceContext();
  float t2d;
  dc->GetTwipsToDevUnits(t2d);
  nsFont font(DIALOG_FONT, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
              NS_FONT_WEIGHT_NORMAL, 0,
              nscoord(t2d * NSIntPointsToTwips(DIALOG_FONT_SIZE)));
  NS_RELEASE(dc);

  nscoord dialogWidth = 375;
  // create a Dialog
  //
  nsRect rect;
  rect.SetRect(0, 0, dialogWidth, 162);  

  nsComponentManager::CreateInstance(kDialogCID, nsnull, kIDialogIID, (void**)&mRobotDialog);
  if (nsnull == mRobotDialog)
  	return PR_FALSE;
  
  NS_CreateDialog(aParent, mRobotDialog,rect,HandleRobotEvent,&font);
  mRobotDialog->SetLabel("Debug Robot Options");

  nscoord txtHeight   = 24;
  nscolor textBGColor = NS_RGB(255,255,255);
  nscolor textFGColor = NS_RGB(255,255,255);

  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
     lookAndFeel->GetMetric(nsILookAndFeel::eMetric_TextFieldHeight, txtHeight);
     lookAndFeel->GetColor(nsILookAndFeel::eColor_TextBackground, textBGColor);
     lookAndFeel->GetColor(nsILookAndFeel::eColor_TextForeground, textFGColor);
  }
  
  nscoord w  = 65;
  nscoord x  = 5;
  nscoord y  = 10;

  // Create Update CheckButton
  rect.SetRect(x, y, 150, 24);  
  nsComponentManager::CreateInstance(kCheckButtonCID, nsnull, kICheckButtonIID, (void**)&mUpdateChkBtn);
  NS_CreateCheckButton(mRobotDialog, mUpdateChkBtn,rect,HandleRobotEvent,&font);
  mUpdateChkBtn->SetLabel("Update Display (Visual)");
  mUpdateChkBtn->SetState(PR_TRUE);
  y += 24 + 2;

  // Create Label
  w = 115;
  rect.SetRect(x, y+3, w, 24);  
  nsComponentManager::CreateInstance(kLabelCID, nsnull, kILabelIID, (void**)&label);
  NS_CreateLabel(mRobotDialog,label,rect,HandleRobotEvent,&font);
  label->SetAlignment(eAlign_Right);
  label->SetLabel("Verfication Directory:");
  x += w + 1;

  // Create TextField
  nsIWidget* widget = nsnull;
  rect.SetRect(x, y, 225, txtHeight);  
  nsComponentManager::CreateInstance(kTextFieldCID, nsnull, kITextWidgetIID, (void**)&mVerDirTxt);
  NS_CreateTextWidget(mRobotDialog,mVerDirTxt,rect,HandleRobotEvent,&font);
  if (mVerDirTxt && NS_OK == mVerDirTxt->QueryInterface(kIWidgetIID,(void**)&widget))
  {
    widget->SetBackgroundColor(textBGColor);
    widget->SetForegroundColor(textFGColor);
  }
  nsString str(DEBUG_EMPTY);
  PRUint32 size;
  mVerDirTxt->SetText(str,size);

  y += txtHeight + 2;
  
  x = 5;
  w = 55;
  rect.SetRect(x, y+4, w, 24);  
  nsComponentManager::CreateInstance(kLabelCID, nsnull, kILabelIID, (void**)&label);
  NS_CreateLabel(mRobotDialog,label,rect,HandleRobotEvent,&font);
  label->SetAlignment(eAlign_Right);
  label->SetLabel("Stop after:");
  x += w + 2;

  // Create TextField
  rect.SetRect(x, y, 75, txtHeight);  
  nsComponentManager::CreateInstance(kTextFieldCID, nsnull, kITextWidgetIID, (void**)&mStopAfterTxt);
  NS_CreateTextWidget(mRobotDialog,mStopAfterTxt,rect,HandleRobotEvent,&font);
  if (mStopAfterTxt && NS_OK == mStopAfterTxt->QueryInterface(kIWidgetIID,(void**)&widget))
  {
    widget->SetBackgroundColor(textBGColor);
    widget->SetForegroundColor(textFGColor);
    mStopAfterTxt->SetText("5000",size);
  }
  x += 75 + 2;

  w = 75;
  rect.SetRect(x, y+4, w, 24);  
  nsComponentManager::CreateInstance(kLabelCID, nsnull, kILabelIID, (void**)&label);
  NS_CreateLabel(mRobotDialog,label,rect,HandleRobotEvent,&font);
  label->SetAlignment(eAlign_Left);
  label->SetLabel("(page loads)");
  y += txtHeight + 2;
  

  y += 10;
  w = 75;
  nscoord xx = (dialogWidth - (2*w)) / 3;
  // Create Find Start Button
  rect.SetRect(xx, y, w, 24);  
  nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID, (void**)&mStartBtn);
  NS_CreateButton(mRobotDialog,mStartBtn,rect,HandleRobotEvent,&font);
  mStartBtn->SetLabel("Start");
  
  xx += w + xx;
  // Create Cancel Button
  rect.SetRect(xx, y, w, 24);  
  nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID, (void**)&mCancelBtn);
  NS_CreateButton(mRobotDialog,mCancelBtn,rect,HandleRobotEvent,&font);
  mCancelBtn->SetLabel("Cancel");
  
  NS_ShowWidget(mRobotDialog,PR_TRUE);
  NS_SetFocusToWidget(mStartBtn);
  return result;
}


NS_IMETHODIMP
nsViewerApp::CreateRobot(nsBrowserWindow* aWindow)
{
  if (CreateRobotDialog(aWindow->mWindow))
  {
    nsIPresShell* shell = aWindow->GetPresShell();
    if (nsnull != shell) {
      nsCOMPtr<nsIDocument> doc;
      shell->GetDocument(getter_AddRefs(doc));
      if (doc) {
        const char * str;
        nsresult rv = doc->GetDocumentURL()->GetSpec(&str);
        if (NS_FAILED(rv)) {
          return rv;
        }
        nsVoidArray * gWorkList = new nsVoidArray();
        gWorkList->AppendElement(new nsString(str));
#if defined(XP_PC) && defined(NS_DEBUG)
        DebugRobot( 
          gWorkList, 
          gVisualDebug ? aWindow->mWebShell : nsnull, 
          gDebugRobotLoads, 
          PL_strdup(gVerifyDir),
          yieldProc);
#endif
      }
    }
  }
  return NS_OK;
}


//----------------------------------------
static nsBrowserWindow* gWinData;
static int gTop100Pointer = 0;
static char * gTop100List[] = {
   "http://www.yahoo.com",
   "http://www.netscape.com",
   "http://www.mozilla.org",
   "http://www.microsoft.com",
   "http://www.excite.com",
   "http://www.mckinley.com",
   "http://www.city.net",
   "http://www.webcrawler.com",
   "http://www.mirabilis.com",
   "http://www.infoseek.com",
   "http://www.warnerbros.com",
   "http://www.cnn.com",
   "http://www.altavista.com",
   "http://www.usatoday.com",
   "http://www.disney.com",
   "http://www.starwave.com",
   "http://www.hotwired.com",
   "http://www.hotbot.com",
   "http://www.lycos.com",
   "http://www.pointcast.com",
   "http://www.cnet.com",
   "http://www.search.com",
   "http://www.news.com",
   "http://www.download.com",
   "http://www.geocities.com",
   "http://www.aol.com",
   "http://members.aol.com",
   "http://www.imdb.com",
   "http://uk.imdb.com",
   "http://www.macromedia.com",
   "http://www.infobeat.com",
   "http://www.fxweb.com",
   "http://www.whowhere.com",
   "http://www.real.com",
   "http://www.sportsline.com",
   "http://www.dejanews.com",
   "http://www.cmpnet.com",
   "http://www.go2net.com",
   "http://www.metacrawler.com",
   "http://www.playsite.com",
   "http://www.stocksite.com",
   "http://www.sony.com",
   "http://www.music.sony.com",
   "http://www.station.sony.com",
   "http://www.scea.sony.com",
   "http://www.infospace.com",
   "http://www.zdnet.com",
   "http://www.hotfiles.com",
   "http://www.chathouse.com",
   "http://www.looksmart.com",
   "http://www.imaginegames.com",
   "http://www.macaddict.com",
   "http://www.rsac.org",
   "http://www.apple.com",
   "http://www.beseen.com",
   "http://www.dogpile.com",
   "http://www.xoom.com",
   "http://www.tucows.com",
   "http://www.freethemes.com",
   "http://www.winfiles.com",
   "http://www.vservers.com",
   "http://www.mtv.com",
   "http://www.the-xfiles.com",
   "http://www.datek.com",
   "http://www.cyberthrill.com",
   "http://www.surplusdirect.com",
   "http://www.tomshardware.com",
   "http://www.bigyellow.com",
   "http://www.100hot.com",
   "http://www.messagemates.com",
   "http://www.onelist.com",
   "http://www.ea.com",
   "http://www.bullfrog.co.uk",
   "http://www.travelocity.com",
   "http://www.ibm.com",
   "http://www.bigcharts.com",
   "http://www.davesclassics.com",
   "http://www.goto.com",
   "http://www.weather.com",
   "http://www.gamespot.com",
   "http://www.bloomberg.com",
   "http://www.winzip.com",
   "http://www.filez.com",
   "http://www.westwood.com",
   "http://www.internet.com",
   "http://www.cardmaster.com",
   "http://www.creaf.com",
   "http://netaddress.usa.net",
   "http://www.occ.com",
   "http://www.as.org",
   "http://www.amazon.com",
   "http://www.drudgereport.com",
   "http://www.hardradio.com",
   "http://www.intel.com",
   "http://www.mp3.com",
   "http://www.ebay.com",
   "http://www.msn.com",
   "http://www.fifa.com",
   "http://www.attitude.com",
   "http://www.happypuppy.com",
   "http://www.gamesdomain.com",
   "http://www.onsale.com",
   "http://www.tm.com",
   "http://www.xlnc1.com",
   "http://www.greatsports.com",
   "http://www.discovery.com",
   "http://www.nai.com",
   "http://www.nasa.gov",
   "http://www.ogr.com",
   "http://www.warzone.com",
   "http://www.gamestats.com",
   "http://www.winamp.com",
   "http://java.sun.com",
   "http://www.hp.com",
   "http://www.cdnow.com",
   "http://www.nytimes.com",
   "http://www.majorleaguebaseball.com",
   "http://www.washingtonpost.com",
   "http://www.planetquake.com",
   "http://www.wsj.com",
   "http://www.slashdot.org",
   "http://www.adobe.com",
   "http://www.quicken.com",
   "http://www.talkcity.com",
   "http://www.developer.com",
   "http://www.mapquest.com",
   "http://www.bluemountain.com",
   "http://www.the-park.com",
   "http://www.pathfinder.com",
   0
   };


/**--------------------------------------------------------------------------------
 * HandleSiteEvent
 *--------------------------------------------------------------------------------
 */
static
nsEventStatus PR_CALLBACK HandleSiteEvent(nsGUIEvent *aEvent)
{
  nsEventStatus result = nsEventStatus_eIgnore;
  if (aEvent == nsnull || aEvent->widget == nsnull) {
    return result;
  }

  switch(aEvent->message) {

    case NS_MOUSE_LEFT_BUTTON_UP: {
      if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetWidgetNativeData(mSiteCancelBtn)) {
        NS_ShowWidget(mSiteDialog,PR_FALSE);
      } else if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetWidgetNativeData(mSitePrevBtn)) {
        if (gTop100Pointer > 0) {
          NS_EnableWidget(mSiteNextBtn,PR_TRUE);
          if (gWinData) {
            nsString urlStr(gTop100List[--gTop100Pointer]);
            mSiteLabel->SetLabel(urlStr);
            gWinData->GoTo(urlStr);
          }
        } else  {
          NS_EnableWidget(mSitePrevBtn,PR_FALSE);
          NS_EnableWidget(mSiteNextBtn,PR_TRUE);
        }

      } else if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetWidgetNativeData(mSiteNextBtn)) {

        char * p = gTop100List[++gTop100Pointer];
        if (p) {
          if (gWinData) {
            nsString urlStr(gTop100List[gTop100Pointer]);
            mSiteLabel->SetLabel(urlStr);
            gWinData->GoTo(urlStr);
          }
          NS_EnableWidget(mSitePrevBtn,PR_TRUE);
        } else {
          NS_EnableWidget(mSitePrevBtn,PR_TRUE);
          NS_EnableWidget(mSiteNextBtn,PR_FALSE);
          mSiteLabel->SetLabel("[END OF LIST]");
        }
      }
      } break;
      
      case NS_PAINT: 
#ifndef XP_UNIX
        // paint the background
        if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetWidgetNativeData(mSiteDialog) ) {
            nsIRenderingContext *drawCtx = ((nsPaintEvent*)aEvent)->renderingContext;
            drawCtx->SetColor(aEvent->widget->GetBackgroundColor());
            drawCtx->FillRect(*(((nsPaintEvent*)aEvent)->rect));

            return nsEventStatus_eIgnore;
        }
#endif
      break;

      default:
        result = nsEventStatus_eIgnore;
    }

    return result;
}

//-----------------------------------------
//--
//-----------------------------------------
static
PRBool CreateSiteDialog(nsIWidget * aParent)
{

  PRBool result = PR_TRUE;

  if (mSiteDialog == nsnull) {
    nsILookAndFeel * lookAndFeel;
    if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
       //lookAndFeel->GetMetric(nsILookAndFeel::eMetric_TextFieldHeight, txtHeight);
       //lookAndFeel->GetColor(nsILookAndFeel::eColor_TextBackground, textBGColor);
       //lookAndFeel->GetColor(nsILookAndFeel::eColor_TextForeground, textFGColor);
    }

    nsILabel * label;

    nsIDeviceContext* dc = aParent->GetDeviceContext();
    float t2d;
    dc->GetTwipsToDevUnits(t2d);
    nsFont font(DIALOG_FONT, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                NS_FONT_WEIGHT_NORMAL, 0,
                nscoord(t2d * NSIntPointsToTwips(DIALOG_FONT_SIZE)));
    NS_RELEASE(dc);

    nscoord dialogWidth = 375;
    // create a Dialog
    //
    nsRect rect;
    rect.SetRect(0, 0, dialogWidth, 125);  

    nsIWidget* widget = nsnull;
    nsComponentManager::CreateInstance(kDialogCID, nsnull, kIDialogIID, (void**)&mSiteDialog);
    if (nsnull == mSiteDialog)
      return PR_FALSE;
    
    if (NS_OK == mSiteDialog->QueryInterface(kIWidgetIID,(void**)&widget))
    {
      widget->Create(aParent, rect, HandleSiteEvent, NULL);
      mSiteDialog->SetLabel("Top 100 Site Walker");
    }
    //mSiteDialog->SetClientData(this);

    nscoord w  = 65;
    nscoord x  = 5;
    nscoord y  = 10;

    // Create Label
    w = 50;
    rect.SetRect(x, y+3, w, 24);  
    nsComponentManager::CreateInstance(kLabelCID, nsnull, kILabelIID, (void**)&label);
    NS_CreateLabel(mSiteDialog,label,rect,HandleSiteEvent,&font);
    label->SetAlignment(eAlign_Right);
    label->SetLabel("Site:");
    x += w + 1;

    w = 250;
    rect.SetRect(x, y+3, w, 24);  
    nsComponentManager::CreateInstance(kLabelCID, nsnull, kILabelIID, (void**)&mSiteLabel);
    NS_CreateLabel(mSiteDialog,mSiteLabel,rect,HandleSiteEvent,&font);
    mSiteLabel->SetAlignment(eAlign_Left);
    mSiteLabel->SetLabel("");

    y += 34;
    w = 75;
    nscoord spacing = (dialogWidth - (3*w)) / 4;
    x = spacing;
    // Create Previous Button
    rect.SetRect(x, y, w, 24);  
    nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID, (void**)&mSitePrevBtn);
    NS_CreateButton(mSiteDialog,mSitePrevBtn,rect,HandleSiteEvent,&font);
    mSitePrevBtn->SetLabel("<< Previous");
    x += spacing + w;

    // Create Next Button
    rect.SetRect(x, y, w, 24);  
    nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID, (void**)&mSiteNextBtn);
    NS_CreateButton(mSiteDialog,mSiteNextBtn,rect,HandleSiteEvent,&font);
    mSiteNextBtn->SetLabel("Next >>");
    x += spacing + w;
  
    // Create Cancel Button
    rect.SetRect(x, y, w, 24);  
    nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID, (void**)&mSiteCancelBtn);
    NS_CreateButton(mSiteDialog,mSiteCancelBtn,rect,HandleSiteEvent,&font);
    mSiteCancelBtn->SetLabel("Cancel");
  }

  NS_ShowWidget(mSiteDialog,PR_TRUE);
  NS_SetFocusToWidget(mSiteNextBtn);

  // Init
  NS_EnableWidget(mSitePrevBtn,PR_FALSE);
 
  if (gWinData) {
    nsString urlStr(gTop100List[gTop100Pointer]);
    gWinData->GoTo(urlStr);
    mSiteLabel->SetLabel(urlStr);
  }

  return result;
}


NS_IMETHODIMP
nsViewerApp::CreateSiteWalker(nsBrowserWindow* aWindow)
{
  if (nsnull == gWinData) {
    gWinData = aWindow;
    NS_ADDREF(aWindow);
    CreateSiteDialog(aWindow->mWindow);
  }
  return NS_OK;
}

//----------------------------------------

#ifdef XP_PC
#include "jsconsres.h"

static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);

static void DestroyConsole()
{
 if (gConsole) {
    gConsole->SetNotification(NULL);
    delete gConsole;
    gConsole = NULL;
  }
}

static void ShowConsole(nsBrowserWindow* aWindow)
{
    HWND hWnd = (HWND)aWindow->mWindow->GetNativeData(NS_NATIVE_WIDGET);
    if (!gConsole) {

      // load the accelerator table for the console
      if (!JSConsole::sAccelTable) {
        JSConsole::sAccelTable = LoadAccelerators(gInstance,
                                                  MAKEINTRESOURCE(ACCELERATOR_TABLE));
      }
      
      nsIScriptContextOwner *owner = nsnull;
      nsIScriptContext *context = nsnull;        
      // XXX needs to change to aWindow->mWebShell
      if (NS_OK == aWindow->mWebShell->QueryInterface(kIScriptContextOwnerIID, (void **)&owner)) {
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
#endif

NS_IMETHODIMP
nsViewerApp::CreateJSConsole(nsBrowserWindow* aWindow)
{
#ifdef XP_PC
  if (nsnull == gConsole) {
    ShowConsole(aWindow);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsViewerApp::DoPrefs(nsBrowserWindow* aWindow)
{
#if defined(XP_PC) && defined(NGPREFS)
  INGLayoutPrefs *pPrefs;
  CoInitialize(NULL);

  HRESULT res = CoCreateInstance(CLSID_NGLayoutPrefs, NULL, 
                                 CLSCTX_INPROC_SERVER,
                                 IID_INGLayoutPrefs, (void**)&pPrefs);

  if (SUCCEEDED(res)) {
      pPrefs->Show(NULL);
      pPrefs->Release();
  }
  CoUninitialize();
#endif
  return NS_OK;
}
