/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 3 -*- */
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

#ifndef __nsProxiedServiceManager_h_
#define __nsProxiedServiceManager_h_

#include "nsIServiceManager.h"
#include "nsIProxyObjectManager.h"

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
//      rv = pIProxyObjectManager->GetProxyForObject(pIProxyQueue, 
//                                                              NS_GET_IID(nsIMyService), 
//                                                              pIMyService, PROXY_SYNC,
//                                                              (void**)&pIProxiedObject);
//      pIProxiedObject->DoIt(...);  // Executed on same thread as pIProxyQueue
//      ...
//      pIProxiedObject->Release();  // Must be done as not managed for you.
//      }
//  with this:
//      {
//      nsresult rv;
//      NS_WITH_PROXIED_SERVICE(nsIMyService, pIMyService, kMyServiceCID, 
//                                      pIProxyQueue, &rv);
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

class nsProxiedService
{
 public:
   
    nsProxiedService(const nsCID &aClass, const nsIID &aIID, 
                     nsIEventQueue* pIProxyQueue, PRBool always, nsresult*rv)
    {
       static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);


       *rv = nsServiceManager::GetService(aClass, 
                                          aIID, 
                                          getter_AddRefs(mService));
       if (NS_FAILED(*rv)) return;

       nsCOMPtr<nsIProxyObjectManager> pIProxyObjectManager = 
                do_GetService(kProxyObjectManagerCID, rv);
       if (NS_FAILED(*rv)) return;

       PRInt32 proxyType = PROXY_SYNC;
       if (always) proxyType |= PROXY_ALWAYS;
       *rv = pIProxyObjectManager->GetProxyForObject(pIProxyQueue, 
                                                  aIID, 
                                                  mService,
                                                  proxyType, 
                                                  getter_AddRefs(mProxiedService));
    }


    ~nsProxiedService()
    {
    }

   nsISupports* operator->() const
   {
       NS_PRECONDITION(mProxiedService != 0, "Your code should test the error result from the constructor.");
       return mProxiedService;
   }

   PRBool operator==(const nsISupports* other)
   {
      return ((mProxiedService == other) || (mService == other));
   }

   operator nsISupports*() const
   {
       return mProxiedService;
   }

 protected:
   nsCOMPtr<nsISupports> mProxiedService;
   nsCOMPtr<nsISupports> mService;
 
 };


#endif //__nsProxiedServiceManager_h_
