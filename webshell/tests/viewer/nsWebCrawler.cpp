/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */
#include "nsCOMPtr.h"
#include "nsWebCrawler.h"
#include "nsViewerApp.h"
#include "nsIWebShell.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsIFrame.h"
#include "nsIFrameDebug.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsITimer.h"
#include "nsIAtom.h"
#include "nsIFrameUtil.h"
#include "nsIComponentManager.h"
#include "nsLayoutCID.h"
#include "nsRect.h"
#include "plhash.h"
#include "nsINameSpaceManager.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "prprf.h"
#include "nsIContentViewer.h"
#include "nsIContentViewerFile.h"
#include "nsIDocShell.h"
#include "nsIWebNavigation.h"

static NS_DEFINE_IID(kIDocumentLoaderObserverIID, NS_IDOCUMENTLOADEROBSERVER_IID);
static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kFrameUtilCID, NS_FRAME_UTIL_CID);
static NS_DEFINE_IID(kIFrameUtilIID, NS_IFRAME_UTIL_IID);
static NS_DEFINE_IID(kIXMLContentIID, NS_IXMLCONTENT_IID);

static PLHashNumber
HashKey(nsIAtom* key)
{
  return (PLHashNumber) key;
}

static PRIntn
CompareKeys(nsIAtom* key1, nsIAtom* key2)
{
  return key1 == key2;
}

class AtomHashTable {
public:
  AtomHashTable();
  ~AtomHashTable();

  const void* Get(nsIAtom* aKey);
  const void* Put(nsIAtom* aKey, const void* aValue);
  const void* Remove(nsIAtom* aKey);

protected:
  PLHashTable* mTable;
};

AtomHashTable::AtomHashTable()
{
  mTable = PL_NewHashTable(8, (PLHashFunction) HashKey,
                           (PLHashComparator) CompareKeys,
                           (PLHashComparator) nsnull,
                           nsnull, nsnull);
}

static PRIntn PR_CALLBACK
DestroyEntry(PLHashEntry *he, PRIntn i, void *arg)
{
  ((nsIAtom*)he->key)->Release();
  return HT_ENUMERATE_NEXT;
}

AtomHashTable::~AtomHashTable()
{
  PL_HashTableEnumerateEntries(mTable, DestroyEntry, 0);
  PL_HashTableDestroy(mTable);
}

/**
 * Get the data associated with a Atom.
 */
const void*
AtomHashTable::Get(nsIAtom* aKey)
{
  PRInt32 hashCode = (PRInt32) aKey;
  PLHashEntry** hep = PL_HashTableRawLookup(mTable, hashCode, aKey);
  PLHashEntry* he = *hep;
  if (nsnull != he) {
    return he->value;
  }
  return nsnull;
}

/**
 * Create an association between a Atom and some data. This call
 * returns an old association if there was one (or nsnull if there
 * wasn't).
 */
const void*
AtomHashTable::Put(nsIAtom* aKey, const void* aData)
{
  PRInt32 hashCode = (PRInt32) aKey;
  PLHashEntry** hep = PL_HashTableRawLookup(mTable, hashCode, aKey);
  PLHashEntry* he = *hep;
  if (nsnull != he) {
    const void* oldValue = he->value;
    he->value = NS_CONST_CAST(void*, aData);
    return oldValue;
  }
  NS_ADDREF(aKey);
  PL_HashTableRawAdd(mTable, hep, hashCode, aKey, NS_CONST_CAST(void*, aData));
  return nsnull;
}

/**
 * Remove an association between a Atom and it's data. This returns
 * the old associated data.
 */
const void*
AtomHashTable::Remove(nsIAtom* aKey)
{
  PRInt32 hashCode = (PRInt32) aKey;
  PLHashEntry** hep = PL_HashTableRawLookup(mTable, hashCode, aKey);
  PLHashEntry* he = *hep;
  void* oldValue = nsnull;
  if (nsnull != he) {
    oldValue = he->value;
    PL_HashTableRawRemove(mTable, hep, he);
  }
  return oldValue;
}

//----------------------------------------------------------------------

