/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef NGPREFS
#define INITGUID
#endif

#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsXPBaseWindow.h" 
#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsWidgetsCID.h"
#include "nsIAppShell.h"
#include "nsIPref.h"

// Form Processor
#include "nsIFormProcessor.h"


#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsWebCrawler.h"
#include "nsSpecialSystemDirectory.h"    // For exe dir
#include "prprf.h"
#include "plstr.h"
#include "prenv.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsGUIEvent.h"

// Needed for Dialog GUI
#include "nsICheckButton.h"
#include "nsILabel.h"
#include "nsIButton.h"
#include "nsITextWidget.h"
#include "nsILookAndFeel.h"
#include "nsColor.h"
#include "nsWidgetSupport.h"
#include "nsVector.h"


// XXX For font setting below
#include "nsFont.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"

#include "nsIDOMHTMLSelectElement.h"


// new widget stuff
#ifdef USE_LOCAL_WIDGETS
  extern nsresult NS_NewButton(nsIButton** aControl);
  extern nsresult NS_NewLabel(nsILabel** aControl);
  extern nsresult NS_NewTextWidget(nsITextWidget** aControl);
  extern nsresult NS_NewCheckButton(nsICheckButton** aControl);
#endif

// cookie
#include "nsICookieService.h"

#define DIALOG_FONT      "Helvetica"
#define DIALOG_FONT_SIZE 10


#if defined(XP_PC) && !defined(XP_OS2)
#include "JSConsole.h"
#ifdef NGPREFS
#include "ngprefs.h"
#endif
#endif
#ifdef XP_WIN
#include <crtdbg.h>
#endif

extern nsresult NS_NewXPBaseWindowFactory(nsIFactory** aFactory);
extern "C" void NS_SetupRegistry();

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kXPBaseWindowCID, NS_XPBASE_WINDOW_CID);
static NS_DEFINE_IID(kCookieServiceCID, NS_COOKIESERVICE_CID);

static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIXPBaseWindowIID, NS_IXPBASE_WINDOW_IID);

static NS_DEFINE_CID(kFormProcessorCID,   NS_FORMPROCESSOR_CID);
static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);

#define DEFAULT_WIDTH 620
#define DEFAULT_HEIGHT 400

nsViewerApp::nsViewerApp()
{
  NS_INIT_REFCNT(); 

  char * text = PR_GetEnv("NGLAYOUT_HOME");
  mStartURL.AssignWithConversion(text ? text : "resource:/res/samples/test0.html");

  //rickg 20Nov98: For the sake of a good demo, pre-load a decent URL...
//  mStartURL = text ? text : "http://developer.netscape.com/software/communicator/ngl/index.html";

  mDelay = 1;
  mRepeatCount = 1;
  mNumSamples = 14;
  mAllowPlugins = PR_TRUE;
  mIsInitialized = PR_FALSE;
  mWidth = DEFAULT_WIDTH;
  mHeight = DEFAULT_HEIGHT;
  mJustShutdown = PR_FALSE;
}

nsViewerApp::~nsViewerApp()
{
  Destroy();
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
  if (nsnull != mCrawler) {
    // break cycle between crawler and window.
    mCrawler->SetBrowserWindow(nsnull);
    NS_RELEASE(mCrawler);
  }

  if (nsnull != mPrefs) {
    NS_RELEASE(mPrefs);
  }
}

class nsTestFormProcessor : public nsIFormProcessor {
public:
  nsTestFormProcessor();
  NS_IMETHOD ProcessValue(nsIDOMHTMLElement *aElement, 
                          const nsString& aName, 
                          nsString& aValue);

  NS_IMETHOD ProvideContent(const nsString& aFormType, 
                            nsVoidArray& aContent,
                            nsString& aAttribute);
  NS_DECL_ISUPPORTS
};



NS_IMPL_ISUPPORTS1(nsTestFormProcessor, nsIFormProcessor);

nsTestFormProcessor::nsTestFormProcessor()
{
   NS_INIT_REFCNT();
}

