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
  NS_IMETHOD OpenWindow(PRUint32 aNewChromeMask, nsBrowserWindow*& aNewWindow);
  NS_IMETHOD CreateRobot(nsBrowserWindow* aWindow);
  NS_IMETHOD CreateSiteWalker(nsBrowserWindow* aWindow);
  NS_IMETHOD CreateJSConsole(nsBrowserWindow* aWindow);
  NS_IMETHOD Exit();

  NS_IMETHOD DoPrefs(nsBrowserWindow* aWindow);

  virtual int Run() = 0;

protected:
  nsViewerApp();

  void Destroy();
  nsresult AutoregisterComponents();

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
  PRBool mJustShutdown;
};

class nsNativeViewerApp : public nsViewerApp {
public:
  nsNativeViewerApp();
  ~nsNativeViewerApp();

  virtual int Run();
#if defined(XP_MAC) || defined(XP_MACOSX)
  static void DispatchMenuItemWithoutWindow(PRInt32 menuResult);
#endif
};

#endif /* nsViewerApp_h___ */

