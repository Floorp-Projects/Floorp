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
#include "nsICommonDialogs.h"
#include "nsIDialogParamBlock.h"

static NS_DEFINE_CID(kAppCoresManagerCID,  NS_APPCORESMANAGER_CID);
static NS_DEFINE_CID(kToolkitCoreCID,      NS_TOOLKITCORE_CID);
static NS_DEFINE_CID(kDOMPropsCoreCID,     NS_DOMPROPSCORE_CID);
static NS_DEFINE_CID(kProfileCoreCID,      NS_PROFILECORE_CID); 
static NS_DEFINE_CID(kBrowserAppCoreCID,   NS_BROWSERAPPCORE_CID);
static NS_DEFINE_CID(kRDFCoreCID,          NS_RDFCORE_CID);
static NS_DEFINE_CID(kSessionHistoryCID,   NS_SESSION_HISTORY_CID);
static NS_DEFINE_CID(	kCommonDialogsCID, NS_CommonDialog_CID );
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

// XP_UNIX || XP_BEOS
#define APPSHELL_DLL	"libnsappshell"MOZ_DLL_SUFFIX
#define APPCORES_DLL	"libappcores"MOZ_DLL_SUFFIX
#define EDITOR_DLL	"libender"MOZ_DLL_SUFFIX

#endif // XP_MAC

#endif // XP_PC

// Class IDs
static NS_DEFINE_CID(kCAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kCCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_CID(kFileLocatorCID,     NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kXPConnectFactoryCID, NS_XPCONNECTFACTORY_CID);
static NS_DEFINE_CID(kNetSupportDialogCID,    NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kProtocolHelperCID,  NS_PROTOCOL_HELPER_CID);
static NS_DEFINE_CID(kWindowMediatorCID,  NS_WINDOWMEDIATOR_CID);
static NS_DEFINE_CID( kDialogParamBlockCID, NS_DialogParamBlock_CID );
nsresult NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                                 NULL /* default */);
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
  NS_AutoregisterComponents();

  /*
   * Call the standard NS_SetupRegistry() implemented in 
   * webshell/tests/viewer/nsSetupregistry.cpp
   */
  NS_SetupRegistry();

  // APPSHELL_DLL
  nsComponentManager::RegisterComponentLib(kFileLocatorCID,      NULL, NS_FILELOCATOR_PROGID, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCAppShellServiceCID, NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCCmdLineServiceCID,  NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kXPConnectFactoryCID, NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kNetSupportDialogCID, NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kProtocolHelperCID,   NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kWindowMediatorCID,
                                         "window-mediator", NS_RDF_DATASOURCE_PROGID_PREFIX "window-mediator",
                                         APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kSessionHistoryCID,   NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
   nsComponentManager::RegisterComponentLib(kCommonDialogsCID,   NULL, "component://netscape/appshell/commonDialogs", APPSHELL_DLL, PR_FALSE, PR_FALSE);
 nsComponentManager::RegisterComponentLib(kDialogParamBlockCID,   NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);

  // APPCORES_DLL
  nsComponentManager::RegisterComponentLib(kAppCoresManagerCID, NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kToolkitCoreCID,     NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kDOMPropsCoreCID,    NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kProfileCoreCID,     NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE); 
  nsComponentManager::RegisterComponentLib(kBrowserAppCoreCID,  NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kRDFCoreCID,         NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);

  //All Editor registration is done in webshell/tests/viewer/nsSetupregistry.cpp

}




