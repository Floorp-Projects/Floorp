#include "test.h"
#include <stdio.h>

NS_DEFINE_IID(testIID, MY_TEST_IID);

NS_IMPL_ISUPPORTS(testC, testIID);

testC::testC() : msg(L"TEST:"), x(99)
{
	NS_INIT_REFCNT();
}

nsresult testC::foo(PRInt32 i, nsSTR s)
{
	printf("%ws %ws %d %ws\n", msg, s, i, s);
	return NS_OK;
}

nsresult testC::get_x(PRInt32 *_x)
{
	*_x = x;
	return NS_OK;
}

nsresult testC::set_x(PRInt32 _x)
{
	x = _x;
	return NS_OK;
}

testI* CreateTestI()
{
	return new testC();
}
