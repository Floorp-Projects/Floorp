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

#define NS_IMPL_IDS
#include "nsIAppShellService.h"
#include "nsICmdLineService.h"
#include "nsIDOMXPConnectFactory.h"
#include "nsAppShellCIDs.h"
#include "nsINetSupportDialogService.h"
#include "nsIEditor.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsFileSpec.h"
#include "nsFileLocations.h"
#include "nsIFileLocator.h"
#include "nsIFileSpec.h"

#include "nsAppCoresCIDs.h"
#include "nsIDOMAppCoresManager.h"
#include "nsIDOMBrowserAppCore.h"
#include "nsISessionHistory.h"
#include "rdf.h"
#include "nsIWindowMediator.h"

static NS_DEFINE_IID(kIAppCoresManagerIID, NS_IDOMAPPCORESMANAGER_IID);
static NS_DEFINE_IID(kAppCoresManagerCID,  NS_APPCORESMANAGER_CID);
static NS_DEFINE_IID(kToolkitCoreCID,      NS_TOOLKITCORE_CID);
static NS_DEFINE_IID(kDOMPropsCoreCID,     NS_DOMPROPSCORE_CID);
static NS_DEFINE_IID(kProfileCoreCID,      NS_PROFILECORE_CID); 
static NS_DEFINE_IID(kBrowserAppCoreCID,   NS_BROWSERAPPCORE_CID);
static NS_DEFINE_IID(kRDFCoreCID,          NS_RDFCORE_CID);
static NS_DEFINE_IID(kSessionHistoryCID,   NS_SESSION_HISTORY_CID);

#ifdef XP_PC

#define APPSHELL_DLL "nsappshell.dll"
#define BROWSER_DLL  "nsbrowser.dll"
#define APPCORES_DLL  "appcores.dll"
#define EDITOR_DLL "ender.dll"

#else

#ifdef XP_MAC

#define APPSHELL_DLL "APPSHELL_DLL"
#define APPCORES_DLL  "APPCORES_DLL"
#define EDITOR_DLL	"ENDER_DLL"

#else

// XP_UNIX
#define APPSHELL_DLL	"libnsappshell"MOZ_DLL_SUFFIX
#define APPCORES_DLL	"libappcores"MOZ_DLL_SUFFIX
#define EDITOR_DLL	"libender"MOZ_DLL_SUFFIX

#endif // XP_MAC

#endif // XP_PC

// Class IDs
static NS_DEFINE_IID(kCAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kCCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_IID(kFileLocatorCID,     NS_FILELOCATOR_CID);
static NS_DEFINE_IID(kXPConnectFactoryCID, NS_XPCONNECTFACTORY_CID);
static NS_DEFINE_IID(kNetSupportDialogCID,    NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_IID(kProtocolHelperCID,  NS_PROTOCOL_HELPER_CID);
static NS_DEFINE_IID(kWindowMediatorCID,  NS_WINDOWMEDIATOR_CID);
nsresult NS_AutoregisterComponents()
{
  nsIFileSpec* spec = NS_LocateFileOrDirectory(
  							nsSpecialFileSpec::App_ComponentsDirectory);
  if (!spec)
  	return NS_ERROR_FAILURE;

  char *componentsDirPath;
  nsresult rv = spec->GetNSPRPath(&componentsDirPath);
  if (NS_FAILED(rv))
    return rv;

  if (componentsDirPath)
    rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, componentsDirPath);
  NS_RELEASE(spec);
  return rv;
}

/*
 * This evil file will go away when the XPCOM registry can be 
 * externally initialized!
 *
 * Until then, include the real file to keep everything in sync.
 */
#ifdef XP_MAC
#include ":::webshell:tests:viewer:nsSetupRegistry.cpp"
#else
#include "../../webshell/tests/viewer/nsSetupRegistry.cpp"
#endif

extern "C" void
NS_SetupRegistry_1()
{
  nsComponentManager::RegisterComponent(kFileLocatorCID,  NULL, NS_FILELOCATOR_PROGID, APPSHELL_DLL, PR_FALSE, PR_FALSE);

  // This *has* to happen after registration of nsIFileLocator as the call
  // required it.
  //
  // The right thing for autoregistration which causes dynamic component
  // registraion is to happen after all the static RegisterComponents()
  // have been registered. But doing that is causing a coredump with
  // jsdom.dll. Puttin that off until that gets resolved.
  NS_AutoregisterComponents();

  /*
   * Call the standard NS_SetupRegistry() implemented in 
   * webshell/tests/viewer/nsSetupregistry.cpp
   */
  NS_SetupRegistry();

  nsComponentManager::RegisterComponent(kCAppShellServiceCID, NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCCmdLineServiceCID,  NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kXPConnectFactoryCID, NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kNetSupportDialogCID, NULL, NULL, APPSHELL_DLL, PR_TRUE, PR_TRUE);
  nsComponentManager::RegisterComponent(kProtocolHelperCID, NULL, NULL, APPSHELL_DLL, PR_TRUE, PR_TRUE);


  nsComponentManager::RegisterComponent(kAppCoresManagerCID,       NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kToolkitCoreCID,    NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kDOMPropsCoreCID,       NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kProfileCoreCID,       NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE); 
  nsComponentManager::RegisterComponent(kBrowserAppCoreCID, NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kRDFCoreCID,     NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kSessionHistoryCID,     NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kWindowMediatorCID,
                                         "window-mediator", NS_RDF_DATASOURCE_PROGID_PREFIX "window-mediator",
                                         APPSHELL_DLL, PR_FALSE, PR_FALSE);
  //All Editor registration is done in webshell/tests/viewer/nsSetupregistry.cpp

}




