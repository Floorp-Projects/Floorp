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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (Original Author)
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

#include "nsIGenericFactory.h"

#ifdef XP_WIN
#include "nsAlertsService.h"
#endif
#include "nsToolkitCompsCID.h"
#include "nsDocShellCID.h"
#include "nsAutoCompleteController.h"
#include "nsAutoCompleteMdbResult.h"
#include "nsDownloadManager.h"
#include "nsDownloadProxy.h"
#include "nsExtensionManager.h"
#include "nsFormHistory.h"
#include "nsFormFillController.h"
#include "nsGlobalHistory.h"
#include "nsPasswordManager.h"
#include "nsSingleSignonPrompt.h"

/////////////////////////////////////////////////////////////////////////////

#ifdef XP_WIN
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAlertsService)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAutoCompleteController)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAutoCompleteMdbResult)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDownloadProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsDownloadManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsExtensionManager, Init)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsFormHistory, nsFormHistory::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFormFillController)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsGlobalHistory, Init)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsPasswordManager, nsPasswordManager::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSingleSignonPrompt)

/////////////////////////////////////////////////////////////////////////////
//// Module Destructor

static void PR_CALLBACK nsToolkitCompModuleDtor(nsIModule* self)
{
  nsFormHistory::ReleaseInstance();
  nsPasswordManager::Shutdown();
}

/////////////////////////////////////////////////////////////////////////////

static const nsModuleComponentInfo components[] =
{
#ifdef XP_WIN
  { "Alerts Service",
    NS_ALERTSSERVICE_CID, 
    NS_ALERTSERVICE_CONTRACTID,
    nsAlertsServiceConstructor },
#endif
  { "AutoComplete Controller",
    NS_AUTOCOMPLETECONTROLLER_CID, 
    NS_AUTOCOMPLETECONTROLLER_CONTRACTID,
    nsAutoCompleteControllerConstructor },

  { "AutoComplete Mdb Result",
    NS_AUTOCOMPLETEMDBRESULT_CID, 
    NS_AUTOCOMPLETEMDBRESULT_CONTRACTID,
    nsAutoCompleteMdbResultConstructor },

  { "Download Manager",
    NS_DOWNLOADMANAGER_CID,
    NS_DOWNLOADMANAGER_CONTRACTID,
    nsDownloadManagerConstructor },

  { "Download",
    NS_DOWNLOAD_CID,
    NS_DOWNLOAD_CONTRACTID,
    nsDownloadProxyConstructor },

  { "Extension Manager",
    NS_EXTENSIONMANAGER_CID,
    NS_EXTENSIONMANAGER_CONTRACTID,
    nsExtensionManagerConstructor,
    nsExtensionManager::Register,
    nsnull },

  { "HTML Form History",
    NS_FORMHISTORY_CID, 
    NS_FORMHISTORY_CONTRACTID,
    nsFormHistoryConstructor },

  { "HTML Form Fill Controller",
    NS_FORMFILLCONTROLLER_CID, 
    "@mozilla.org/satchel/form-fill-controller;1",
    nsFormFillControllerConstructor },

  { "HTML Form History AutoComplete",
    NS_FORMFILLCONTROLLER_CID, 
    NS_FORMHISTORYAUTOCOMPLETE_CONTRACTID,
    nsFormFillControllerConstructor },

  { "Global History",
    NS_GLOBALHISTORY_CID,
    NS_GLOBALHISTORY2_CONTRACTID,
    nsGlobalHistoryConstructor },
    
  { "Global History",
    NS_GLOBALHISTORY_CID,
    NS_GLOBALHISTORY_DATASOURCE_CONTRACTID,
    nsGlobalHistoryConstructor },
    
  { "Global History",
    NS_GLOBALHISTORY_CID,
    NS_GLOBALHISTORY_AUTOCOMPLETE_CONTRACTID,
    nsGlobalHistoryConstructor },

  { "Password Manager",
    NS_PASSWORDMANAGER_CID,
    NS_PASSWORDMANAGER_CONTRACTID,
    nsPasswordManagerConstructor,
    nsPasswordManager::Register,
    nsPasswordManager::Unregister },

  { "Single Signon Prompt",
    NS_SINGLE_SIGNON_PROMPT_CID,
    "@mozilla.org/wallet/single-sign-on-prompt;1",
    nsSingleSignonPromptConstructor },
};

NS_IMPL_NSGETMODULE_WITH_DTOR(nsToolkitCompsModule, components, nsToolkitCompModuleDtor)
