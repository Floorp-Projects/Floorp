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
#include "nscore.h"
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
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "prprf.h"
#include "nsIContentViewer.h"
#include "nsIContentViewerFile.h"
#include "nsIDocShell.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"

static NS_DEFINE_IID(kFrameUtilCID, NS_FRAME_UTIL_CID);
static NS_DEFINE_IID(kIFrameUtilIID, NS_IFRAME_UTIL_IID);
static NS_DEFINE_IID(kIXMLContentIID, NS_IXMLCONTENT_IID);

static PLHashNumber
HashKey(nsIAtom* key)
{
  return NS_PTR_TO_INT32(key);
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
  PRInt32 hashCode = NS_PTR_TO_INT32(aKey);
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
  PRInt32 hashCode = NS_PTR_TO_INT32(aKey);
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
  PRInt32 hashCode = NS_PTR_TO_INT32(aKey);
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
  mDelay = 200 /*msec*/; // XXXwaterson straigt outta my arse
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
  mRegressionOutputLevel = 0;     // full output
  mIncludeStyleInfo = PR_TRUE;
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

NS_IMPL_ISUPPORTS2(nsWebCrawler, 
                   nsIWebProgressListener,
                   nsISupportsWeakReference)

void
nsWebCrawler::DumpRegressionData()
{
#ifdef NS_DEBUG
  nsCOMPtr<nsIWebShell> webshell;
  mBrowser->GetWebShell(*getter_AddRefs(webshell));
  if (! webshell)
    return;

  if (mOutputDir.Length() > 0) {
    nsIPresShell* shell = GetPresShell(webshell);
    if (!shell) return;
    if ( mPrinterTestType > 0 ) {
      nsCOMPtr <nsIContentViewer> viewer;
      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webshell));
      docShell->GetContentViewer(getter_AddRefs(viewer));

      if (viewer){
        nsCOMPtr<nsIContentViewerFile> viewerFile = do_QueryInterface(viewer);
        if (viewerFile) {
          nsAutoString regressionFileName;
          FILE *fp = GetOutputFile(mLastURL, regressionFileName);
            
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
    } 
    else {
      nsIFrame* root;
      shell->GetRootFrame(&root);
      if (nsnull != root) {
        nsCOMPtr<nsIPresContext> presContext;
        shell->GetPresContext(getter_AddRefs(presContext));
        
        if (mOutputDir.Length() > 0) {
          nsAutoString regressionFileName;
          FILE *fp = GetOutputFile(mLastURL, regressionFileName);
          if (fp) {
            nsIFrameDebug* fdbg;
            if (NS_SUCCEEDED(root->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**) &fdbg))) {
              fdbg->DumpRegressionData(presContext, fp, 0, mIncludeStyleInfo);
            }
            fclose(fp);
            if (mRegressing) {
              PerformRegressionTest(regressionFileName);
            }
            else {
              fputs(NS_LossyConvertUCS2toASCII(regressionFileName).get(),
                    stdout);
              printf(" - being written\n");
            }
          }
          else {
            char* file;
            (void)mLastURL->GetPath(&file);
            printf("could not open output file for %s\n", file);
            nsCRT::free(file);
          }
        }
        else {
          nsIFrameDebug* fdbg;
          if (NS_SUCCEEDED(root->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**) &fdbg))) {
            fdbg->DumpRegressionData(presContext, stdout, 0, mIncludeStyleInfo);
          }
        }
      }
    }
    NS_RELEASE(shell);
  }
#endif
}

void
nsWebCrawler::LoadNextURLCallback(nsITimer *aTimer, void *aClosure)
{
  nsWebCrawler* self = (nsWebCrawler*) aClosure;

  // if we are doing printing regression tests, check to see 
  // if we can print (a previous job is not printing)
  if (self->mPrinterTestType > 0) {
    nsCOMPtr<nsIWebShell> webshell;
    self->mBrowser->GetWebShell(*getter_AddRefs(webshell));
    if (webshell){
      nsCOMPtr <nsIContentViewer> viewer;
      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webshell));
      docShell->GetContentViewer(getter_AddRefs(viewer));
      if (viewer){
        nsCOMPtr<nsIContentViewerFile> viewerFile = do_QueryInterface(viewer);
        if (viewerFile) {
          PRBool printable;
          viewerFile->GetPrintable(&printable);
          if (PR_TRUE !=printable){
            self->mTimer = do_CreateInstance("@mozilla.org/timer;1");
            self->mTimer->Init(LoadNextURLCallback, self, self->mDelay);
            return;
          }
        }
      }
    }
  }

  self->DumpRegressionData();
  self->LoadNextURL(PR_FALSE);
}