NS_METHOD 
nsTestFormProcessor::ProcessValue(nsIDOMHTMLElement *aElement, 
                                  const nsString& aName, 
                                  nsString& aValue)
{
#ifdef DEBUG_kmcclusk
  char *name = ToNewCString(aName);
  char *value = ToNewCString(aValue);
  printf("ProcessValue: name %s value %s\n",  name, value);
  delete [] name;
  delete [] value;
#endif

  return NS_OK;
}

NS_METHOD nsTestFormProcessor::ProvideContent(const nsString& aFormType, 
                               nsVoidArray& aContent,
                               nsString& aAttribute)
{
  return NS_OK;
}


nsresult
nsViewerApp::AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                                 NULL /* default */);

  return rv;
}


nsresult
nsViewerApp::SetupRegistry()
{
  nsresult rv;
  AutoregisterComponents();

  NS_SetupRegistry();

  // Register our browser window factory
  nsIFactory* bwf;
  NS_NewXPBaseWindowFactory(&bwf);
  nsComponentManager::RegisterFactory(kXPBaseWindowCID, 0, 0, bwf, PR_FALSE);
  NS_RELEASE(bwf);

  // register the cookie manager
  nsCOMPtr<nsICookieService> cookieService = 
           do_GetService(kCookieServiceCID, &rv);
  if (NS_FAILED(rv) || (nsnull == cookieService)) {
#ifdef DEBUG
    printf("Unable to instantiate Cookie Manager\n");
#endif
  }

   // Register a form processor. The form processor has the opportunity to
   // modify the value's passed during form submission.
  nsTestFormProcessor* testFormProcessor = new nsTestFormProcessor();
  nsCOMPtr<nsISupports> formProcessor;
  rv = testFormProcessor->QueryInterface(kISupportsIID, getter_AddRefs(formProcessor));
  if (NS_SUCCEEDED(rv) && formProcessor) {
    rv = nsServiceManager::RegisterService(kFormProcessorCID, formProcessor);
  }

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
  mPrefs->ReadUserPrefs(nsnull);

  // Finally process our arguments
  rv = ProcessArguments(argc, argv);

  mIsInitialized = PR_TRUE;
  return rv;
}

nsresult
nsViewerApp::Exit()
{
  nsresult rv = NS_OK;
  if (mAppShell) {
    Destroy();
    mAppShell->Exit();
    NS_RELEASE(mAppShell);
  }

    // Unregister the test form processor registered in nsViewerApp::SetupRegistry
  rv = nsServiceManager::UnregisterService(kFormProcessorCID);

  return rv;
}

static void
PrintHelpInfo(char **argv)
{
  fprintf(stderr, "Usage: %s [options] [starting url]\n", argv[0]);
  fprintf(stderr, "-M -- measure (and show) page load time\n");
  fprintf(stderr, "-p[#]   -- autload tests 0-#\n");
  fprintf(stderr, "-q -- jiggles window width after page has autoloaded\n");
  fprintf(stderr, "-f filename -- read a list of URLs to autoload from <filename>\n");
  fprintf(stderr, "-d # -- set the delay between autoloads to # (in milliseconds)\n");
  fprintf(stderr, "-np -- no plugins\n");
  fprintf(stderr, "-v -- verbose (debug noise)\n");
  fprintf(stderr, "-r # -- how many times a page is loaded when autoloading\n");
  fprintf(stderr, "-o dirname -- create an output file for the frame dump of each page\n  and put it in <dirname>. <dirname> must include the trailing\n  <slash> character appropriate for your OS\n");
  fprintf(stderr, "-rd dirname -- specify a regression directory whose contents are from\n  a previous -o run to compare against with this run\n");
  fprintf(stderr, "-regnostyle -- exclude style data from the regression test output: valid only with -o and -rd\n");
  fprintf(stderr, "-h # -- the initial height of the viewer window\n");
  fprintf(stderr, "-w # -- the initial width of the viewer window\n");
  fprintf(stderr, "-C -- enable crawler\n");
  fprintf(stderr, "-R filename -- record pages crawled to in <filename>\n");
  fprintf(stderr, "-S domain -- add a domain/host that is safe to crawl (e.g. www.netscape.com)\n");
  fprintf(stderr, "-A domain -- add a domain/host that should be avoided (e.g. microsoft.com)\n");
  fprintf(stderr, "-N pages -- set the max # of pages to crawl\n");
  fprintf(stderr, "-x -- startup and just shutdown to test for leaks under Purify\n");
  fprintf(stderr, "-Prt -- number of the printer test 1=regression, no printout \n");
  fprintf(stderr, "-B -- Setting for regression output 1=brief, 2=verbose \n");
#if defined(NS_DEBUG) && defined(XP_WIN)
  fprintf(stderr, "-md # -- set the crt debug flags to #\n");
#endif
}

