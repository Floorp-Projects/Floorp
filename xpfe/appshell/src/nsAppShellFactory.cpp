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
#include "nsIFactory.h"
#include "nsRepository.h"
#include "nscore.h"
#include "nsIComponentManager.h"
#include "nsAppShellCIDs.h"
#include "nsICmdLineService.h"
#include "nsIFileLocator.h"
#include "nsINetSupportDialogService.h"
#include "nsIWindowMediator.h"
#include "nsISessionHistory.h"
#include "rdf.h"
#include "nsICommonDialogs.h"
#include "nsIDialogParamBlock.h"
/* extern the factory entry points for each component... */
nsresult NS_NewAppShellServiceFactory(nsIFactory** aFactory);
nsresult NS_NewXPConnectFactoryFactory(nsIFactory** aResult);

nsresult NS_NewDefaultProtocolHelperFactory(nsIFactory** aResult);



static NS_DEFINE_IID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kCmdLineServiceCID,  NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_IID(kProtocolHelperCID,  NS_PROTOCOL_HELPER_CID);
static NS_DEFINE_IID(kXPConnectFactoryCID, NS_XPCONNECTFACTORY_CID);
static NS_DEFINE_IID(kFileLocatorCID,     NS_FILELOCATOR_CID);
static NS_DEFINE_IID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kWindowMediatorCID,				  NS_WINDOWMEDIATOR_CID);
static NS_DEFINE_CID(kSessionHistoryCID,				  NS_SESSION_HISTORY_CID);

static NS_DEFINE_CID(	kCommonDialogsCID, NS_CommonDialog_CID );
static NS_DEFINE_CID( kDialogParamBlockCID, NS_DialogParamBlock_CID );
/*
 * Global entry point to register all components in the registry...
 */
extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* serviceMgr, const char *path)
{
    nsComponentManager::RegisterComponent(kAppShellServiceCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kCmdLineServiceCID,  NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kFileLocatorCID,
                                         NULL, NS_FILELOCATOR_PROGID, path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kProtocolHelperCID,  NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kXPConnectFactoryCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
   	nsComponentManager::RegisterComponent(kNetSupportDialogCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kSessionHistoryCID, NULL, NULL, path, PR_TRUE, PR_TRUE);

    nsComponentManager::RegisterComponent(kWindowMediatorCID,
                                         "window-mediator", NS_RDF_DATASOURCE_PROGID_PREFIX "window-mediator",
                                         path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kCommonDialogsCID, NULL, "component://netscape/appshell/commonDialogs", path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kDialogParamBlockCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
   return NS_OK;
}

/*
 * Global entry point to unregister all components in the registry...
 */
extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* serviceMgr, const char *path)
{
    nsComponentManager::UnregisterComponent(kAppShellServiceCID, path);
    nsComponentManager::UnregisterComponent(kCmdLineServiceCID,  path);
    nsComponentManager::UnregisterComponent(kFileLocatorCID,  path);
    nsComponentManager::UnregisterComponent(kProtocolHelperCID,  path);
    nsComponentManager::UnregisterComponent(kXPConnectFactoryCID, path);
    nsComponentManager::UnregisterComponent(kNetSupportDialogCID, path);
    nsComponentManager::UnregisterComponent(kWindowMediatorCID, path);
    nsComponentManager::UnregisterComponent(kSessionHistoryCID, path);
    nsComponentManager::UnregisterComponent(kCommonDialogsCID, path);
    nsComponentManager::UnregisterComponent(kDialogParamBlockCID, path);
      
    return NS_OK;
}


/*
 * Global entry point to create class factories for the components
 * available withing the DLL...
 */
#if defined(XP_MAC) && defined(MAC_STATIC)
extern "C" NS_APPSHELL nsresult 
NSGetFactory_APPSHELL_DLL(nsISupports* serviceMgr,
                          const nsCID &aClass,
                          const char *aClassName,
                          const char *aProgID,
                          nsIFactory **aFactory)
#else
extern "C" NS_APPSHELL nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
#endif
{
  nsresult rv = NS_ERROR_FACTORY_NOT_REGISTERED;

  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aClass.Equals(kAppShellServiceCID)) {
    rv = NS_NewAppShellServiceFactory(aFactory);
  }
  else if (aClass.Equals(kCmdLineServiceCID)) {
    rv = NS_NewCmdLineServiceFactory(aFactory);
  }
  else if (aClass.Equals(kFileLocatorCID)) {
    rv = NS_NewFileLocatorFactory(aFactory);
  }
  else if (aClass.Equals(kXPConnectFactoryCID)) {
    rv = NS_NewXPConnectFactoryFactory(aFactory);
  }
  else if (aClass.Equals(kProtocolHelperCID)) {
    rv = NS_NewDefaultProtocolHelperFactory(aFactory);
  }
  else if ( aClass.Equals( kNetSupportDialogCID ) )
  {
  	 rv = NS_NewNetSupportDialogFactory(aFactory);
  } 
  else if ( aClass.Equals( kWindowMediatorCID ) )
  {
  	rv = NS_NewWindowMediatorFactory( aFactory );
  }
  else if ( aClass.Equals( kSessionHistoryCID ) )
  {
  	rv = NS_NewSessionHistoryFactory( aFactory );
  }
  else if ( aClass.Equals( kCommonDialogsCID ) )
  {
  	rv = NS_NewCommonDialogsFactory( aFactory );
  }
  else  if ( aClass.Equals( kDialogParamBlockCID ) )
  {
  	rv = NS_NewDialogParamBlockFactory( aFactory );
  }
  return rv;
}

