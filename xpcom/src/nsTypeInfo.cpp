#include "nsTypeInfo.h"

NS_DEFINE_IID(typeInfoIID, NS_TYPEINFO_IID);
NS_DEFINE_IID(nsSupportsIID, NS_ISUPPORTS_IID);

NS_IMPL_ADDREF(nsTypeInfo);
NS_IMPL_RELEASE(nsTypeInfo);

nsresult nsTypeInfo::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
	if (NULL == aInstancePtr) {                                            
		return NS_ERROR_NULL_POINTER;
	}
	*aInstancePtr = NULL;
	if (aIID.Equals(typeInfoIID)) {
		*aInstancePtr = (void*)(nsITypeInfo*)this;
		NS_ADDREF_THIS();
		return NS_OK;
	}
	if (aIID.Equals(nsSupportsIID)) {
		*aInstancePtr = (void*) ((nsISupports*)this);
		NS_ADDREF_THIS();
		return NS_OK;
	}
	return NS_NOINTERFACE;
}

nsTypeInfo::nsTypeInfo(nsITypeLib* l, PRUint32 nfuns, PRUint32 nvars) 
{
	NS_INIT_REFCNT();
	lib = l;
	lib->AddRef();
	fnames = new nsSTR[nfuns];
	fdesc = new nsFUNCDESC[nfuns];
	paramdesc = new nsVARTYPE*[nfuns];
	vnames = new nsSTR[nvars];
	vdesc = new nsVARDESC[nvars];
}

nsTypeInfo::~nsTypeInfo() 
{
	if (lib)
		lib->Release();
	delete vdesc;
	delete vnames;
	delete paramdesc;
	delete fdesc;
	delete fnames;
}

nsresult nsTypeInfo::GetTypeAttr(
	nsTYPEATTR **ppTypeAttr)
{
	*ppTypeAttr = &tattr;
	return NS_OK;
}

nsresult nsTypeInfo::GetFuncDesc(
	PRUint32 index,
	nsFUNCDESC **ppFuncDesc)
{
	if (index >= tattr.cFuncs)
		return NS_NOSUCH_INDEX;
	*ppFuncDesc = &fdesc[index];
	return NS_OK;
}

nsresult nsTypeInfo::GetVarDesc(
	PRUint32 index,
	nsVARDESC **ppVarDesc)
{
	if (index >= tattr.cFuncs)
		return NS_NOSUCH_INDEX;
	*ppVarDesc = &vdesc[index];
	return NS_OK;
}

nsresult nsTypeInfo::GetNames(
	nsDISPID memid,
	nsSTR *rgBstrNames,
	PRUint32 cMaxNames,
	PRUint32 *pcNames)
{
	PRUint32 i;
	if (memid < tattr.cFuncs) {
		for (i=0; i<tattr.cFuncs; i++)
			if (fdesc[i].dispid == memid) {
				rgBstrNames[0]= fnames[memid];
				*pcNames = 1;
				return NS_OK;	
			}
		return NS_NOSUCH_INDEX;
	}
	else {
		for (i=0; i<tattr.cVars; i++)
			if (vdesc[i].dispid == memid) {
				rgBstrNames[0]= vnames[memid];
				*pcNames = 1;
				return NS_OK;	
			}
		return NS_NOSUCH_INDEX;
	}
}

nsresult nsTypeInfo::GetIDOfName( 
	nsSTR name, 
	nsDISPID *pMemId)
{
	PRUint32 i;
	for (i=0; i<tattr.cFuncs; i++)
		if (wcscmp(name,fnames[i]) == 0) {
			*pMemId = fdesc[i].dispid;
			return NS_OK;
		}
	for (i=0; i<tattr.cVars; i++)
		if (wcscmp(name,vnames[i]) == 0) {
			*pMemId = vdesc[i].dispid;
			return NS_OK;
		}
	return NS_NOSUCH_NAME;
}

nsresult nsTypeInfo::InvokeCheck( 
	nsDISPID memid,
	nsDISPATCHKIND dispKind,
	nsDISPPARAMS *pDispParams,
	PRInt32 *puArgErr)
{
	PRUint32 i;

	if (memid > (tattr.cFuncs + tattr.cVars))
		return NS_NOSUCH_INDEX;
	switch (dispKind) {
	case DISPATCH_FUNC:
		if (pDispParams->argc != fdesc[memid].cParams)
			return NS_ARGUMENT_MISMATCH;
		for (i=0; i<pDispParams->argc; i++)
			if (pDispParams->argv[i].kind != fdesc[memid].paramDesc[i]) {
				*puArgErr = i;
				return NS_TYPE_MISMATCH;
			}
		return NS_OK;
	case DISPATCH_PROPERTYGET:
		if (pDispParams->argc != 1)
			return NS_ARGUMENT_MISMATCH;
		if ((pDispParams->argv[0].kind & VT_BYREF) == 0) {
			*puArgErr = 0;
			return NS_TYPE_MISMATCH;
		}
		if ((pDispParams->argv[0].kind & ~VT_BYREF) != vdesc[memid-tattr.cFuncs].varType) {
			*puArgErr = 0;
			return NS_TYPE_MISMATCH;
		}
		return NS_OK;
	case DISPATCH_PROPERTYPUT:
		if (pDispParams->argc != 1)
			return NS_ARGUMENT_MISMATCH;
		if (pDispParams->argv[0].kind == vdesc[memid-tattr.cFuncs].varType) {
			*puArgErr = 0;
			return NS_TYPE_MISMATCH;
		}
		return NS_OK;
	default:
		return NS_DISPATCH_MISMATCH;
	}
}

nsresult nsTypeInfo::AddressOfMember( 
	nsDISPID memid,
	nsINVOKEKIND invKind,
	void **ppv)
{
	return NS_COMFALSE;			// no static members
}

nsresult nsTypeInfo::CheckInstance( 
	nsISupports *nsi,
	REFNSIID riid,
	void **ppvObj)
{
	if (nsi == NULL) {
		return NS_COMFALSE;
	}
	nsresult res = nsi->QueryInterface(riid, ppvObj);
	if (NS_FAILED(res)) {
		*ppvObj = NULL;
		delete nsi;
	}
	return res;
}

nsresult nsTypeInfo::GetContainingTypeLib( 
	nsITypeLib **ppTLib,
	PRUint32 *pIndex)
{
	*ppTLib = lib;
	pIndex = 0;
	return NS_OK;
}

void nsTypeInfo::ReleaseTypeAttr(nsTYPEATTR *pTypeAttr)
{

}

void nsTypeInfo::ReleaseFuncDesc(nsFUNCDESC *pFuncDesc)
{

}

void nsTypeInfo::ReleaseVarDesc(nsVARDESC *pVarDesc)
{
	
}
