#include "nsITypeLib.h"
#include "nsITypeInfo.h"

class nsTypeLib : public nsITypeLib {
protected:
    nsITypeInfo **ti;
	nsTYPEKIND *kind;
	nsTLIBATTR libattr;
	PRUint32 ti_count;
    NS_DECL_ISUPPORTS_EXPORTED
public:
	NS_EXPORT NS_IMETHODIMP GetTypeInfoCount(PRInt32 *count);
        
	NS_EXPORT NS_IMETHODIMP GetTypeInfo( 
		PRUint32 index,
		nsITypeInfo **ppTInfo);
        
	NS_EXPORT NS_IMETHODIMP GetTypeInfoType( 
	    PRUint32 index,
		nsTYPEKIND *pTKind);
        
	NS_EXPORT NS_IMETHODIMP GetTypeInfoOfGuid( 
		REFNSIID guid,
		nsITypeInfo **ppTinfo);
        
	NS_EXPORT NS_IMETHODIMP GetLibAttr( 
		nsTLIBATTR **ppTLibAttr);
        
	NS_EXPORT NS_IMETHODIMP_(void) ReleaseLibAttr( 
		nsTLIBATTR *pTLibAttr);

	NS_EXPORT nsTypeLib(PRUint32 count);
	NS_EXPORT ~nsTypeLib();
};
