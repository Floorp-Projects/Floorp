/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Mark 'Mook' Yen <mook@songbirdnest.com>
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include <stdio.h>

#include "nsXPCOM.h"
#include "nsXPCOMCIDInternal.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIServiceManager.h"
#include "nsAutoPtr.h"
#include "nsAutoLock.h"
#include "nsCOMPtr.h"

#include "nscore.h"
#include "nspr.h"
#include "prmon.h"

#include "nsITestProxy.h"
#include "nsISupportsPrimitives.h"

#include "nsIRunnable.h"
#include "nsIProxyObjectManager.h"
#include "nsXPCOMCIDInternal.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsISupportsUtils.h"

/*

A quick diagram of how this test works:

This tests for bug 400450, where the creation of a proxy event object
causes a deadlock.  We use a counter and a monitor to make things
be more deterministic.

The leftmost column marks the thread that is running.

M	Create two threads
1	Get a proxy
1		proxy event object created
1		(Proxy object QIs the target)
1			Lock [*]
2	Get a proxy
2		proxy event object created
2		(Proxy object calls QI)
2			Unlock [*]
2		proxy event object stored
2	proxy obtained
1		proxy event object released


 */

/***************************************************************************/
/* ProxyTest                                                               */
/***************************************************************************/

class ProxyTest : public nsIRunnable,
                  public nsITestProxy,
                  public nsISupportsPrimitive
{
public:
    ProxyTest()
        : mCounter(0)
    {}

    NS_IMETHOD Run()
    {
        nsresult rv;
        nsCOMPtr<nsIProxyObjectManager> pom =
            do_GetService(NS_XPCOMPROXY_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        
        nsCOMPtr<nsISupportsPrimitive> prim;
        rv = pom->GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                                    NS_GET_IID(nsISupportsPrimitive),
                                    NS_ISUPPORTS_CAST(nsIRunnable*, this),
                                    NS_PROXY_SYNC | NS_PROXY_ALWAYS,
                                    getter_AddRefs(prim));
        NS_ENSURE_SUCCESS(rv, rv);
        /* we don't actually need to use the proxied object */
        return NS_OK;
    }

    NS_IMETHOD Test(PRInt32 p1, PRInt32 p2, PRInt32 *_retval)
    {
        nsresult rv;

        if (!NS_IsMainThread())
            return NS_ERROR_UNEXPECTED;

        mCounterLock = nsAutoLock::NewLock(__FILE__ " counter lock");
        NS_ENSURE_TRUE(mCounterLock, NS_ERROR_OUT_OF_MEMORY);
        mEvilMonitor = nsAutoMonitor::NewMonitor(__FILE__ " evil monitor");
        NS_ENSURE_TRUE(mEvilMonitor, NS_ERROR_OUT_OF_MEMORY);

        /* note that we don't have an event queue... */

        rv = NS_NewThread(getter_AddRefs(mThreadOne),
                          static_cast<nsIRunnable*>(this));
        NS_ENSURE_SUCCESS(rv, rv);
        rv = NS_NewThread(getter_AddRefs(mThreadTwo),
                          static_cast<nsIRunnable*>(this));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mThreadOne->Shutdown();
        NS_ENSURE_SUCCESS(rv, rv);
        rv = mThreadTwo->Shutdown();
        NS_ENSURE_SUCCESS(rv, rv);

        return NS_OK;
    }

    NS_IMETHOD Test2(void)
    {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD Test3(nsISupports *p1, nsISupports **p2)
    {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD GetType(PRUint16 *_retval)
    {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr)
    {
        NS_ASSERTION(aInstancePtr,
                     "QueryInterface requires a non-NULL destination!");
        nsISupports* foundInterface;
        if ( aIID.Equals(NS_GET_IID(nsIRunnable)) ) {
            foundInterface = static_cast<nsIRunnable*>(this);
        } else if ( aIID.Equals(NS_GET_IID(nsITestProxy)) ) {
            foundInterface = static_cast<nsITestProxy*>(this);
        } else if ( aIID.Equals(NS_GET_IID(nsISupports)) ) {
            foundInterface = NS_ISUPPORTS_CAST(nsIRunnable*, this);
        } else if ( aIID.Equals(NS_GET_IID(nsISupportsPrimitive)) ) {
            {
                nsAutoLock counterLock(mCounterLock);
                switch(mCounter) {
                    case 0:
                        ++mCounter;
                    {
                        /* be evil here and hang */
                        nsAutoUnlock counterUnlock(mCounterLock);
                        nsAutoMonitor evilMonitor(mEvilMonitor);
                        nsresult rv = evilMonitor.Wait();
                        NS_ENSURE_SUCCESS(rv, rv);
                        break;
                    }
                    case 1:
                        ++mCounter;
                    {
                        /* okay, we had our fun, un-hang */
                        nsAutoUnlock counterUnlock(mCounterLock);
                        nsAutoMonitor evilMonitor(mEvilMonitor);
                        nsresult rv = evilMonitor.Notify();
                        NS_ENSURE_SUCCESS(rv, rv);
                        break;
                    }
                    default: {
                        /* nothing special here */
                        ++mCounter;
                    }
                }
                ++mCounter;
            }
            // remeber to admit to supporting this interface
            foundInterface = static_cast<nsISupportsPrimitive*>(this);
        } else {
            foundInterface = nsnull;
        }
        nsresult status;
        if (!foundInterface) {
            status = NS_ERROR_NO_INTERFACE;
        } else {
            NS_ADDREF(foundInterface);
            status = NS_OK;
        }
        *aInstancePtr = foundInterface;
        return status;
    }

    NS_IMETHOD_(nsrefcnt) AddRef(void);
    NS_IMETHOD_(nsrefcnt) Release(void);

protected:
    nsAutoRefCnt mRefCnt;
    NS_DECL_OWNINGTHREAD

private:
    PRLock* mCounterLock;
    PRMonitor* mEvilMonitor;
    PRInt32 mCounter;
    nsCOMPtr<nsIThread> mThreadOne;
    nsCOMPtr<nsIThread> mThreadTwo;
};

NS_IMPL_THREADSAFE_ADDREF(ProxyTest)
NS_IMPL_THREADSAFE_RELEASE(ProxyTest)

int
main(int argc, char **argv)
{
    NS_InitXPCOM2(nsnull, nsnull, nsnull);

    // Scope code so everything is destroyed before we run call NS_ShutdownXPCOM
    {
        nsCOMPtr<nsIComponentRegistrar> registrar;
        NS_GetComponentRegistrar(getter_AddRefs(registrar));
        registrar->AutoRegister(nsnull);

        nsCOMPtr<nsITestProxy> tester = new ProxyTest();
        tester->Test(0, 0, nsnull);
    }

    NS_ShutdownXPCOM(nsnull);

    return 0;
}

