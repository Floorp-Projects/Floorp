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
#include "plevent.h"

//Interfaces Needed
#include "nsIXULWindow.h"
#include "nsIWindowMediator.h"

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
                                 nsIXULWindow **aResult);
  void InitializeComponent( const nsCID &aComponentCID );
  void ShutdownComponent( const nsCID &aComponentCID );
  typedef void (nsAppShellService::*EnumeratorMemberFunction)(const nsCID&);
  void EnumerateComponents( void (nsAppShellService::*function)(const nsCID&) );
  nsresult SetXPConnectSafeContext();
  nsresult ClearXPConnectSafeContext();

  nsCOMPtr<nsIAppShell> mAppShell;
  nsCOMPtr<nsISupportsArray> mWindowList;
  nsCOMPtr<nsICmdLineService> mCmdLineService;
  nsCOMPtr<nsIWindowMediator> mWindowMediator;
  nsCOMPtr<nsIXULWindow>      mHiddenWindow;
  PRBool mDeleteCalled;
  nsCOMPtr<nsISplashScreen> mSplashScreen;
  nsCOMPtr<nsINativeAppSupport> mNativeAppSupport;

  // Set when the appshell service is going away.
  PRBool mShuttingDown;

  // A "last event" that is used to flush the appshell's event queue.
  struct ExitEvent {
    PLEvent            mEvent;
    nsAppShellService* mService;
  };

  static void* PR_CALLBACK HandleExitEvent(PLEvent* aEvent);
  static void PR_CALLBACK DestroyExitEvent(PLEvent* aEvent);
};

#endif