nsWebCrawler::nsWebCrawler(nsViewerApp* aViewer)
  : mHaveURLList(PR_FALSE),
    mQueuedLoadURLs(0)
{
  NS_INIT_REFCNT();

  mBrowser = nsnull;
  mViewer = aViewer;
  mCrawl = PR_FALSE;
  mJiggleLayout = PR_FALSE;
  mPostExit = PR_FALSE;
  mDelay = 0;
  mMaxPages = -1;
  mRecord = nsnull;
  mLinkTag = getter_AddRefs(NS_NewAtom("a"));
  mFrameTag = getter_AddRefs(NS_NewAtom("frame"));
  mIFrameTag = getter_AddRefs(NS_NewAtom("iframe"));
  mHrefAttr = getter_AddRefs(NS_NewAtom("href"));
  mSrcAttr = getter_AddRefs(NS_NewAtom("src"));
  mBaseHrefAttr = getter_AddRefs(NS_NewAtom("_base_href"));
  mVisited = new AtomHashTable();
  mVerbose = nsnull;
  LL_I2L(mStartLoad, 0);
  mRegressing = PR_FALSE;
  mPrinterTestType = 0;
}

static void FreeStrings(nsVoidArray& aArray)
{
  PRInt32 i, n = aArray.Count();
  for (i = 0; i < n; i++) {
    nsString* s = (nsString*) aArray.ElementAt(i);
    delete s;
  }
  aArray.Clear();
}

nsWebCrawler::~nsWebCrawler()
{
  FreeStrings(mSafeDomains);
  FreeStrings(mAvoidDomains);
  NS_IF_RELEASE(mBrowser);
  delete mVisited;
}

NS_IMPL_ISUPPORTS(nsWebCrawler, kIDocumentLoaderObserverIID)

NS_IMETHODIMP
nsWebCrawler::OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* aURL,
                                  const char* aCommand)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebCrawler::OnEndDocumentLoad(nsIDocumentLoader* loader,
                                nsIChannel* channel,
                                nsresult aStatus)
{
  nsresult rv;
  PRTime endLoadTime = PR_Now();

  if (loader != mDocLoader.get()) {
    // This notifications is not for the "main" document...
    return NS_OK;
  }

  if (NS_BINDING_ABORTED == aStatus) {
    //
    // Sometimes a Refresh will interrupt a document that is loading...
    // When this happens just ignore the ABORTED notification and wait
    // for the notification that the Refreshed document has finished..
    //
    return NS_OK;
  }

  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (nsnull == aURL) {
    return NS_OK;
  }

  // Ignore this notification unless its for the current url. That way
  // we skip over embedded webshell notifications (e.g. frame cells,
  // iframes, etc.)
  char* spec;
  aURL->GetSpec(&spec);
  if (!spec) {
    nsCRT::free(spec);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIURI> currentURL;
  rv = NS_NewURI(getter_AddRefs(currentURL), mCurrentURL);
  if (NS_FAILED(rv)) {
    nsCRT::free(spec);
    return rv;
  }
  char* spec2;
  currentURL->GetSpec(&spec2);
  if (!spec2) {
    nsCRT::free(spec);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (PL_strcmp(spec, spec2)) {
    nsCRT::free(spec);
    nsCRT::free(spec2);
    return NS_OK;
  }
  nsCRT::free(spec2);

  char buf[400];
  PRTime delta, cvt, rounder;
  LL_I2L(cvt, 1000);
  LL_I2L(rounder, 499);
  LL_SUB(delta, endLoadTime, mStartLoad);
  LL_ADD(delta, delta, rounder);
  LL_DIV(delta, delta, cvt);
  PR_snprintf(buf, sizeof(buf), "%s: done loading (%lld msec)",
              spec, delta);
  printf("%s\n", buf);
  nsCRT::free(spec);

  // Make sure the document bits make it to the screen at least once
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    nsCOMPtr<nsIViewManager> vm;
    shell->GetViewManager(getter_AddRefs(vm));
    if (vm) {
      nsIView* rootView;
      vm->GetRootView(rootView);
      vm->UpdateView(rootView, NS_VMREFRESH_IMMEDIATE);
    }

#ifdef NS_DEBUG
    if (mOutputDir.Length() > 0) {
      if ( mPrinterTestType > 0 ) {
        nsCOMPtr <nsIContentViewer> viewer;
        nsIWebShell* webshell = nsnull;
        mBrowser->GetWebShell(webshell);

        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webshell));
        docShell->GetContentViewer(getter_AddRefs(viewer));

        if (viewer){
          nsCOMPtr<nsIContentViewerFile> viewerFile = do_QueryInterface(viewer);
          if (viewerFile) {
            nsAutoString regressionFileName;
            FILE *fp = GetOutputFile(aURL, regressionFileName);
            
            switch (mPrinterTestType) {
              case 1:
                // dump print data to a file for regression testing
                viewerFile->Print(PR_TRUE,fp);
                break;
              case 2:
                // visual printing tests, all go to the printer, no printer dialog
                viewerFile->Print(PR_TRUE,0);
                break;
              case 3:
                // visual printing tests, all go to the printer, with a printer dialog
                viewerFile->Print(PR_FALSE,0);
                break;
              default:
                break;
            }
          fclose(fp);
          }
        }
      } else {
        nsIFrame* root;
        shell->GetRootFrame(&root);
        if (nsnull != root) {
          nsCOMPtr<nsIPresContext> presContext;
          shell->GetPresContext(getter_AddRefs(presContext));
        
          if (mOutputDir.Length() > 0)
          {
            nsAutoString regressionFileName;
            FILE *fp = GetOutputFile(aURL, regressionFileName);
            if (fp) {
              nsIFrameDebug* fdbg;
              if (NS_SUCCEEDED(root->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**) &fdbg))) {
                fdbg->DumpRegressionData(presContext, fp, 0);
              }
              fclose(fp);
              if (mRegressing) {
                PerformRegressionTest(regressionFileName);
              }
              else {
                fputs(regressionFileName, stdout);
                printf(" - being written\n");
              }
            }
            else {
              char* file;
              (void)aURL->GetPath(&file);
              printf("could not open output file for %s\n", file);
              nsCRT::free(file);
            }
          }
          else {
            nsIFrameDebug* fdbg;
            if (NS_SUCCEEDED(root->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**) &fdbg))) {
              fdbg->DumpRegressionData(presContext, stdout, 0);
            }
          }
        }
      }
    }
