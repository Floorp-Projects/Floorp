#include <stdio.h>
#include <stream.h>
#include <fstream.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include "RPCServerService.h"
#include "nsISupports.h"
#include "nsIJVMManager.h"
#include "nsIRPCTestInOut.h"
#include "IDispatcher.h"
#include "nsIThread.h"
#include "deftest.h"
#include "proto.h"

class nsRPCTestInOutImpl : public  nsIRPCTestInOut {
    NS_DECL_ISUPPORTS 
    nsRPCTestInOutImpl() {
	NS_INIT_REFCNT();
    }

    NS_IMETHOD TestInOut1(PRBool *bool) {
        printf("TestInOut1 this=%p\n", this);
	printf("--nsRPCTestInOutImpl::Test1 %d\n",*bool);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestInOut1 before %d\n",*bool);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *bool = PR_FALSE;
        sprintf(tmpstr,"TestInOut1 after %d\n",*bool);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestInOut2(PRUint8 *octet) {
        printf("TestInOut2 this=%p\n", this);
	printf("--nsRPCTestInOutImpl::Test2 %o\n",*octet);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestInOut2 before %o\n",*octet);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *octet = 88; 
        sprintf(tmpstr,"TestInOut2 after %o\n",*octet);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }  

    NS_IMETHOD TestInOut3(PRInt16 *sInt) {
        printf("TestInOut3 this=%p\n", this);
	printf("--nsRPCTestInOutImpl::Test3 %d\n",*sInt);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestInOut3 before %d\n",*sInt);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *sInt = SHRT_MAX;
        sprintf(tmpstr,"TestInOut3 after %d\n",*sInt);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestInOut4(PRInt32 *lInt) {
        printf("TestInOut4 this=%p\n", this);
	printf("--nsRPCTestInOutImpl::Test4 %d\n",*lInt);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestInOut4 before %l\n",*lInt);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *lInt = LONG_MAX;
        sprintf(tmpstr,"TestInOut4 after %l\n",*lInt);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestInOut5(PRInt64 *llInt) {
        printf("TestInOut5 this=%p\n", this);
	printf("--nsRPCTestInOutImpl::Test5 %d\n",*llInt);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestInOut5 before %l\n",*llInt);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *llInt = LONG_MAX;
        sprintf(tmpstr,"TestInOut5 after %l\n",*llInt);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestInOut6(PRUint16 *usInt) {
        printf("TestInOut6 this=%p\n", this);
	printf("--nsRPCTestInOutImpl::Test6 %15u\n",*usInt);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestInOut6 before %10u\n",*usInt);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *usInt = USHRT_MAX;
        sprintf(tmpstr,"TestInOut6 after %10u\n",*usInt);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestInOut7(PRUint32 *ulInt) {
        printf("TestInOut7 this=%p\n", this);
	printf("--nsRPCTestInOutImpl::Test7 %15u\n",*ulInt);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestInOut7 before %10u\n",*ulInt);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *ulInt = ULONG_MAX;
        sprintf(tmpstr,"TestInOut7 after %10u\n",*ulInt);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestInOut8(PRUint64 *ullInt) {
        printf("TestInOut8 this=%p\n", this);
	printf("--nsRPCTestInOutImpl::Test8 %15u\n",*ullInt);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestInOut8 before %10u\n",*ullInt);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *ullInt = ULONG_MAX;
        sprintf(tmpstr,"TestInOut8 after %10u\n",*ullInt);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestInOut9(float *f) {
        printf("TestInOut9 this=%p\n", this);
	printf("--nsRPCTestInOutImpl::Test9 %.50f\n",*f);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestInOut9 before %.50f\n",*f);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *f = FLT_MAX;
        sprintf(tmpstr,"TestInOut9 after %.50f\n",*f);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestInOut10(double *d) {
        printf("TestInOut10 this=%p\n", this);
	printf("--nsRPCTestInOutImpl::Test10 %.50f\n",*d);
        char *tmpstr = new char[500];
        sprintf(tmpstr,"TestInOut10 before %.50f\n",*d);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *d = DBL_MAX;
        sprintf(tmpstr,"TestInOut10 after %.50f\n",*d);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestInOut11(char *c) {
        printf("TestInOut11 this=%p\n", this);
	printf("--nsRPCTestInOutImpl::Test11 %c\n",*c);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestInOut11 before %c\n",*c);
        printf("THIS THIS THIS IS A TEST.... %s\n", tmpstr);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *c = 'G';
        sprintf(tmpstr,"TestInOut11 after %c\n",*c);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestInOut12(PRUnichar *unic) {
        printf("TestInOut12 this=%p\n", this);
	printf("--nsRPCTestInOutImpl::Test12 %c\n",*unic);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestInOut12 before %c\n",*unic);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *unic = 'G';
        sprintf(tmpstr,"TestInOut12 after %c\n",*unic);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestInOut13(char **s) {
        printf("TestInOut13 this=%p\n", this);
	printf("--nsRPCTestInOutImpl::Test13 %s\n",*s);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestInOut13 before %s\n",*s);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *s = "remote ipc";
        sprintf(tmpstr,"TestInOut13 after %s\n",*s);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestInOut14(PRUnichar **unis) {
        printf("TestInOut14 this=%p\n", this);
	printf("--nsRPCTestInOutImpl::Test14 %s\n",*unis);
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestInOut14 before %s\n",*unis);
        writeResult(SERVER_INOUT_RES, tmpstr);
        //*unis = "remote ipc";
        for (int i = 0; i < 10; i++) { 
           (*unis)[i] = 'a' + 1; 
        } 
        (*unis)[i] = '\0'; 
        sprintf(tmpstr,"TestInOut14 after %s\n",*unis);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestInOut15(PRUint32 *count, char **valueArray) {
        printf("--TestInOut15\n");
        char *tmpstr = new char[200];
        printf("TestInOut15 before count %d\n", *count);
        sprintf(tmpstr, "TestInOut15 before count %d\n", *count);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut15 before valueArray %s\n", *valueArray);
        sprintf(tmpstr,"TestInOut15 before Array=%s\n",*valueArray);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *valueArray = "Going in... remote ipc test.";
        *count = strlen(*valueArray);
        printf("TestInOut15 after count %d\n", *count);
        sprintf(tmpstr, "TestInOut15 after count %d\n", *count);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut15 after valueArray %s\n", *valueArray);
        sprintf(tmpstr,"TestInOut15 after Array=%s\n",*valueArray);
        writeResult(SERVER_INOUT_RES, tmpstr);
        delete tmpstr;
        return NS_OK;
    }


    NS_IMETHOD TestInOut16(PRBool *bBool, char *cChar, 
                        PRUint16 *nUShort,
                        PRInt32 *nLong,PRUint32 *nULong,
                        PRInt64 *nHyper, PRUint64 *nUHyper,
                        float *fFloat, double *fDouble, char **aString)
    {

        printf("TestInOut16 before this=%p\n", this) ;
        char *tmpstr = new char[200];
        printf("TestInOut16 before %d\n",*bBool);
        sprintf(tmpstr,"TestInOut16 before %d\n",*bBool);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 before %d\n",*nLong);
        sprintf(tmpstr,"TestInOut16 before %d\n",*nLong);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 before %d\n",*nHyper);
        sprintf(tmpstr,"TestInOut16 before %d\n",*nHyper);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 before %u\n",*nUShort);
        sprintf(tmpstr,"TestInOut16 before %u\n",*nUShort);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 before %u\n",*nULong);
        sprintf(tmpstr,"TestInOut16 before %u\n",*nULong);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 before %u\n",*nUHyper);
        sprintf(tmpstr,"TestInOut16 before %u\n",*nUHyper);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 before %.50f\n",*fFloat);
        sprintf(tmpstr,"TestInOut16 before %.50f\n",*fFloat);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 before %.50f\n",*fDouble);
        sprintf(tmpstr,"TestInOut16 before %.50f\n",*fDouble);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 before %c\n",*cChar);
        sprintf(tmpstr,"TestInOut16 before %c\n",*cChar);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 before %s\n",*aString);
        sprintf(tmpstr,"TestInOut16 before %s\n",*aString);
        writeResult(SERVER_INOUT_RES, tmpstr);

        *bBool = PR_TRUE;
        *cChar = 'A';
        *nUShort = 2;
        *nLong = 3;
        *nULong = 4;
        *nHyper = 5;
        *nUHyper = 6;
        *fFloat = 7;
        *fDouble = 8;
        *aString = "Remote ipc test.";

        printf("TestInOut16 after %d\n",*bBool);
        sprintf(tmpstr,"TestInOut16 after %d\n",*bBool);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 after %d\n",*nLong);
        sprintf(tmpstr,"TestInOut16 after %d\n",*nLong);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 after %d\n",*nHyper);
        sprintf(tmpstr,"TestInOut16 after %d\n",*nHyper);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 after %u\n",*nUShort);
        sprintf(tmpstr,"TestInOut16 after %u\n",*nUShort);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 after %u\n",*nULong);
        sprintf(tmpstr,"TestInOut16 after %u\n",*nULong);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 after %u\n",*nUHyper);
        sprintf(tmpstr,"TestInOut16 after %u\n",*nUHyper);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 after %.50f\n",*fFloat);
        sprintf(tmpstr,"TestInOut16 after %.50f\n",*fFloat);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 after %.50f\n",*fDouble);
        sprintf(tmpstr,"TestInOut16 after %.50f\n",*fDouble);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 after %c\n",*cChar);
        sprintf(tmpstr,"TestInOut16 after %c\n",*cChar);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut16 after %s\n",*aString);
        sprintf(tmpstr,"TestInOut16 after %s\n",*aString);
        writeResult(SERVER_INOUT_RES, tmpstr);

        delete tmpstr;
        return NS_OK;
    }

    NS_IMETHOD TestInOut17(PRUint32 *count, PRInt32 **longArray) {
        printf("--TestInOut17\n");
        char *tmpstr = new char[200];
        sprintf(tmpstr, "TestInOut17 before count %d\n", *count);
        writeResult(SERVER_INOUT_RES, tmpstr);
        for (int i = 0; i < *count; i++) {
            printf("TestInOut17 before Array[%d]=%d\n",i,(*longArray)[i]);
            sprintf(tmpstr,"TestInOut17 before Array[%d]=%d\n",i,(*longArray)[i]);
            writeResult(SERVER_INOUT_RES, tmpstr);
        }
        *count = 5;
        sprintf(tmpstr, "TestInOut17 after count %d\n", *count);
        writeResult(SERVER_INOUT_RES, tmpstr);
        *longArray = new PRInt32[*count];
        for (i = 0; i < *count; i++) {
            //longArray[i] = new PRInt32;
            (*longArray)[i] = 55555 + i;
            printf("TestInOut17 after Array[%d]=%d\n",i,(*longArray)[i]);
            sprintf(tmpstr,"TestInOut17 after Array[%d]=%d\n",i,(*longArray)[i]);
            writeResult(SERVER_INOUT_RES, tmpstr);
        }
        delete tmpstr;
        return NS_OK;
    }

    NS_IMETHOD TestInOut18(PRBool *bBool, char *cChar, PRUint8 *nByte,
                        PRInt16 *nShort, PRUint16 *nUShort,
                        PRInt32 *nLong,PRUint32 *nULong,
                        PRInt64 *nHyper, PRUint64 *nUHyper,
                        float *fFloat, double *fDouble, char **aString)
    {
 
        printf("TestInOut18 before this=%p\n", this) ;
        char *tmpstr = new char[200];
        printf("TestInOut18 before %d\n",*bBool);
        sprintf(tmpstr,"TestInOut18 before %d\n",*bBool);
        writeResult(SERVER_INOUT_RES, tmpstr);
        sprintf(tmpstr,"TestInOut18 before %o\n",*nByte);
        writeResult(SERVER_INOUT_RES, tmpstr);
        sprintf(tmpstr,"TestInOut18 before %d\n",*nShort);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 before %d\n",*nLong);
        sprintf(tmpstr,"TestInOut18 before %d\n",*nLong);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 before %d\n",*nHyper);
        sprintf(tmpstr,"TestInOut18 before %d\n",*nHyper);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 before %u\n",*nUShort);
        sprintf(tmpstr,"TestInOut18 before %u\n",*nUShort);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 before %u\n",*nULong);
        sprintf(tmpstr,"TestInOut18 before %u\n",*nULong);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 before %u\n",*nUHyper);
        sprintf(tmpstr,"TestInOut18 before %u\n",*nUHyper);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 before %.50f\n",*fFloat);
        sprintf(tmpstr,"TestInOut18 before %.50f\n",*fFloat);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 before %.50f\n",*fDouble);
        sprintf(tmpstr,"TestInOut18 before %.50f\n",*fDouble);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 before %c\n",*cChar);
        sprintf(tmpstr,"TestInOut18 before %c\n",*cChar);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 before %s\n",*aString);
        sprintf(tmpstr,"TestInOut18 before %s\n",*aString);
        writeResult(SERVER_INOUT_RES, tmpstr);

        *bBool = PR_TRUE;
        *nByte = 777;
        *cChar = 'A';
        *nShort = 1;
        *nUShort = 2;
        *nLong = 3;
        *nULong = 4;
        *nHyper = 5;
        *nUHyper = 6;
        *fFloat = 7;
        *fDouble = 8;
        *aString = "Remote ipc test.";
 
        printf("TestInOut18 after %d\n",*bBool);
        sprintf(tmpstr,"TestInOut18 after %d\n",*bBool);
        writeResult(SERVER_INOUT_RES, tmpstr);
        sprintf(tmpstr,"TestInOut18 after %o\n",*nByte);
        writeResult(SERVER_INOUT_RES, tmpstr);
        sprintf(tmpstr,"TestInOut18 after %d\n",*nShort);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 after %d\n",*nLong);
        sprintf(tmpstr,"TestInOut18 after %d\n",*nLong);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 after %d\n",*nHyper);
        sprintf(tmpstr,"TestInOut18 after %d\n",*nHyper);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 after %u\n",*nUShort);
        sprintf(tmpstr,"TestInOut18 after %u\n",*nUShort);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 after %u\n",*nULong);
        sprintf(tmpstr,"TestInOut18 after %u\n",*nULong);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 after %u\n",*nUHyper);
        sprintf(tmpstr,"TestInOut18 after %u\n",*nUHyper);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 after %.50f\n",*fFloat);
        sprintf(tmpstr,"TestInOut18 after %.50f\n",*fFloat);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 after %.50f\n",*fDouble);
        sprintf(tmpstr,"TestInOut18 after %.50f\n",*fDouble);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 after %c\n",*cChar);
        sprintf(tmpstr,"TestInOut18 after %c\n",*cChar);
        writeResult(SERVER_INOUT_RES, tmpstr);
        printf("TestInOut18 after %s\n",*aString);
        sprintf(tmpstr,"TestInOut18 after %s\n",*aString);
        writeResult(SERVER_INOUT_RES, tmpstr);
 
        delete tmpstr;
        return NS_OK;
    }


};

NS_IMPL_ISUPPORTS(nsRPCTestInOutImpl, NS_GET_IID(nsIRPCTestInOut));
int main(int argc, char **args) {

   int i;
    const short num = 2;
    nsRPCTestInOutImpl * test[num];

    setProcessId(INOUT_FDATA);
    for(i = 0; i < num; i++) {
       test[i] = new nsRPCTestInOutImpl();
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
