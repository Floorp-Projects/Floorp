#include <stdio.h>
#include <stream.h>
#include <fstream.h>
#include <stdlib.h>
#include "RPCServerService.h"
#include "nsISupports.h"
#include "nsIJVMManager.h"
#include "nsIRPCTestIn.h"
#include "IDispatcher.h"
#include "nsIThread.h"
#include "deftest.h"
#include "proto.h"


class nsRPCTestInImpl : public  nsIRPCTestIn {
    NS_DECL_ISUPPORTS 
    nsRPCTestInImpl() {
	NS_INIT_REFCNT();
    }

    NS_IMETHOD TestIn1(PRBool bool) {
        printf("TestIn1 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test1 %d\n",bool);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestIn1 %d\n",bool);
        writeResult(SERVER_IN_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestIn2(PRUint8 octet) {
        printf("TestIn2 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test2 %d\n",octet);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestIn2 %o\n",octet);
        writeResult(SERVER_IN_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestIn3(PRInt16 sInt) {
        printf("TestIn3 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test3 %d\n",sInt);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestIn3 %d\n",sInt);
        writeResult(SERVER_IN_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestIn4(PRInt32 lInt) {
        printf("TestIn4 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test4 %d\n",lInt);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestIn4 %d\n",lInt);
        writeResult(SERVER_IN_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestIn5(PRInt64 llInt) {
        printf("TestIn5 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test5 %d\n",llInt);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestIn5 %d\n",llInt);
        writeResult(SERVER_IN_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestIn6(PRUint16 usInt) {
        printf("TestIn6 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test6 %u\n",usInt);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestIn6 %u\n",usInt);
        writeResult(SERVER_IN_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestIn7(PRUint32 ulInt) {
        printf("TestIn7 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test7 %u\n",ulInt);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestIn7 %u\n",ulInt);
        writeResult(SERVER_IN_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestIn8(PRUint64 ullInt) {
        printf("TestIn8 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test8 %u\n",ullInt);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestIn8 %u\n",ullInt);
        writeResult(SERVER_IN_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestIn9(float f) {
        printf("TestIn9 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test9 %.50f\n",f);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestIn9 %.50f\n",f);
        writeResult(SERVER_IN_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestIn10(double d) {
        printf("TestIn10 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test10 %.50f\n",d);
        char *tmpstr = new char[500];
        sprintf(tmpstr,"TestIn10 %.50f\n",d);
        writeResult(SERVER_IN_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestIn11(char c) {
        printf("TestIn11 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test11 %c\n",c);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestIn11 %c\n",c);
        writeResult(SERVER_IN_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestIn12(PRUnichar unic) {
        printf("TestIn12 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test12 %c\n",unic);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestIn12 %c\n",unic);
        writeResult(SERVER_IN_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestIn13(const char* s) {
        printf("TestIn13 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test13 %s\n",s);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestIn13 %s\n",s);
        writeResult(SERVER_IN_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestIn14(const PRUnichar* unis) {
        printf("TestIn14 this=%p\n", this);
	printf("--nsRPCTestInImpl::Test14 %s\n",unis);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestIn14 %s\n",unis);
        writeResult(SERVER_IN_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestIn15(PRUint32 count, const char **valueArray) {
        printf("--TestIn15\n");
        printf("count %d\n",count);
        char *tmpstr = new char[200];
        sprintf(tmpstr, "TestIn15 count %d\n", count);
        writeResult(SERVER_IN_RES, tmpstr);
        for (int i = 0; i < count; i++) {
            printf("TestIn15 Array[%d]=%s\n",i,valueArray[i]);
            sprintf(tmpstr,"TestIn15 Array[%d]=%s\n",i,valueArray[i]);
            writeResult(SERVER_IN_RES, tmpstr);
        }
        delete tmpstr;
        return NS_OK;
    }

    NS_IMETHOD TestIn16(PRUint32 count, PRInt32 *longArray) {
        printf("--TestIn16\n");
        printf("count %d\n",count);
        char *tmpstr = new char[200];
        sprintf(tmpstr, "TestIn16 count %d\n", count);
        writeResult(SERVER_IN_RES, tmpstr);
        for (int i = 0; i < count; i++) {
            printf("TestIn16 Array[%d]=%d\n",i,longArray[i]);
            sprintf(tmpstr,"TestIn16 Array[%d]=%d\n",i,longArray[i]);
            writeResult(SERVER_IN_RES, tmpstr);
        }
        delete tmpstr;
        return NS_OK;
    }

    NS_IMETHOD TestIn17(PRBool bBool, char cChar, PRUint8 nByte, 
                        PRInt16 nShort, PRUint16 nUShort, 
                        PRInt32 nLong,PRUint32 nULong, 
                        PRInt64 nHyper, PRUint64 nUHyper, 
                        float fFloat, double fDouble, const char *aString, 
                        PRUint32 count, PRInt32 *longArray) 
    {

        printf("TestIn17 this=%p\n", this) ;
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestIn17 PRBool %d\n",bBool);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn17 PRUint8 %o\n",nByte);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn17 PRInt16 %d\n",nShort);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn17 PRInt32 %d\n",nLong);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn17 PRInt64 %d\n",nHyper);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn17 PRUint16 %u\n",nUShort);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn17 PRUint32 %u\n",nULong);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn17 PRUint64 %u\n",nUHyper);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn17 float %.50f\n",fFloat);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn17 double %.50f\n",fDouble);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn17 char %c\n",cChar);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn17 string %s\n",aString);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn17 count %d\n", count);
        writeResult(SERVER_IN_RES, tmpstr);
        for (int i = 0; i < count; i++) {
             printf("TestIn17 Array[%d]=%d\n",i,longArray[i]);
             sprintf(tmpstr,"TestIn17 Array[%d]=%d\n",i,longArray[i]);
             writeResult(SERVER_IN_RES, tmpstr);
        }

        delete tmpstr;
        return NS_OK;
        
    }

    NS_IMETHOD TestIn18(char cChar, PRUint8 nByte, 
                        PRInt16 nShort, PRUint16 nUShort, 
                        PRInt32 nLong,PRUint32 nULong, 
                        PRInt64 nHyper, PRUint64 nUHyper, 
                        float fFloat, double fDouble, const char *aString, 
                        PRUint32 count, PRInt32 *longArray) 
    {
        printf("TestIn18 this=%p\n", this);
        char *tmpstr = new char[200];

        sprintf(tmpstr,"TestIn18 PRUint8 %o\n",nByte);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn18 PRInt16 %d\n",nShort);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn18 PRInt32 %d\n",nLong);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn18 PRInt64 %d\n",nHyper);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn18 PRUint16 %u\n",nUShort);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn18 PRUint32 %u\n",nULong);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn18 PRUint64 %u\n",nUHyper);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn18 float %.50f\n",fFloat);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn18 double %.50f\n",fDouble);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn18 char %c\n",cChar);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn18 string %s\n",aString);
        writeResult(SERVER_IN_RES, tmpstr);
        sprintf(tmpstr,"TestIn18 count %d\n", count);
        writeResult(SERVER_IN_RES, tmpstr);
        for (int i = 0; i < count; i++) {
             printf("TestIn18 Array[%d]=%d\n",i,longArray[i]);
             sprintf(tmpstr,"TestIn18 Array[%d]=%d\n",i,longArray[i]);
             writeResult(SERVER_IN_RES, tmpstr);
        }

        delete tmpstr;
        return NS_OK;
        
    }

};

NS_IMPL_ISUPPORTS(nsRPCTestInImpl, NS_GET_IID(nsIRPCTestIn));
int main(int argc, char **args) {
   int i;
    const short num = 2;
    nsRPCTestInImpl * test[num];

    setProcessId(IN_FDATA);
    for(i = 0; i < num; i++) {
       test[i] = new nsRPCTestInImpl();
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
