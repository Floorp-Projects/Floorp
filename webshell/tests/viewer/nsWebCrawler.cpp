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
#include "nsCOMPtr.h"
#include "nsWebCrawler.h"
#include "nsViewerApp.h"
#include "nsIWebShell.h"
#include "nsIBrowserWindow.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsIFrame.h"
#include "nsIURL.h"
#include "nsITimer.h"
#include "nsIAtom.h"
#include "nsIFrameUtil.h"
#include "nsIComponentManager.h"
#include "nsLayoutCID.h"
#include "nsRect.h"
#include "plhash.h"
#include "nsINameSpaceManager.h"

static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);
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

static PR_CALLBACK PRIntn
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
{
  NS_INIT_REFCNT();

  mBrowser = nsnull;
  mViewer = aViewer;
  mTimer = nsnull;
  mCrawl = PR_FALSE;
  mJiggleLayout = PR_FALSE;
  mPostExit = PR_FALSE;
  mDelay = 0;
  mWidth = -1;
  mHeight = -1;
  mMaxPages = -1;
  mRecord = nsnull;
  mLinkTag = NS_NewAtom("a");
  mFrameTag = NS_NewAtom("frame");
  mIFrameTag = NS_NewAtom("iframe");
  mHrefAttr = NS_NewAtom("href");
  mSrcAttr = NS_NewAtom("src");
  mBaseHrefAttr = NS_NewAtom("_base_href");
  mVisited = new AtomHashTable();
  mVerbose = nsnull;
  mRegressing = PR_FALSE;
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
  NS_IF_RELEASE(mTimer);
  NS_IF_RELEASE(mLinkTag);
  NS_IF_RELEASE(mFrameTag);
  NS_IF_RELEASE(mIFrameTag);
  NS_IF_RELEASE(mHrefAttr);
  NS_IF_RELEASE(mSrcAttr);
  NS_IF_RELEASE(mBaseHrefAttr);
  delete mVisited;
}

NS_IMPL_ISUPPORTS(nsWebCrawler, kIStreamObserverIID)

