#include "nsITypeInfo.h"

// {3959EF90-77FC-11d2-ABAD-00805F6177CE}
#define MY_TEST_IID { 0x3959ef90, 0x77fc, 0x11d2, { 0xab, 0xad, 0x0, 0x80, 0x5f, 0x61, 0x77, 0xce } }

class testI : public nsISupports {
public:
	NS_IMETHOD foo(PRInt32 i, nsSTR s) = 0;
	NS_IMETHOD get_x(PRInt32 *_x) = 0;
	NS_IMETHOD set_x(PRInt32 i) = 0;
};

class testC : public testI {
	nsSTR msg;
	PRInt32 x;
	NS_DECL_ISUPPORTS
public:
	NS_EXPORT NS_IMETHODIMP foo(PRInt32 i, nsSTR s);
	NS_EXPORT NS_IMETHODIMP get_x(PRInt32 *_x);
	NS_EXPORT NS_IMETHODIMP set_x(PRInt32 i);
	NS_EXPORT testC();
};


extern "C" NS_EXPORT testI* CreateTestI();
