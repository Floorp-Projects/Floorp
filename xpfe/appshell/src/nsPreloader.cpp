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
 * Alec Flett <alecf@netscape.com>
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


#include "nsPreloader.h"
#include "nsString.h"
#include "nsIObserver.h"
#include "nsIAppStartupNotifier.h"

struct preloadEntry {
  PRBool isService;
  const char* contractId;
};

static preloadEntry preloadedObjects[] = {
  // necko
  {PR_TRUE, "@mozilla.org/network/io-service;1" },
  // rdf
  {PR_TRUE, "@mozilla.org/rdf/rdf-service;1" },
  // preferences
  {PR_TRUE, "@mozilla.org/preferences-service;1" },
  // string bundles
  {PR_TRUE, "@mozilla.org/intl/stringbundle;1" },
  // docshell
  {PR_FALSE, "@mozilla.org/docshell/html;1" },
  // imglib
  
  // appcomps
  // parser
};

#define PRELOADED_SERVICES_COUNT (sizeof(preloadedObjects) / sizeof(preloadedObjects[0]))

NS_IMPL_ISUPPORTS1(nsPreloader, nsICmdLineHandler)
                   
nsPreloader::nsPreloader()
{
    NS_INIT_ISUPPORTS();
}

nsresult
nsPreloader::Init()
{
    nsresult rv;
    
    nsCOMPtr<nsIObserver> startupNotifier =
      do_CreateInstance(NS_APPSTARTUPNOTIFIER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = startupNotifier->Observe(nsnull, NS_LITERAL_STRING("preloaded-services").get(), nsnull);

    return rv;
}

nsPreloader::~nsPreloader()
{
}

CMDLINEHANDLER1_IMPL(nsPreloader, // classname
                     "-server", // arguments
                     "general.startup.server",     // pref name (not used)
                     "Load and stay resident!", // command line help text
                     PR_FALSE,   // handles window arguments?
                     "",     // no window arguments
                     PR_FALSE)   // don't open a window
  
CMDLINEHANDLER_GETCHROMEURL_IMPL(nsPreloader,nsnull)
CMDLINEHANDLER_GETDEFAULTARGS_IMPL(nsPreloader,"")
                                   
NS_METHOD
nsPreloader::RegisterProc(nsIComponentManager *aCompMgr, nsIFile *aPath,
                          const char *registryLocation,
                          const char *componentType,
                          const nsModuleComponentInfo *info)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman =
                do_GetService("@mozilla.org/categorymanager;1", &rv);
  if (NS_FAILED(rv)) return rv;

  rv = catman->AddCategoryEntry(COMMAND_LINE_ARGUMENT_HANDLERS,
                                "Preloader Commandline Handler",
                                NS_PRELOADER_CONTRACTID,
                                PR_TRUE, PR_TRUE, nsnull);
  if (NS_FAILED(rv)) return rv;

  // now register all the well-known services

  PRInt32 i;
  for (i=0; i<PRELOADED_SERVICES_COUNT; i++) {
    nsCAutoString categoryEntry;
    
    if (preloadedObjects[i].isService)
      categoryEntry.Append("service,");
    
    categoryEntry.Append(preloadedObjects[i].contractId);

    catman->AddCategoryEntry("preloaded-services",
                             categoryEntry.get(), // key - must be unique
                             categoryEntry.get(), // value - the contractid
                             PR_TRUE, PR_TRUE, nsnull);
  }
  
  return NS_OK;
}

NS_METHOD
nsPreloader::UnregisterProc(nsIComponentManager *aCompMgr,
                          nsIFile *aFile, const char *registryLocation,
                          const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman = do_GetService("@mozilla.org/categorymanager;1", &rv);
    if (NS_FAILED(rv)) return rv;

    rv = catman->DeleteCategoryEntry(COMMAND_LINE_ARGUMENT_HANDLERS,
                                     NS_PRELOADER_CONTRACTID,
                                     PR_TRUE);

    // unregister well-known services?
    
    return NS_OK;
}