#endif

    if (mJiggleLayout) {
      nsRect r;
      mBrowser->GetContentBounds(r);
      nscoord oldWidth = r.width;
      while (r.width > 100) {
        r.width -= 10;
        mBrowser->SizeWindowTo(r.width, r.height, PR_FALSE, PR_FALSE);
      }
      while (r.width < oldWidth) {
        r.width += 10;
        mBrowser->SizeWindowTo(r.width, r.height, PR_FALSE, PR_FALSE);
      }
    }

    if (mCrawl) {
      FindMoreURLs();
    }

    if (0 == mDelay) {
      LoadNextURL(PR_TRUE);
    }
    NS_RELEASE(shell);
  }
  else {
    fputs("null pres shell\n", stdout);
  }

  if (mPostExit && (0 == mQueuedLoadURLs) && (0==mPendingURLs.Count())) {
    QueueExit();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebCrawler::OnStartURLLoad(nsIDocumentLoader* loader,
                             nsIChannel* channel)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebCrawler::OnProgressURLLoad(nsIDocumentLoader* loader,
                                nsIChannel* channel,
                                PRUint32 aProgress, 
                                PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebCrawler::OnStatusURLLoad(nsIDocumentLoader* loader,
                              nsIChannel* channel, 
                              nsString& aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebCrawler::OnEndURLLoad(nsIDocumentLoader* loader, nsIChannel* channel,
                           nsresult aStatus)
{
  return NS_OK;
}

FILE*
nsWebCrawler::GetOutputFile(nsIURI *aURL, nsString& aOutputName)
{
  static const char kDefaultOutputFileName[] = "test.txt";   // the default
  FILE *result = nsnull;
  if (nsnull!=aURL)
  {
    char *inputFileName;
    char* file;
    (void)aURL->GetPath(&file);
    nsAutoString inputFileFullPath; inputFileFullPath.AssignWithConversion(file);
    nsCRT::free(file);
    PRInt32 fileNameOffset = inputFileFullPath.RFindChar('/');
    if (-1==fileNameOffset)
    {
      inputFileName = new char[strlen(kDefaultOutputFileName) + 1];
      strcpy (inputFileName, kDefaultOutputFileName);
    }
    else
    {
      PRInt32 len = inputFileFullPath.Length() - fileNameOffset;
      inputFileName = new char[len + 1 + 20];
      char *c = inputFileName;
      for (PRInt32 i=fileNameOffset+1; i<fileNameOffset+len; i++)
      {
        char ch = (char) inputFileFullPath.CharAt(i);
        if (ch == '.') {
          // Stop on dot so that we don't keep the old extension
          break;
        }
        *c++ = ch;
      }

      // Tack on ".rgd" extension for "regression data"
      *c++ = '.';
      *c++ = 'r';
      *c++ = 'g';
      *c++ = 'd';
      *c++ = '\0';
      aOutputName.Truncate();
      aOutputName.AppendWithConversion(inputFileName);
    }
    nsAutoString outputFileName(mOutputDir);
    outputFileName.AppendWithConversion(inputFileName);
    PRInt32 bufLen = outputFileName.Length()+1;
    char *buf = new char[bufLen+1];
    outputFileName.ToCString(buf, bufLen);
    result = fopen(buf, "wt");
    delete [] buf;
    delete [] inputFileName;
  }
  return result;
}

void
nsWebCrawler::AddURL(const nsString& aURL)
{
  nsString* s = new nsString(aURL);
  mPendingURLs.AppendElement(s);
  if (mVerbose) {
    printf("WebCrawler: adding '");
    fputs(aURL, stdout);
    printf("'\n");
  }
}

void
nsWebCrawler::AddSafeDomain(const nsString& aDomain)
{
  nsString* s = new nsString(aDomain);
  mSafeDomains.AppendElement(s);
}

void
nsWebCrawler::AddAvoidDomain(const nsString& aDomain)
{
  nsString* s = new nsString(aDomain);
  mAvoidDomains.AppendElement(s);
}

void 
nsWebCrawler::SetOutputDir(const nsString& aOutputDir)
{
  mOutputDir = aOutputDir;
}

void 
nsWebCrawler::SetRegressionDir(const nsString& aDir)
{
  mRegressionDir = aDir;
}

void
nsWebCrawler::Start()
{
  // Enable observing each URL load...
  nsIWebShell* shell = nsnull;
  mBrowser->GetWebShell(shell);
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(shell));
  docShell->SetDocLoaderObserver(this);
  shell->GetDocumentLoader(*getter_AddRefs(mDocLoader));
  NS_RELEASE(shell);
  if (mPendingURLs.Count() > 1) {
    mHaveURLList = PR_TRUE;
  }
  LoadNextURL(PR_FALSE);
}

void
nsWebCrawler::EnableCrawler()
{
  mCrawl = PR_TRUE;
}

static const unsigned char kLowerLookup[256] = {
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
  16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
  32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
  48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
  64,
    97,98,99,100,101,102,103,104,105,106,107,108,109,
    110,111,112,113,114,115,116,117,118,119,120,121,122,

   91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,

  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

static PRBool
EndsWith(const nsString& aDomain, const char* aHost, PRInt32 aHostLen)
{
  PRInt32 slen = aDomain.Length();
  if (slen < aHostLen) {
    return PR_FALSE;
  }
  const PRUnichar* uc = aDomain.GetUnicode();
  uc += slen - aHostLen;
  const PRUnichar* end = uc + aHostLen;
  while (uc < end) {
    unsigned char uch = (unsigned char) ((*uc++) & 0xff);
    unsigned char ch = (unsigned char) ((*aHost++) & 0xff);
    if (kLowerLookup[uch] != kLowerLookup[ch]) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

static PRBool
StartsWith(const nsString& s1, const char* s2)
{
  PRInt32 s1len = s1.Length();
  PRInt32 s2len = strlen(s2);
  if (s1len < s2len) {
    return PR_FALSE;
  }
  const PRUnichar* uc = s1.GetUnicode();
  const PRUnichar* end = uc + s2len;
  while (uc < end) {
    unsigned char uch = (unsigned char) ((*uc++) & 0xff);
    unsigned char ch = (unsigned char) ((*s2++) & 0xff);
    if (kLowerLookup[uch] != kLowerLookup[ch]) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

PRBool
nsWebCrawler::OkToLoad(const nsString& aURLSpec)
{
  if (!StartsWith(aURLSpec, "http:") && !StartsWith(aURLSpec, "ftp:") &&
      !StartsWith(aURLSpec, "file:") &&
      !StartsWith(aURLSpec, "resource:")) {
    return PR_FALSE;
  }

  PRBool ok = PR_TRUE;
  nsIURI* url;
  nsresult rv;
  rv = NS_NewURI(&url, aURLSpec);

  if (NS_OK == rv) {
    nsXPIDLCString host;
    rv = url->GetHost(getter_Copies(host));
    if (rv == NS_OK) {
      PRInt32 hostlen = PL_strlen(host);

      // Check domains to avoid
      PRInt32 i, n = mAvoidDomains.Count();
      for (i = 0; i < n; i++) {
        nsString* s = (nsString*) mAvoidDomains.ElementAt(i);
        if (s && EndsWith(*s, host, hostlen)) {
          printf("Avoiding '");
          fputs(aURLSpec, stdout);
          printf("'\n");
          return PR_FALSE;
        }
      }

      // Check domains to stay within
      n = mSafeDomains.Count();
      if (n == 0) {
        // If we don't care then all the domains that we aren't
        // avoiding are OK
        return PR_TRUE;
      }
      for (i = 0; i < n; i++) {
        nsString* s = (nsString*) mSafeDomains.ElementAt(i);
        if (s && EndsWith(*s, host, hostlen)) {
          return PR_TRUE;
        }
      }
      ok = PR_FALSE;
    }
    NS_RELEASE(url);
  }
  return ok;
}

void
nsWebCrawler::RecordLoadedURL(const nsString& aURL)
{
  if (nsnull != mRecord) {
    fputs(aURL, mRecord);
    fputs("\n", mRecord);
    fflush(mRecord);
  }
}

void
nsWebCrawler::FindURLsIn(nsIDocument* aDocument, nsIContent* aNode)
{
  nsCOMPtr<nsIAtom> atom;
  aNode->GetTag(*getter_AddRefs(atom));
  if ((atom == mLinkTag) || (atom == mFrameTag) || (atom == mIFrameTag)) {
    // Get absolute url that tag targets
    nsAutoString base, src, absURLSpec;
    if (atom == mLinkTag) {
      aNode->GetAttribute(kNameSpaceID_HTML, mHrefAttr, src);
    }
    else {
      aNode->GetAttribute(kNameSpaceID_HTML, mSrcAttr, src);
    }
    nsIURI* docURL = aDocument->GetDocumentURL();
    nsresult rv;
    rv = NS_MakeAbsoluteURI(absURLSpec, src, docURL);
    if (NS_OK == rv) {
      nsCOMPtr<nsIAtom> urlAtom = getter_AddRefs(NS_NewAtom(absURLSpec));
      if (0 == mVisited->Get(urlAtom)) {
        // Remember the URL as visited so that we don't go there again
        mVisited->Put(urlAtom, "visited");
        if (OkToLoad(absURLSpec)) {
          mPendingURLs.AppendElement(new nsString(absURLSpec));
          if (mVerbose) {
            printf("Adding '");
            fputs(absURLSpec, stdout);
            printf("'\n");
          }
        }
        else {
          if (mVerbose) {
            printf("Skipping '");
            fputs(absURLSpec, stdout);
            printf("'\n");
          }
        }
      }
      else {
        if (mVerbose) {
          printf("Already visited '");
          fputs(absURLSpec, stdout);
          printf("'\n");
        }
      }
    }
    NS_RELEASE(docURL);
  }

  PRBool canHaveKids;
  aNode->CanContainChildren(canHaveKids);
  if (canHaveKids) {
    PRInt32 i, n;
    aNode->ChildCount(n);
    for (i = 0; i < n; i++) {
      nsIContent* kid;
      aNode->ChildAt(i, kid);
      if (nsnull != kid) {
        FindURLsIn(aDocument, kid);
        NS_RELEASE(kid);
      }
    }
  }
}

void
nsWebCrawler::FindMoreURLs()
{
  nsIWebShell* shell = nsnull;
  mBrowser->GetWebShell(shell);
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(shell));
  if (docShell) {
    nsIContentViewer* cv = nsnull;
    docShell->GetContentViewer(&cv);
    if (nsnull != cv) {
      nsIDocumentViewer* docv = nsnull;
      cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
      if (nsnull != docv) {
        nsIDocument* doc = nsnull;
        docv->GetDocument(doc);
        if (nsnull != doc) {
          nsIContent* root;
          root = doc->GetRootContent();
          if (nsnull != root) {
            FindURLsIn(doc, root);
            NS_RELEASE(root);
          }
          NS_RELEASE(doc);
        }
        NS_RELEASE(docv);
      }
      NS_RELEASE(cv);
    }
    NS_RELEASE(shell);
  }
}

void 
nsWebCrawler::SetBrowserWindow(nsBrowserWindow* aWindow) 
{
  NS_IF_RELEASE(mBrowser);
  mBrowser = aWindow;
  NS_IF_ADDREF(mBrowser);
}

void
nsWebCrawler::GetBrowserWindow(nsBrowserWindow** aWindow)
{
  NS_IF_ADDREF(mBrowser);
  *aWindow = mBrowser;
}

static void
TimerCallBack(nsITimer *aTimer, void *aClosure)
{
  nsWebCrawler* wc = (nsWebCrawler*) aClosure;
  wc->LoadNextURL(PR_TRUE);
}

void
nsWebCrawler::LoadNextURL(PRBool aQueueLoad)
{
  if (0 != mDelay) {
    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    mTimer->Init(TimerCallBack, (void *)this, mDelay * 1000);
  }

  if ((mMaxPages < 0) || (mMaxPages > 0)) {
    while (0 != mPendingURLs.Count()) {
      nsString* url = (nsString*) mPendingURLs.ElementAt(0);
      mPendingURLs.RemoveElementAt(0);
      if (nsnull != url) {
        if (OkToLoad(*url)) {
          RecordLoadedURL(*url);
          nsIWebShell* webShell;
          mBrowser->GetWebShell(webShell);
          if (aQueueLoad) {
            // Call stop to cancel any pending URL Refreshes...
///            webShell->Stop();
            QueueLoadURL(*url);
          }
          else {
            mCurrentURL = *url;
            mStartLoad = PR_Now();
            nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(webShell));
            webNav->LoadURI(url->GetUnicode());
          }
          NS_RELEASE(webShell);

          if (mMaxPages > 0) {
            --mMaxPages;
          }
          delete url;
          return;
        }
        delete url;
      }
    }
  }

  if (nsnull != mRecord) {
    fclose(mRecord);
    mRecord = nsnull;
  }

} 

nsIPresShell*
nsWebCrawler::GetPresShell()
{
  nsIWebShell* webShell;
  mBrowser->GetWebShell(webShell);
  nsIPresShell* shell = nsnull;
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webShell));
  if (nsnull != webShell) {
    nsIContentViewer* cv = nsnull;
    docShell->GetContentViewer(&cv);
    if (nsnull != cv) {
      nsIDocumentViewer* docv = nsnull;
      cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
      if (nsnull != docv) {
        nsIPresContext* cx;
        docv->GetPresContext(cx);
        if (nsnull != cx) {
          cx->GetShell(&shell);
          NS_RELEASE(cx);
        }
        NS_RELEASE(docv);
      }
      NS_RELEASE(cv);
    }
    NS_RELEASE(webShell);
  }
  return shell;
}

static FILE*
OpenRegressionFile(const nsString& aBaseName, const nsString& aOutputName)
{
  nsAutoString a;
  a.Append(aBaseName);
  a.AppendWithConversion("/");
  a.Append(aOutputName);
  char* fn = a.ToNewCString();
  FILE* fp = fopen(fn, "r");
  if (!fp) {
    printf("Unable to open regression data file %s\n", fn);
  }
  delete[] fn;
  return fp;
}

#define BUF_SIZE 1024
// Load up both data files (original and the one we just output) into
// two independent xml content trees. Then compare them.
void
nsWebCrawler::PerformRegressionTest(const nsString& aOutputName)
{
  // First load the trees
  nsIFrameUtil* fu;
  nsresult rv = nsComponentManager::CreateInstance(kFrameUtilCID, nsnull,
                                             kIFrameUtilIID, (void **)&fu);
  if (NS_FAILED(rv)) {
    printf("Can't find nsIFrameUtil implementation\n");
    return;
  }
  FILE* f1 = OpenRegressionFile(mRegressionDir, aOutputName);
  if (!f1) {
    NS_RELEASE(fu);
    return;
  }
  FILE* f2 = OpenRegressionFile(mOutputDir, aOutputName);
  if (!f2) {
    fclose(f1);
    NS_RELEASE(fu);
    return;
  }
  rv = fu->CompareRegressionData(f1, f2);
  NS_RELEASE(fu);

  char dirName[BUF_SIZE];
  char fileName[BUF_SIZE];
  mOutputDir.ToCString(dirName, BUF_SIZE-1);
  aOutputName.ToCString(fileName, BUF_SIZE-1);

  printf("regression test %s%s %s\n", dirName, fileName, NS_SUCCEEDED(rv) ? "passed" : "failed");
}

//----------------------------------------------------------------------

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

static nsresult
QueueEvent(PLEvent* aEvent)
{
  nsISupports* is;
  nsresult rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                             kIEventQueueServiceIID,
                                             &is,
                                             nsnull);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIEventQueueService> eqs = do_QueryInterface(is);
  if (eqs) {
    nsCOMPtr<nsIEventQueue> eq;
    rv = eqs->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eq));
    if (eq) {
      eq->PostEvent(aEvent);
    }
  }

  nsServiceManager::ReleaseService(kEventQueueServiceCID, is, nsnull);
  return rv;
}

