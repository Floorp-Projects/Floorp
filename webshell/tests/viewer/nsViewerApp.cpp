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
#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsWidgetsCID.h"
#include "nsIAppShell.h"
#include "nsIPref.h"
#include "nsINetService.h"
#include "nsRepository.h"

#ifdef VIEWER_PLUGINS
static nsIPluginManager *gPluginManager = nsnull;
static nsIPluginHost *gPluginHost = nsnull;
#endif

extern nsresult NS_NewBrowserWindowFactory(nsIFactory** aFactory);
extern "C" void NS_SetupRegistry();
extern "C" int NET_PollSockets();

static NS_DEFINE_IID(kAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_IID(kBrowserWindowCID, NS_BROWSER_WINDOW_CID);

static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
static NS_DEFINE_IID(kIBrowserWindowIID, NS_IBROWSER_WINDOW_IID);
static NS_DEFINE_IID(kINetContainerApplicationIID,
                     NS_INETCONTAINERAPPLICATION_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsViewerApp::nsViewerApp()
{
}

nsViewerApp::~nsViewerApp()
{
}

NS_IMPL_ADDREF(nsViewerApp)
NS_IMPL_RELEASE(nsViewerApp)

nsresult
nsViewerApp::QueryInterface(REFNSIID aIID, void** aInstancePtrResult) 
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kINetContainerApplicationIID)) {
    *aInstancePtrResult = (void*) ((nsIBrowserWindow*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)((nsIBrowserWindow*)this));
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsViewerApp::SetupRegistry()
{
  NS_SetupRegistry();

  // Register our browser window factory
  nsIFactory* bwf;
  NS_NewBrowserWindowFactory(&bwf);
  NSRepository::RegisterFactory(kBrowserWindowCID, bwf, PR_FALSE);

  return NS_OK;
}

nsresult
nsViewerApp::Initialize(int argc, char** argv)
{
  nsresult rv;

  rv = SetupRegistry();
  if (NS_OK != rv) {
    return rv;
  }

  // Create widget application shell
  rv = NSRepository::CreateInstance(kAppShellCID, nsnull, kIAppShellIID,
                                    (void**)&mAppShell);
  if (NS_OK != rv) {
    return rv;
  }
  mAppShell->Create(&argc, argv);
  mAppShell->SetDispatchListener((nsDispatchListener*) this);

  // Load preferences
  rv = NSRepository::CreateInstance(kPrefCID, NULL, kIPrefIID,
                                    (void **) &mPrefs);
  if (NS_OK != rv) {
    return rv;
  }
  mPrefs->Startup("prefs.js");

  // Setup networking library
  rv = NS_InitINetService((nsINetContainerApplication*) this);
  if (NS_OK != rv) {
    return rv;
  }

#if 0
  // XXX where should this live
  for (int i=0; i<MAX_RL; i++)
    gRLList[i] = nsnull;
  mRelatedLinks = NS_NewRelatedLinks();
  gRelatedLinks = mRelatedLinks;
  if (mRelatedLinks) {
    mRLWindow = mRelatedLinks->MakeRLWindowWithCallback(DumpRLValues, this);
  }
#endif

#ifdef VIEWER_PLUGINS
  if (nsnull == gPluginManager) {
    rv = NSRepository::CreateInstance(kCPluginHostCID, nsnull,
                                      kIPluginManagerIID,
                                      (void**)&gPluginManager);
    if (NS_OK==rv)
    {
      if (NS_OK == gPluginManager->QueryInterface(kIPluginHostIID,
                                                  (void **)&gPluginHost))
      {
        gPluginHost->Init();
        gPluginHost->LoadPlugins();
      }
    }

    // It's ok if we can't host plugins
    rv = NS_OK;
  }
#endif

  // Finally process our arguments
  rv = ProcessArguments(argc, argv);

  return rv;
}

nsresult
nsViewerApp::Exit()
{
  mAppShell->Exit();
  return NS_OK;
}

NS_IMETHODIMP
nsViewerApp::ProcessArguments(int argc, char** argv)
{
  return NS_OK;
}

NS_IMETHODIMP
nsViewerApp::OpenWindow()
{
  nsBrowserWindow* bw = nsnull;
  nsresult rv = NSRepository::CreateInstance(kBrowserWindowCID, nsnull,
                                             kIBrowserWindowIID,
                                             (void**) &bw);
  bw->SetApp(this);
  bw->Init(mAppShell, nsRect(0, 0, 620, 400), PRUint32(~0));
  bw->Show();
  {
    bw->LoadURL("resource:/res/samples/test0.html");
  }

  return NS_OK;
}

//----------------------------------------

// nsINetContainerApplication implementation

NS_IMETHODIMP    
nsViewerApp::GetAppCodeName(nsString& aAppCodeName)
{
  aAppCodeName.SetString("Mozilla");
  return NS_OK;
}
 
NS_IMETHODIMP
nsViewerApp::GetAppVersion(nsString& aAppVersion)
{
  aAppVersion.SetString("5.0 [en] (Windows;I)");
  return NS_OK;
}
 
NS_IMETHODIMP
nsViewerApp::GetAppName(nsString& aAppName)
{
  aAppName.SetString("Netscape");
  return NS_OK;
}
 
NS_IMETHODIMP
nsViewerApp::GetLanguage(nsString& aLanguage)
{
  aLanguage.SetString("en");
  return NS_OK;
}
 
NS_IMETHODIMP    
nsViewerApp::GetPlatform(nsString& aPlatform)
{
  aPlatform.SetString("Win32");
  return NS_OK;
}

//----------------------------------------

// nsDispatchListener implementation

void
nsViewerApp::AfterDispatch()
{
   NET_PollSockets();
}
