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
#include "nsAppShellCIDs.h"

//#if defined(XP_PC) || defined(XP_MAC)
#include "nsAppCoresCIDs.h"
#include "nsIDOMAppCores.h"
#include "nsIDOMMailCore.h"
#include "nsIDOMBrowserAppCore.h"

static nsIDOMAppCores *gAppCores = NULL;
static NS_DEFINE_IID(kIAppCoresIID,      NS_IDOMAPPCORES_IID);
static NS_DEFINE_IID(kAppCoresCID,       NS_AppCores_CID);
static NS_DEFINE_IID(kMailCoreCID,       NS_MailCore_CID);
static NS_DEFINE_IID(kToolbarCoreCID,    NS_TOOLBARCORE_CID);
static NS_DEFINE_IID(kBrowserAppCoreCID, NS_BROWSERAPPCORE_CID);
//#endif

#ifdef XP_PC

#define APPSHELL_DLL "nsappshell.dll"
#define BROWSER_DLL  "nsbrowser.dll"
#define APPCORES_DLL  "appcores.dll"

#else

#ifdef XP_MAC

#define APPSHELL_DLL "APPSHELL_DLL"
#define APPCORES_DLL  "APPCORES_DLL"

#else

// XP_UNIX
#define APPSHELL_DLL  "libnsappshell.so"
#define APPCORES_DLL  "libappcores.so"

#endif // XP_MAC

#endif // XP_PC

// Class ID's
static NS_DEFINE_IID(kCAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kCCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);
///static NS_DEFINE_IID(kCBrowserControllerCID, NS_BROWSERCONTROLLER_CID);


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

  nsRepository::RegisterFactory(kCAppShellServiceCID, APPSHELL_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCmdLineServiceCID,  APPSHELL_DLL, PR_FALSE, PR_FALSE);

//#if defined(XP_PC) || defined(XP_MAC)
  nsRepository::RegisterFactory(kAppCoresCID,       APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kMailCoreCID,       APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kToolbarCoreCID,    APPCORES_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kBrowserAppCoreCID, APPCORES_DLL, PR_FALSE, PR_FALSE);
//#endif
///  nsRepository::RegisterFactory(kCBrowserControllerCID, BROWSER_DLL, PR_FALSE, PR_FALSE);
}
