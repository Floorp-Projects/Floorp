#ifndef nsITestEventsApi_h__
#define nsITestEventsApi_h__

#include "nsISupports.h"
#include "layprobe.h"

// {10709450-24C0-11d2-9A6D-0080C8845D91}
#define NS_ITESTEVENTSAPI_IID \
	{ 0x10709450, 0x24c0, 0x11d2, \
	{ 0x9a, 0x6d, 0x0, 0x80, 0xc8, 0x84, 0x5d, 0x91 }};

// 
//	nsITestApi Interface declaration
//////////////////////////////////////

class nsITestEventsApi: public nsISupports {
public:

	NS_IMETHOD RegisterNotifyCallback(
		int32*			OUT_lpCallbackID,
		ID_NOTIFY_PT*	lpFunc,
		int32			EventID
	)=0;

	NS_IMETHOD RegisterElementCallback(
		int32*				OUT_lpCallbackID,
		ELEMENT_NOTIFY_PT*	lpFunc,
		int32				EventID
	)=0;

	NS_IMETHOD UnregisterCallbackFunction(
		void* CalbackID
	)=0;
};

#endif /* nsITestEventsApi */