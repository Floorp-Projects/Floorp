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

#define NS_IMPL_IDS
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "prprf.h"
#include <iostream.h>
#include <iomanip.h>   // needed for libstdc++-v3
#include "nsWeakReference.h"

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
    const char *cstr = ToNewCString(str);
    s << cstr;
    delete [] (char*)cstr;
    return s;
}

class TestObserver : public nsIObserver, public nsSupportsWeakReference {
public:
    TestObserver( const nsString &name = NS_ConvertASCIItoUCS2("unknown") )
        : mName( name ) {
        NS_INIT_REFCNT();
    }
    virtual ~TestObserver() {}
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    nsString mName;
};

NS_IMPL_ISUPPORTS2( TestObserver, nsIObserver, nsISupportsWeakReference );

NS_IMETHODIMP
TestObserver::Observe( nsISupports     *aSubject,
                       const PRUnichar *aTopic,
                       const PRUnichar *someData ) {
    nsString topic( aTopic );
    nsString data( someData );
    	/*
    		The annoying double-cast below is to work around an annoying bug in
    		the compiler currently used on wensleydale.  This is a test.
    	*/
    cout << mName << " has observed something: subject@" << (void*)aSubject
         << " name=" << NS_REINTERPRET_CAST(TestObserver*, NS_REINTERPRET_CAST(void*, aSubject))->mName
         << " aTopic=" << topic
         << " someData=" << data << endl;
    return NS_OK;
}

int main(int argc, char *argv[])
{
    nsString topicA; topicA.AssignWithConversion( "topic-A" );
    nsString topicB; topicB.AssignWithConversion( "topic-B" );
    nsresult rv;

    nsresult res = nsComponentManager::CreateInstance(NS_OBSERVERSERVICE_CONTRACTID,
                                                NULL,
                                                 NS_GET_IID(nsIObserverService),
                                                (void **) &anObserverService);
	
    if (res == NS_OK) {

        nsIObserver *anObserver;
        nsIObserver *aObserver = new TestObserver(NS_ConvertASCIItoUCS2("Observer-A"));
        aObserver->AddRef();
        nsIObserver *bObserver = new TestObserver(NS_ConvertASCIItoUCS2("Observer-B"));
        bObserver->AddRef();
            
        cout << "Adding Observer-A as observer of topic-A..." << endl;
        rv = anObserverService->AddObserver(aObserver, topicA.get());
        testResult(rv);
 
        cout << "Adding Observer-B as observer of topic-A..." << endl;
        rv = anObserverService->AddObserver(bObserver, topicA.get());
        testResult(rv);
 
        cout << "Adding Observer-B as observer of topic-B..." << endl;
        rv = anObserverService->AddObserver(bObserver, topicB.get());
        testResult(rv);

        cout << "Testing Notify(observer-A, topic-A)..." << endl;
        rv = anObserverService->Notify( aObserver,
                                   topicA.get(),
                                   NS_ConvertASCIItoUCS2("Testing Notify(observer-A, topic-A)").get() );
        testResult(rv);

        cout << "Testing Notify(observer-B, topic-B)..." << endl;
        rv = anObserverService->Notify( bObserver,
                                   topicB.get(),
                                   NS_ConvertASCIItoUCS2("Testing Notify(observer-B, topic-B)").get() );
        testResult(rv);
 
        cout << "Testing EnumerateObserverList (for topic-A)..." << endl;
        nsIEnumerator* e;
        rv = anObserverService->EnumerateObserverList(topicA.get(), &e);
        testResult(rv);

        cout << "Enumerating observers of topic-A..." << endl;
        if ( NS_SUCCEEDED( rv ) ) {
            nsISupports *inst;
    
            for (e->First(); e->IsDone() != NS_OK; e->Next()) {
                rv = e->CurrentItem(&inst);
                if (NS_SUCCEEDED(rv)) {
                  rv = inst->QueryInterface(NS_GET_IID(nsIObserver),(void**)&anObserver);
                  cout << "Calling observe on enumerated observer "
                        << NS_REINTERPRET_CAST(TestObserver*, NS_REINTERPRET_CAST(void*, inst))->mName << "..." << endl;
                  rv = anObserver->Observe( inst, topicA.get(), NS_ConvertASCIItoUCS2("during enumeration").get() );
                  testResult(rv);
                }
            }
        }
        cout << "...done enumerating observers of topic-A" << endl;

        cout << "Removing Observer-A..." << endl;
        rv = anObserverService->RemoveObserver(aObserver, topicA.get());
        testResult(rv);


        cout << "Removing Observer-B (topic-A)..." << endl;
        rv = anObserverService->RemoveObserver(bObserver, topicB.get());
        testResult(rv);
        cout << "Removing Observer-B (topic-B)..." << endl;
        rv = anObserverService->RemoveObserver(bObserver, topicA.get());
        testResult(rv);
       
    }
    return NS_OK;
}