static void
AddTestDocsFromFile(nsWebCrawler* aCrawler, const nsString& aFileName)
{
  NS_LossyConvertUCS2toASCII cfn(aFileName);
#ifdef XP_PC
  FILE* fp = fopen(cfn.get(), "rb");
#else
  FILE* fp = fopen(cfn.get(), "r");
#endif

  if (nsnull==fp)
  {
    fprintf(stderr, "Input file not found: %s\n", cfn.get());
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
      line.AssignWithConversion(linebuf);
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
      if (PL_strcmp(argv[i], "-x") == 0) {
        mJustShutdown = PR_TRUE;
      }
#if defined(NS_DEBUG) && defined(XP_WIN)
      else if (PL_strcmp(argv[i], "-md") == 0) {
        int old = _CrtSetDbgFlag(0);
        old |= _CRTDBG_CHECK_ALWAYS_DF;
        _CrtSetDbgFlag(old);
      }
#endif
      else if (PL_strncmp(argv[i], "-p", 2) == 0) {
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
        mInputFileName.AssignWithConversion(argv[i]);
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
        mCrawler->SetRegressionDir(NS_ConvertASCIItoUCS2(argv[i]));
      }
      else if (PL_strcmp(argv[i], "-o") == 0) {
        i++;
        if (i>=argc || nsnull==argv[i] || nsnull==*(argv[i]))
        {
          PrintHelpInfo(argv);
          exit(-1);
        }
        mCrawler->SetOutputDir(NS_ConvertASCIItoUCS2(argv[i]));
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
        mCrawler->AddSafeDomain(NS_ConvertASCIItoUCS2(argv[i]));
      }
      else if (PL_strcmp(argv[i], "-A") == 0) {
        i++;
        if (i>=argc) {
          PrintHelpInfo(argv);
          exit(-1);
        }
        mCrawler->AddAvoidDomain(NS_ConvertASCIItoUCS2(argv[i]));
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
      else if (PL_strcmp(argv[i], "-M") == 0) {
        mShowLoadTimes = PR_TRUE;
      }
      else if (PL_strcmp(argv[i], "-?") == 0) {
        PrintHelpInfo(argv);
      }
      else if (PL_strcmp(argv[i], "-B") == 0) {
        int regressionOutput;
        i++;
        if (i>=argc || 1!=sscanf(argv[i], "%d", &regressionOutput)){
          PrintHelpInfo(argv);
          exit(-1);
        }
        mCrawler->RegressionOutput(regressionOutput);
      }
      else if (PL_strcmp(argv[i], "-Prt") == 0) {
        int printTestType;
        i++;
        if (i>=argc || 1!=sscanf(argv[i], "%d", &printTestType)){
          PrintHelpInfo(argv);
          exit(-1);
        }
        mCrawler->SetPrintTest(printTestType);
      }
      else if(PL_strcmp(argv[i], "-regnostyle") == 0) {
        mCrawler->IncludeStyleData(PR_FALSE);
      }
    }
    else
      break;
  }
  if (i < argc) {
    mStartURL.AssignWithConversion(argv[i]);
#if defined(XP_UNIX) || defined(XP_BEOS)
    if (argv[i][0] == '/') {
      mStartURL.InsertWithConversion("file:", 0);
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
  nsBrowserWindow* bw = new nsNativeBrowserWindow();
  NS_ENSURE_TRUE(bw, NS_ERROR_FAILURE);
  NS_ADDREF(bw);

  bw->SetApp(this);
  bw->SetShowLoadTimes(mShowLoadTimes);
  bw->Init(mAppShell, nsRect(0, 0, mWidth, mHeight),
           (PRUint32(~0) & ~nsIWebBrowserChrome::CHROME_OPENAS_CHROME), mAllowPlugins);
  bw->SetVisibility(PR_TRUE);
  nsBrowserWindow*	bwCurrent;
  mCrawler->GetBrowserWindow(&bwCurrent);
  if (!bwCurrent) {
	  mCrawler->SetBrowserWindow(bw);
    bw->SetWebCrawler(mCrawler);
  }
  NS_IF_RELEASE(bwCurrent);

  if (mDoPurify) {
    for (PRInt32 i = 0; i < mRepeatCount; i++) {
      for (int docnum = 0; docnum < mNumSamples; docnum++) {
        char url[500];
        PR_snprintf(url, 500, "%s/test%d.html", SAMPLES_BASE_URL, docnum);
        mCrawler->AddURL(NS_ConvertASCIItoUCS2(url));
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
    bw->GoTo(mStartURL.get());
  }
  NS_RELEASE(bw);

  return NS_OK;
}

NS_IMETHODIMP
nsViewerApp::CloseWindow(nsBrowserWindow* aBrowserWindow)
{
  aBrowserWindow->Destroy();
  nsBrowserWindow* bw;
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
  nsBrowserWindow* bw = new nsNativeBrowserWindow();
  NS_ENSURE_TRUE(bw, NS_ERROR_FAILURE);
  NS_ADDREF(bw);

  bw->SetApp(this);
  bw->Init(mAppShell, nsRect(0, 0, 620, 400), PRUint32(~0), mAllowPlugins);
  bw->SetTitle(NS_ConvertASCIItoUCS2("View Source").get());
  bw->SetVisibility(PR_TRUE);

  nsAutoString strUrl; strUrl.AppendWithConversion("view-source:");
  strUrl.Append(aURL);
  bw->GoTo(strUrl.get());

  NS_RELEASE(bw);

  return NS_OK;
}

NS_IMETHODIMP
nsViewerApp::OpenWindow(PRUint32 aNewChromeMask, nsBrowserWindow*& aNewWindow)
{
  // Create browser window
  nsBrowserWindow* bw = new nsNativeBrowserWindow();
  NS_ENSURE_TRUE(bw, NS_ERROR_FAILURE);
  NS_ADDREF(bw);

  bw->SetApp(this);
  bw->Init(mAppShell, nsRect(0, 0, 620, 400), aNewChromeMask, mAllowPlugins);
  // Defer showing chrome windows until the chrome has loaded
  if (!(aNewChromeMask & nsIWebBrowserChrome::CHROME_OPENAS_CHROME))
    bw->SetVisibility(PR_TRUE);

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
static nsIWidget      * mRobotDialog = nsnull;
static nsIButton      * mCancelBtn;
static nsIButton      * mStartBtn;
static nsITextWidget  * mVerDirTxt;
static nsITextWidget  * mStopAfterTxt;
static nsICheckButton * mUpdateChkBtn;

// Site
static nsIWidget * mSiteDialog = nsnull;
static nsIButton      * mSiteCancelBtn;
static nsIButton      * mSitePrevBtn;
static nsIButton      * mSiteNextBtn;
static nsILabel       * mSiteLabel;
static nsIButton      * mSiteJumpBtn;
static nsITextWidget  * mSiteIndexTxt;

static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kButtonCID,      NS_BUTTON_CID);
static NS_DEFINE_IID(kTextFieldCID,   NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kWindowCID,      NS_WINDOW_CID);
static NS_DEFINE_IID(kCheckButtonCID, NS_CHECKBUTTON_CID);
static NS_DEFINE_IID(kLabelCID,       NS_LABEL_CID);


static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
static NS_DEFINE_IID(kIButtonIID,      NS_IBUTTON_IID);
static NS_DEFINE_IID(kITextWidgetIID,  NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kIWidgetIID,      NS_IWIDGET_IID);
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




#if defined(XP_PC) && !defined(XP_OS2)
extern JSConsole *gConsole;
// XXX temporary robot code until it's made XP
extern HINSTANCE gInstance, gPrevInstance;

extern "C" NS_EXPORT int DebugRobot(
  nsVoidArray * workList, nsIDocShell * ww,
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
        char * cStr = ToNewCString(str);
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
        if (aEvent->widget == mRobotDialog ) {
          nsIRenderingContext *drawCtx = ((nsPaintEvent*)aEvent)->renderingContext;
          drawCtx->SetColor(aEvent->widget->GetBackgroundColor());
          drawCtx->FillRect(*(((nsPaintEvent*)aEvent)->rect));

          return nsEventStatus_eIgnore;
      }
#endif
      return nsEventStatus_eIgnore;
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

  nsComponentManager::CreateInstance(kWindowCID, nsnull, kIWidgetIID, (void**)&mRobotDialog);
  if (nsnull == mRobotDialog)
  	return PR_FALSE;

  nsIWidget* dialogWidget = nsnull;
  if (NS_OK == mRobotDialog->QueryInterface(kIWidgetIID,(void**)&dialogWidget))
  {
    dialogWidget->Create(aParent, rect, HandleRobotEvent, NULL);
    NS_RELEASE(dialogWidget);
  }
  
  //mRobotDialog->SetLabel("Debug Robot Options");

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
#ifdef USE_LOCAL_WIDGETS
  NS_NewCheckButton(&mUpdateChkBtn);
#else
  nsComponentManager::CreateInstance(kCheckButtonCID, nsnull, kICheckButtonIID, (void**)&mUpdateChkBtn);
#endif
  NS_CreateCheckButton(mRobotDialog, mUpdateChkBtn,rect,HandleRobotEvent,&font);
  mUpdateChkBtn->SetLabel(NS_ConvertASCIItoUCS2("Update Display (Visual)"));
  mUpdateChkBtn->SetState(PR_TRUE);
  y += 24 + 2;

  // Create Label
  w = 115;
  rect.SetRect(x, y+3, w, 24);  
#ifdef USE_LOCAL_WIDGETS
  NS_NewLabel(&label);
#else
  nsComponentManager::CreateInstance(kLabelCID, nsnull, kILabelIID, (void**)&label);
#endif
  NS_CreateLabel(mRobotDialog,label,rect,HandleRobotEvent,&font);
  label->SetAlignment(eAlign_Right);
  label->SetLabel(NS_ConvertASCIItoUCS2("Verfication Directory:"));
  x += w + 1;

  // Create TextField
  nsIWidget* widget = nsnull;
  rect.SetRect(x, y, 225, txtHeight);  
#ifdef USE_LOCAL_WIDGETS
  NS_NewTextWidget(&mVerDirTxt);
#else
  nsComponentManager::CreateInstance(kTextFieldCID, nsnull, kITextWidgetIID, (void**)&mVerDirTxt);
#endif
  NS_CreateTextWidget(mRobotDialog,mVerDirTxt,rect,HandleRobotEvent,&font);
  if (mVerDirTxt && NS_OK == mVerDirTxt->QueryInterface(kIWidgetIID,(void**)&widget))
  {
    widget->SetBackgroundColor(textBGColor);
    widget->SetForegroundColor(textFGColor);
  }
  nsString str; str.AssignWithConversion(DEBUG_EMPTY);
  PRUint32 size;
  mVerDirTxt->SetText(str,size);

  y += txtHeight + 2;
  
  x = 5;
  w = 55;
  rect.SetRect(x, y+4, w, 24);  
#ifdef USE_LOCAL_WIDGETS
  NS_NewLabel(&label);
#else
  nsComponentManager::CreateInstance(kLabelCID, nsnull, kILabelIID, (void**)&label);
#endif
  NS_CreateLabel(mRobotDialog,label,rect,HandleRobotEvent,&font);
  label->SetAlignment(eAlign_Right);
  label->SetLabel(NS_ConvertASCIItoUCS2("Stop after:"));
  x += w + 2;

  // Create TextField
  rect.SetRect(x, y, 75, txtHeight);  
#ifdef USE_LOCAL_WIDGETS
  NS_NewTextWidget(&mStopAfterTxt);
#else
  nsComponentManager::CreateInstance(kTextFieldCID, nsnull, kITextWidgetIID, (void**)&mStopAfterTxt);
#endif
  NS_CreateTextWidget(mRobotDialog,mStopAfterTxt,rect,HandleRobotEvent,&font);
  if (mStopAfterTxt && NS_OK == mStopAfterTxt->QueryInterface(kIWidgetIID,(void**)&widget))
  {
    widget->SetBackgroundColor(textBGColor);
    widget->SetForegroundColor(textFGColor);
    mStopAfterTxt->SetText(NS_ConvertASCIItoUCS2("5000"),size);
  }
  x += 75 + 2;

  w = 75;
  rect.SetRect(x, y+4, w, 24);  
#ifdef USE_LOCAL_WIDGETS
  NS_NewLabel(&label);
#else
  nsComponentManager::CreateInstance(kLabelCID, nsnull, kILabelIID, (void**)&label);
#endif
  NS_CreateLabel(mRobotDialog,label,rect,HandleRobotEvent,&font);
  label->SetAlignment(eAlign_Left);
  label->SetLabel(NS_ConvertASCIItoUCS2("(page loads)"));
  y += txtHeight + 2;
  

  y += 10;
  w = 75;
  nscoord xx = (dialogWidth - (2*w)) / 3;
  // Create Find Start Button
  rect.SetRect(xx, y, w, 24);  
#ifdef USE_LOCAL_WIDGETS
  NS_NewButton(&mStartBtn);
#else
  nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID, (void**)&mStartBtn);
#endif
  NS_CreateButton(mRobotDialog,mStartBtn,rect,HandleRobotEvent,&font);
  mStartBtn->SetLabel(NS_ConvertASCIItoUCS2("Start"));
  
  xx += w + xx;
  // Create Cancel Button
  rect.SetRect(xx, y, w, 24);  
#ifdef USE_LOCAL_WIDGETS
  NS_NewButton(&mCancelBtn);
#else
  nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID, (void**)&mCancelBtn);
#endif
  NS_CreateButton(mRobotDialog,mCancelBtn,rect,HandleRobotEvent,&font);
  mCancelBtn->SetLabel(NS_ConvertASCIItoUCS2("Cancel"));
  
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
        char * str;
        nsCOMPtr<nsIURI> uri;
        doc->GetDocumentURL(getter_AddRefs(uri));
        nsresult rv = uri->GetSpec(&str);
        if (NS_FAILED(rv)) {
          return rv;
        }
        nsVoidArray * gWorkList = new nsVoidArray();

        {
          nsString* tempStr = new nsString;
          if ( tempStr )
            tempStr->AssignWithConversion(str);
          gWorkList->AppendElement(tempStr);
        }
#if defined(XP_PC) && defined(NS_DEBUG) && !defined(XP_OS2)
        DebugRobot( 
          gWorkList, 
          gVisualDebug ? aWindow->mDocShell : nsnull, 
          gDebugRobotLoads, 
          PL_strdup(gVerifyDir),
          yieldProc);
#endif
        nsCRT::free(str);
      }
    }
  }
  return NS_OK;
}