NS_IMETHODIMP
nsWebCrawler::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  if (mVerbose) {
    printf("Crawler: starting ");
    PRUnichar* tmp;
    aURL->ToString(&tmp);
    nsAutoString tmp2 = tmp;
    fputs(tmp2, stdout);
    delete tmp;
    printf("\n");
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebCrawler::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebCrawler::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWebCrawler::OnStopBinding(nsIURL* aURL, nsresult status, const PRUnichar* aMsg)
{
  if (mVerbose) {
    printf("Crawler: stopping ");
    PRUnichar* tmp;
    aURL->ToString(&tmp);
    nsAutoString tmp2 = tmp;
    fputs(tmp2, stdout);
    delete tmp;
    printf("\n");
  }
  return NS_OK;
}

void
nsWebCrawler:: EndLoadURL(nsIWebShell* aShell,
                          const PRUnichar* aURL,
                          PRInt32 aStatus)
{
  if (nsnull == aURL) {
    return;
  }

  nsAutoString tmp(aURL);
  if (mVerbose) {
    printf("Crawler: done loading ");
    fputs(tmp, stdout);
    printf("\n");
  }

  // Make sure the document bits make it to the screen at least once
  nsIPresShell* shell = GetPresShell();
  if (nsnull != shell) {
    nsCOMPtr<nsIViewManager> vm;
    shell->GetViewManager(getter_AddRefs(vm));
    if (vm) {
      nsIView* rootView;
      vm->GetRootView(rootView);
      vm->UpdateView(rootView, nsnull, NS_VMREFRESH_IMMEDIATE);
    }
    NS_RELEASE(shell);
  }

  if (mOutputDir.Length() > 0) {
    nsIPresShell* shell = GetPresShell();
    if (nsnull != shell) {
      nsIFrame* root;
      shell->GetRootFrame(&root);
      if (nsnull != root) {
        if (mOutputDir.Length() > 0)
        {
          nsIURL* url;
          nsresult rv = NS_NewURL(&url, tmp);
          if (NS_SUCCEEDED(rv) && (nsnull != url)) {
            nsAutoString regressionFileName;
            FILE *fp = GetOutputFile(url, regressionFileName);
            if (nsnull!=fp)
            {
              root->DumpRegressionData(fp, 0);
              fclose(fp);
              if (mRegressing) {
                PerformRegressionTest(regressionFileName);
              }
            }
            else {
              const char* file;
              (void)url->GetFile(&file);
              printf("could not open output file for %s\n", file);
            }
            NS_RELEASE(url);
          }
        }
        else
          root->DumpRegressionData(stdout, 0);
      }
      NS_RELEASE(shell);
    }
    else {
      fputs("null pres shell\n", stdout);
    }
  }

  if (mJiggleLayout) {
    nsRect r;
    mBrowser->GetBounds(r);
    nscoord oldWidth = r.width;
    while (r.width > 100) {
      r.width -= 10;
      mBrowser->SizeTo(r.width, r.height);
    }
    while (r.width < oldWidth) {
      r.width += 10;
      mBrowser->SizeTo(r.width, r.height);
    }
  }

  if (mCrawl) {
    FindMoreURLs();
  }

  if (0 == mDelay) {
    LoadNextURL();
  }
}

FILE*
nsWebCrawler::GetOutputFile(nsIURL *aURL, nsString& aOutputName)
{
  static const char kDefaultOutputFileName[] = "test.txt";   // the default
  FILE *result = nsnull;
  if (nsnull!=aURL)
  {
    char *inputFileName;
    const char* file;
    (void)aURL->GetFile(&file);
    nsAutoString inputFileFullPath(file);
    PRInt32 fileNameOffset = inputFileFullPath.RFind('/');
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
        char ch = (char) inputFileFullPath[i];
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
      aOutputName.Append(inputFileName);
    }
    nsAutoString outputFileName(mOutputDir);
    outputFileName += inputFileName;
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
  shell->SetObserver(this);

  LoadNextURL();
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
  nsIURL* url;
  nsresult rv = NS_NewURL(&url, aURLSpec);
  if (NS_OK == rv) {
    const char* host;
    rv = url->GetHost(&host);
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
  nsIAtom* atom;
  aNode->GetTag(atom);
  if ((atom == mLinkTag) || (atom == mFrameTag) || (atom == mIFrameTag)) {
    // Get absolute url that tag targets
    nsAutoString base, src, absURLSpec;
    if (atom == mLinkTag) {
      aNode->GetAttribute(kNameSpaceID_HTML, mHrefAttr, src);
    }
    else {
      aNode->GetAttribute(kNameSpaceID_HTML, mSrcAttr, src);
    }
    aNode->GetAttribute(kNameSpaceID_HTML, mBaseHrefAttr, base);/* XXX not public knowledge! */
    nsIURL* docURL = aDocument->GetDocumentURL();
    nsresult rv = NS_MakeAbsoluteURL(docURL, base, src, absURLSpec);
    if (NS_OK == rv) {
      nsIAtom* urlAtom = NS_NewAtom(absURLSpec);
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
      NS_RELEASE(urlAtom);
    }
    NS_RELEASE(docURL);
  }
  NS_IF_RELEASE(atom);
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
  if (nsnull != shell) {
    nsIContentViewer* cv = nsnull;
    shell->GetContentViewer(&cv);
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
nsWebCrawler::SetBrowserWindow(nsIBrowserWindow* aWindow) 
{
  NS_IF_RELEASE(mBrowser);
  mBrowser = aWindow;
  NS_IF_ADDREF(mBrowser);
}

void
nsWebCrawler::GetBrowserWindow(nsIBrowserWindow** aWindow)
{
  NS_IF_ADDREF(mBrowser);
  *aWindow = mBrowser;
}

static void
TimerCallBack(nsITimer *aTimer, void *aClosure)
{
  nsWebCrawler* wc = (nsWebCrawler*) aClosure;
  wc->LoadNextURL();
}

void
nsWebCrawler::LoadNextURL()
{
  if (0 != mDelay) {
    NS_IF_RELEASE(mTimer);
    NS_NewTimer(&mTimer);
    mTimer->Init(TimerCallBack, (void *)this, mDelay * 1000);
  }

  if ((mMaxPages < 0) || (mMaxPages > 0)) {
    while (0 != mPendingURLs.Count()) {
      nsString* url = (nsString*) mPendingURLs.ElementAt(0);
      mPendingURLs.RemoveElementAt(0);
      if (nsnull != url) {
        if (OkToLoad(*url)) {
          RecordLoadedURL(*url);
          if (0<=mWidth || 0<=mHeight)
          {
            nsRect r;
            mBrowser->GetBounds(r);
            if (0<=mWidth)
              r.width = mWidth;
            if (0<=mHeight)
              r.height = mHeight;
            mBrowser->SizeTo(r.width, r.height);
          }
          nsIWebShell* webShell;
          mBrowser->GetWebShell(webShell);
          mCurrentURL = *url;
          webShell->LoadURL(*url);
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

  if (mPostExit) {
    mViewer->Exit();
  }
}

nsIPresShell*
nsWebCrawler::GetPresShell()
{
  nsIWebShell* webShell;
  mBrowser->GetWebShell(webShell);
  nsIPresShell* shell = nsnull;
  if (nsnull != webShell) {
    nsIContentViewer* cv = nsnull;
    webShell->GetContentViewer(&cv);
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
  a.Append("/");
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
