#include <stdio.h>
#include <stream.h>
#include <fstream.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include "RPCClientService.h"
#include "nsISupports.h"
#include "nsISample.h"
#include "nsIRPCTestInOut.h"
#include "deftest.h"
#include "proto.h"

extern char* transportName;
int main(int argc, char **args) {

    nsIID iid;
    nsIRPCTestInOut *t1; 
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
    int i;

    getProcessId(INOUT_FDATA, &spid[0]);
    ofstream f_res(CLIENT_INOUT_RES);
    if(!f_res) {
       cerr << "Cannot open result file.";
    }

    RPCClientService::Initialize(spid);
    RPCClientService * rpcService = RPCClientService::GetInstance();
    rpcService->CreateObjectProxy(1, NS_GET_IID(nsIRPCTestInOut),(nsISupports**)&t1);


    b = PR_TRUE;
    printf("--before call TestInOut1 %d\n",b);
    f_res << form("TestInOut1 before %d\n",b);
    t1->TestInOut1(&b);
    printf("--after call TestInOut1 %d\n",b);
    f_res << form("TestInOut1 after %d\n",b);
    printf("____________________________________________\n");

    o = 10;
    printf("--before call TestInOut2 %o\n",o);
    f_res << form("TestInOut2 before %o\n",o);
    t1->TestInOut2(&o);
    printf("--after call TestInOut2 %o\n",o);
    f_res << form("TestInOut2 after %o\n",o);
    printf("____________________________________________\n");

    sInt = SHRT_MIN;
    printf("--before call TestInOut3 %d\n",sInt);
    f_res << form("TestInOut3 before %d\n",sInt);
    t1->TestInOut3(&sInt);
    printf("--after call TestInOut3 %d\n",sInt);
    f_res << form("TestInOut3 after %d\n",sInt);
    printf("____________________________________________\n");

    lInt = LONG_MIN;
    printf("--before call TestInOut4 %d\n",lInt);
    f_res << form("TestInOut4 before %l\n",lInt);
    t1->TestInOut4(&lInt);
    printf("--after call TestInOut4 %d\n",lInt);
    f_res << form("TestInOut4 after %l\n",lInt);
    printf("____________________________________________\n");

    llInt = LONG_MIN;
    printf("--before call TestInOut5 %d\n",llInt);
    f_res << form("TestInOut5 before %l\n",llInt);
    t1->TestInOut5(&llInt);
    printf("--after call TestInOut5 %d\n",llInt);
    f_res << form("TestInOut5 after %l\n",llInt);
    printf("____________________________________________\n");

    usInt = 555;
    printf("--before call TestInOut6 %u\n",usInt);
    f_res << form("TestInOut6 before %10u\n",usInt);
    t1->TestInOut6(&usInt);
    printf("--after call TestInOut6 %u\n",usInt);
    f_res << form("TestInOut6 after %10u\n",usInt);
    printf("____________________________________________\n");

    ulInt = 555;
    printf("--before call TestInOut7 %u\n",ulInt);
    f_res << form("TestInOut7 before %10u\n",ulInt);
    t1->TestInOut7(&ulInt);
    printf("--after call TestInOut7 %u\n",ulInt);
    f_res << form("TestInOut7 after %10u\n",ulInt);
    printf("____________________________________________\n");

    ullInt = 555;
    printf("--before call TestInOut8 %u\n",ullInt);
    f_res << form("TestInOut8 before %10u\n",ullInt);
    t1->TestInOut8(&ullInt);
    printf("--after call TestInOut8 %u\n",ullInt);
    f_res << form("TestInOut8 after %10u\n",ullInt);
    printf("____________________________________________\n");

    f = FLT_MIN;
    printf("--before call TestInOut9 %.50f\n",f);
    f_res << form("TestInOut9 before %.50f\n",f);
    t1->TestInOut9(&f);
    printf("--after call TestInOut9 %.50f\n",f);
    f_res << form("TestInOut9 after %.50f\n",f);
    printf("____________________________________________\n");

    d = DBL_MIN;
    printf("--before call TestInOut10 %.50f\n",d);
    f_res << form("TestInOut10 before %.50f\n",d);
    t1->TestInOut10(&d);
    printf("--after call TestInOut10 %.50f\n",d);
    f_res << form("TestInOut10 after %.50f\n",d);
    printf("____________________________________________\n");

    c = 'A';
    printf("--before call TestInOut11 %d\n",c);
    f_res << form("TestInOut11 before %c\n",c);
    t1->TestInOut11(&c);
    printf("--after call TestInOut11 %d\n",c);
    f_res << form("TestInOut11 after %c\n",c);
    printf("____________________________________________\n");

    wc = 'A';
    printf("--before call TestInOut12 %d\n",wc);
    f_res << form("TestInOut12 before %c\n",wc);
    t1->TestInOut12(&wc);
    printf("--after call TestInOut12 %d\n",wc);
    f_res << form("TestInOut12 after %c\n",wc);
    printf("____________________________________________\n");

    s = "This is a test.";
    printf("--before call TestInOut13 %s\n",s);
    f_res << form("TestInOut13 before %s\n",s);
    t1->TestInOut13(&s);
    printf("--before call TestInOut13 %s\n",s);
    f_res << form("TestInOut13 after %s\n",s);
    printf("____________________________________________\n");
/*
    ws = new PRUnichar[50];
    for (i = 0; i < 10; i++) {
       ws[i] = 'z';
    }
    ws[i] = '\0';
    printf("--before call TestInOut14 %s\n",ws);
    f_res << form("TestInOut14 before %s\n",ws);
    t1->TestInOut14(&ws);
    printf("--before call TestInOut14 %s\n",ws);
    f_res << form("TestInOut14 after %s\n",ws);
    printf("____________________________________________\n");
*/
    PRUint32 arrCount = 0;
    char *array = new char [100];
    strcpy(array,"Going out... remote ipc test.");
    arrCount = strlen(array);
    printf("TestInOut15 before count %d\n", arrCount);
    f_res << form("TestInOut15 before count %d\n", arrCount);
    printf("TestInOut15 before Array=%s\n", array);
    f_res << form("TestInOut15 before Array=%s\n",array);
    printf("--before call TestInOut15 data array.\n");
    t1->TestInOut15(&arrCount, &array);
    printf("TestInOut15 after count %d\n", arrCount);
    f_res << form("TestInOut15 after count %d\n", arrCount);
    printf("TestInOut15 after Array=%s\n", array);
    f_res << form("TestInOut15 after Array=%s\n",array);
    printf("____________________________________________\n");


    printf("--before call TestInOut16 combination.\n");

    b = PR_FALSE;
    o = 555;
    c = 'Z';
    sInt = 9;
    usInt = 9;
    lInt = 9;
    ulInt = 9;
    llInt = 9;
    ullInt = 9;
    f = 9;
    d = 9;
    s = "This is an inout parameter test.";

    f_res << form("TestInOut16 before %d\n",b);
    f_res << form("TestInOut16 before %d\n",lInt);
    f_res << form("TestInOut16 before %d\n",llInt);
    f_res << form("TestInOut16 before %u\n",usInt);
    f_res << form("TestInOut16 before %u\n",ulInt);
    f_res << form("TestInOut16 before %u\n",ullInt);
    f_res << form("TestInOut16 before %.50f\n",f);
    f_res << form("TestInOut16 before %.50f\n",d);
    f_res << form("TestInOut16 before %c\n",c);
    f_res << form("TestInOut16 before %s\n",s);

    t1->TestInOut16( &b, &c, &usInt, &lInt, &ulInt, &llInt, &ullInt, &f, &d, &s);

    printf("After calling TestInOut16.\n");
    f_res << form("TestInOut16 after %d\n",b);
    f_res << form("TestInOut16 after %d\n",lInt);
    f_res << form("TestInOut16 after %d\n",llInt);
    f_res << form("TestInOut16 after %u\n",usInt);
    f_res << form("TestInOut16 after %u\n",ulInt);
    f_res << form("TestInOut16 after %u\n",ullInt);
    f_res << form("TestInOut16 after %.50f\n",f);
    f_res << form("TestInOut16 after %.50f\n",d);
    f_res << form("TestInOut16 after %c\n",c);
    f_res << form("TestInOut16 after %s\n",s);
    printf("____________________________________________\n");

    PRUint32 arrCount2 = 5;
    PRInt32 *arrLong = new PRInt32 [arrCount2];
    f_res << form("TestInOut17 before count %d\n", arrCount2);
    for (i = 0; i < arrCount2; i++) {
        arrLong[i] = 11111 + i;
        f_res << form("TestInOut17 before Array[%d]=%d\n",i,arrLong[i]);
    }
    printf("--before call TestInOut17 data array.\n");
    t1->TestInOut17(&arrCount2, &arrLong);
    f_res << form("TestInOut17 after count %d\n", arrCount2);
    for (i = 0; i < arrCount2; i++) {
        f_res << form("TestInOut17 after Array[%d]=%d\n",i,arrLong[i]);
    }
    delete arrLong;
    printf("____________________________________________\n");

    printf("--before call TestInOut18 combination.\n");

    b = PR_FALSE;
    o = 555;    
    c = 'Z';
    sInt = 9;
    usInt = 9;
    lInt = 9;
    ulInt = 9;
    llInt = 9;
    ullInt = 9;
    f = 9;
    d = 9;
    s = "This is an inout parameter test.";

    f_res << form("TestInOut18 before %d\n",b);
    f_res << form("TestInOut18 before %o\n",o);
    f_res << form("TestInOut18 before %d\n",sInt);
    f_res << form("TestInOut18 before %d\n",lInt);
    f_res << form("TestInOut18 before %d\n",llInt);
    f_res << form("TestInOut18 before %u\n",usInt);
    f_res << form("TestInOut18 before %u\n",ulInt);
    f_res << form("TestInOut18 before %u\n",ullInt);
    f_res << form("TestInOut18 before %.50f\n",f);
    f_res << form("TestInOut18 before %.50f\n",d);
    f_res << form("TestInOut18 before %c\n",c);
    f_res << form("TestInOut18 before %s\n",s);

    t1->TestInOut18(&b, &c, &o, &sInt, &usInt, &lInt, &ulInt, &llInt, &ullInt, &f, &d, &s);
 
    printf("After calling TestInOut18.\n");
    f_res << form("TestInOut18 after %d\n",b);
    f_res << form("TestInOut18 after %o\n",o);
    f_res << form("TestInOut18 after %d\n",sInt);
    f_res << form("TestInOut18 after %d\n",lInt);
    f_res << form("TestInOut18 after %d\n",llInt);
    f_res << form("TestInOut18 after %u\n",usInt);
    f_res << form("TestInOut18 after %u\n",ulInt);
    f_res << form("TestInOut18 after %u\n",ullInt);
    f_res << form("TestInOut18 after %.50f\n",f);
    f_res << form("TestInOut18 after %.50f\n",d);
    f_res << form("TestInOut18 after %c\n",c);
    f_res << form("TestInOut18 after %s\n",s);
    printf("____________________________________________\n");


    f_res.close();

}
