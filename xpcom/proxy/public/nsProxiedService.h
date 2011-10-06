/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef nsProxiedService_h__
#define nsProxiedService_h__

#include "nsServiceManagerUtils.h"
#include "nsIProxyObjectManager.h"
#include "nsXPCOMCIDInternal.h"

////////////////////////////////////////////////////////////////////////////////
// NS_WITH_PROXIED_SERVICE: macro to make using services that need to be proxied
//                                   before using them easier. 
// Now you can replace this:
// {
//      nsresult rv;
//      nsCOMPtr<nsIMyService> pIMyService = 
//               do_GetService(kMyServiceCID, &rv);
//      if(NS_FAILED(rv))
//          return;
//      nsCOMPtr<nsIProxyObjectManager> pIProxyObjectManager = 
//               do_GetService(kProxyObjectManagerCID, &rv);
//      if(NS_FAILED(rv))
//          return;
//      nsIMyService pIProxiedObject = NULL;
//      rv = pIProxyObjectManager->GetProxyForObject(pIDispatchTarget, 
//                                                   NS_GET_IID(nsIMyService), 
//                                                   pIMyService, NS_PROXY_SYNC,
//                                                   (void**)&pIProxiedObject);
//      pIProxiedObject->DoIt(...);  // Executed on same thread as pIProxyQueue
//      ...
//      pIProxiedObject->Release();  // Must be done as not managed for you.
//      }
//  with this:
//      {
//      nsresult rv;
//      NS_WITH_PROXIED_SERVICE(nsIMyService, pIMyService, kMyServiceCID, 
//                              pIDispatchTarget, &rv);
//      if(NS_FAILED(rv))
//          return;
//      pIMyService->DoIt(...);  // Executed on the same thread as pIProxyQueue
//      }
// and the automatic destructor will take care of releasing the service and
// the proxied object for you. 
// 
// Note that this macro requires you to link with the xpcom DLL to pick up the
// static member functions from nsServiceManager.

#define NS_WITH_PROXIED_SERVICE(T, var, cid, Q, rvAddr)     \
    nsProxiedService _serv##var(cid, NS_GET_IID(T), Q, PR_FALSE, rvAddr);     \
    T* var = (T*)(nsISupports*)_serv##var;

#define NS_WITH_ALWAYS_PROXIED_SERVICE(T, var, cid, Q, rvAddr)     \
    nsProxiedService _serv##var(cid, NS_GET_IID(T), Q, PR_TRUE, rvAddr);       \
    T* var = (T*)(nsISupports*)_serv##var;

////////////////////////////////////////////////////////////////////////////////
// nsProxiedService
////////////////////////////////////////////////////////////////////////////////

class NS_STACK_CLASS nsProxiedService
{
public:
    nsProxiedService(const nsCID &aClass, const nsIID &aIID, 
                     nsIEventTarget* aTarget, PRBool always, nsresult* rv)
    {
        nsCOMPtr<nsISupports> svc = do_GetService(aClass, rv);
        if (NS_SUCCEEDED(*rv))
            InitProxy(svc, aIID, aTarget, always, rv);
    }

    nsProxiedService(const char* aContractID, const nsIID &aIID, 
                     nsIEventTarget* aTarget, PRBool always, nsresult* rv)
    {
        nsCOMPtr<nsISupports> svc = do_GetService(aContractID, rv);
        if (NS_SUCCEEDED(*rv))
            InitProxy(svc, aIID, aTarget, always, rv);
    }
    
    operator nsISupports*() const
    {
        return mProxiedService;
    }

private:

    void InitProxy(nsISupports *aObj, const nsIID &aIID,
                   nsIEventTarget* aTarget, PRBool always, nsresult*rv)
    {
        PRInt32 proxyType = NS_PROXY_SYNC;
        if (always)
            proxyType |= NS_PROXY_ALWAYS;

        nsCOMPtr<nsIProxyObjectManager> proxyObjMgr = do_GetService(NS_XPCOMPROXY_CONTRACTID, rv);
        if (NS_FAILED(*rv))
          return;

        *rv = proxyObjMgr->GetProxyForObject(aTarget,
                                             aIID,
                                             aObj,
                                             proxyType,
                                             getter_AddRefs(mProxiedService));
    }

    nsCOMPtr<nsISupports> mProxiedService;
};

#endif // nsProxiedService_h__