//----------------------------------------
static nsBrowserWindow* gWinData;
static int gTop100Pointer = 0;
static int gTop100LastPointer = 0;
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
   "http://www.amazon.com",
   "http://www.intel.com",
   "http://www.mp3.com",
   "http://www.ebay.com",
   "http://www.msn.com",
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
   "http://www.drudgereport.com",
   "http://www.hardradio.com",
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
   "http://www.macaddict.com",
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

      } else if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetWidgetNativeData(mSiteIndexTxt)) {
        // no op

      } else {

        PRInt32 oldIndex = gTop100Pointer;

        if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetWidgetNativeData(mSitePrevBtn)) {
          gTop100Pointer--;
        } else if (aEvent->widget->GetNativeData(NS_NATIVE_WIDGET) == GetWidgetNativeData(mSiteNextBtn)) {
          gTop100Pointer++;
        } else {
          nsString str;
          PRUint32 size;
          PRInt32  inx;

          mSiteIndexTxt->GetText(str, 255, size);
          char * cStr = ToNewCString(str);
          sscanf(cStr, "%d", &inx);
          if (inx >= 0 && inx < gTop100LastPointer) {
            gTop100Pointer = inx;
          }
          delete[] cStr;
        }

        PRBool loadPage = PR_FALSE;
        if (gTop100Pointer < 0) {
          gTop100Pointer = 0;
        } else if (gTop100Pointer >= gTop100LastPointer) {
          gTop100Pointer = gTop100LastPointer-1;
        } else {
          loadPage = PR_TRUE;
        }

        NS_EnableWidget(mSitePrevBtn, gTop100Pointer > 0);
        NS_EnableWidget(mSiteNextBtn, gTop100Pointer < (gTop100LastPointer-1));

        if (gWinData && loadPage && oldIndex != gTop100Pointer) {
          nsString urlStr; urlStr.AssignWithConversion(gTop100List[gTop100Pointer]);
          mSiteLabel->SetLabel(urlStr);
          gWinData->GoTo(urlStr.get());
        }

        nsAutoString str;
        str.AppendInt(gTop100Pointer);
        PRUint32 size;
        mSiteIndexTxt->SetText(str, size);
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
  // Dynamically find the index of the last pointer
  gTop100LastPointer = 0;
  char * p;
  do {
    p = gTop100List[gTop100LastPointer++];
  } while (p);
  gTop100LastPointer--;

  PRBool result = PR_TRUE;

  if (mSiteDialog == nsnull) {

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
    rect.SetRect(0, 0, dialogWidth, 125+24+10);  

    nsIWidget* widget = nsnull;
    nsComponentManager::CreateInstance(kWindowCID, nsnull, 
                                       kIWidgetIID, (void**)&mSiteDialog);
    if (nsnull == mSiteDialog)
      return PR_FALSE;
    
    if (NS_OK == mSiteDialog->QueryInterface(kIWidgetIID,(void**)&widget))
    {
      widget->Create((nsIWidget *) nsnull, rect, HandleSiteEvent, NULL);
      //mSiteDialog->SetLabel("Top 100 Site Walker");
    }
    //mSiteDialog->SetClientData(this);
    nsAutoString titleStr; titleStr.AssignWithConversion("Top ");
    titleStr.AppendInt(gTop100LastPointer);
    titleStr.AppendWithConversion(" Sites");
    mSiteDialog->SetTitle(titleStr);

    nscoord w  = 65;
    nscoord x  = 5;
    nscoord y  = 10;

    // Create Label
    w = 50;
    rect.SetRect(x, y+3, w, 24);  
#ifdef USE_LOCAL_WIDGETS
    NS_NewLabel(&label);
#else
    nsComponentManager::CreateInstance(kLabelCID, nsnull, kILabelIID, (void**)&label);
#endif
    NS_CreateLabel(mSiteDialog,label,rect,HandleSiteEvent,&font);
    label->SetAlignment(eAlign_Right);
    label->SetLabel(NS_ConvertASCIItoUCS2("Site:"));
    x += w + 1;

    w = 250;
    rect.SetRect(x, y+3, w, 24);  
#ifdef USE_LOCAL_WIDGETS
    NS_NewLabel(&mSiteLabel);
#else
   nsComponentManager::CreateInstance(kLabelCID, nsnull, kILabelIID, (void**)&mSiteLabel);
#endif
    NS_CreateLabel(mSiteDialog,mSiteLabel,rect,HandleSiteEvent,&font);
    mSiteLabel->SetAlignment(eAlign_Left);
    mSiteLabel->SetLabel(nsAutoString());

    y += 34;
    w = 75;
    nscoord spacing = (dialogWidth - (3*w)) / 4;
    x = spacing;
    // Create Previous Button
    rect.SetRect(x, y, w, 24);  
#ifdef USE_LOCAL_WIDGETS
    NS_NewButton(&mSitePrevBtn);
#else
    nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID, (void**)&mSitePrevBtn);
