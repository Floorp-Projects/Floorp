/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsBrowserWindow_h___
#define nsBrowserWindow_h___

// Local Includes
#include "nsWebBrowserChrome.h"
#include "nsThrobber.h"

// Helper Classes

// Interfaces Needed
#include "nsIBaseWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIFile.h"
#include "nsILocalFile.h"

#include "nsIWebBrowser.h"
#include "nsIStreamListener.h"
#include "nsIProgressEventSink.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsCRT.h"
#include "prtime.h"

#include "nsIXPBaseWindow.h"
#include "nsPrintSetupDialog.h"
#include "nsTableInspectorDialog.h"
#include "nsImageInspectorDialog.h"
#include "nsITextWidget.h"

#include "nsIPrintSettings.h"

class nsILabel;
class nsICheckButton;
class nsIButton;
class nsThrobber;
class nsViewerApp;
class nsIDocumentViewer;
class nsPresContext;
class nsIPresShell;
class nsIContentConnector;
class nsWebCrawler;
class nsIContent;

#define SAMPLES_BASE_URL "resource:/res/samples"

/**
 * Abstract base class for our test app's browser windows
 */
class nsBrowserWindow : public nsIBaseWindow,
                        public nsIInterfaceRequestor,
                        public nsIProgressEventSink,
                        public nsIWebShellContainer
{
friend class nsWebBrowserChrome;

public:
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS
  NS_DECL_NSIBASEWINDOW
  NS_DECL_NSIINTERFACEREQUESTOR

protected:
   void DestroyWidget(nsISupports* aWidget);

public:

  // nsIBrowserWindow
  NS_IMETHOD Init(nsIAppShell* aAppShell,
                  const nsRect& aBounds,
                  PRUint32 aChromeMask,
                  PRBool aAllowPlugins = PR_TRUE);
                  
  // virtual method for platform-specific initialization
  NS_IMETHOD InitNativeWindow() { return NS_OK; }
  
  // nsIBrowserWindow
  NS_IMETHOD MoveTo(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD SizeContentTo(PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD SizeWindowTo(PRInt32 aWidth, PRInt32 aHeight,
                          PRBool aWidthTransient, PRBool aHeightTransient);
  NS_IMETHOD GetContentBounds(nsRect& aBounds);
  NS_IMETHOD GetWindowBounds(nsRect& aBounds);
  NS_IMETHOD SetChrome(PRUint32 aNewChromeMask);
  NS_IMETHOD GetChrome(PRUint32& aChromeMaskResult);
  NS_IMETHOD SetProgress(PRInt32 aProgress, PRInt32 aProgressMax);
  NS_IMETHOD ShowMenuBar(PRBool aShow);
  NS_IMETHOD GetWebShell(nsIWebShell*& aResult);
  NS_IMETHOD GetContentWebShell(nsIWebShell **aResult);

  // nsIProgressEventSink
  NS_DECL_NSIPROGRESSEVENTSINK

  // nsIWebShellContainer
  NS_IMETHOD WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason);
  NS_IMETHOD BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL);
  NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax);
  NS_IMETHOD EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsresult aStatus);
  NS_IMETHOD NewWebShell(PRUint32 aChromeMask,
                         PRBool aVisible,
                         nsIWebShell *&aNewWebShell);
  NS_IMETHOD ContentShellAdded(nsIWebShell* aChildShell, nsIContent* frameNode);
  NS_IMETHOD FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult);

  // nsBrowserWindow
  void SetWebCrawler(nsWebCrawler* aWebCrawler);
  virtual nsresult CreateMenuBar(PRInt32 aWidth) = 0;
  virtual nsresult GetMenuBarHeight(PRInt32 * aHeightOut);
  virtual nsresult CreateToolBar(PRInt32 aWidth);
  virtual nsresult CreateStatusBar(PRInt32 aWidth);
  void Layout(PRInt32 aWidth, PRInt32 aHeight);
  void Back();
  void Forward();
  void GoTo(const PRUnichar* aURL);
  void StartThrobber();
  void StopThrobber();
  void LoadThrobberImages();
  void DestroyThrobberImages();
  virtual nsEventStatus DispatchMenuItem(PRInt32 aID) = 0;
  virtual nsresult GetWindow(nsIWidget** aWindow);

  void DoFileOpen();
  void DoCopy();
  void DoPaste();
  void DoJSConsole();
  void DoPrefs();
  void DoEditorMode(nsIDocShell* aDocShell);
  void DoEditorTest(nsIDocShell* aDocShell, PRInt32 aCommandID);
  nsIPresShell* GetPresShell();

  void DoFind();
  NS_IMETHOD FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound);
  NS_IMETHOD ForceRefresh();

  void DoTreeView();
  void DoToolbarDemo();

  void ShowPrintPreview(PRInt32 aID);
  void DoPrint(void);
  void DoTableInspector(void);
  void DoImageInspector(void);

