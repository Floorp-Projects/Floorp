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
#ifndef nsBrowserWindow_h___
#define nsBrowserWindow_h___

#include "nsIBrowserWindow.h"
#include "nsIXPBaseWindow.h"
#include "nsIStreamListener.h"
#include "nsINetSupport.h"
#include "nsIWebShell.h"
#include "nsIScriptContextOwner.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsCRT.h"

#include "nsIToolbarManagerListener.h"
#include "nsIImageButtonListener.h"

#include "nsIEditor.h"

class nsILabel;
class nsICheckButton;
class nsIRadioButton;
class nsIDialog;
class nsITextWidget;
class nsIButton;
class nsIThrobber;
class nsViewerApp;
class nsIPresShell;
class nsIPref;
class nsIImageButton;
class nsIMenuButton;
class nsIToolbar;
class nsIToolbarManager;

#define SAMPLES_BASE_URL "resource:/res/samples"

/**
 * Abstract base class for our test app's browser windows
 */
class nsBrowserWindow : public nsIBrowserWindow,
                        public nsIStreamObserver,
                        public nsINetSupport,
                        public nsIWebShellContainer,
                        public nsIToolbarManagerListener,
                        public nsIImageButtonListener
{
public:
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsBrowserWindow();
  virtual ~nsBrowserWindow();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIBrowserWindow
  NS_IMETHOD Init(nsIAppShell* aAppShell,
                  nsIPref* aPrefs,
                  const nsRect& aBounds,
                  PRUint32 aChromeMask,
                  PRBool aAllowPlugins = PR_TRUE);
  NS_IMETHOD MoveTo(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD SizeTo(PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD GetWindowBounds(nsRect& aBounds);
  NS_IMETHOD GetBounds(nsRect& aBounds);
  NS_IMETHOD Show();
  NS_IMETHOD Hide();
  NS_IMETHOD Close();
  NS_IMETHOD SetChrome(PRUint32 aNewChromeMask);
  NS_IMETHOD GetChrome(PRUint32& aChromeMaskResult);
  NS_IMETHOD SetTitle(const PRUnichar* aTitle);
  NS_IMETHOD GetTitle(PRUnichar** aResult);
  NS_IMETHOD SetStatus(const PRUnichar* aStatus);
  NS_IMETHOD SetStatus(const nsString &aStatus);
  NS_IMETHOD GetStatus(PRUnichar** aResult);
  NS_IMETHOD SetProgress(PRInt32 aProgress, PRInt32 aProgressMax);
  NS_IMETHOD GetWebShell(nsIWebShell*& aResult);

  NS_IMETHOD HandleEvent(nsGUIEvent * anEvent);

  // nsIStreamObserver
  NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
  NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
  NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);
  NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult status, const PRUnichar* aMsg);

  // nsIWebShellContainer
  NS_IMETHOD WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason);
  NS_IMETHOD BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL);
  NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax);
  NS_IMETHOD EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aStatus);
  NS_IMETHOD NewWebShell(PRUint32 aChromeMask,
                         PRBool aVisible,
                         nsIWebShell *&aNewWebShell);
  NS_IMETHOD FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult);
  NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken);

  // nsINetSupport
  NS_IMETHOD_(void) Alert(const nsString &aText);
  NS_IMETHOD_(PRBool) Confirm(const nsString &aText);
  NS_IMETHOD_(PRBool) Prompt(const nsString &aText,
                             const nsString &aDefault,
                             nsString &aResult);
  NS_IMETHOD_(PRBool) PromptUserAndPassword(const nsString &aText,
                                            nsString &aUser,
                                            nsString &aPassword);
  NS_IMETHOD_(PRBool) PromptPassword(const nsString &aText,
                                     nsString &aPassword);

  // nsBrowserWindow
  virtual nsresult CreateMenuBar(PRInt32 aWidth);
  virtual nsresult CreateToolBar(PRInt32 aWidth);
  virtual nsresult CreateStatusBar(PRInt32 aWidth);
 
  // XXX: This method is temporary until javascript event handlers come
  // through the content model.
  void ExecuteJavaScriptString(nsIWebShell* aWebShell, nsString& aJavaScript);
  void Layout(PRInt32 aWidth, PRInt32 aHeight);
  void Back();
  void Forward();
  void GoTo(const PRUnichar* aURL);
  void StartThrobber();
  void StopThrobber();
  void LoadThrobberImages();
  void DestroyThrobberImages();
  virtual nsEventStatus DispatchMenuItem(PRInt32 aID);

  void DoFileOpen();
  void DoViewSource();
  void DoCopy();
  void DoJSConsole();
  void DoEditorMode(nsIWebShell* aWebShell);
  nsIPresShell* GetPresShell();

  void DoFind();
  void DoSelectAll();
  void DoAppsDialog();
  NS_IMETHOD FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound);
  NS_IMETHOD ForceRefresh();

  // nsIToolbarManager Listener Interface
  NS_IMETHOD NotifyToolbarManagerChangedSize(nsIToolbarManager* aToolbarMgr);

  // nsIImageButtonListener
  NS_IMETHOD NotifyImageButtonEvent(nsIImageButton * aImgBtn, nsGUIEvent* anEvent);

#ifdef NS_DEBUG
  void DumpContent(FILE *out = stdout);
  void DumpFrames(FILE *out = stdout, nsString *aFilter = nsnull);
  void DumpViews(FILE *out = stdout);
  void DumpWebShells(FILE *out = stdout);
  void DumpStyleSheets(FILE *out = stdout);
  void DumpStyleContexts(FILE *out = stdout);
  void ToggleFrameBorders();
  void ShowContentSize();
  void ShowFrameSize();
  void ShowStyleSize();
  void DoDebugSave();
  void DoToggleSelection();
  void DoDebugRobot();
  nsEventStatus DispatchDebugMenu(PRInt32 aID);
