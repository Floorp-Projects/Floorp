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
#ifndef __nsAppShellService_h
#define __nsAppShellService_h

#include "nsIAppShellService.h"
#include "nsINativeAppSupport.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsIAppShell.h"
#include "plevent.h"

//Interfaces Needed
#include "nsIXULWindow.h"
#include "nsIWindowMediator.h"
#include "nsPIWindowWatcher.h"
#include "nsISplashScreen.h"

class nsAppShellService : public nsIAppShellService,
                          public nsIObserver,
                          public nsSupportsWeakReference
{
public:
  nsAppShellService(void);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAPPSHELLSERVICE
  NS_DECL_NSIOBSERVER

protected:
  virtual ~nsAppShellService();

  void RegisterObserver(PRBool aRegister);
  NS_IMETHOD JustCreateTopWindow(nsIXULWindow *aParent,
                                 nsIURI *aUrl, 
                                 PRBool aShowWindow, PRBool aLoadDefaultPage,
                                 PRUint32 aChromeMask,
                                 PRInt32 aInitialWidth, PRInt32 aInitialHeight,
                                 PRBool aIsHiddenWindow, nsIXULWindow **aResult);
  PRUint32 CalculateWindowZLevel(nsIXULWindow *aParent, PRUint32 aChromeMask);
  nsresult SetXPConnectSafeContext();
  nsresult ClearXPConnectSafeContext();
  void     AttemptingQuit(PRBool aAttempt);

  nsCOMPtr<nsIAppShell> mAppShell;
  nsCOMPtr<nsICmdLineService> mCmdLineService;
  nsCOMPtr<nsIWindowMediator> mWindowMediator;
  nsCOMPtr<nsPIWindowWatcher> mWindowWatcher;
  nsCOMPtr<nsIXULWindow>      mHiddenWindow;
  PRBool mXPCOMShuttingDown;
#ifndef MOZ_XUL_APP
  nsCOMPtr<nsISplashScreen> mSplashScreen;
#endif
  nsCOMPtr<nsINativeAppSupport> mNativeAppSupport;

  PRUint16     mModalWindowCount;
  PRInt32      mConsiderQuitStopper; // if > 0, Quit(eConsiderQuit) fails
  PRPackedBool mShuttingDown;   // Quit method reentrancy check
  PRPackedBool mAttemptingQuit; // Quit(eAttemptQuit) still trying

  // A "last event" that is used to flush the appshell's event queue.
  PR_STATIC_CALLBACK(void*) HandleExitEvent(PLEvent* aEvent);
  PR_STATIC_CALLBACK(void) DestroyExitEvent(PLEvent* aEvent);

private:
#ifndef MOZ_XUL_APP
  nsresult CheckAndRemigrateDefunctProfile();
#endif

  nsresult LaunchTask(const char *aParam,
                      PRInt32 height, PRInt32 width,
                      PRBool *windowOpened);
  
  nsresult OpenWindow(const nsAFlatCString& aChromeURL,
                      const nsAFlatString& aAppArgs,
                      PRInt32 aWidth, PRInt32 aHeight);
                      
  nsresult OpenBrowserWindow(PRInt32 height, PRInt32 width);

};

#endif
