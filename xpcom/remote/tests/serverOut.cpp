/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include <stdio.h>
#include <stream.h>
#include "RPCServerService.h"
#include "nsISupports.h"
#include "nsIJVMManager.h"
#include "nsIRPCTestOut.h"
#include <unistd.h>
#include "IDispatcher.h"
#include "nsIThread.h"

class nsRPCTestOutImpl : public  nsIRPCTestOut {
    NS_DECL_ISUPPORTS 
    nsRPCTestOutImpl() {
	NS_INIT_REFCNT();
    }

    NS_IMETHOD Test1(PRBool *bool) {
        printf("Test1 this=%p\n", this);
	printf("--nsRPCTestOutImpl::Test1 %d\n",*bool);
        *bool = PR_FALSE;
	return NS_OK;
    }

    NS_IMETHOD Test2(PRUint8 *octet) {
        printf("Test2 this=%p\n", this);
	printf("--nsRPCTestOutImpl::Test2 %d\n",*octet);
        *octet = 88; 
	return NS_OK;
    }

    NS_IMETHOD Test3(PRInt16 *sInt) {
        printf("Test3 this=%p\n", this);
	printf("--nsRPCTestOutImpl::Test3 %d\n",*sInt);
        *sInt = 9999;
	return NS_OK;
    }

    NS_IMETHOD Test4(PRInt32 *lInt) {
        printf("Test4 this=%p\n", this);
	printf("--nsRPCTestOutImpl::Test4 %d\n",*lInt);
        *lInt = 99999;
	return NS_OK;
    }

    NS_IMETHOD Test5(PRInt64 *llInt) {
        printf("Test5 this=%p\n", this);
	printf("--nsRPCTestOutImpl::Test5 %d\n",*llInt);
        *llInt = 999999;
	return NS_OK;
    }

    NS_IMETHOD Test6(PRUint16 *usInt) {
        printf("Test6 this=%p\n", this);
	printf("--nsRPCTestOutImpl::Test6 %15u\n",*usInt);
        *usInt = 9999;
	return NS_OK;
    }

    NS_IMETHOD Test7(PRUint32 *ulInt) {
        printf("Test7 this=%p\n", this);
	printf("--nsRPCTestOutImpl::Test7 %15u\n",*ulInt);
        *ulInt = 99999;
	return NS_OK;
    }

    NS_IMETHOD Test8(PRUint64 *ullInt) {
        printf("Test8 this=%p\n", this);
	printf("--nsRPCTestOutImpl::Test8 %15u\n",*ullInt);
        *ullInt = 9999999;
	return NS_OK;
    }

    NS_IMETHOD Test9(float *f) {
        printf("Test9 this=%p\n", this);
	printf("--nsRPCTestOutImpl::Test9 %.50f\n",*f);
        *f = 999.999;
	return NS_OK;
    }

    NS_IMETHOD Test10(double *d) {
        printf("Test10 this=%p\n", this);
	printf("--nsRPCTestOutImpl::Test10 %.50f\n",*d);
        *d = 9999999.9999999;
	return NS_OK;
    }

    NS_IMETHOD Test11(char *c) {
        printf("Test11 this=%p\n", this);
	printf("--nsRPCTestOutImpl::Test11 %c\n",*c);
        *c = 'G';
	return NS_OK;
    }

    NS_IMETHOD Test12(PRUnichar *unic) {
        printf("Test12 this=%p\n", this);
	printf("--nsRPCTestOutImpl::Test12 %c\n",*unic);
        *unic = 'G';
	return NS_OK;
    }

    NS_IMETHOD Test13(char **s) {
        printf("Test13 this=%p\n", this);
	printf("--nsRPCTestOutImpl::Test13 %s\n",*s);
        *s = "remote ipc";
	return NS_OK;
    }

//  NS_IMETHOD Test14(PRUnichar **unis) {
//      printf("Test14 this=%p\n", this);
//	printf("--nsRPCTestOutImpl::Test14 %s\n",*unis);
//	return NS_OK;
//    }

};

NS_IMPL_ISUPPORTS(nsRPCTestOutImpl, NS_GET_IID(nsIRPCTestOut));
int main(int argc, char **args) {
    printf("%x\n",getpid());
    nsRPCTestOutImpl * test1 = new nsRPCTestOutImpl();
    nsRPCTestOutImpl * test2 = new nsRPCTestOutImpl();
    nsRPCTestOutImpl * test3 = new nsRPCTestOutImpl();
    nsRPCTestOutImpl * test4 = new nsRPCTestOutImpl();
    nsRPCTestOutImpl * test5 = new nsRPCTestOutImpl();
    nsRPCTestOutImpl * test6 = new nsRPCTestOutImpl();
    nsRPCTestOutImpl * test7 = new nsRPCTestOutImpl();
    nsRPCTestOutImpl * test8 = new nsRPCTestOutImpl();
    nsRPCTestOutImpl * test9 = new nsRPCTestOutImpl();
    nsRPCTestOutImpl * test10 = new nsRPCTestOutImpl();
    nsRPCTestOutImpl * test11 = new nsRPCTestOutImpl();
    nsRPCTestOutImpl * test12 = new nsRPCTestOutImpl();
    nsRPCTestOutImpl * test13 = new nsRPCTestOutImpl();
    RPCServerService * rpcService = RPCServerService::GetInstance();
    IDispatcher *dispatcher;
    rpcService->GetDispatcher(&dispatcher);
    dispatcher->RegisterWithOID(test1, 1);
    dispatcher->RegisterWithOID(test2, 5);
    dispatcher->RegisterWithOID(test3, 7);
    dispatcher->RegisterWithOID(test4, 9);
    dispatcher->RegisterWithOID(test5, 11);
    dispatcher->RegisterWithOID(test6, 13);
    dispatcher->RegisterWithOID(test7, 15);
    dispatcher->RegisterWithOID(test8, 17);
    dispatcher->RegisterWithOID(test9, 19);
    dispatcher->RegisterWithOID(test10, 21);
    dispatcher->RegisterWithOID(test11, 23);
    dispatcher->RegisterWithOID(test12, 25);
    dispatcher->RegisterWithOID(test13, 27);
    while(1) {
	PR_Sleep(100);
    }
    
}

