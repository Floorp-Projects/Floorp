/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#define NS_IMPL_IDS
#include "nsISupports.h"
#include "nsRepository.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "prprf.h"
#include <iostream.h>

static nsIObserverService *anObserverService = NULL;

static void testResult( nsresult rv ) {
    if ( NS_SUCCEEDED( rv ) ) {
        cout << "...ok" << endl;
    } else {
        cout << "...failed, rv=0x" << hex << (int)rv << endl;
    }
    return;
}

extern ostream &operator<<( ostream &s, nsString &str ) {
    const char *cstr = str.ToNewCString();
    s << cstr;
    delete [] (char*)cstr;
    return s;
}

class TestObserver : public nsIObserver {
public:
    TestObserver( const nsString &name = "unknown" )
        : mName( name ) {
        NS_INIT_REFCNT();
    }
    virtual ~TestObserver() {}
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    nsString mName;
};

NS_IMPL_ISUPPORTS( TestObserver, nsIObserver::GetIID() );

NS_IMETHODIMP
TestObserver::Observe( nsISupports     *aSubject,
                       const PRUnichar *aTopic,
                       const PRUnichar *someData ) {
    nsString topic( aTopic );
    nsString data( someData );
    cout << mName << " has observed something: subject@" << (void*)aSubject
         << " name=" << ((TestObserver*)aSubject)->mName
         << " aTopic=" << topic
         << " someData=" << data << endl;
    return NS_OK;
}

int main(int argc, char *argv[])
{
    nsString topicA( "topic-A" );
    nsString topicB( "topic-B" );
    nsresult rv;

    nsresult res = nsComponentManager::CreateInstance(NS_OBSERVERSERVICE_PROGID,
                                                NULL,
                                                 nsIObserverService::GetIID(),
                                                (void **) &anObserverService);
	
    if (res == NS_OK) {

        nsIObserver *anObserver;
        nsIObserver *aObserver = new TestObserver("Observer-A");
        aObserver->AddRef();
        nsIObserver *bObserver = new TestObserver("Observer-B");
        bObserver->AddRef();
            
        cout << "Adding Observer-A as observer of topic-A..." << endl;
        rv = anObserverService->AddObserver(aObserver, topicA.GetUnicode());
        testResult(rv);
 
        cout << "Adding Observer-B as observer of topic-A..." << endl;
        rv = anObserverService->AddObserver(bObserver, topicA.GetUnicode());
        testResult(rv);
 
        cout << "Adding Observer-B as observer of topic-B..." << endl;
        rv = anObserverService->AddObserver(bObserver, topicB.GetUnicode());
        testResult(rv);

        cout << "Testing Notify(observer-A, topic-A)..." << endl;
        rv = anObserverService->Notify( aObserver,
                                   topicA.GetUnicode(),
                                   nsString("Testing Notify(observer-A, topic-A)").GetUnicode() );
        testResult(rv);

        cout << "Testing Notify(observer-B, topic-B)..." << endl;
        rv = anObserverService->Notify( bObserver,
                                   topicB.GetUnicode(),
                                   nsString("Testing Notify(observer-B, topic-B)").GetUnicode() );
        testResult(rv);
 
        cout << "Testing EnumerateObserverList (for topic-A)..." << endl;
        nsIEnumerator* e;
        rv = anObserverService->EnumerateObserverList(topicA.GetUnicode(), &e);
        testResult(rv);

        cout << "Enumerating observers of topic-A..." << endl;
        if ( NS_SUCCEEDED( rv ) ) {
            nsISupports *inst;
    
            for (e->First(); e->IsDone() != NS_OK; e->Next()) {
                rv = e->CurrentItem(&inst);
                if (NS_SUCCEEDED(rv)) {
                  rv = inst->QueryInterface(nsIObserver::GetIID(),(void**)&anObserver);
                  cout << "Calling observe on enumerated observer "
                        << ((TestObserver*)inst)->mName << "..." << endl;
                  rv = anObserver->Observe( inst, topicA.GetUnicode(), nsString("during enumeration").GetUnicode() );
                  testResult(rv);
                }
            }
        }
        cout << "...done enumerating observers of topic-A" << endl;

        cout << "Removing Observer-A..." << endl;
        rv = anObserverService->RemoveObserver(aObserver, topicA.GetUnicode());
        testResult(rv);


        cout << "Removing Observer-B (topic-A)..." << endl;
        rv = anObserverService->RemoveObserver(bObserver, topicB.GetUnicode());
        testResult(rv);
        cout << "Removing Observer-B (topic-B)..." << endl;
        rv = anObserverService->RemoveObserver(bObserver, topicA.GetUnicode());
        testResult(rv);
       
    }
    return NS_OK;
}
