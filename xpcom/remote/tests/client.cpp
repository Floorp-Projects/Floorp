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
#if 0
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
#endif
    printf("-----------------------------------------------\n");
    PRUint32 arrayCount = 2;
    char * array[2] = {"Hello","Pic"};
    t1->Test4(arrayCount,array);
}
