/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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


/*

  A sample of XPConnect. This file contains an implementation of
  nsIAccountServices.

*/
#include "windows.h"
#include "nscore.h"
#include "nsIAccountServices.h"
#include "nsIMemory.h"
#include "plstr.h"
#include "stdio.h"
#include "nsIAccount.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"


static NS_DEFINE_CID(kAccountCID, NS_Account_CID);

class AccountServicesImpl : public nsIAccountServices
{
public:
    AccountServicesImpl();
    virtual ~AccountServicesImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIAccount interface
    NS_DECL_NSIACCOUNTSERVICES

private:
	nsIAccount *mAccount;
};

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewAccountServices(nsIAccountServices** aAccount)
{
    NS_PRECONDITION(aAccount != nsnull, "null ptr");
    if (! aAccount)
        return NS_ERROR_NULL_POINTER;

    *aAccount = new AccountServicesImpl();
    if (! *aAccount)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aAccount);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////


AccountServicesImpl::AccountServicesImpl()
:	mAccount(nsnull)
{
    NS_INIT_REFCNT();

	nsresult rv = nsServiceManager::GetService(kAccountCID, 
                                    NS_GET_IID(nsIAccount), 
                                    (nsISupports **)&mAccount);
}


AccountServicesImpl::~AccountServicesImpl()
{
}



NS_IMPL_ISUPPORTS1(AccountServicesImpl, nsIAccountServices)


NS_IMETHODIMP
AccountServicesImpl::SetDialerConfig(const char* aPrefix)
{
    NS_PRECONDITION(aPrefix != nsnull, "null ptr");
    if (! aPrefix)
        return NS_ERROR_NULL_POINTER;

    printf("Hey : You are here..........%s \n", aPrefix);
	mAccount->SetDialerConfig((char *)aPrefix);
    return NS_OK;
}

NS_IMETHODIMP
AccountServicesImpl::GetAcctConfig(char** AccountList)
{
	nsString localVar;
	mAccount->GetAcctConfig(localVar);
	*AccountList = PL_strdup(localVar.ToNewCString());

    return NS_OK;
}

NS_IMETHODIMP
AccountServicesImpl::GetModemConfig(char** ModemList)
{
	nsString localVar;
	mAccount->GetModemConfig(localVar);
	*ModemList = PL_strdup(localVar.ToNewCString());

    return NS_OK;
}

NS_IMETHODIMP
AccountServicesImpl::GetSiteName(char** SiteList)
{
	nsString localVar;
	mAccount->GetSiteName(localVar);
	*SiteList = PL_strdup(localVar.ToNewCString());

    return NS_OK;
}

NS_IMETHODIMP
AccountServicesImpl::GetPhone(char** PhoneList)
{
	nsString localVar;
	mAccount->GetPhone(localVar);
	*PhoneList = PL_strdup(localVar.ToNewCString());

    return NS_OK;
}

NS_IMETHODIMP
AccountServicesImpl::LoadValues(void)
{
	mAccount->LoadValues();

	return NS_OK;
}
NS_IMETHODIMP
AccountServicesImpl::CheckForDun(char** dunlist)
{
	nsString localVar;
	mAccount->CheckForDun(localVar);
	*dunlist =PL_strdup(localVar.ToNewCString());
	return NS_OK;
}