//----------------------------------------------------------------------

struct ExitEvent : public PLEvent {
  ExitEvent(nsWebCrawler* aCrawler);
  ~ExitEvent();

  void DoIt() {
    crawler->Exit();
  }

  nsWebCrawler* crawler;

  static void PR_CALLBACK HandleMe(ExitEvent* e);
  static void PR_CALLBACK DeleteMe(ExitEvent* e);
};

ExitEvent::ExitEvent(nsWebCrawler* aCrawler)
  : crawler(aCrawler)
{
  PL_InitEvent(this, crawler, (PLHandleEventProc) HandleMe,
               (PLDestroyEventProc) DeleteMe);
  NS_ADDREF(aCrawler);
}

ExitEvent::~ExitEvent()
{
  NS_RELEASE(crawler);
}

void
ExitEvent::HandleMe(ExitEvent* e)
{
  e->DoIt();
}

void
ExitEvent::DeleteMe(ExitEvent* e)
{
  delete e;
}

void
nsWebCrawler::QueueExit()
{
  ExitEvent* event = new ExitEvent(this);
  QueueEvent(event);
}

void
nsWebCrawler::Exit()
{
  mViewer->Exit();
}

//----------------------------------------------------------------------

struct LoadEvent : public PLEvent {
  LoadEvent(nsWebCrawler* aCrawler, const nsString& aURL);
  ~LoadEvent();

