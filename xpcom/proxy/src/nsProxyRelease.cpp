#include "nsProxyRelease.h"

PR_STATIC_CALLBACK(void*)
HandleProxyReleaseEvent(PLEvent *self)
{              
    nsISupports* owner = (nsISupports*) self->owner;
    NS_RELEASE(owner);                                                                                              
    return nsnull;                                            
}

PR_STATIC_CALLBACK(void)
DestroyProxyReleaseEvent(PLEvent *self)
{
    delete self;
}

NS_COM nsresult
NS_ProxyRelease(nsIEventTarget *target, nsISupports *doomed, PRBool alwaysProxy)
{
    nsresult rv;

    if (!target) {
        NS_RELEASE(doomed);
        return NS_OK;
    }

    if (!alwaysProxy) {
        PRBool onCurrentThread = PR_FALSE;
        rv = target->IsOnCurrentThread(&onCurrentThread);
        if (NS_SUCCEEDED(rv) && onCurrentThread) {
            NS_RELEASE(doomed);
            return NS_OK;
        }
    }

    PLEvent *ev = new PLEvent;
    if (!ev) {
        // we do not release doomed here since it may cause a delete on the
        // wrong thread.  better to leak than crash. 
        return NS_ERROR_OUT_OF_MEMORY;
    }

    PL_InitEvent(ev, doomed,
                 HandleProxyReleaseEvent,
                 DestroyProxyReleaseEvent);

    rv = target->PostEvent(ev);
    if (NS_FAILED(rv)) {
        NS_WARNING("failed to post proxy release event");
        PL_DestroyEvent(ev);
        // again, it is better to leak the doomed object than risk crashing as
        // a result of deleting it on the wrong thread.
    }
    return rv;
}
