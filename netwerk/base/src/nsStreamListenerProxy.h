#ifndef nsStreamListenerProxy_h__
#define nsStreamListenerProxy_h__

#include "nsStreamObserverProxy.h"
#include "nsIStreamListener.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"

class nsStreamListenerProxy : public nsStreamProxyBase
                            , public nsIStreamListenerProxy
                            , public nsIInputStreamObserver
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISTREAMLISTENERPROXY
    NS_DECL_NSIINPUTSTREAMOBSERVER

    nsStreamListenerProxy();
    virtual ~nsStreamListenerProxy();

    nsIStreamListener *GetListener()
    {
        return NS_STATIC_CAST(nsIStreamListener *, GetReceiver());
    }

    void SetListenerStatus(nsresult status)
    {
        mListenerStatus = status;
    }

    nsresult GetListenerStatus()
    {
        return mListenerStatus;
    }

    PRUint32 GetPendingCount();

protected:
    nsCOMPtr<nsIInputStream>  mPipeIn;
    nsCOMPtr<nsIOutputStream> mPipeOut;
    nsCOMPtr<nsIChannel>      mChannelToResume;
    PRLock                   *mLock;
    PRUint32                  mPendingCount;
    nsresult                  mListenerStatus;
};

#endif /* !nsStreamListenerProxy_h__ */
