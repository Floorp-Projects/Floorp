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
#include "RPCClientService.h"
#include "nsISupports.h"
#include "nsISample.h"
#include "nsIRPCTest.h"

extern char* transportName;
int main(int argc, char **args) {
    PRInt32 i;
    if (argc < 2) {
	printf("format clinet <server pid>\n");
	exit(1);
    }
    RPCClientService::Initialize(args[1]);
    RPCClientService * rpcService = RPCClientService::GetInstance();
    nsIID iid;
    nsIRPCTest *t1, *t2;

    rpcService->CreateObjectProxy(1, NS_GET_IID(nsIRPCTest),(nsISupports**)&t1);
    i = 1999;
    printf("--before call %d\n",i);
    t1->Test2(4321,&i);
    printf("--after call %d\n",i);
    rpcService->CreateObjectProxy(5, NS_GET_IID(nsIRPCTest),(nsISupports**)&t2);
    i = 1998;
    printf("____________________________________________\n");
    printf("--before call %d\n",i);
    t2->Test2(4320,&i);
    printf("--after call %d\n",i);
    printf("---------------------------------------------\n");

    char *s1 = "hello";
    char *s2 = "world";
    t1->Test3(s1,&s2);
    printf("s2 %s\n",s2);
    printf("-----------------------------------------------\n");
    PRUint32 arrayCount = 2;
    char * array[2] = {"Hello","Pic"};
    t1->Test4(arrayCount,array);
}