#endif
    NS_CreateButton(mSiteDialog,mSitePrevBtn,rect,HandleSiteEvent,&font);
    mSitePrevBtn->SetLabel(NS_ConvertASCIItoUCS2("<< Previous"));
    x += spacing + w;

    // Create Next Button
    rect.SetRect(x, y, w, 24);  
#ifdef USE_LOCAL_WIDGETS
    NS_NewButton(&mSiteNextBtn);
#else
    nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID, (void**)&mSiteNextBtn);
#endif
    NS_CreateButton(mSiteDialog,mSiteNextBtn,rect,HandleSiteEvent,&font);
    mSiteNextBtn->SetLabel(NS_ConvertASCIItoUCS2("Next >>"));
    x += spacing + w;
  
    // Create Cancel Button
    rect.SetRect(x, y, w, 24);  
#ifdef USE_LOCAL_WIDGETS
    NS_NewButton(&mSiteCancelBtn);
#else
    nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID, (void**)&mSiteCancelBtn);
#endif
    NS_CreateButton(mSiteDialog,mSiteCancelBtn,rect,HandleSiteEvent,&font);
    mSiteCancelBtn->SetLabel(NS_ConvertASCIItoUCS2("Cancel"));

    /////////////////////////
    w  = 65;
    x  = spacing;
    y  += 24 + 10;

    nscoord txtHeight   = 24;
    nscolor textBGColor = NS_RGB(255,255,255);
    nscolor textFGColor = NS_RGB(255,255,255);

    nsILookAndFeel * lookAndFeel;
    if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
       lookAndFeel->GetMetric(nsILookAndFeel::eMetric_TextFieldHeight, txtHeight);
       lookAndFeel->GetColor(nsILookAndFeel::eColor_TextBackground, textBGColor);
       lookAndFeel->GetColor(nsILookAndFeel::eColor_TextForeground, textFGColor);
    }

    // Create TextField
    rect.SetRect(x, y, w, txtHeight);  
