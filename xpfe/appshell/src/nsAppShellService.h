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
 */
#ifndef __nsAppShellService_h
#define __nsAppShellService_h

#include "nsISupports.h"
#include "nsIAppShellService.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsIAppShell.h"
#include "nsITimer.h"

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
  NS_IMETHOD JustCreateTopWindow(nsIWebShellWindow *aParent,
                                 nsIURI *aUrl, 
                                 PRBool aShowWindow, PRBool aLoadDefaultPage,
                                 PRUint32 aChromeMask,
                                 nsIXULWindowCallbacks *aCallbacks,
                                 PRInt32 aInitialWidth, PRInt32 aInitialHeight,
                                 nsIWebShellWindow **aResult);
  void InitializeComponent( const nsCID &aComponentCID );
  void ShutdownComponent( const nsCID &aComponentCID );
  typedef void (nsAppShellService::*EnumeratorMemberFunction)(const nsCID&);
  void EnumerateComponents( void (nsAppShellService::*function)(const nsCID&) );

  nsIAppShell* mAppShell;
  nsISupportsArray* mWindowList;
  nsICmdLineService* mCmdLineService;
  nsIWindowMediator* mWindowMediator;
  nsCOMPtr<nsIWebShellWindow> mHiddenWindow;
  PRBool mDeleteCalled;
  nsISplashScreen *mSplashScreen;

  // The mShutdownTimer is set in Quit() to asynchronously call the
  // ExitCallback(). This allows one last pass through any events in
  // the event queue before shutting down the appshell.
  nsCOMPtr<nsITimer> mShutdownTimer;
  static void ExitCallback(nsITimer* aTimer, void* aClosure);
};

#endif