#ifdef NS_DEBUG
  void DumpContent(FILE *out = stdout);
  void DumpFrames(FILE *out = stdout, nsString *aFilter = nsnull);
  void DumpViews(FILE *out = stdout);
  void DumpWebShells(FILE *out = stdout);
  void DumpStyleSheets(FILE *out = stdout);
  void DumpStyleContexts(FILE *out = stdout);
  void DumpReflowStats(FILE *out = stdout);
  void DisplayReflowStats(PRBool aIsOn);
  void ToggleFrameBorders();
  void ToggleVisualEventDebugging();
  void ToggleBoolPrefAndRefresh(const char * aPrefName);

  void DoDebugSave();
  void DoToggleSelection();
  void DoDebugRobot();
  void DoSiteWalker();
  nsEventStatus DispatchDebugMenu(PRInt32 aID);
#endif
  void SetBoolPref(const char * aPrefName, PRBool aValue);
  void SetStringPref(const char * aPrefName, const nsString& aValue);
  void GetStringPref(const char * aPrefName, nsString& aValue);

  nsEventStatus DispatchStyleMenu(PRInt32 aID);
  void SetCompatibilityMode(PRUint32 aMode);

  nsEventStatus ProcessDialogEvent(nsGUIEvent *aEvent);

  // Initialize a second view on a different browser windows document
  // viewer.
  nsresult Init(nsIAppShell* aAppShell,
                const nsRect& aBounds,
                PRUint32 aChromeMask,
                PRBool aAllowPlugins,
                nsIDocumentViewer* aDocViewer,
                nsPresContext* aPresContext);

  void SetApp(nsViewerApp* aApp) {
    mApp = aApp;
  }

  static void CloseAllWindows();

  void SetShowLoadTimes(PRBool aOn) {
    mShowLoadTimes = aOn;
  }

  nsViewerApp* mApp;
  nsWebCrawler* mWebCrawler;

  PRUint32 mChromeMask;
  nsString mTitle;

  nsCOMPtr<nsIWidget> mWindow;
  nsCOMPtr<nsIDocShell> mDocShell;
  nsCOMPtr<nsIWebBrowser> mWebBrowser;

  nsCOMPtr<nsILocalFile> mOpenFileDirectory;

  PRTime mLoadStartTime;
  PRBool mShowLoadTimes;

  // "Toolbar"
  nsITextWidget* mLocation;
  nsIButton* mBack;
  nsIButton* mForward;
  nsThrobber* mThrobber;
  
  // "Status bar"
  nsITextWidget* mStatus;

  nsIXPBaseWindow * mXPDialog;
  nsIXPBaseWindow * mTableInspectorDialog;
  nsIXPBaseWindow * mImageInspectorDialog;
  PrintSetupInfo    mPrintSetupInfo;

  nsTableInspectorDialog * mTableInspector;
  nsImageInspectorDialog * mImageInspector;

  //for creating more instances
  nsIAppShell* mAppShell;
  PRBool mAllowPlugins;

  // Global window collection
  static nsVoidArray *gBrowsers;
  static void AddBrowser(nsBrowserWindow* aBrowser);
  static void RemoveBrowser(nsBrowserWindow* aBrowser);
  static nsBrowserWindow* FindBrowserFor(nsIWidget* aWidget, PRIntn aWhich);

protected:
  nsIDOMDocument* GetDOMDocument(nsIDocShell *aDocShell);
  NS_IMETHOD EnsureWebBrowserChrome();

  nsBrowserWindow();
  virtual ~nsBrowserWindow();

  nsWebBrowserChrome*   mWebBrowserChrome;
  PRBool mHaveMenuBar;

  nsCOMPtr<nsIPrintSettings> mPrintSettings;
};

// XXX This is bad; because we can't hang a closure off of the event
// callback we have no way to store our This pointer; therefore we
// have to hunt to find the browswer that events belong too!!!

// aWhich for FindBrowserFor
#define FIND_WINDOW   0
#define FIND_BACK     1
#define FIND_FORWARD  2
#define FIND_LOCATION 3

//----------------------------------------------------------------------

class nsNativeBrowserWindow : public nsBrowserWindow {
public:
  nsNativeBrowserWindow();
  ~nsNativeBrowserWindow();
  
  virtual nsresult CreateMenuBar(PRInt32 aWidth);
  virtual nsEventStatus DispatchMenuItem(PRInt32 aID);
  virtual nsresult GetMenuBarHeight(PRInt32 * aHeightOut);
  
protected:
	  NS_IMETHOD InitNativeWindow();

};

#endif /* nsBrowserWindow_h___ */
