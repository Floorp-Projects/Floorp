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
#include "RPCServerService.h"
#include "nsISupports.h"
#include "nsIJVMManager.h"
#include "nsIRPCTestIn.h"
#include <unistd.h>
#include "IDispatcher.h"
#include "nsIThread.h"

class nsRPCTestInImpl : public  nsIRPCTestIn {
    NS_DECL_ISUPPORTS 
    nsRPCTestInImpl() {
	NS_INIT_REFCNT();
    }

    NS_IMETHOD Test1(PRBool bool) {
        printf("Test1 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test1 %d\n",bool);
	return NS_OK;
    }

    NS_IMETHOD Test2(PRUint8 octet) {
        printf("Test2 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test2 %d\n",octet);
	return NS_OK;
    }

    NS_IMETHOD Test3(PRInt16 sInt) {
        printf("Test3 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test3 %d\n",sInt);
	return NS_OK;
    }

    NS_IMETHOD Test4(PRInt32 lInt) {
        printf("Test4 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test4 %d\n",lInt);
	return NS_OK;
    }

    NS_IMETHOD Test5(PRInt64 llInt) {
        printf("Test5 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test5 %d\n",llInt);
	return NS_OK;
    }

    NS_IMETHOD Test6(PRUint16 usInt) {
        printf("Test6 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test6 %15u\n",usInt);
	return NS_OK;
    }

    NS_IMETHOD Test7(PRUint32 ulInt) {
        printf("Test7 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test7 %15u\n",ulInt);
	return NS_OK;
    }

    NS_IMETHOD Test8(PRUint64 ullInt) {
        printf("Test8 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test8 %15u\n",ullInt);
	return NS_OK;
    }

    NS_IMETHOD Test9(float f) {
        printf("Test9 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test9 %.50f\n",f);
	return NS_OK;
    }

    NS_IMETHOD Test10(double d) {
        printf("Test10 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test10 %.50f\n",d);
	return NS_OK;
    }

    NS_IMETHOD Test11(char c) {
        printf("Test11 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test11 %c\n",c);
	return NS_OK;
    }

    NS_IMETHOD Test12(PRUnichar unic) {
        printf("Test12 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test12 %c\n",unic);
	return NS_OK;
    }

    NS_IMETHOD Test13(const char* s) {
        printf("Test13 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test13 %s\n",s);
	return NS_OK;
    }

//  NS_IMETHOD Test14(PRUnichar* unis) {
//      printf("Test14 this=%p\n", this);
//	printf("--nsRPCTestInImpl::Test14 %s\n",unis);
//	return NS_OK;
//    }
  
};

NS_IMPL_ISUPPORTS(nsRPCTestInImpl, NS_GET_IID(nsIRPCTestIn));
int main(int argc, char **args) {
    printf("%x\n",getpid());
    nsRPCTestInImpl * test1 = new nsRPCTestInImpl();
    nsRPCTestInImpl * test2 = new nsRPCTestInImpl();
    nsRPCTestInImpl * test3 = new nsRPCTestInImpl();
    nsRPCTestInImpl * test4 = new nsRPCTestInImpl();
    nsRPCTestInImpl * test5 = new nsRPCTestInImpl();
    nsRPCTestInImpl * test6 = new nsRPCTestInImpl();
    nsRPCTestInImpl * test7 = new nsRPCTestInImpl();
    nsRPCTestInImpl * test8 = new nsRPCTestInImpl();
    nsRPCTestInImpl * test9 = new nsRPCTestInImpl();
    nsRPCTestInImpl * test10 = new nsRPCTestInImpl();
    nsRPCTestInImpl * test11 = new nsRPCTestInImpl();
    nsRPCTestInImpl * test12 = new nsRPCTestInImpl();
    nsRPCTestInImpl * test13 = new nsRPCTestInImpl();
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