#ifdef USE_LOCAL_WIDGETS
    NS_NewTextWidget(&mSiteIndexTxt);
#else
    nsComponentManager::CreateInstance(kTextFieldCID, nsnull, kITextWidgetIID, (void**)&mSiteIndexTxt);
#endif
    NS_CreateTextWidget(mSiteDialog,mSiteIndexTxt,rect,HandleSiteEvent,&font);
    if (mVerDirTxt && NS_OK == mSiteIndexTxt->QueryInterface(kIWidgetIID,(void**)&widget)) {
      widget->SetBackgroundColor(textBGColor);
      widget->SetForegroundColor(textFGColor);
    }
    NS_IF_RELEASE(lookAndFeel);

    nsString str;
    str.AppendInt(0);
    PRUint32 size;
    mSiteIndexTxt->SetText(str,size);

    x += spacing + w;
    w = 100;
    // Create Jump Button
    rect.SetRect(x, y, w, 24);  
#ifdef USE_LOCAL_WIDGETS
    NS_NewButton(&mSiteJumpBtn);
#else
    nsComponentManager::CreateInstance(kButtonCID, nsnull, kIButtonIID, (void**)&mSiteJumpBtn);
#endif
    NS_CreateButton(mSiteDialog,mSiteJumpBtn,rect,HandleSiteEvent,&font);
    mSiteJumpBtn->SetLabel(NS_ConvertASCIItoUCS2("Jump to Index"));
  }


  NS_ShowWidget(mSiteDialog,PR_TRUE);
  NS_SetFocusToWidget(mSiteNextBtn);

  // Init
  NS_EnableWidget(mSitePrevBtn,PR_FALSE);
 
  if (gWinData) {
    nsString urlStr; urlStr.AssignWithConversion(gTop100List[gTop100Pointer]);
    gWinData->GoTo(urlStr.get());
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
  }
  CreateSiteDialog(aWindow->mWindow);
  return NS_OK;
}

//----------------------------------------


#if defined(XP_PC) && !defined(XP_OS2)
#include "jsconsres.h"

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
      
      nsIScriptContext *context = nsnull;
      nsCOMPtr<nsIScriptGlobalObject> scriptGlobal(do_GetInterface(aWindow->mDocShell));
      if (scriptGlobal) {       
        if ((NS_OK == scriptGlobal->GetContext(&context)) && context) {

          // create the console
          gConsole = JSConsole::CreateConsole();
          gConsole->SetContext(context);
          // lifetime of the context is still unclear at this point.
          // Anyway, as long as the web widget is alive the context is alive.
          // Maybe the context shouldn't even be RefCounted
          context->Release();
          gConsole->SetNotification(DestroyConsole);
        }
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
#if defined(XP_PC) && !defined(XP_OS2)
  if (nsnull == gConsole) {
    ShowConsole(aWindow);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsViewerApp::DoPrefs(nsBrowserWindow* aWindow)
{
#if defined(XP_PC) && defined(NGPREFS) && !defined(XP_OS2)

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
