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
#ifndef nsViewerApp_h___
#define nsViewerApp_h___

#include "nsIAppShell.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsVoidArray.h"
#include "nsWebCrawler.h"

class nsIEventQueueService;
class nsIPref;
class nsBrowserWindow;
class nsIBrowserWindow;

class nsViewerApp : public nsISupports, public nsDispatchListener
{
public:
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  virtual ~nsViewerApp();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsDispatchListener
  virtual void AfterDispatch();

  // nsViewerApp
  NS_IMETHOD SetupRegistry();
  NS_IMETHOD Initialize(int argc, char** argv);
  NS_IMETHOD ProcessArguments(int argc, char** argv);
  NS_IMETHOD OpenWindow();
  NS_IMETHOD CloseWindow(nsBrowserWindow* aBrowserWindow);
  NS_IMETHOD ViewSource(nsString& aURL);
  NS_IMETHOD OpenWindow(PRUint32 aNewChromeMask, nsIBrowserWindow*& aNewWindow);
  NS_IMETHOD CreateRobot(nsBrowserWindow* aWindow);
  NS_IMETHOD CreateSiteWalker(nsBrowserWindow* aWindow);
  NS_IMETHOD CreateJSConsole(nsBrowserWindow* aWindow);
  NS_IMETHOD Exit();

  NS_IMETHOD DoPrefs(nsBrowserWindow* aWindow);

  void EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aStatus)
  {
    if (nsnull != mCrawler) {
      mCrawler->EndLoadURL(aShell, aURL, aStatus);
    }
  }

  virtual int Run() = 0;

protected:
  nsViewerApp();

  void Destroy();

  nsIAppShell* mAppShell;
  nsIPref* mPrefs;
  nsIEventQueueService* mEventQService;
  nsString mStartURL;
  PRBool mDoPurify;
  PRBool mLoadTestFromFile;
  PRBool mCrawl;
  nsString mInputFileName;
  PRInt32 mNumSamples;
  PRInt32 mDelay;
  PRInt32 mRepeatCount;
  nsWebCrawler* mCrawler;
  PRBool mAllowPlugins;
  PRBool mIsInitialized;
  PRInt32 mWidth, mHeight;
  PRBool mShowLoadTimes;
};

class nsNativeViewerApp : public nsViewerApp {
public:
  nsNativeViewerApp();
  ~nsNativeViewerApp();

  virtual int Run();
#ifdef XP_MAC
  static void DispatchMenuItemWithoutWindow(PRInt32 menuResult);
#endif
};

#endif /* nsViewerApp_h___ */

