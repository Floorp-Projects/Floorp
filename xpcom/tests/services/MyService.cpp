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

#include "MyService.h"
#include "nsIServiceManager.h"
#include <stdio.h>
#include "nsIFactory.h"

////////////////////////////////////////////////////////////////////////////////

class MyService : public IMyService {
public:

    NS_IMETHOD
    Doit(void);

    MyService(nsISupports* outer);
    virtual ~MyService(void);

    NS_DECL_ISUPPORTS

};

////////////////////////////////////////////////////////////////////////////////

class MyServiceFactory : public nsIFactory {
public:

    NS_DECL_ISUPPORTS

    // nsIFactory methods:

    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              REFNSIID aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

    // MyService methods:

    MyServiceFactory(void);
    virtual ~MyServiceFactory(void);

    PRBool CanUnload(void) { return mOutstandingInstances == 0; }

    friend class MyService;

protected:
    PRBool      mStarted;
    PRUint32    mOutstandingInstances;
};

MyServiceFactory* gFact = NULL;

////////////////////////////////////////////////////////////////////////////////
// MyService Implementation

NS_IMPL_ISUPPORTS1(MyService, IMyService);

MyService::MyService(nsISupports* outer)
{
    NS_INIT_REFCNT();
    // incrementing this will keep our factory from getting unloaded
    gFact->mOutstandingInstances++;
    printf("  creating my service\n");
}

MyService::~MyService(void)
{
    // decrementing this will allow our factory to get unloaded
    --gFact->mOutstandingInstances;
    printf("  destroying my service\n");
}

nsresult
MyService::Doit(void)
{
    printf("    invoking my service\n");
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// MyServiceFactory Implementation

NS_IMPL_ISUPPORTS1(MyServiceFactory, nsIFactory);

MyServiceFactory::MyServiceFactory(void)
    : mStarted(PR_FALSE), mOutstandingInstances(0)
{
    NS_INIT_REFCNT();
    printf("initializing my service factory\n");
}

MyServiceFactory::~MyServiceFactory(void)
{
    NS_ASSERTION(mOutstandingInstances == 0, "unloading factory when there are still instances");
    printf("finalizing my service factory\n");
}

nsresult
MyServiceFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    MyService* serv = new MyService(aOuter);
    if (serv == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    return serv->QueryInterface(aIID, aResult);
}

nsresult
MyServiceFactory::LockFactory(PRBool aLock)
{
    mOutstandingInstances += aLock ? 1 : -1;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// DLL Entry Points:

static NS_DEFINE_CID(kIMyServiceCID, NS_IMYSERVICE_CID);

extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aContractID,
             nsIFactory **aFactory)
{
    if (!aClass.Equals(kIMyServiceCID))
        return NS_ERROR_FACTORY_NOT_REGISTERED;
    if (gFact == NULL) {
        printf("loading my service factory\n");
        gFact = new MyServiceFactory();
        if (gFact == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
        gFact->AddRef();    // one for our global
    }
    gFact->AddRef();        // one for the client
    *aFactory = gFact;
    return NS_OK;
}

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* serviceMgr)
{
    if (gFact && gFact->CanUnload()) {
        nsrefcnt cnt = gFact->Release();
        NS_ASSERTION(cnt == 0, "can't release service factory");
        gFact = NULL;
        printf("unloading my service factory\n");
        return PR_TRUE;
    }
    return PR_FALSE;
}

////////////////////////////////////////////////////////////////////////////////
