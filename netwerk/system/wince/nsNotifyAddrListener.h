
#ifndef NSNOTIFYADDRLISTENER_H_
#define NSNOTIFYADDRLISTENER_H_

#include <windows.h>

#include "nsITimer.h"
#include "nsINetworkLinkService.h"
#include "nsCOMPtr.h"


class nsNotifyAddrListener : public nsINetworkLinkService, nsITimerCallback
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSINETWORKLINKSERVICE
    NS_DECL_NSITIMERCALLBACK

    nsNotifyAddrListener();
    virtual ~nsNotifyAddrListener();

    nsresult Init(void);

protected:
    nsCOMPtr<nsITimer> mTimer;

    PRPackedBool mLinkUp;
    PRPackedBool mStatusKnown;
    HANDLE mConnectionHandle;
};

#endif /* NSNOTIFYADDRLISTENER_H_ */
