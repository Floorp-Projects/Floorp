#include "nsIStreamProvider.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"

class nsSimpleStreamProvider : public nsISimpleStreamProvider
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMPROVIDER
    NS_DECL_NSISIMPLESTREAMPROVIDER

    nsSimpleStreamProvider() { NS_INIT_ISUPPORTS(); }
    virtual ~nsSimpleStreamProvider() {}

protected:
    nsCOMPtr<nsIInputStream>    mSource;
    nsCOMPtr<nsIStreamObserver> mObserver;
};
