#ifndef __nstypeinfo__
#define __nstypeinfo__

#include "nsITypeLib.h"
#include "nsITypeInfo.h"

class nsTypeInfo : public nsITypeInfo {
protected:
	nsITypeLib* lib;
	nsTYPEATTR tattr;
	nsVARTYPE **paramdesc;
	nsFUNCDESC *fdesc;
	nsSTR *fnames;
	nsVARDESC *vdesc;
	nsSTR *vnames;
    NS_DECL_ISUPPORTS_EXPORTED
public:

	NS_EXPORT NS_IMETHODIMP GetTypeAttr(
		nsTYPEATTR **ppTypeAttr);
        
	NS_EXPORT NS_IMETHODIMP GetFuncDesc(
		PRUint32 index,
		nsFUNCDESC **ppFuncDesc);
        
	NS_EXPORT NS_IMETHODIMP GetVarDesc(
		PRUint32 index,
		nsVARDESC **ppVarDesc);
        
	NS_EXPORT NS_IMETHODIMP GetNames(
		nsDISPID memid,
		nsSTR *rgBstrNames,
		PRUint32 cMaxNames,
		PRUint32 *pcNames);
        
	NS_EXPORT NS_IMETHODIMP GetIDOfName( 
		nsSTR name,
		nsDISPID *pMemId);
    
	NS_EXPORT NS_IMETHODIMP InvokeCheck( 
		nsDISPID memid,
		nsDISPATCHKIND dispKind,
		nsDISPPARAMS *pDispParams,
		PRInt32 *puArgErr);
        
	NS_EXPORT NS_IMETHODIMP AddressOfMember( 
		nsDISPID memid,
		nsINVOKEKIND invKind,
		void **ppv);
        
	NS_EXPORT NS_IMETHODIMP CheckInstance( 
		nsISupports *nsi,
		REFNSIID riid,
		void **ppvObj);
        
	NS_EXPORT NS_IMETHODIMP GetContainingTypeLib( 
		nsITypeLib **ppTLib,
		PRUint32 *pIndex);
        
	NS_EXPORT NS_IMETHODIMP_(void) ReleaseTypeAttr(
		nsTYPEATTR *pTypeAttr);
        
	NS_EXPORT NS_IMETHODIMP_(void) ReleaseFuncDesc(
		nsFUNCDESC *pFuncDesc);
        
	NS_EXPORT NS_IMETHODIMP_(void) ReleaseVarDesc(
		nsVARDESC *pVarDesc);

	NS_EXPORT nsTypeInfo(nsITypeLib *l, PRUint32 nfuns, PRUint32 nvars);
	NS_EXPORT ~nsTypeInfo();
};

#endif
