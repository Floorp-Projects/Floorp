#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"

class nsSimpleStreamListener : public nsISimpleStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISIMPLESTREAMLISTENER

    nsSimpleStreamListener() { NS_INIT_ISUPPORTS(); }
    virtual ~nsSimpleStreamListener() {}

protected:
    nsCOMPtr<nsIOutputStream>   mSink;
    nsCOMPtr<nsIStreamObserver> mObserver;
};
