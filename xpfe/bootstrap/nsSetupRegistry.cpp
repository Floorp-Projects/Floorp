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
#include "nsIFileSpec.h"

#include "rdf.h"
#include "nsIWindowMediator.h"
#include "nsICommonDialogs.h"
#include "nsIDialogParamBlock.h"
// #include "nsAbout.h"
#include "nsIAboutModule.h"
static NS_DEFINE_CID(	kCommonDialogsCID, NS_CommonDialog_CID );
#ifdef XP_OS2

#define WIDGET_DLL      "WDGTOS2"
#define GFXWIN_DLL      "GFX_OS2"
#define VIEW_DLL        "NGVIEW"
#define WEB_DLL         "WEBSHELL"
#define PREF_DLL        "PREF"
#define PARSER_DLL      "HTMLPARS"
#define DOM_DLL         "JSDOM"
#define LAYOUT_DLL      "NGLAYOUT"
#define NETLIB_DLL      "NECKO"
#define EDITOR_DLL      "ENDER"
#define APPSHELL_DLL    "APPSHELL"
#define APPCORES_DLL    "APPCORES"
#define CAPS_DLL        "CAPS"
#define LIVECONNECT_DLL "JSJ"
#define OJI_DLL         "OJI"

#else

#ifdef XP_PC

#define BROWSER_DLL  "nsbrowser.dll"
#define EDITOR_DLL "ender.dll"

#else

#ifdef XP_MAC

#define EDITOR_DLL	"ENDER_DLL"

#else

// XP_UNIX || XP_BEOS
#define EDITOR_DLL	"libender"MOZ_DLL_SUFFIX

#endif // XP_MAC

#endif // XP_PC

#endif // XP_OS2

// Class IDs
static NS_DEFINE_CID(kCAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kCCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_CID(kXPConnectFactoryCID, NS_XPCONNECTFACTORY_CID);
static NS_DEFINE_CID(kNetSupportDialogCID,    NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kProtocolHelperCID,  NS_PROTOCOL_HELPER_CID);
static NS_DEFINE_CID(kWindowMediatorCID,  NS_WINDOWMEDIATOR_CID);
static NS_DEFINE_CID( kDialogParamBlockCID, NS_DialogParamBlock_CID );
#define NS_ABOUT_CID                    \
{ /* {1f1ce501-663a-11d3-b7a0-be426e4e69bc} */         \
0x1f1ce501, 0x663a, 0x11d3, { 0xb7, 0xa0, 0xbe, 0x42, 0x6e, 0x4e, 0x69, 0xbc } \
}
static NS_DEFINE_CID( kAboutModuleCID,      NS_ABOUT_CID);
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
NS_SetupRegistry_1( PRBool needAutoreg )
{
  if ( needAutoreg )
    NS_AutoregisterComponents();

  /*
   * Call the standard NS_SetupRegistry() implemented in 
   * webshell/tests/viewer/nsSetupregistry.cpp
   */
  NS_SetupRegistry();

  //All Editor registration is done in webshell/tests/viewer/nsSetupregistry.cpp

}




