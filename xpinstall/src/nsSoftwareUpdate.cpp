/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Townsend <dtownsend@oxymoronical.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "nsIGenericFactory.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsCURILoader.h"

#include "nsSoftwareUpdateIIDs.h"

#include "nsInstallTrigger.h"
#include "nsXPInstallManager.h"

//----------------------------------------------------------------------

// Functions used to create new instances of a given object by the
// generic factory.

NS_GENERIC_FACTORY_CONSTRUCTOR(nsInstallTrigger)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPInstallManager)

//----------------------------------------------------------------------

static NS_METHOD
RegisterInstallTrigger( nsIComponentManager *aCompMgr,
                        nsIFile *aPath,
                        const char *registryLocation,
                        const char *componentType,
                        const nsModuleComponentInfo *info)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsICategoryManager> catman =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString previous;
  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_PROPERTY_CATEGORY,
                                "InstallTrigger",
                                NS_INSTALLTRIGGERCOMPONENT_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// The list of components we register
static const nsModuleComponentInfo components[] =
{
    { "InstallTrigger Component",
       NS_SoftwareUpdateInstallTrigger_CID,
       NS_INSTALLTRIGGERCOMPONENT_CONTRACTID,
       nsInstallTriggerConstructor,
       RegisterInstallTrigger
    },

    { "XPInstall Content Handler",
      NS_SoftwareUpdateInstallTrigger_CID,
      NS_CONTENT_HANDLER_CONTRACTID_PREFIX"application/x-xpinstall",
      nsInstallTriggerConstructor
    },

    { "XPInstallManager Component",
      NS_XPInstallManager_CID,
      NS_XPINSTALLMANAGERCOMPONENT_CONTRACTID,
      nsXPInstallManagerConstructor
    }
};

NS_IMPL_NSGETMODULE(nsSoftwareUpdate, components)
