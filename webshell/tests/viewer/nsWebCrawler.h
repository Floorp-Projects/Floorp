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
#ifndef nsWebCrawler_h___
#define nsWebCrawler_h___

#include "nsCOMPtr.h"
#include "nsBrowserWindow.h"
#include "nsIWebProgressListener.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsWeakReference.h"
#include "nsIURI.h"


class nsIContent;
class nsIDocument;
class nsITimer;
class nsIURI;
class nsIPresShell;
class nsViewerApp;
class AtomHashTable;

class nsWebCrawler : public nsIWebProgressListener,
                     public nsSupportsWeakReference {
public:
  // Make a new web-crawler for the given viewer. Note: the web
  // crawler does not addref the viewer.
  nsWebCrawler(nsViewerApp* aViewer);

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER

  // Add a url to load
  void AddURL(const nsString& aURL);

  // Add a domain that is safe to load url's from
  void AddSafeDomain(const nsString& aDomain);

  // Add a domain that must be avoided
  void AddAvoidDomain(const nsString& aDomain);

  void SetBrowserWindow(nsBrowserWindow* aWindow);
  void GetBrowserWindow(nsBrowserWindow** aWindow);

  void SetPrintTest(PRInt32 aTestType) { mPrinterTestType = aTestType; }

  void RegressionOutput(PRInt32 aRegressionOutputLevel) { mRegressionOutputLevel = aRegressionOutputLevel; }

  void EnableJiggleLayout() {
    mJiggleLayout = PR_TRUE;
  }

  // If set to TRUE the loader will post an exit message on exit
  void SetExitOnDone(PRBool aPostExit) {
    mPostExit = aPostExit;
  }

  // Start loading documents
  void Start();

  // Enable the crawler; when a document contains links to other
  // documents the crawler will go to them subject to the limitations
  // on the total crawl count and the domain name checks.
  void EnableCrawler();

  void SetRecordFile(FILE* aFile) {
    mRecord = aFile;
  }

  void SetMaxPages(PRInt32 aMax) {
    mMaxPages = aMax;
  }

  void SetOutputDir(const nsString& aOutputDir);

  void DumpRegressionData();
  void SetRegressionDir(const nsString& aOutputDir);

  void SetEnableRegression(PRBool aSetting) {
    mRegressing = aSetting;
  }

  static void
  LoadNextURLCallback(nsITimer* aTimer, void* aClosure);

  void LoadNextURL(PRBool aQueueLoad);

  nsresult QueueLoadURL(const nsString& aURL);

  void GoToQueuedURL(const nsString& aURL);

  static void
  QueueExitCallback(nsITimer* atimer, void* aClosure);

  void QueueExit();

  void Exit();

  void SetVerbose(PRBool aSetting) {
    mVerbose = aSetting;
  }

  PRBool Crawling() const {
    return mCrawl;
  }

  PRBool LoadingURLList() const {
    return mHaveURLList;
  }

  void IncludeStyleData(PRBool aIncludeStyle) {
    mIncludeStyleInfo = aIncludeStyle;
  }

protected:
  virtual ~nsWebCrawler();

  void FindURLsIn(nsIDocument* aDocument, nsIContent* aNode);

  void FindMoreURLs();

  PRBool OkToLoad(const nsString& aURLSpec);

  void RecordLoadedURL(const nsString& aURLSpec);

  /** generate an output name from a URL */
  FILE* GetOutputFile(nsIURI *aURL, nsString& aOutputName);

  nsIPresShell* GetPresShell(nsIWebShell* aWebShell = nsnull);

  void PerformRegressionTest(const nsString& aOutputName);

  nsBrowserWindow* mBrowser;
  nsViewerApp* mViewer;
  nsCOMPtr<nsITimer> mTimer;
  FILE* mRecord;
  nsCOMPtr<nsIAtom> mLinkTag;
  nsCOMPtr<nsIAtom> mFrameTag;
  nsCOMPtr<nsIAtom> mIFrameTag;
  nsCOMPtr<nsIAtom> mHrefAttr;
  nsCOMPtr<nsIAtom> mSrcAttr;
  nsCOMPtr<nsIAtom> mBaseHrefAttr;
  AtomHashTable* mVisited;
  nsString mOutputDir;

  PRBool mCrawl;
  PRBool mHaveURLList;
  PRBool mJiggleLayout;
  PRBool mPostExit;
  PRInt32 mDelay;
  PRInt32 mMaxPages;

  nsString mCurrentURL;
  nsCOMPtr<nsIURI>  mLastURL;

  PRTime mStartLoad;
  PRBool mVerbose;
  PRBool mRegressing;
  PRInt32 mPrinterTestType;
  PRInt32 mRegressionOutputLevel;
  nsString mRegressionDir;
  PRBool mIncludeStyleInfo;

  nsVoidArray mPendingURLs;
  nsVoidArray mSafeDomains;
  nsVoidArray mAvoidDomains;

  PRInt32 mQueuedLoadURLs;
};

#endif /* nsWebCrawler_h___ */