void
nsWebCrawler::QueueExitCallback(nsITimer *aTimer, void *aClosure)
{
  nsWebCrawler* self = (nsWebCrawler*) aClosure;
  self->DumpRegressionData();
  self->QueueExit();
}

// nsIWebProgressListener implementation 
NS_IMETHODIMP
nsWebCrawler::OnStateChange(nsIWebProgress* aWebProgress, 
                            nsIRequest* aRequest, 
                            PRInt32 progressStateFlags, 
                            nsresult aStatus)
{
  // Make sure that we're being notified for _our_ shell, and not some
  // subshell that's been created e.g. for an IFRAME.
  nsCOMPtr<nsIWebShell> shell;
  mBrowser->GetWebShell(*getter_AddRefs(shell));
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(shell);
  if (docShell) {
    nsCOMPtr<nsIWebProgress> progress = do_GetInterface(docShell);
    if (aWebProgress != progress)
      return NS_OK;
  }

  // Make sure that we're being notified for the whole document, not a
  // sub-load.
  if (! (progressStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT))
    return NS_OK;

  if (progressStateFlags & nsIWebProgressListener::STATE_START) {
    // If the document load is starting, remember its URL as the last
    // URL we've loaded.
    nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
    if (! channel) {
      NS_ERROR("no channel avail");
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));

    mLastURL = uri;
  }
  //XXXwaterson are these really _not_ mutually exclusive?
  // else
  if ((progressStateFlags & nsIWebProgressListener::STATE_STOP) && (aStatus == NS_OK)) {
    // If the document load is finishing, then wrap up and maybe load
    // some more URLs.
    nsresult rv;
    PRTime endLoadTime = PR_Now();

    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
    rv = channel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    // Ignore this notification unless its for the current url. That way
    // we skip over embedded webshell notifications (e.g. frame cells,
    // iframes, etc.)
    nsXPIDLCString spec;
    uri->GetSpec(getter_Copies(spec));

    PRTime delta, cvt, rounder;
    LL_I2L(cvt, 1000);
    LL_I2L(rounder, 499);
    LL_SUB(delta, endLoadTime, mStartLoad);
    LL_ADD(delta, delta, rounder);
    LL_DIV(delta, delta, cvt);
    printf("+++ %s: done loading (%lld msec)\n", spec.get(), delta);

    // Make sure the document bits make it to the screen at least once
    nsCOMPtr<nsIPresShell> shell = dont_AddRef(GetPresShell());
    if (shell) {
      // Force the presentation shell to flush any pending reflows
      shell->FlushPendingNotifications(PR_FALSE);

      // Force the view manager to update itself
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        nsIView* rootView;
        vm->GetRootView(rootView);
        vm->UpdateView(rootView, NS_VMREFRESH_IMMEDIATE);
      }

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
    }

    if (mCrawl) {
      FindMoreURLs();
    }

    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    if(mPrinterTestType>0){
      mDelay = 5000;     // printing needs more time to load, so give it plenty
    } else {
      mDelay = 200;
    }    
    
    if ((0 < mQueuedLoadURLs) || (0 < mPendingURLs.Count())) {
      mTimer->Init(LoadNextURLCallback, this, mDelay);
    }
    else if (mPostExit) {
      mTimer->Init(QueueExitCallback, this, mDelay);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebCrawler::OnProgressChange(nsIWebProgress *aWebProgress,
                               nsIRequest *aRequest,
                               PRInt32 aCurSelfProgress,
                               PRInt32 aMaxSelfProgress,
                               PRInt32 aCurTotalProgress,
                               PRInt32 aMaxTotalProgress) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebCrawler::OnLocationChange(nsIWebProgress* aWebProgress,
                               nsIRequest* aRequest,
                               nsIURI *location)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsWebCrawler::OnStatusChange(nsIWebProgress* aWebProgress,
                             nsIRequest* aRequest,
                             nsresult aStatus,
                             const PRUnichar* aMessage)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsWebCrawler::OnSecurityChange(nsIWebProgress *aWebProgress, 
                               nsIRequest *aRequest, 
                               PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
  nsString* url = new nsString(aURL);
  mPendingURLs.AppendElement(url);
  if (mVerbose) {
    printf("WebCrawler: adding '");
    fputs(NS_LossyConvertUCS2toASCII(aURL).get(), stdout);
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
  nsCOMPtr<nsIWebShell> shell;
  mBrowser->GetWebShell(*getter_AddRefs(shell));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(shell));
  if (docShell) {
    nsCOMPtr<nsIWebProgress> progress(do_GetInterface(docShell));
    if (progress) {
      progress->AddProgressListener(this);
      LoadNextURL(PR_FALSE);
    }
  }
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
  const PRUnichar* uc = aDomain.get();
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
  const PRUnichar* uc = s1.get();
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
          fputs(NS_LossyConvertUCS2toASCII(aURLSpec).get(), stdout);
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
    fputs(NS_LossyConvertUCS2toASCII(aURL).get(), mRecord);
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
      aNode->GetAttr(kNameSpaceID_HTML, mHrefAttr, src);
    }
    else {
      aNode->GetAttr(kNameSpaceID_HTML, mSrcAttr, src);
    }
    nsCOMPtr<nsIURI> docURL;
    aDocument->GetDocumentURL(getter_AddRefs(docURL));
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
            fputs(NS_LossyConvertUCS2toASCII(absURLSpec).get(), stdout);
            printf("'\n");
          }
        }
        else {
          if (mVerbose) {
            printf("Skipping '");
            fputs(NS_LossyConvertUCS2toASCII(absURLSpec).get(), stdout);
            printf("'\n");
          }
        }
      }
      else {
        if (mVerbose) {
          printf("Already visited '");
          fputs(NS_LossyConvertUCS2toASCII(absURLSpec).get(), stdout);
          printf("'\n");
        }
      }
    }
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
  nsCOMPtr<nsIWebShell> shell;
  mBrowser->GetWebShell(*getter_AddRefs(shell));

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(shell));
  if (docShell) {
    nsCOMPtr<nsIContentViewer> cv;
    docShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
      nsCOMPtr<nsIDocumentViewer> docv = do_QueryInterface(cv);
      if (docv) {
        nsCOMPtr<nsIDocument> doc;
        docv->GetDocument(*getter_AddRefs(doc));
        if (doc) {
          nsCOMPtr<nsIContent> root;
          doc->GetRootContent(getter_AddRefs(root));
          if (root) {
            FindURLsIn(doc, root);
          }
        }
      }
    }
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

