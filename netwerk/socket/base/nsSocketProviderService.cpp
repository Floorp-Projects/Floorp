/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nsString.h"
#include "nsIServiceManager.h"

#include "nsSocketProviderService.h"
#include "nsISocketProvider.h"

static NS_DEFINE_CID(kSocketProviderService, NS_SOCKETPROVIDERSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsSocketProviderService::nsSocketProviderService()
{
  NS_INIT_REFCNT();
}

nsresult
nsSocketProviderService::Init()
{
  return NS_OK;
}

nsSocketProviderService::~nsSocketProviderService()
{
}

NS_METHOD
nsSocketProviderService::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;

  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsSocketProviderService* pSockProvServ = new nsSocketProviderService();
  if (pSockProvServ == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(pSockProvServ);
  rv = pSockProvServ->Init();
  if (NS_FAILED(rv))
    {
    delete pSockProvServ;
    return rv;
    }
  rv = pSockProvServ->QueryInterface(aIID, aResult);
  NS_RELEASE(pSockProvServ);

  return rv;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsSocketProviderService, nsISocketProviderService);

////////////////////////////////////////////////////////////////////////////////

#define MAX_TYPE_LENGTH 128

#define MAX_SOCKET_TYPE_CONTRACTID_LENGTH   (MAX_TYPE_LENGTH + NS_NETWORK_SOCKET_CONTRACTID_PREFIX_LENGTH + 1)

NS_IMETHODIMP
nsSocketProviderService::GetSocketProvider(const char *aSocketType, nsISocketProvider **_result)
{
  nsresult rv;

  char buf[MAX_SOCKET_TYPE_CONTRACTID_LENGTH];

    // STRING USE WARNING: perhaps |contractID| should be an |nsCAutoString| -- scc
  nsAutoString contractID;
  contractID.AssignWithConversion(NS_NETWORK_SOCKET_CONTRACTID_PREFIX);

  contractID.AppendWithConversion(aSocketType);
  contractID.ToCString(buf, MAX_SOCKET_TYPE_CONTRACTID_LENGTH);

  rv = nsServiceManager::GetService(buf, NS_GET_IID(nsISocketProvider), (nsISupports **)_result);
  if (NS_FAILED(rv)) 
      return NS_ERROR_UNKNOWN_SOCKET_TYPE;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
