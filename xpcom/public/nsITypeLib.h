#ifndef _NSTYPELIB_
#define _NSTYPELIB_
#include "nsISupports.h"
#include "nsTypeInfoDefs.h"

// {4f50cda0-75a5-11d2-abad-00805f6177ce}
#define NS_TYPELIB_IID \
{ 0x4f50cda0, 0x75a5, 0x11d2, \
  { 0xab, 0xad, 0x00, 0x80, 0x5f, 0x61, 0x77, 0xce } }

class nsITypeLib : public nsISupports
{
public:
	NS_IMETHOD GetTypeInfoCount(PRInt32 *count) = 0;
        
	NS_IMETHOD GetTypeInfo( 
		PRUint32 index,
		nsITypeInfo **ppTInfo) = 0;
        
	NS_IMETHOD GetTypeInfoType( 
	    PRUint32 index,
		nsTYPEKIND *pTKind) = 0;
        
	NS_IMETHOD GetTypeInfoOfGuid( 
		REFNSIID guid,
		nsITypeInfo **ppTinfo) = 0;
        
	NS_IMETHOD GetLibAttr( 
		nsTLIBATTR **ppTLibAttr) = 0;
        
	NS_IMETHOD_(void) ReleaseLibAttr( 
		nsTLIBATTR *pTLibAttr) = 0;
};
#endif // _NSTYPELIB_
