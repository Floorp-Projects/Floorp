#ifndef nsTestEventsApiCID_h__
#define nsTestEventsApiCID_h__

#include "nsIFactory.h"

// {10709451-24C0-11d2-9A6D-0080C8845D91}
#define NS_TESTEVENTSAPI_CID \
{ 0x10709451, 0x24c0, 0x11d2, \
{ 0x9a, 0x6d, 0x0, 0x80, 0xc8, 0x84, 0x5d, 0x91 } }

extern nsresult GetTestEventsApiFactory(nsIFactory **aResult);

#endif  /* nsTestEventsApiCID_h__ */