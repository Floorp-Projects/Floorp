/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nscore.h"
#include "nsIComponentManager.h"
#include "nsAppShellCIDs.h"
#include "nsICmdLineService.h"
#include "nsIWindowMediator.h"
#include "rdf.h"
#include "nsAbout.h"
#include "nsIGenericFactory.h"


#include "nsIAppShellService.h"
#include "nsCommandLineService.h"  
#include "nsAppShellService.h"
#include "nsWindowMediator.h"
#include "nsTimingService.h"

#ifdef XP_MAC
#include "nsMacMIMEDataSource.h"
#include "nsInternetConfigService.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacMIMEDataSource);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsInternetConfigService);
#endif

#include "nsUserInfo.h"

/* extern the factory entry points for each component... */
nsresult NS_NewAppShellServiceFactory(nsIFactory** aFactory);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCmdLineService);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAppShellService);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindowMediator);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUserInfo);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimingService);

static nsModuleComponentInfo gAppShellModuleInfo[] =
{
  { "AppShell Service",
    NS_APPSHELL_SERVICE_CID,
    "@mozilla.org/appshell/appShellService;1",
    nsAppShellServiceConstructor,
  },
  { "CommandLine Service",
    NS_COMMANDLINE_SERVICE_CID,
    "@mozilla.org/appshell/commandLineService;1",
    nsCmdLineServiceConstructor,
  },
  { "Window Mediator",
    NS_WINDOWMEDIATOR_CID,
    NS_RDF_DATASOURCE_CONTRACTID_PREFIX "window-mediator",
    nsWindowMediatorConstructor,
  },
  { "kAboutModuleCID",
    NS_ABOUT_CID,
    NS_ABOUT_MODULE_CONTRACTID_PREFIX,
    nsAbout::Create,
  },
  { "User Info Service",
    NS_USERINFO_CID,
    NS_USERINFO_CONTRACTID,
    nsUserInfoConstructor,
  },
  { "Timing Service",
    NS_TIMINGSERVICE_CID,
    NS_TIMINGSERVICE_CONTRACTID,
    nsTimingServiceConstructor,
  },
#if XP_MAC
   { "MacMIME data source",
    NS_NATIVEMIMEDATASOURCE_CID,
    NS_NATIVEMIMEDATASOURCE_CONTRACTID,
    nsMacMIMEDataSourceConstructor,
  },
   { "Internet Config Service",
   NS_INTERNETCONFIGSERVICE_CID,
   NS_INTERNETCONFIGSERVICE_CONTRACTID,
   nsInternetConfigServiceConstructor,
   },
#endif
};

NS_IMPL_NSGETMODULE(appshell, gAppShellModuleInfo)

