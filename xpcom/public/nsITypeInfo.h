#ifndef _NSTYPEINFO_
#define _NSTYPEINFO_
#include "nsISupports.h"
#include "nsTypeInfoDefs.h"

// {969dac50-75a5-11d2-abad-00805f6177ce}
#define NS_TYPEINFO_IID \
{ 0x969dac50, 0x75a5, 0x11d2, \
  { 0xab, 0xad, 0x00, 0x80, 0x5f, 0x61, 0x77, 0xce } }

class nsITypeInfo : public nsISupports
{
public:
	NS_IMETHOD GetTypeAttr(
		nsTYPEATTR **ppTypeAttr) = 0;
        
	NS_IMETHOD GetFuncDesc(
		PRUint32 index,
		nsFUNCDESC **ppFuncDesc) = 0;
        
	NS_IMETHOD GetVarDesc(
		PRUint32 index,
		nsVARDESC **ppVarDesc) = 0;
        
	NS_IMETHOD GetNames(
		nsDISPID memid,
		nsSTR *rgBstrNames,
		PRUint32 cMaxNames,
		PRUint32 *pcNames) = 0;
        
	NS_IMETHOD GetIDOfName( 
		nsSTR name,
		nsDISPID *pMemId) = 0;
        
	NS_IMETHOD Invoke( 
		void *pvInstance,
		nsDISPID memid,
		nsDISPATCHKIND dispKind,
		nsDISPPARAMS *pDispParams,
		nsVARIANT *pVarResult,
		nsEXCEP *pExcepInfo,
		PRInt32 *puArgErr) = 0;
        
	NS_IMETHOD AddressOfMember( 
		nsDISPID memid,
		nsINVOKEKIND invKind,
		void **ppv) = 0;
        
	NS_IMETHOD CreateInstance( 
		nsISupports *delegate,
		REFNSIID riid,
		void **ppvObj) = 0;
        
	NS_IMETHOD GetContainingTypeLib( 
		nsITypeLib **ppTLib,
		PRUint32 *pIndex) = 0;
        
	NS_IMETHOD_(void) ReleaseTypeAttr(
		nsTYPEATTR *pTypeAttr) = 0;
        
	NS_IMETHOD_(void) ReleaseFuncDesc(
		nsFUNCDESC *pFuncDesc) = 0;
        
	NS_IMETHOD_(void) ReleaseVarDesc(
		nsVARDESC *pVarDesc) = 0;
};
#endif // _NSTYPEINFO_
