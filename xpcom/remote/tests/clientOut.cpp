#include <stdio.h>
#include <stream.h>
#include <fstream.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include "RPCClientService.h"
#include "nsISupports.h"
#include "nsISample.h"
#include "nsIRPCTestOut.h"
#include "deftest.h"
#include "proto.h"

extern char* transportName;
int main(int argc, char **args) {

    nsIID iid;
    nsIRPCTestOut *t1; 
    PRBool b;
    PRUint8 o;
    PRInt16 sInt;
    PRInt32 lInt;
    PRInt64 llInt;
    PRUint16 usInt;
    PRUint32 ulInt;
    PRUint64 ullInt;
    float  f;
    double d;
    char   c;
    PRUnichar wc;
    char* s;
    PRUnichar *ws;
    char spid[20];

    getProcessId(OUT_FDATA, &spid[0]);
    ofstream f_res(CLIENT_OUT_RES);
    if(!f_res) {
       cerr << "Cannot open result file.";
    }

    RPCClientService::Initialize(spid);
    RPCClientService * rpcService = RPCClientService::GetInstance();
    rpcService->CreateObjectProxy(1, NS_GET_IID(nsIRPCTestOut),(nsISupports**)&t1);

    b = PR_TRUE;
    printf("--before call TestOut1 %d\n",b);
    t1->TestOut1(&b);
    printf("--after call TestOut1 %d\n",b);
    f_res << form("TestOut1 %d\n",b);
    printf("____________________________________________\n");

    o = 124;
    printf("--before call TestOut2 %d\n",o);
    t1->TestOut2(&o);
    printf("--after call TestOut2 %d\n",o);
    f_res << form("TestOut2 %o\n",o);
    printf("____________________________________________\n");

    sInt = SHRT_MIN;
    printf("--before call TestOut3 %d\n",sInt);
    t1->TestOut3(&sInt);
    printf("--after call TestOut3 %d\n",sInt);
    f_res << form("TestOut3 %d\n",sInt);
    printf("____________________________________________\n");

    lInt = LONG_MIN;
    printf("--before call TestOut4 %d\n",lInt);
    t1->TestOut4(&lInt);
    printf("--after call TestOut4 %d\n",lInt);
    f_res << form("TestOut4 %d\n",lInt);
    printf("____________________________________________\n");

    llInt = LONG_MIN;
    printf("--before call TestOut5 %d\n",llInt);
    t1->TestOut5(&llInt);
    printf("--after call TestOut5 %d\n",llInt);
    f_res << form("TestOut5 %d\n",llInt);
    printf("____________________________________________\n");

    usInt = USHRT_MIN;
    printf("--before call TestOut6 %u\n",usInt);
    t1->TestOut6(&usInt);
    printf("--after call TestOut6 %u\n",usInt);
    f_res << form("TestOut6 %u\n",usInt);
    printf("____________________________________________\n");

    ulInt = ULONG_MIN;
    printf("--before call TestOut7 %u\n",ulInt);
    t1->TestOut7(&ulInt);
    printf("--after call TestOut7 %u\n",ulInt);
    f_res << form("TestOut7 %u\n",ulInt);
    printf("____________________________________________\n");

    ullInt = ULONG_MIN;
    printf("--before call TestOut8 %u\n",ullInt);
    t1->TestOut8(&ullInt);
    printf("--after call TestOut8 %u\n",ullInt);
    f_res << form("TestOut8 %u\n",ullInt);
    printf("____________________________________________\n");

    f = FLT_MIN;
    printf("--before call TestOut9 %.38f\n",f);
    t1->TestOut9(&f);
    printf("--after call TestOut9 %f\n",f);
    f_res << form("TestOut9 %.50f\n",f);
    printf("____________________________________________\n");

    d = DBL_MIN;
    printf("--before call TestOut10 %.50f\n",d);
    t1->TestOut10(&d);
    printf("--after call TestOut10 %f\n",d);
    f_res << form("TestOut10 %.50f\n",d);
    printf("____________________________________________\n");

    c = 'A';
    printf("--before call TestOut11 %d\n",c);
    t1->TestOut11(&c);
    printf("--after call TestOut11 %d\n",c);
    f_res << form("TestOut11 %c\n",c);
    printf("____________________________________________\n");

    wc = 'A';
    printf("--before call TestOut12 %d\n",wc);
    t1->TestOut12(&wc);
    printf("--after call TestOut12 %d\n",wc);
    f_res << form("TestOut12 %c\n",wc);
    printf("____________________________________________\n");

    s = new char[50];
    printf("--before call TestOut13 %s\n",s);
    t1->TestOut13(&s);
    printf("--after call TestOut13 %s\n",s);
    f_res << form("TestOut13 %s\n",s);
    delete s;
    printf("____________________________________________\n");
/*
    ws = new PRUnichar[50];
    printf("--before call TestOut14 %s\n",ws);
    t1->TestOut14(&ws);
    printf("--after call TestOut14 %s\n",ws);
    f_res << form("TestOut14 %s\n",ws);
    delete ws;
    printf("____________________________________________\n");
*/

    PRUint32 arrCount = 0;
    char *array = new char [200];
    printf("--before call TestOut15 data array.\n");
    t1->TestOut15(&arrCount, &array);
    printf("TestOut15 count %d\n", arrCount);
    f_res << form("TestOut15 count %d\n", arrCount);
    printf("TestOut15 Array=%s\n", array);
    f_res << form("TestOut15 Array=%s\n",array);
    delete array;
    printf("____________________________________________\n");


    printf("--before call TestOut16 combination.\n");
    t1->TestOut16(&b, &c, &o, &sInt, &usInt, &lInt, &ulInt, &llInt, &ullInt, &f, &d, &s);
    printf("After calling TestOut16.\n");

    f_res << form("TestOut16 PRBool %d\n",b);
    f_res << form("TestOut16 PRUint8 %o\n",o);
    f_res << form("TestOut16 PRInt16 %d\n",sInt);
    f_res << form("TestOut16 PRInt32 %d\n",lInt);
    f_res << form("TestOut16 PRInt64 %d\n",llInt);
    f_res << form("TestOut16 PRUint16 %u\n",usInt);
    f_res << form("TestOut16 PRUint32 %u\n",ulInt);
    f_res << form("TestOut16 PRUint64 %u\n",ullInt);
    f_res << form("TestOut16 float %.50f\n",f);
    f_res << form("TestOut16 double %.50f\n",d);
    f_res << form("TestOut16 char %c\n",c);
    f_res << form("TestOut16 string %s\n",s);
    printf("____________________________________________\n");

    PRUint32 arrCount2 = 0;
    PRInt32 *arrLong = new PRInt32 [50];
    printf("--before call TestOut17 data array.\n");
    t1->TestOut17(&arrCount2, &arrLong);
    f_res << form("TestOut17 count %d\n", arrCount2);
    for (int i = 0; i < arrCount2; i++) {
        f_res << form("TestOut17 Array[%d]=%d\n",i,arrLong[i]);
    }
    delete arrLong;

    printf("____________________________________________\n");


    f_res.close();
}
