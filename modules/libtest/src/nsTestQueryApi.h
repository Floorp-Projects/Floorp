#ifndef nsTestQueryApiCID_h__
#define nsTestQueryApiCID_h__

#include "nsIFactory.h"

// {10709451-24C0-11d2-9A6D-0080C8845D91}
#define NS_TESTQUERYAPI_CID \
{ 0x47a6d120, 0x3c0c, 0x11d2, \
{ 0x9a, 0x9e, 0x0, 0x80, 0xc8, 0x84, 0x5d, 0x91 } }

extern nsresult GetTestQueryApiFactory(nsIFactory **aResult);

#endif /* nsTestQueryApiCID_h__ */