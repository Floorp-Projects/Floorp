#include <stdio.h>
#include <stream.h>
#include <fstream.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include "RPCServerService.h"
#include "nsISupports.h"
#include "nsIJVMManager.h"
#include "nsIRPCTestOut.h"
#include "IDispatcher.h"
#include "nsIThread.h"
#include "deftest.h"
#include "proto.h"

class nsRPCTestOutImpl  : public  nsIRPCTestOut {
    NS_DECL_ISUPPORTS 
    nsRPCTestOutImpl () {
	NS_INIT_REFCNT();
    }

    NS_IMETHOD TestOut1(PRBool *bool) {
        printf("TestOut1 this=%p\n", this);
        *bool = PR_FALSE;
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestOut1 %d\n",*bool);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestOut2(PRUint8 *octet) {
        printf("TestOut2 this=%p\n", this);
        *octet = 88; 
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestOut2 %o\n",*octet);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestOut3(PRInt16 *sInt) {
        printf("TestOut3 this=%p\n", this);
        *sInt = SHRT_MAX;
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestOut3 %d\n",*sInt);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestOut4(PRInt32 *lInt) {
        printf("TestOut4 this=%p\n", this);
        *lInt = LONG_MAX;
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestOut4 %d\n",*lInt);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestOut5(PRInt64 *llInt) {
        printf("TestOut5 this=%p\n", this);
        *llInt = LONG_MAX;
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestOut5 %d\n",*llInt);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestOut6(PRUint16 *usInt) {
        printf("TestOut6 this=%p\n", this);
        *usInt = USHRT_MAX;
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestOut6 %u\n",*usInt);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestOut7(PRUint32 *ulInt) {
        printf("TestOut7 this=%p\n", this);
        *ulInt = ULONG_MAX;
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestOut7 %u\n",*ulInt);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestOut8(PRUint64 *ullInt) {
        printf("TestOut8 this=%p\n", this);
        *ullInt = ULONG_MAX;
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestOut8 %u\n",*ullInt);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestOut9(float *f) {
        printf("TestOut9 this=%p\n", this);
        *f = FLT_MAX;
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestOut9 %.50f\n",*f);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestOut10(double *d) {
        printf("TestOut10 this=%p\n", this);
        *d = DBL_MAX;
        char *tmpstr = new char[500];
        sprintf(tmpstr,"TestOut10 %.50f\n",*d);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestOut11(char *c) {
        printf("TestOut11 this=%p\n", this);
        *c = 'G';
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestOut11 %c\n",*c);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestOut12(PRUnichar *unic) {
        printf("TestOut12 this=%p\n", this);
        *unic = 'G';
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestOut12 %c\n",*unic);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestOut13(char **s) {
        printf("TestOut13 this=%p\n", this);
        *s = "remote ipc";
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestOut13 %s\n",*s);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestOut14(PRUnichar **unis) {
        printf("TestOut14 this=%p\n", this);
        //*unis = "remote ipc";
        *unis = new PRUnichar[50];
        for (int i = 0; i < 10; i++) {
           (*unis)[i] = 'a' + 1;
        }
        (*unis)[i] = '\0';
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestOut14 %s\n",*unis);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
	return NS_OK;
    }

    NS_IMETHOD TestOut15(PRUint32 *count, char **valueArray) {
        printf("--TestOut15\n");
        *valueArray = "This is a remote ipc test.";
        *count = strlen(*valueArray);
        char *tmpstr = new char[200];
        printf("TestOut15 count %d\n", *count);
        sprintf(tmpstr, "TestOut15 count %d\n", *count);
        writeResult(SERVER_OUT_RES, tmpstr);
        printf("TestOut15 valueArray %s\n", *valueArray);
        sprintf(tmpstr,"TestOut15 Array=%s\n",*valueArray);
        writeResult(SERVER_OUT_RES, tmpstr);
        delete tmpstr;
        return NS_OK;
    }

    NS_IMETHOD TestOut16(PRBool *bBool, char *cChar, PRUint8 *nByte,
                        PRInt16 *nShort, PRUint16 *nUShort,
                        PRInt32 *nLong,PRUint32 *nULong,
                        PRInt64 *nHyper, PRUint64 *nUHyper,
                        float *fFloat, double *fDouble, char **aString)
    {
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

        printf("TestOut16 this=%p\n", this) ;
        char *tmpstr = new char[200];
        sprintf(tmpstr,"TestOut16 PRBool %d\n",*bBool);
        writeResult(SERVER_OUT_RES, tmpstr);
        sprintf(tmpstr,"TestOut16 PRUint8 %o\n",*nByte);
        writeResult(SERVER_OUT_RES, tmpstr);
        sprintf(tmpstr,"TestOut16 PRInt16 %d\n",*nShort);
        writeResult(SERVER_OUT_RES, tmpstr);
        sprintf(tmpstr,"TestOut16 PRInt32 %d\n",*nLong);
        writeResult(SERVER_OUT_RES, tmpstr);
        sprintf(tmpstr,"TestOut16 PRInt64 %d\n",*nHyper);
        writeResult(SERVER_OUT_RES, tmpstr);
        sprintf(tmpstr,"TestOut16 PRUint16 %u\n",*nUShort);
        writeResult(SERVER_OUT_RES, tmpstr);
        sprintf(tmpstr,"TestOut16 PRUint32 %u\n",*nULong);
        writeResult(SERVER_OUT_RES, tmpstr);
        sprintf(tmpstr,"TestOut16 PRUint64 %u\n",*nUHyper);
        writeResult(SERVER_OUT_RES, tmpstr);
        sprintf(tmpstr,"TestOut16 float %.50f\n",*fFloat);
        writeResult(SERVER_OUT_RES, tmpstr);
        sprintf(tmpstr,"TestOut16 double %.50f\n",*fDouble);
        writeResult(SERVER_OUT_RES, tmpstr);
        sprintf(tmpstr,"TestOut16 char %c\n",*cChar);
        writeResult(SERVER_OUT_RES, tmpstr);
        sprintf(tmpstr,"TestOut16 string %s\n",*aString);
        writeResult(SERVER_OUT_RES, tmpstr);

        delete tmpstr;
        return NS_OK;
    }

    NS_IMETHOD TestOut17(PRUint32 *count, PRInt32 **longArray) {
        printf("--TestOut17\n");
        char *tmpstr = new char[200];
        *count = 5;
        sprintf(tmpstr, "TestOut17 count %d\n", *count);
        writeResult(SERVER_OUT_RES, tmpstr);
        *longArray = new PRInt32[*count];
        for (int i = 0; i < *count; i++) {
            (*longArray)[i] = 55555 + i;
            printf("TestOut17 Array[%d]=%d\n",i,(*longArray)[i]);
            sprintf(tmpstr,"TestOut17 Array[%d]=%d\n",i,(*longArray)[i]);
            writeResult(SERVER_OUT_RES, tmpstr);
        }
        delete tmpstr;
        return NS_OK;
    }




};

NS_IMPL_ISUPPORTS(nsRPCTestOutImpl , NS_GET_IID(nsIRPCTestOut));
int main(int argc, char **args) {
   int i;
    const short num = 5;
    nsRPCTestOutImpl * test[num];

    setProcessId(OUT_FDATA);
    for(i = 0; i < num; i++) {
       test[i] = new nsRPCTestOutImpl();
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

