#include <stdio.h>
#include <stream.h>
#include <fstream.h>
#include <stdlib.h>
#include <string.h>
#include "RPCClientService.h"
#include "nsISupports.h"
#include "nsISample.h"
#include "nsIRPCTestComb.h"
#include "deftest.h"
#include "proto.h"

extern char* transportName;
int main(int argc, char **args) 
{
    nsIID iid;
    nsIRPCTestComb *t1; 
    PRBool b, b2;
    PRUint8 o;
    PRInt16 sInt;
    PRUint16 usInt, usInt2;
    PRInt32 lInt;
    double d;
    char   c;
    PRUnichar  wc;
    char* sio;
    char* s;
    char spid[20];

    getProcessId(COMB_FDATA, &spid[0]);
    ofstream f_res(CLIENT_COMB_RES);
    if(!f_res) {
       cerr << "Cannot open result file.";
    }

    RPCClientService::Initialize(spid);
    RPCClientService * rpcService = RPCClientService::GetInstance();
    rpcService->CreateObjectProxy(1, NS_GET_IID(nsIRPCTestComb),(nsISupports**)&t1);

    b = PR_TRUE;
    c = 'A';
    printf("--before call TestComb1 %c\n",c);
    t1->TestComb1(b, &c);
    printf("--after call TestComb1 %c\n",c);
    f_res << form("TestComb1 %c\n",c);
    printf("____________________________________________\n");

    b = PR_TRUE;
    sio = "This is a test.";
    printf("--before call TestComb2 %s\n",sio);
    t1->TestComb2(b, &sio);
    printf("--after call TestComb2 %s\n",sio);
    f_res << form("TestComb2 %s\n",sio);
    printf("____________________________________________\n");

    c = 'A';
    sio = "This is a test.";
    printf("--before call TestComb3 %c\n",c);
    printf("--before call TestComb3 %s\n",sio);
    t1->TestComb3(&c, &sio);
    printf("--after call TestComb3 %c\n",c);
    printf("--after call TestComb3 %s\n",sio);
    f_res << form("TestComb3 %c %s\n",c,sio);
    printf("____________________________________________\n");


    b = PR_TRUE;
    c = 'A';
    s = "This is a test.";
    printf("--before call TestComb4 %c\n",c);
    printf("--before call TestComb4 %s\n",s);
    t1->TestComb4(&s, b, &c);
    printf("--after call TestComb4 %c\n",c);
    printf("--after call TestComb4 %s\n",s);
    f_res << form("TestComb4 %c %s\n",c,s);
    printf("____________________________________________\n");

    sInt = 555;
    c = 'A';
    printf("--before call TestComb5 %c\n",c);
    t1->TestComb5(sInt, &c);
    printf("--after call TestComb5 %c\n",c);
    f_res << form("TestComb5 %c\n",c);
    printf("____________________________________________\n");

    b = PR_TRUE;
    sInt = 555;
    printf("--before call TestComb6 %d\n",sInt);
    t1->TestComb6(b, &sInt);
    printf("--after call TestComb6 %d\n",sInt);
    f_res << form("TestComb6 %d\n",sInt);
    printf("____________________________________________\n");

    b = PR_TRUE;
    sInt = 555;
    printf("--before call TestComb7 %d\n",sInt);
    t1->TestComb7(b, &sInt);
    printf("--after call TestComb7 %d\n",sInt);
    f_res << form("TestComb7 %d\n",sInt);
    printf("____________________________________________\n");

    b = PR_TRUE;
    sInt = 555;
    printf("--before call TestComb8 %d\n",b);
    printf("--before call TestComb8 %d\n",sInt);
    t1->TestComb8(b, sInt);
    f_res << form("TestComb8 %d %d\n",b, sInt);
    printf("____________________________________________\n");

    b = PR_TRUE;
    c = 'a';
    printf("--before call TestComb9 %d\n",b);
    printf("--before call TestComb9 %c\n",c);
    t1->TestComb9(b, c);
    printf("--after call TestComb9 %c\n",c);
    f_res << form("TestComb9 %d %c\n",b, c);
    printf("____________________________________________\n");

    b = PR_TRUE;
    s = (char *) malloc(sizeof(char) * 50);
    strcpy(s,"This is a test.");
    t1->TestComb10(b, &s);
    printf("--after call TestComb10 %s\n",s);
    f_res << form("TestComb10 %s\n", s);
    printf("____________________________________________\n");

    b = PR_TRUE;
    c = 'A';
    printf("--before call TestComb11 %c\n",c);
    t1->TestComb11(b, &c);
    printf("--after call TestComb11 %c\n",c);
    f_res << form("TestComb11 %c\n",c);
    printf("____________________________________________\n");

    b = PR_TRUE;
    lInt = 100;
    printf("--before call TestComb12 %d\n",lInt);
    t1->TestComb12(b, &lInt);
    printf("--after call TestComb12 %d\n",lInt);
    f_res << form("TestComb12 %d\n",lInt);
    printf("____________________________________________\n");

    b = PR_TRUE;
    lInt = 100;
    printf("--before call TestComb13 %d\n",lInt);
    t1->TestComb13(b, &lInt);
    printf("--after call TestComb13 %d\n",lInt);
    f_res << form("TestComb13 %d\n",lInt);
    printf("____________________________________________\n");

    b = PR_TRUE;
    lInt = 100;
    printf("--before call TestComb14 %d\n",b);
    printf("--before call TestComb14 %d\n",lInt);
    t1->TestComb14(b, lInt);
    f_res << form("TestComb14 %d %d\n",b, lInt);
    printf("____________________________________________\n");

    b = PR_TRUE;
    d = 100.99;
    printf("--before call TestComb15 %d\n",b);
    printf("--before call TestComb15 %.50f\n",d);
    t1->TestComb15(b, &d);
    printf("--after call TestComb15 %.50f\n",d);
    f_res << form("TestComb15 %.50f\n",d);
    printf("____________________________________________\n");

    b = PR_TRUE;
    d = 100.99;
    printf("--before call TestComb16 %d\n",b);
    printf("--before call TestComb16 %.50f\n",d);
    t1->TestComb16(b, &d);
    printf("--after call TestComb16 %.50f\n",d);
    f_res << form("TestComb16 %.50f\n",d);
    printf("____________________________________________\n");

    b = PR_TRUE;
    usInt = 555;
    printf("--before call TestComb17 %d\n",b);
    printf("--before call TestComb17 %u\n",usInt);
    t1->TestComb17(b, &usInt);
    printf("--after call TestComb17 %u\n",usInt);
    f_res << form("TestComb17 %d\n",usInt);
    printf("____________________________________________\n");

    b = PR_TRUE;
    usInt = 555;
    printf("--before call TestComb18 %d\n",b);
    printf("--before call TestComb18 %u\n",usInt);
    t1->TestComb18(b, &usInt);
    printf("--after call TestComb18 %u\n",usInt);
    f_res << form("TestComb18 %d\n",usInt);
    printf("____________________________________________\n");

    b = PR_TRUE;
    o = 7;
    printf("--before call TestComb19 %d\n",b);
    printf("--before call TestComb19 %d\n",o);
    t1->TestComb19(b, &o);
    printf("--after call TestComb19 %d\n",o);
    f_res << form("TestComb19 %o\n",o);
    printf("____________________________________________\n");

    b = PR_TRUE;
    o = 7;
    printf("--before call TestComb20 %d\n",b);
    printf("--before call TestComb20 %d\n",o);
    t1->TestComb20(b, &o);
    printf("--after call TestComb20 %d\n",o);
    f_res << form("TestComb20 %o\n",o);
    printf("____________________________________________\n");

    b = PR_TRUE;
    o = 7;
    printf("--before call TestComb21 %d\n",o);
    t1->TestComb21(b, o);
    f_res << form("TestComb21 %d %o\n",b,o);
    printf("____________________________________________\n");

    b = PR_TRUE;
    b2 = PR_TRUE;
    printf("--before call TestComb22 %d\n",b2);
    t1->TestComb22(b, b2);
    f_res << form("TestComb22 %d %d\n",b,b2);
    printf("____________________________________________\n");

    b = PR_TRUE;
    b2 = PR_TRUE;
    printf("--before call TestComb23 %d\n",b2);
    t1->TestComb23(b, &b2);
    printf("--after call TestComb23 %d\n",b2);
    f_res << form("TestComb23 %d\n",b2);
    printf("____________________________________________\n");

    b = PR_TRUE;
    b2 = PR_TRUE;
    printf("--before call TestComb24 %d\n",b2);
    t1->TestComb24(b, &b2);
    printf("--after call TestComb24 %d\n",b2);
    f_res << form("TestComb24 %d\n",b2);
    printf("____________________________________________\n");

    b = PR_TRUE;
    c = 'A';
    printf("--before call TestComb25 %d\n",b);
    printf("--before call TestComb25 %d\n",c);
    t1->TestComb25(&b, &c);
    printf("--after call TestComb25 %d\n",b);
    printf("--after call TestComb25 %d\n",c);
    f_res << form("TestComb25 %d %c\n",b,c);
    printf("____________________________________________\n");

    b = PR_TRUE;
    c = 'A';
    printf("--before call TestComb26 %d\n",b);
    printf("--before call TestComb26 %d\n",c);
    t1->TestComb26(&b, &c);
    printf("--after call TestComb26 %d\n",c);
    printf("--after call TestComb26 %d\n",b);
    f_res << form("TestComb26 %d %c\n",b,c);
    printf("____________________________________________\n");

    b = PR_TRUE;
    wc = 'A';
    printf("--before call TestComb27 %c\n",wc);
    t1->TestComb27(b, &wc);
    printf("--after call TestComb27 %c\n",wc);
    f_res << form("TestComb27 %c\n",wc);
    printf("____________________________________________\n");

    sio = "This is a test.";
    s = "This is a second test.";
    printf("--before call TestComb30 %s\n",sio);
    printf("--before call TestComb30 %s\n",s);
    t1->TestComb30(s, &sio);
    printf("--after call TestComb30 %s\n",sio);
    f_res << form("TestComb30 %s\n",sio);
    printf("____________________________________________\n");

    o = 55;
    usInt = 55;
    printf("--before call TestComb31 %o\n",o);
    printf("--before call TestComb31 %u\n",usInt);
    t1->TestComb31(&o, &usInt);
    printf("--after call TestComb31 %o\n",o);
    printf("--after call TestComb31 %u\n",usInt);
    f_res << form("TestComb31 %o %u\n",o,usInt);
    printf("____________________________________________\n");

    o = 55;
    lInt = 55;
    printf("--before call TestComb32 %o\n",o);
    printf("--before call TestComb32 %d\n",lInt);
    t1->TestComb32(&o, &lInt);
    printf("--after call TestComb32 %o\n",o);
    printf("--after call TestComb32 %d\n",lInt);
    f_res << form("TestComb32 %o %d\n",o,lInt);
    printf("____________________________________________\n");

    lInt = 55;
    usInt2 = 55;
    printf("--before call TestComb33 %u\n",usInt2);
    printf("--before call TestComb33 %d\n",lInt);
    t1->TestComb33(&lInt, &usInt);
    printf("--after call TestComb33 %u\n",usInt2);
    printf("--after call TestComb33 %d\n",lInt);
    f_res << form("TestComb33 %d %u\n",lInt,usInt);
    printf("____________________________________________\n");

    f_res.close();
}

