#include "testTypeLib.h"
#include "testTypeInfo.h"
#include "test.h"
#include <stdio.h>

/*
 * main for testing typeinfo and typelibs.
 */

int main(int argc, char *argv[])
{
	nsIID myLib = MY_TYPELIB_IID;
	nsIID myTypeInf = MY_TYPEINFO_IID;
	nsIID myTest = MY_TEST_IID;
	nsITypeLib* lib1;
	nsITypeInfo* info1;
	void *inst;
	nsresult nr;
	nsSTR nfoo = L"foo";
	nsSTR nx = L"x";
	nsDISPID id;
	nsDISPPARAMS params;
	nsVARIANT args[2];
	nsVARIANT res;
	nsEXCEP excep;
	PRInt32 argerr = -1;

	nr = GetTypeLib(myLib, &lib1);
	if (nr != NS_OK)
		return -1;
	nr = lib1->GetTypeInfoOfGuid(myTypeInf, &info1);
	if (nr != NS_OK) {
		lib1->Release();
		return -1;
	}
	nr = info1->CreateInstance(NULL,myTest,&inst);
	if (nr != NS_OK) {
		lib1->Release();
		return -1;
	}
	nr = info1->GetIDOfName(nfoo,&id);
	if (nr != NS_OK) {
		lib1->Release();
		return -1;
	}
	args[0].kind = VT_I4;
	args[0].u.lVal = 42;
	args[1].kind = VT_WSTR;
	args[1].u.wstrVal = L"Kaviar";
	params.argv = (nsVARIANT*)&args;
	params.argc = 2;
	excep.scode = 0;			// 0=no error
	nr = info1->Invoke(inst, id, DISPATCH_FUNC, &params, &res, &excep, &argerr);
	if (nr != NS_OK) {
		info1->Release();
		lib1->Release();
		return -1;
	}
	if ((res.kind & VT_BYREF) != 0) {
		printf("By reference:\n");
		res.u.dblVal = *(double*)res.u.voidVal;
		res.kind &= ~VT_BYREF;
	}
	switch (res.kind) {
	case VT_EMPTY:
		printf("no result!\n");
		break;
    case VT_NULL:
		printf("NULL result!\n");
		break;
	case VT_I2:
		printf("short result: %d!\n", res.u.iVal);
		break;
    case VT_I4:
		printf("long result: %d!\n", res.u.lVal);
		break;
    case VT_R4:
		printf("float result: %e!\n", res.u.fltVal);
		break;
    case VT_R8:
		printf("double result: %e!\n", res.u.dblVal);
		break;
    case VT_ERROR:
		printf("error result: %d!\n", res.u.scode);
		break;
    case VT_BOOL:
		printf("boolean result: %d!\n", res.u.boolVal);
		break;
    case VT_SUPPORT:
		printf("ISupports result: %x!\n", res.u.suppVal);
		break;
    case VT_STR:
		printf("string result: %s!\n", res.u.strVal);
		break;
    case VT_WSTR:
		printf("string result: %ws!\n", res.u.wstrVal);
		break;
    case VT_VOID:
		printf("void * result: %x!\n", res.u.voidVal);
		break;
    case VT_C1:
		printf("character result: %d!\n", res.u.cVal);
		break;
    case VT_C2:
		printf("character result: %d!\n", res.u.wVal);
		break;
	default:
		printf("HUH!?\n");
	}
	PRInt32 x;
	args[0].kind = VT_I4 | VT_BYREF;
	args[0].u.plVal = &x;
	params.argv = (nsVARIANT*)&args;
	params.argc = 1;
	nr = info1->GetIDOfName(nx,&id);
	if (nr != NS_OK) {
		lib1->Release();
		return -1;
	}
	nr = info1->Invoke(inst, id, DISPATCH_PROPERTYGET, &params, &res, &excep, &argerr);
	printf("x was set to: %d\n",x);
	info1->Release();
	lib1->Release();
	return 0;
}
