#include <stdio.h>
#include <stream.h>
#include <fstream.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include "RPCClientService.h"
#include "nsISupports.h"
#include "nsISample.h"
#include "nsIRPCTestIn.h"
#include "deftest.h"
#include "proto.h"

extern char* transportName;
int main(int argc, char **args) {

    nsIID iid;
    nsIRPCTestIn *t1; 
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

    getProcessId(IN_FDATA, &spid[0]);
    ofstream f_res(CLIENT_IN_RES);
    if(!f_res) {
       cerr << "Cannot open result file.";
    }

    RPCClientService::Initialize(spid);
    RPCClientService * rpcService = RPCClientService::GetInstance();
    rpcService->CreateObjectProxy(1, NS_GET_IID(nsIRPCTestIn),(nsISupports**)&t1);

    b = PR_TRUE;
    printf("--before call TestIn1 %d\n",b);
    t1->TestIn1(b);
    f_res << form("TestIn1 %d\n",b);
    b = PR_FALSE;
    printf("--before call TestIn1 %d\n",b);
    t1->TestIn1(b);
    f_res << form("TestIn1 %d\n",b);
    printf("____________________________________________\n");

    o = 0;
    printf("--before call TestIn2 %d\n",o);
    t1->TestIn2(o);
    f_res << form("TestIn2 %o\n",o);
    o = 124;
    printf("--before call TestIn2 %d\n",o);
    t1->TestIn2(o);
    f_res << form("TestIn2 %o\n",o);
    printf("____________________________________________\n");

    sInt = SHRT_MIN;
    printf("--before call TestIn3 %d\n",sInt);
    t1->TestIn3(sInt);
    f_res << form("TestIn3 %d\n",sInt);
    sInt = SHRT_MAX;
    printf("--before call TestIn3 %d\n",sInt);
    t1->TestIn3(sInt);
    f_res << form("TestIn3 %d\n",sInt);
    printf("____________________________________________\n");

    lInt = LONG_MIN;
    printf("--before call TestIn4 %d\n",lInt);
    t1->TestIn4(lInt);
    f_res << form("TestIn4 %d\n",lInt);
    lInt = LONG_MAX;
    printf("--before call TestIn4 %d\n",lInt);
    t1->TestIn4(lInt);
    f_res << form("TestIn4 %d\n",lInt);
    printf("____________________________________________\n");

    llInt = LONG_MIN;
    printf("--before call TestIn5 %d\n",llInt);
    t1->TestIn5(llInt);
    f_res << form("TestIn5 %d\n",llInt);
    llInt = LONG_MAX;
    printf("--before call TestIn5 %d\n",llInt);
    t1->TestIn5(llInt);
    f_res << form("TestIn5 %d\n",llInt);
    printf("____________________________________________\n");

    usInt = 555;
    printf("--before call TestIn6 %u\n",usInt);
    t1->TestIn6(usInt);
    f_res << form("TestIn6 %u\n",usInt);
    usInt = USHRT_MAX;
    printf("--before call TestIn6 %u\n",usInt);
    t1->TestIn6(usInt);
    f_res << form("TestIn6 %u\n",usInt);
    printf("____________________________________________\n");

    ulInt = 555;
    printf("--before call TestIn7 %u\n",ulInt);
    t1->TestIn7(ulInt);
    f_res << form("TestIn7 %u\n",ulInt);
    ulInt = ULONG_MAX;
    printf("--before call TestIn7 %u\n",ulInt);
    t1->TestIn7(ulInt);
    f_res << form("TestIn7 %u\n",ulInt);
    printf("____________________________________________\n");

    ullInt = 555;
    printf("--before call TestIn8 %u\n",ullInt);
    t1->TestIn8(ullInt);
    f_res << form("TestIn8 %u\n",ullInt);
    ullInt = ULONG_MAX;
    printf("--before call TestIn8 %u\n",ullInt);
    t1->TestIn8(ullInt);
    f_res << form("TestIn8 %u\n",ullInt);
    printf("____________________________________________\n");

    f = FLT_MIN;
    printf("--before call TestIn9 %.50f\n",f);
    t1->TestIn9(f);
    f_res << form("TestIn9 %.50f\n",f);
    f = FLT_MAX;
    printf("--before call TestIn9 %f\n",f);
    t1->TestIn9(f);
    f_res << form("TestIn9 %.50f\n",f);
    printf("____________________________________________\n");

    d = DBL_MIN;
    printf("--before call TestIn10 %.50f\n",d);
    t1->TestIn10(d);
    f_res << form("TestIn10 %.50f\n",d);
    d = DBL_MAX;
    printf("--before call TestIn10 %f\n",d);
    t1->TestIn10(d);
    f_res << form("TestIn10 %.50f\n",d);
    printf("____________________________________________\n");

    c = 'A';
    printf("--before call TestIn11 %d\n",c);
    t1->TestIn11(c);
    f_res << form("TestIn11 %c\n",c);
    c = 'Z';
    printf("--before call TestIn11 %d\n",c);
    t1->TestIn11(c);
    f_res << form("TestIn11 %c\n",c);
    printf("____________________________________________\n");

    wc = 'A';
    printf("--before call TestIn12 %d\n",wc);
    t1->TestIn12(wc);
    f_res << form("TestIn12 %c\n",wc);
    wc = 'Z';
    printf("--before call TestIn12 %d\n",wc);
    t1->TestIn12(wc);
    f_res << form("TestIn12 %c\n",wc);
    printf("____________________________________________\n");

    s = "";
    printf("--before call TestIn13 %s\n",s);
    t1->TestIn13(s);
    f_res << form("TestIn13 %s\n",s);
    strcpy(s,"This is a test.");
    printf("--before call TestIn13 %s\n",s);
    t1->TestIn13(s);
    f_res << form("TestIn13 %s\n",s);

    printf("____________________________________________\n");
/*
    ws = new PRUnichar[50];
    ws[0] = '\0';
    printf("--before call TestIn14 %s\n",ws);
    t1->TestIn14(ws);
    f_res << form("TestIn14 %s\n",ws);

    for (i = 0; i < 10; i++) {
       ws[i] = 'z';
    }
    ws[i] = '\0';
    printf("--before call TestIn14 %s\n",ws);
    t1->TestIn14(ws);
    f_res << form("TestIn14 %s\n",ws);
    delete ws;
    printf("____________________________________________\n");
*/

    PRUint32 arrCount = 3;
    const char *array[3] = {"Blackwood","MCD","StarLite"};
    printf("--before call TestIn15 data array.\n");
    t1->TestIn15(arrCount, array);
    f_res << form("TestIn15 count %d\n", arrCount);
    for (i = 0; i < arrCount; i++) {
        f_res << form("TestIn15 Array[%d]=%s\n",i,array[i]);
    }
    printf("____________________________________________\n");

    PRUint32 arrCount2 = 10;
    PRInt32 arrLong[10] = {1111111, 2222222, 3333333, 4444444, 5555555, 
                          6666666, 7777777, 8888888, 9999999, 0};
    printf("--before call TestIn16 data array.\n");
    t1->TestIn16(arrCount2, &arrLong[0]);
    f_res << form("TestIn16 count %d\n", arrCount2);
    for (i = 0; i < arrCount2; i++) {
        f_res << form("TestIn16 Array[%d]=%d\n",i,arrLong[i]);
    }
    printf("____________________________________________\n");

    b = PR_TRUE;
    o = 8;
    sInt = 1;
    lInt = 2;
    llInt = 3;
    usInt = 4;
    ulInt = 5;
    ullInt = 6;
    f = 7;
    d = 8;
    c = 'A';
    
    printf("--before call TestIn17 combination.\n");
    t1->TestIn17(b, c, o, sInt, usInt, lInt, ulInt, llInt, ullInt, f, d, s, arrCount2, &arrLong[0]);   
    f_res << form("TestIn17 PRBool %d\n",b);
    f_res << form("TestIn17 PRUint8 %o\n",o);
    f_res << form("TestIn17 PRInt16 %d\n",sInt);
    f_res << form("TestIn17 PRInt32 %d\n",lInt);
    f_res << form("TestIn17 PRInt64 %d\n",llInt);
    f_res << form("TestIn17 PRUint16 %u\n",usInt);
    f_res << form("TestIn17 PRUint32 %u\n",ulInt);
    f_res << form("TestIn17 PRUint64 %u\n",ullInt);
    f_res << form("TestIn17 float %.50f\n",f);
    f_res << form("TestIn17 double %.50f\n",d);
    f_res << form("TestIn17 char %c\n",c);
    f_res << form("TestIn17 string %s\n",s);
    f_res << form("TestIn17 count %d\n", arrCount2);
    for (i = 0; i < arrCount2; i++) {
        f_res << form("TestIn17 Array[%d]=%d\n",i,arrLong[i]);
    }
    printf("____________________________________________\n");

    printf("--before call TestIn18 combination.\n");
    t1->TestIn18(c, o, sInt, usInt, lInt, ulInt, llInt, ullInt, f, d, s, arrCount2, &arrLong[0]);   
    f_res << form("TestIn18 PRUint8 %o\n",o);
    f_res << form("TestIn18 PRInt16 %d\n",sInt);
    f_res << form("TestIn18 PRInt32 %d\n",lInt);
    f_res << form("TestIn18 PRInt64 %d\n",llInt);
    f_res << form("TestIn18 PRUint16 %u\n",usInt);
    f_res << form("TestIn18 PRUint32 %u\n",ulInt);
    f_res << form("TestIn18 PRUint64 %u\n",ullInt);
    f_res << form("TestIn18 float %.50f\n",f);
    f_res << form("TestIn18 double %.50f\n",d);
    f_res << form("TestIn18 char %c\n",c);
    f_res << form("TestIn18 string %s\n",s);
    f_res << form("TestIn18 count %d\n", arrCount2);
    for (i = 0; i < arrCount2; i++) {
        f_res << form("TestIn18 Array[%d]=%d\n",i,arrLong[i]);
    }
    printf("____________________________________________\n");

    f_res.close();
}

