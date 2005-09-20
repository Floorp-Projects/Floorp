/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is SSR
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsXPCOM.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsIObserver.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "nsICategoryManager.h"

#include "string.h"
#include "nsNetUtil.h"

#include "nsIStyleSheetService.h"


// TODO auto reload SSR in C.

class nsSSRSupport : public nsIObserver
{
public:  
  nsSSRSupport();  
  virtual ~nsSSRSupport();  
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsresult SetSSREnabled(PRBool aSsrEnabled);
  nsresult SetSiteSSREnabled(PRBool aSiteSSREnabled);

  nsCOMPtr<nsIStyleSheetService> m_sss;

  PRBool mUsingSSR;
  PRBool mUsingSiteSSR;
};

nsSSRSupport::nsSSRSupport()  
  : mUsingSiteSSR(PR_FALSE), mUsingSSR(PR_FALSE)
{
  m_sss = do_GetService("@mozilla.org/content/style-sheet-service;1");
}  

nsSSRSupport::~nsSSRSupport()  
{
}

NS_IMPL_ISUPPORTS1(nsSSRSupport, nsIObserver)

nsresult
nsSSRSupport::SetSSREnabled(PRBool aSsrEnabled)
{
  if (!m_sss)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING("resource://gre/res/smallScreen.css"));
  if (NS_FAILED(rv))
    return rv;

  if (aSsrEnabled)
    return m_sss->LoadAndRegisterSheet(uri, nsIStyleSheetService::AGENT_SHEET);

  return m_sss->UnregisterSheet(uri, nsIStyleSheetService::AGENT_SHEET);
}

nsresult
nsSSRSupport::SetSiteSSREnabled(PRBool aSiteSSREnabled)
{
  if (!m_sss)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING("resource://gre/res/sites.css"));
  if (NS_FAILED(rv))
    return rv;

  if (aSiteSSREnabled)
    return m_sss->LoadAndRegisterSheet(uri, nsIStyleSheetService::AGENT_SHEET);

  return m_sss->UnregisterSheet(uri, nsIStyleSheetService::AGENT_SHEET);
}

NS_IMETHODIMP
nsSSRSupport::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  nsresult rv;
  
  if (!strcmp(aTopic,"app-startup")) 
  {
    nsCOMPtr<nsIPrefBranch2> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    prefBranch->AddObserver("ssr.", this, PR_FALSE);

    return NS_OK;
  }
 
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) 
  {
    nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
    nsXPIDLCString cstr;
    
    const char* pref = NS_ConvertUCS2toUTF8(aData).get();
    
    if (!strcmp(pref, "ssr.enabled"))
    {
      PRBool enabled;
      prefBranch->GetBoolPref(pref, &enabled);

      SetSSREnabled(enabled);
      return NS_OK;
    }

    if (!strcmp(pref, "ssr.site.enabled"))
    {
      PRBool enabled;
      prefBranch->GetBoolPref(pref, &enabled);

      SetSiteSSREnabled(enabled);
      return NS_OK;
    }
  }
  return NS_OK;
}


//------------------------------------------------------------------------------
//  XPCOM REGISTRATION BELOW
//------------------------------------------------------------------------------

#define SSRSupport_CID \
{  0x3dc8b3d0, 0xeaff, 0x4904, \
{0x8e, 0x80, 0x99, 0xf8, 0x26, 0x62, 0x96, 0x3a} }

#define SSRSupport_ContractID "@mozilla.org/ssr;1"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSSRSupport)


static NS_METHOD SSRRegistration(nsIComponentManager *aCompMgr,
                                 nsIFile *aPath,
                                 const char *registryLocation,
                                 const char *componentType,
                                 const nsModuleComponentInfo *info)
{
  nsresult rv;
  
  nsCOMPtr<nsIServiceManager> servman = do_QueryInterface((nsISupports*)aCompMgr, &rv);
  if (NS_FAILED(rv))
    return rv;
  
  
  nsCOMPtr<nsICategoryManager> catman;
  servman->GetServiceByContractID(NS_CATEGORYMANAGER_CONTRACTID, 
                                  NS_GET_IID(nsICategoryManager), 
                                  getter_AddRefs(catman));
  
  if (NS_FAILED(rv))
    return rv;
  
  char* previous = nsnull;
  rv = catman->AddCategoryEntry("app-startup",
                                "SSR", 
                                SSRSupport_ContractID,
                                PR_TRUE, 
                                PR_TRUE, 
                                &previous);
  if (previous)
    nsMemory::Free(previous);
  
  return rv;
}

static NS_METHOD SSRUnregistration(nsIComponentManager *aCompMgr,
                                   nsIFile *aPath,
                                   const char *registryLocation,
                                   const nsModuleComponentInfo *info)
{
  nsresult rv;
  
  nsCOMPtr<nsIServiceManager> servman = do_QueryInterface((nsISupports*)aCompMgr, &rv);
  if (NS_FAILED(rv))
    return rv;
  
  nsCOMPtr<nsICategoryManager> catman;
  servman->GetServiceByContractID(NS_CATEGORYMANAGER_CONTRACTID, 
                                  NS_GET_IID(nsICategoryManager), 
                                  getter_AddRefs(catman));
  
  if (NS_FAILED(rv))
    return rv;
  
  rv = catman->DeleteCategoryEntry("app-startup",
                                   "SSR", 
                                   PR_TRUE);
  
  return rv;
}

static const nsModuleComponentInfo components[] =
{
  { "SoftKeyBoardService", 
    SSRSupport_CID, 
    SSRSupport_ContractID,
    nsSSRSupportConstructor,
    SSRRegistration,
    SSRUnregistration
  }
  
};

NS_IMPL_NSGETMODULE(SSRModule, components)