#endif
  void DoSiteWalker();

  nsEventStatus ProcessDialogEvent(nsGUIEvent *aEvent);


  void SetApp(nsViewerApp* aApp) {
    mApp = aApp;
  }

  static void CloseAllWindows();

  nsViewerApp* mApp;
  nsIXPBaseWindow* mXPDialog;

  PRUint32 mChromeMask;
  nsString mTitle;

  nsIWidget* mWindow;
  nsIWebShell* mWebShell;

  nsIWidget ** mAppsDialogBtns;
  PRInt32      mNumAppsDialogBtns;

  nsIWidget ** mMiniAppsBtns;
  PRInt32      mNumMiniAppsBtns;

  nsIWidget ** mToolbarBtns;
  PRInt32      mNumToolbarBtns;

  nsIWidget ** mPersonalToolbarBtns;
  PRInt32      mNumPersonalToolbarBtns;

  // "Toolbar"
  nsIToolbarManager * mToolbarMgr;
  nsIToolbar * mBtnToolbar;
  nsIToolbar * mURLToolbar;

  //nsIImageButton* mBack;
  //nsIImageButton* mForward;
  //nsIImageButton* mReload;
  //nsIImageButton* mHome;
  //nsIImageButton* mPrint;
  //nsIImageButton* mStop;
  nsIThrobber*    mThrobber;

  nsIMenuButton*    mBookmarks;
  nsIMenuButton*    mWhatsRelated;
  nsITextWidget*    mLocation;
  nsIImageButton*   mLocationIcon;

  // nsIWidget for Buttons
  //nsIWidget* mBackWidget;
  //nsIWidget* mForwardWidget;
  //nsIWidget* mReloadWidget;
  //nsIWidget* mHomeWidget;
  //nsIWidget* mPrintWidget;
  //nsIWidget* mStopWidget;

  nsIWidget* mBookmarksWidget;
  nsIWidget* mWhatsRelatedWidget;
  nsIWidget* mLocationWidget;
  nsIWidget* mLocationIconWidget;

  // "Status bar"
  nsITextWidget  * mStatus;
  nsIToolbar     * mStatusBar;
  nsIImageButton * mStatusSecurityLabel;
  nsIImageButton * mStatusProcess;
  nsIImageButton * mStatusText;

  // Mini App Bar (in StatusBar)
  nsIToolbar     * mStatusAppBar;
  nsIImageButton * mMiniTab;
  nsIImageButton * mMiniNav;
  nsIImageButton * mMiniMail;
  nsIImageButton * mMiniAddr;
  nsIImageButton * mMiniComp;

  nsIWidget      * mStatusAppBarWidget;
  nsIWidget      * mMiniTabWidget;

  // Apps Dialog
  nsIDialog      * mAppsDialog;

  // Find Dialog
  nsIDialog      * mDialog;
  nsIButton      * mCancelBtn;
  nsIButton      * mFindBtn;
  nsITextWidget  * mTextField;
  nsICheckButton * mMatchCheckBtn;
  nsIRadioButton * mUpRadioBtn;
  nsIRadioButton * mDwnRadioBtn;
  nsILabel       * mLabel;

  //for creating more instances
  nsIAppShell* mAppShell;   //not addref'ed!
  nsIPref* mPrefs;          //not addref'ed!
  PRBool mAllowPlugins;

  // Global window collection
  static nsVoidArray gBrowsers;
  static void AddBrowser(nsBrowserWindow* aBrowser);
  static void RemoveBrowser(nsBrowserWindow* aBrowser);
  static nsBrowserWindow* FindBrowserFor(nsIWidget* aWidget, PRIntn aWhich);
  static nsBrowserWindow* FindBrowserFor(nsIWidget* aWidget);

protected:


  nsresult AddToolbarItem(nsIToolbar      *aToolbar,
                          PRInt32          aGap,
                          PRBool           aEnable,
								          nsIWidget       *aButtonWidget);

  void UpdateToolbarBtns();

  void AddEditor(nsIEditor *); //this function is temporary and WILL be depricated
  nsIEditor      * mEditor; //this will hold the editor for future commands. we must think about this mjudge
};

// nsViewSourceWindow
//
// Objects of this class are nsBrowserWindows with no chrome and which render the *source*
// for web pages rather than the web pages themselves.
//
// We also override SetTitle to block the nsIWebShell from resetting our nice "Source for..."
// title.
//
// Note that there is no nsViewSourceWindow interface, nor does this class have a CID or
// provide a factory.  Deal with it (but seriously, I explain why somewhere).
class nsViewSourceWindow : public nsBrowserWindow {
public:
  nsViewSourceWindow( nsIAppShell     *anAppShell,
                      nsIPref         *aPrefs,
                      nsViewerApp     *anApp,
                      const PRUnichar *aURL );
  NS_IMETHOD SetTitle( const PRUnichar *aTitle );
};;

// XXX This is bad; because we can't hang a closure off of the event
// callback we have no way to store our This pointer; therefore we
// have to hunt to find the browswer that events belong too!!!

// aWhich for FindBrowserFor
#define FIND_WINDOW   0
#define FIND_BACK     1
#define FIND_FORWARD  2
#define FIND_LOCATION 3

//----------------------------------------------------------------------

/*class nsNativeBrowserWindow : public nsBrowserWindow {
public:
  nsNativeBrowserWindow();
  ~nsNativeBrowserWindow();

  virtual nsresult CreateMenuBar(PRInt32 aWidth);
  virtual nsEventStatus DispatchMenuItem(PRInt32 aID);
};*/

#endif /* nsBrowserWindow_h___ */