void
nsWebCrawler::LoadNextURL(PRBool aQueueLoad)
{
  if ((mMaxPages < 0) || (mMaxPages > 0)) {
    while (0 != mPendingURLs.Count()) {
      nsString* url = NS_REINTERPRET_CAST(nsString*, mPendingURLs.ElementAt(0));
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
            webNav->LoadURI(url->get(), nsIWebNavigation::LOAD_FLAGS_NONE);
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
nsWebCrawler::GetPresShell(nsIWebShell* aWebShell)
{
  nsIWebShell* webShell = aWebShell;
  if (webShell) {
    NS_ADDREF(webShell);
  }
  else {
    mBrowser->GetWebShell(webShell);
  }
  nsIPresShell* shell = nsnull;
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webShell));
  if (nsnull != webShell) {
    nsIContentViewer* cv = nsnull;
    docShell->GetContentViewer(&cv);
    if (nsnull != cv) {
      nsIDocumentViewer* docv = nsnull;
      cv->QueryInterface(NS_GET_IID(nsIDocumentViewer), (void**) &docv);
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
  char* fn = ToNewCString(a);
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
  rv = fu->CompareRegressionData(f1, f2,mRegressionOutputLevel);
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
    webNav->LoadURI(aURL.get(), nsIWebNavigation::LOAD_FLAGS_NONE);
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
