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

#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsWidgetsCID.h"
#include "nsIAppShell.h"
#include "nsIPref.h"
#include "nsINetService.h"
#include "nsRepository.h"
#include "nsWebCrawler.h"
#include "prprf.h"
#include "plstr.h"
#include "prenv.h"

#ifdef XP_PC
#include "JSConsole.h"
#endif

extern nsresult NS_NewBrowserWindowFactory(nsIFactory** aFactory);
extern "C" void NS_SetupRegistry();

static NS_DEFINE_IID(kAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_IID(kBrowserWindowCID, NS_BROWSER_WINDOW_CID);

static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
static NS_DEFINE_IID(kIBrowserWindowIID, NS_IBROWSER_WINDOW_IID);
static NS_DEFINE_IID(kINetContainerApplicationIID,
                     NS_INETCONTAINERAPPLICATION_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsViewerApp::nsViewerApp()
{
  char * text = PR_GetEnv("NGLAYOUT_HOME");
  mStartURL = text ? text : "resource:/res/samples/test0.html";
  mDelay = 1;
  mRepeatCount = 1;
  mNumSamples = 10;
  mAllowPlugins = PR_TRUE;
}

nsViewerApp::~nsViewerApp()
{
  Destroy();
  if (nsnull != mPrefs) {
    mPrefs->Shutdown();
    NS_RELEASE(mPrefs);
  }
}

NS_IMPL_ADDREF(nsViewerApp)
NS_IMPL_RELEASE(nsViewerApp)

nsresult
nsViewerApp::QueryInterface(REFNSIID aIID, void** aInstancePtrResult) 
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kINetContainerApplicationIID)) {
    *aInstancePtrResult = (void*) ((nsIBrowserWindow*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)((nsIBrowserWindow*)this));
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

void
nsViewerApp::Destroy()
{
  NS_IF_RELEASE(mCrawler);
}

nsresult
nsViewerApp::SetupRegistry()
{
  NS_SetupRegistry();

  // Register our browser window factory
  nsIFactory* bwf;
  NS_NewBrowserWindowFactory(&bwf);
  NSRepository::RegisterFactory(kBrowserWindowCID, bwf, PR_FALSE);

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

  // Create widget application shell
  rv = NSRepository::CreateInstance(kAppShellCID, nsnull, kIAppShellIID,
                                    (void**)&mAppShell);
  if (NS_OK != rv) {
    return rv;
  }
  mAppShell->Create(&argc, argv);
  mAppShell->SetDispatchListener((nsDispatchListener*) this);

  // Load preferences
  rv = NSRepository::CreateInstance(kPrefCID, NULL, kIPrefIID,
                                    (void **) &mPrefs);
  if (NS_OK != rv) {
    return rv;
  }
  mPrefs->Startup("prefs.js");

  // Setup networking library
  rv = NS_InitINetService((nsINetContainerApplication*) this);
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

  // Finally process our arguments
  rv = ProcessArguments(argc, argv);

  return rv;
}

nsresult
nsViewerApp::Exit()
{
  Destroy();
  mAppShell->Exit();
  NS_RELEASE(mAppShell);
  return NS_OK;
}

static void
PrintHelpInfo(char **argv)
{
  fprintf(stderr, "Usage: %s [-p][-q][-md #][-f filename][-d #][-np] [starting url]\n", argv[0]);
  fprintf(stderr, "\t-p[#]   -- run purify, optionally with a # that says which sample to stop at.  For example, -p2 says to run samples 0, 1, and 2.\n");
  fprintf(stderr, "\t-q   -- run quantify\n");
  fprintf(stderr, "\t-md # -- set the crt debug flags to #\n");
  fprintf(stderr, "\t-d # -- set the delay between URL loads to # (in milliseconds)\n");
  fprintf(stderr, "\t-r # -- set the repeat count, which is the number of times the URLs will be loaded in batch mode.\n");
  fprintf(stderr, "\t-f filename -- read a list of URLs from <filename>\n");
  fprintf(stderr, "\t-C -- enable crawler\n");
  fprintf(stderr, "\t-R filename -- record pages visited in <filename>\n");
  fprintf(stderr, "\t-S domain -- add a domain/host that is safe to crawl (e.g. www.netscape.com)\n");
  fprintf(stderr, "\t-A domain -- add a domain/host that should be avoided (e.g. microsoft.com)\n");
  fprintf(stderr, "\t-N pages -- set the max # of pages to crawl\n");
  fprintf(stderr, "\t-np -- no plugins\n");
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
  mCrawler->AddRef();

  int i;
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
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
  nsresult rv = NSRepository::CreateInstance(kBrowserWindowCID, nsnull,
                                             kIBrowserWindowIID,
                                             (void**) &bw);
  bw->SetApp(this);
  bw->Init(mAppShell, mPrefs, nsRect(0, 0, 620, 400), PRUint32(~0), mAllowPlugins);
  bw->Show();
  mCrawler->SetBrowserWindow(bw);

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
    bw->LoadURL(mStartURL);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsViewerApp::OpenWindow(PRUint32 aNewChromeMask, nsIBrowserWindow*& aNewWindow)
{
  // Create browser window
  nsBrowserWindow* bw = nsnull;
  nsresult rv = NSRepository::CreateInstance(kBrowserWindowCID, nsnull,
                                             kIBrowserWindowIID,
                                             (void**) &bw);
  bw->SetApp(this);
  bw->Init(mAppShell, mPrefs, nsRect(0, 0, 620, 400), aNewChromeMask, mAllowPlugins);

  aNewWindow = bw;
  NS_ADDREF(bw);

  return NS_OK;
}

//----------------------------------------

// nsINetContainerApplication implementation

NS_IMETHODIMP    
nsViewerApp::GetAppCodeName(nsString& aAppCodeName)
{
  aAppCodeName.SetString("Mozilla");
  return NS_OK;
}
 
NS_IMETHODIMP
nsViewerApp::GetAppVersion(nsString& aAppVersion)
{
  aAppVersion.SetString("4.05 [en] (Windows;I)");
  return NS_OK;
}
 
NS_IMETHODIMP
nsViewerApp::GetAppName(nsString& aAppName)
{
  aAppName.SetString("Netscape");
  return NS_OK;
}
 
NS_IMETHODIMP
nsViewerApp::GetLanguage(nsString& aLanguage)
{
  aLanguage.SetString("en");
  return NS_OK;
}
 
NS_IMETHODIMP    
nsViewerApp::GetPlatform(nsString& aPlatform)
{
  aPlatform.SetString("Win32");
  return NS_OK;
}

//----------------------------------------

// nsDispatchListener implementation

void
nsViewerApp::AfterDispatch()
{
}

//----------------------------------------

#ifdef XP_PC
// XXX temporary robot code until it's made XP
#include "prenv.h"
#include "resources.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIURL.h"

#define DEBUG_EMPTY "(none)"
static int gDebugRobotLoads = 5000;
static char gVerifyDir[_MAX_PATH];
static BOOL gVisualDebug = TRUE;

extern JSConsole *gConsole;
extern HANDLE gInstance, gPrevInstance;

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
#endif

NS_IMETHODIMP
nsViewerApp::CreateRobot(nsBrowserWindow* aWindow)
{
#if defined(XP_PC) && defined(NS_DEBUG)
  if (CreateRobotDialog(aWindow->mWindow->GetNativeData(NS_NATIVE_WIDGET)))
  {
    nsIPresShell* shell = aWindow->GetPresShell();
    if (nsnull != shell) {
      nsIDocument* doc = shell->GetDocument();
      if (nsnull!=doc) {
        const char * str = doc->GetDocumentURL()->GetSpec();
        nsVoidArray * gWorkList = new nsVoidArray();
        gWorkList->AppendElement(new nsString(str));
        DebugRobot( 
          gWorkList, 
          gVisualDebug ? aWindow->mWebShell : nsnull, 
          gDebugRobotLoads, 
          PL_strdup(gVerifyDir),
          yieldProc);
      }
    }
  }
#endif
  return NS_OK;
}

//----------------------------------------

#ifdef XP_PC
static nsBrowserWindow* gWinData;
static int gTop100Pointer = 0;
static char * gTop100List[] = {
   "http://www.yahoo.com",
   "http://www.netscape.com",
   "http://www.microsoft.com",
   "http://www.excite.com",
   "http://www.mckinley.com",
   "http://www.city.net",
   "http://www.webcrawler.com",
   "http://www.mirabilis.com",
   "http://www.infoseek.com",
   "http://www.pathfinder.com",
   "http://www.warnerbros.com",
   "http://www.cnn.com",
   "http://www.altavista.digital.com",
   "http://www.altavista.com",
   "http://www.usatoday.com",
   "http://www.disney.com",
   "http://www.starwave.com",
   "http://www.hotwired.com",
   "http://www.hotbot.com",
   "http://www.lycos.com",
   "http://www.pointcom.com",
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
   "http://www.the-park.com",
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
   "http://www.iamginegames.com",
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
   "http://www.bluemountain.com",
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
   0
   };

// XXX temporary site walker code until it's made XP

BOOL CALLBACK
SiteWalkerDlgProc(HWND hDlg, UINT msg, WPARAM wParam,LPARAM lParam)
{
   BOOL translated = FALSE;
   switch (msg)
   {
      case WM_INITDIALOG:
         {
            SetDlgItemText(hDlg,IDC_SITE_NAME, gTop100List[gTop100Pointer]);
            EnableWindow(GetDlgItem(hDlg,ID_SITE_PREVIOUS),TRUE);
            if (gWinData)
               gWinData->LoadURL(nsString(gTop100List[gTop100Pointer]));
         }
         return FALSE;
      case WM_COMMAND:
         switch (LOWORD(wParam))
         {
            case ID_SITE_NEXT:
               {
                  char * p = gTop100List[++gTop100Pointer];
                  if (p) {
                     EnableWindow(GetDlgItem(hDlg,ID_SITE_NEXT),TRUE);
                     SetDlgItemText(hDlg,IDC_SITE_NAME, p);
                     if (gWinData)
                        gWinData->LoadURL(nsString(gTop100List[gTop100Pointer]));
                  }
                  else  {
                     EnableWindow(GetDlgItem(hDlg,ID_SITE_NEXT),FALSE);
                     EnableWindow(GetDlgItem(hDlg,ID_SITE_PREVIOUS),TRUE);
                     SetDlgItemText(hDlg,IDC_SITE_NAME, "[END OF LIST]");
                  }
               }
               break;
            case ID_SITE_PREVIOUS:
               {
                  if (gTop100Pointer > 0) {
                     EnableWindow(GetDlgItem(hDlg,ID_SITE_PREVIOUS),TRUE);
                     SetDlgItemText(hDlg,IDC_SITE_NAME, gTop100List[--gTop100Pointer]);
                     if (gWinData)
                        gWinData->LoadURL(nsString(gTop100List[gTop100Pointer]));
                  }
                  else  {
                     EnableWindow(GetDlgItem(hDlg,ID_SITE_PREVIOUS),FALSE);
                     EnableWindow(GetDlgItem(hDlg,ID_SITE_NEXT),TRUE);
                  }
               }
               break;
            case ID_EXIT:
               EndDialog(hDlg,IDCANCEL);
               NS_RELEASE(gWinData);
               break;
         }
         break;
      default:
         return FALSE;
   }
   return TRUE;
}

BOOL CreateSiteWalkerDialog(HWND hParent)
{
   BOOL result = (DialogBox(gInstance,MAKEINTRESOURCE(IDD_SITEWALKER),hParent,(DLGPROC)SiteWalkerDlgProc) == IDOK);
   return result;
}
#endif

NS_IMETHODIMP
nsViewerApp::CreateSiteWalker(nsBrowserWindow* aWindow)
{
#ifdef XP_PC
  if (nsnull == gWinData) {
    gWinData = aWindow;
    NS_ADDREF(aWindow);
    CreateSiteWalkerDialog(aWindow->mWindow->GetNativeData(NS_NATIVE_WIDGET));
  }
#endif
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
    HWND hWnd = aWindow->mWindow->GetNativeData(NS_NATIVE_WIDGET);
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
