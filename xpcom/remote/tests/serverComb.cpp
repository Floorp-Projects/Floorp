#include <stdio.h>
#include <stream.h>
#include <fstream.h>
#include <stdlib.h>
#include "RPCServerService.h"
#include "nsISupports.h"
#include "nsIJVMManager.h"
#include "nsIRPCTestComb.h"
#include "IDispatcher.h"
#include "nsIThread.h"
#include "deftest.h"
#include "proto.h"

class nsRPCTestCombImpl : public  nsIRPCTestComb {


    NS_DECL_ISUPPORTS 
    nsRPCTestCombImpl() {
	NS_INIT_REFCNT();
    }
 
    NS_IMETHOD TestComb1(PRBool bool, char *c) {
        printf("TestComb1 this=%p\n", this);
	printf("--nsRPCTestCombImpl::Test1 %d\n",bool);
        printf("Before character assignment.\n");
        *c = 'Z';
        printf("After character assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb1 %c\n",*c);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb2(PRBool bool, char **s) {
        printf("TestComb2 this=%p\n", this);
	printf("--nsRPCTestCombImpl::Test2 %d\n",bool);
        printf("--nsRPCTestCombImpl::Test2 %s\n",*s);
        printf("Before string assignment.\n");
        *s = "Remote ipc tests.";
        printf("After string assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb2 %s\n",*s);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb3(char *c, char **s) {
        printf("TestComb3 this=%p\n", this);
        printf("--nsRPCTestCombImpl::Test3 %s\n",*s);
        printf("Before character assignment.\n");
        *c = 'Z';
        printf("After character assignment.\n");
        printf("Before string assignment.\n");
        *s = "Remote ipc tests.";
        printf("After string assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb3 %c %s\n",*c,*s);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb4(char **s, PRBool bool, char *c) {
        printf("TestComb4 this=%p\n", this);
	printf("--nsRPCTestCombImpl::Test4 %d\n",bool);
        printf("--nsRPCTestCombImpl::Test4 %s\n",*s);
        printf("Before character assignment.\n");
        *c = 'Z';
        printf("After character assignment.\n");
        printf("Before string assignment.\n");
        *s = "Remote ipc tests.";
        printf("After string assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb4 %c %s\n",*c,*s);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb5(PRInt16 sInt, char *c) {
        printf("TestComb5 this=%p\n", this);
        printf("Before character assignment.\n");
        *c = 'Z';
        printf("After character assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb5 %c\n",*c);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb6(PRBool bool, PRInt16 *sInt) {
        printf("TestComb6 this=%p\n", this);
        printf("Before short int assignment.\n");
        *sInt = 777;
        printf("After short int assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb6 %d\n",*sInt);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb7(PRBool bool, PRInt16 *sInt) {
        printf("TestComb7 this=%p\n", this);
        printf("--nsRPCTestCombInImpl::Test7 %d\n",*sInt);
        printf("Before short int assignment.\n");
        *sInt = 777;
        printf("After short int assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb7 %d\n",*sInt);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb8(PRBool bool, PRInt16 sInt) {
        printf("TestComb8 this=%p\n", this);
        printf("--nsRPCTestCombInImpl::Test8 %d\n",sInt);
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb8 %d %d\n",bool, sInt);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb9(PRBool bool, char c) {
        printf("TestComb9 this=%p\n", this);
        printf("--nsRPCTestCombInImpl::Test9 %d\n",c);
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb9 %d %c\n",bool, c);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb10(PRBool bool, char **s) {
        printf("TestComb10 this=%p\n", this);
	printf("--nsRPCTestCombImpl::Test10 %d\n",bool);
        printf("Before string assignment.\n");
        *s = "Remote ipc tests.";
        printf("After string assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb10 %s\n",*s);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb11(PRBool bool, char *c) {
        printf("TestComb11 this=%p\n", this);
	printf("--nsRPCTestCombImpl::Test11 %d\n",*c);
        *c = 'Z';
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb11 %c\n",*c);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb12(PRBool bool, PRInt32 *l) {
        printf("TestComb12 this=%p\n", this);
	printf("--nsRPCTestCombImpl::Test12 %d\n",bool);
        *l = 99999999;
        printf("After long integer assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb12 %d\n",*l);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb13(PRBool bool, PRInt32 *l) {
        printf("TestComb13 this=%p\n", this);
	printf("--nsRPCTestCombImpl::Test13 %d\n",bool);
	printf("--nsRPCTestCombImpl::Test13 %d\n",*l);
        *l = 99999999;
        printf("After long integer assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb13 %d\n",*l);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb14(PRBool bool, PRInt32 l) {
        printf("TestComb14 this=%p\n", this);
	printf("--nsRPCTestCombImpl::Test14 %d\n",bool);
	printf("--nsRPCTestCombImpl::Test14 %d\n",l);
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb14 %d %d\n",bool, l);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb15(PRBool bool, double *d) {
        printf("TestComb15 this=%p\n", this);
	printf("--nsRPCTestCombImpl::Test15 %d\n",bool);
        *d = 9999999.9999999;
        printf("After long integer assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb15 %.50f\n",*d);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb16(PRBool bool, double *d) {
        printf("TestComb16 this=%p\n", this);
	printf("--nsRPCTestCombImpl::Test16 %d\n",bool);
	printf("--nsRPCTestCombImpl::Test16 %d\n",*d);
        *d = 9999999.9999999;
        printf("After long integer assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb16 %.50f\n", *d);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb17(PRBool bool, PRUint16 *usInt) {
        printf("TestComb17 this=%p\n", this);
        printf("--nsRPCTestCombInImpl::Test17 %d\n",*usInt);
        printf("Before unsigned short int assignment.\n");
        *usInt = 777;
        printf("After unsigned short int assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb17 %d\n",*usInt);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb18(PRBool bool, PRUint16 *usInt) {
        printf("TestComb18 this=%p\n", this);
        printf("Before unsigned short int assignment.\n");
        *usInt = 777;
        printf("After unsigned short int assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb18 %d\n",*usInt);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb19(PRBool bool, PRUint8 *oct) {
        printf("TestComb19 this=%p\n", this);
        printf("Before assignment.\n");
        *oct = 77;
        printf("After assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb19 %o\n",*oct);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb20(PRBool bool, PRUint8 *oct) {
        printf("TestComb20 this=%p\n", this);
        printf("--nsRPCTestCombInImpl::Test20 %d\n",*oct);
        printf("Before assignment.\n");
        *oct = 77;
        printf("After assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb20 %o\n",*oct);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb21(PRBool bool, PRUint8 oct) {
        printf("TestComb21 this=%p\n", this);
        printf("--nsRPCTestCombInImpl::Test21 %d\n",oct);
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb21 %d %o\n",bool,oct);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb22(PRBool bool, PRBool bool2 ) {
        printf("TestComb22 this=%p\n", this);
        printf("--nsRPCTestCombInImpl::Test22 %d\n",bool2);
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb22 %d %d\n",bool,bool2);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb23(PRBool bool, PRBool *bool2 ) {
        printf("TestComb23 this=%p\n", this);
        printf("--nsRPCTestCombInImpl::Test23 %d\n",*bool2);
        printf("Before assignment.\n");
        *bool2 = PR_FALSE;
        printf("After assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb23 %d\n",*bool2);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb24(PRBool bool, PRBool *bool2 ) {
        printf("TestComb24 this=%p\n", this);
        printf("Before assignment.\n");
        *bool2 = PR_FALSE;
        printf("After assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb24 %d\n",*bool2);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb25(PRBool *bool, char *c) {
        printf("TestComb25 this=%p\n", this);
        printf("Before assignment.\n");
        *bool = PR_FALSE;
        *c = 'Z';
        printf("After assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb25 %d %c\n",*bool,*c);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb26(PRBool *bool, char *c) {
        printf("TestComb26 this=%p\n", this);
        printf("--nsRPCTestCombInImpl::Test26 %d\n",*bool);
        printf("Before assignment.\n");
        *bool = PR_FALSE;
        *c = 'Z';
        printf("After assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb26 %d %c\n",*bool,*c);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb27(PRBool bool, PRUnichar *wc) {
        printf("TestComb27 this=%p\n", this);
        printf("Before assignment.\n");
        *wc = 'Z';
        printf("After assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb27 %c\n",*wc);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb30(const char *s1, char **s2) {
        printf("TestComb30 this=%p\n", this);
        printf("Before assignment.\n");
        *s2 = "Remote ipc test.";
        printf("After assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb30 %s\n",*s2);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb31(PRUint8 *o, PRUint16 *usInt) {
        printf("TestComb31 this=%p\n", this);
        printf("Before assignment.\n");
        *o = 777;
        *usInt = 999;
        printf("After assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb31 %o %u\n",*o,*usInt);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb32(PRUint8 *o, PRInt32 *lInt) {
        printf("TestComb32 this=%p\n", this);
        printf("Before assignment.\n");
        *o = 99;
        *lInt = 9999999;
        printf("After assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb32 %o %u\n",*o,*lInt);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestComb33(PRInt32 *lInt, PRUint16 *us) {
        printf("TestComb33 this=%p\n", this);
        printf("Before assignment.\n");
        *us = 777;
        *lInt = 9999999;
        printf("After assignment.\n");
        char *tmpstr = new char[200]; 
        sprintf(tmpstr,"TestComb33 %d %u\n",*lInt,*us);
        writeResult(SERVER_COMB_RES,tmpstr);
        delete tmpstr;
	return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(nsRPCTestCombImpl, NS_GET_IID(nsIRPCTestComb));
int main(int argc, char **args) 
{
    int i;
    const short num = 4;
    nsRPCTestCombImpl * test[num];

    setProcessId(COMB_FDATA);
    for(i = 0; i < num; i++) {
       test[i] = new nsRPCTestCombImpl();
    }
    RPCServerService * rpcService = RPCServerService::GetInstance();
    IDispatcher *dispatcher;
    rpcService->GetDispatcher(&dispatcher);
    for(i = 0; i < num; i++) {
       dispatcher->RegisterWithOID(test[i], i + 1);
    }
    while(1) {
	PR_Sleep(100);
    }
    
}
