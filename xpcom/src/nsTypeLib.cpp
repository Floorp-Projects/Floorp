#include "nsTypeLib.h"
#include "nsTypeInfo.h"

NS_DEFINE_IID(typeLibIID, NS_TYPELIB_IID);
NS_DEFINE_IID(typeInfIID, NS_TYPEINFO_IID);
NS_DEFINE_IID(nsSupportsIID, NS_ISUPPORTS_IID);

NS_IMPL_ADDREF(nsTypeLib);
NS_IMPL_RELEASE(nsTypeLib);

nsresult nsTypeLib::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
	if (NULL == aInstancePtr) {                                            
		return NS_ERROR_NULL_POINTER;
	}
	*aInstancePtr = NULL;
	if (aIID.Equals(typeLibIID)) {
		*aInstancePtr = (void*)(nsITypeLib*)this;
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

nsTypeLib::nsTypeLib(PRUint32 count) : ti_count(count)
{
	NS_INIT_REFCNT();
	ti = new nsITypeInfo*[ti_count];
	kind = new nsTYPEKIND[ti_count];
	for (PRUint32 i=0; i<ti_count; i++)
		ti[i] = NULL;
}

nsTypeLib::~nsTypeLib() 
{
	for (PRUint32 i=0; i<ti_count; i++)
		if (ti[i]) {
			if (kind[i] == TKIND_INTERFACE)
				ti[i]->Release();
			else // class
				delete ti[i];
		}
	delete kind;
	delete ti;
}

nsresult nsTypeLib::GetTypeInfoCount(PRInt32 *_count)
{
	*_count = ti_count;
	return NS_OK;
}

nsresult nsTypeLib::GetTypeInfo(PRUint32 index, nsITypeInfo **ppTInfo)
{
	if (index >= ti_count)
		return NS_NOSUCH_INDEX;
	*ppTInfo = ti[index];
	ti[index]->AddRef();
	return NS_OK;
}

nsresult nsTypeLib::GetTypeInfoType(PRUint32 index, nsTYPEKIND *pTKind)
{
	if (index >= ti_count)
		return NS_NOSUCH_INDEX;
	*pTKind = kind[index];
	return NS_OK;
}

nsresult nsTypeLib::GetTypeInfoOfGuid(REFNSIID guid, nsITypeInfo **ppTinfo)
{
	for (PRUint32 i=0; i<ti_count; i++)
		if (ti[i]->QueryInterface(guid,(void**)ppTinfo) == NS_OK)
			return NS_OK;
	*ppTinfo = NULL;
	return NS_NOSUCH_ID;
}

nsresult nsTypeLib::GetLibAttr(nsTLIBATTR **ppTLibAttr)
{
	*ppTLibAttr = &libattr;
	return NS_OK;
}

void nsTypeLib::ReleaseLibAttr(nsTLIBATTR *pTLibAttr)
{

}