  void DoIt() {
    crawler->GoToQueuedURL(url);
  }

  nsString url;
  nsWebCrawler* crawler;

  static void PR_CALLBACK HandleMe(LoadEvent* e);
  static void PR_CALLBACK DeleteMe(LoadEvent* e);
};

LoadEvent::LoadEvent(nsWebCrawler* aCrawler, const nsString& aURL)
  : url(aURL),
    crawler(aCrawler)
{
  PL_InitEvent(this, crawler, (PLHandleEventProc) HandleMe,
               (PLDestroyEventProc) DeleteMe);
  NS_ADDREF(aCrawler);
}

LoadEvent::~LoadEvent()
{
  NS_RELEASE(crawler);
}

void
LoadEvent::HandleMe(LoadEvent* e)
{
  e->DoIt();
}

void
LoadEvent::DeleteMe(LoadEvent* e)
{
  delete e;
}

void
nsWebCrawler::GoToQueuedURL(const nsString& aURL)
{
  nsIWebShell* webShell;
  mBrowser->GetWebShell(webShell);
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(webShell));
  if (webNav) {
    mCurrentURL = aURL;
    mStartLoad = PR_Now();
    webNav->LoadURI(aURL.GetUnicode());
    NS_RELEASE(webShell);
  }
  mQueuedLoadURLs--;

}

nsresult
nsWebCrawler::QueueLoadURL(const nsString& aURL)
{
  LoadEvent* event = new LoadEvent(this, aURL);
  nsresult rv = QueueEvent(event);
  if (NS_SUCCEEDED(rv)) {
    mQueuedLoadURLs++;
  }
  return rv;
}
