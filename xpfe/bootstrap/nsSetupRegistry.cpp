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
#include "nsIGlobalHistory.h"
#include "nsIDOMXPConnectFactory.h"
#include "nsAppShellCIDs.h"
#include "nsIFileLocator.h"
#include "nsINetSupportDialogService.h"
#include "nsIEditor.h"

#include "nsAppCoresCIDs.h"
#include "nsIDOMAppCoresManager.h"
#include "nsIDOMBrowserAppCore.h"
#include "nsIDOMEditorAppCore.h"

static NS_DEFINE_IID(kIAppCoresManagerIID, NS_IDOMAPPCORESMANAGER_IID);
static NS_DEFINE_IID(kAppCoresManagerCID,  NS_APPCORESMANAGER_CID);
static NS_DEFINE_IID(kToolkitCoreCID,      NS_TOOLKITCORE_CID);
static NS_DEFINE_IID(kDOMPropsCoreCID,     NS_DOMPROPSCORE_CID);
static NS_DEFINE_IID(kPrefsCoreCID,        NS_PREFSCORE_CID);
static NS_DEFINE_IID(kToolbarCoreCID,      NS_TOOLBARCORE_CID);
static NS_DEFINE_IID(kBrowserAppCoreCID,   NS_BROWSERAPPCORE_CID);
static NS_DEFINE_IID(kEditorAppCoreCID,    NS_EDITORAPPCORE_CID);
static NS_DEFINE_IID(kRDFCoreCID,          NS_RDFCORE_CID);


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
#define APPSHELL_DLL  "libnsappshell.so"
#define APPCORES_DLL  "libappcores.so"
#define EDITOR_DLL "libender.so"

#endif // XP_MAC

#endif // XP_PC

// Class IDs
static NS_DEFINE_IID(kCAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kCCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_IID(kFileLocatorCID,     NS_FILELOCATOR_CID);
static NS_DEFINE_IID(kXPConnectFactoryCID, NS_XPCONNECTFACTORY_CID);
static NS_DEFINE_IID(kGlobalHistoryCID,    NS_GLOBALHISTORY_CID);
static NS_DEFINE_IID(kNetSupportDialogCID,    NS_NETSUPPORTDIALOG_CID);
///static NS_DEFINE_IID(kCBrowserControllerCID, NS_BROWSERCONTROLLER_CID);
//static NS_DEFINE_IID(kIEditFactoryIID, NS_IEDITORFACTORY_IID);


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
  /*
   * Call the standard NS_SetupRegistry() implemented in 
   * webshell/tests/viewer/nsSetupregistry.cpp
   */
  NS_SetupRegistry();

  nsComponentManager::RegisterComponent(kCAppShellServiceCID, NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCCmdLineServiceCID,  NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kFileLocatorCID,  NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kXPConnectFactoryCID, NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kGlobalHistoryCID, NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kNetSupportDialogCID, NULL, NULL, APPSHELL_DLL, PR_TRUE, PR_TRUE);

  nsComponentManager::RegisterComponent(kAppCoresManagerCID,       NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kToolkitCoreCID,    NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kDOMPropsCoreCID,       NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kPrefsCoreCID,       NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kToolbarCoreCID,    NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kBrowserAppCoreCID, NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kEditorAppCoreCID,  NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kRDFCoreCID,     NULL, NULL, APPCORES_DLL, PR_FALSE, PR_FALSE);
  
  //All Editor registration is done in webshell/tests/viewer/nsSetupregistry.cpp

///  nsComponentManager::RegisterComponent(kCBrowserControllerCID, NULL, NULL, BROWSER_DLL, PR_FALSE, PR_FALSE);
}


