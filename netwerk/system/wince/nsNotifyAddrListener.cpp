#include "nsNotifyAddrListener.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsIObserverService.h"

#include <objbase.h>
#ifdef WINCE_WINDOWS_MOBILE
#include <connmgr.h>
#endif

// pulled from the header so that we do not get multiple define errors during link
static const GUID nal_DestNetInternet =
        { 0x436ef144, 0xb4fb, 0x4863, { 0xa0, 0x41, 0x8f, 0x90, 0x5a, 0x62, 0xc5, 0x72 } };


NS_IMPL_THREADSAFE_ISUPPORTS2(nsNotifyAddrListener,
                              nsINetworkLinkService,
                              nsITimerCallback)

nsNotifyAddrListener::nsNotifyAddrListener()
#ifdef wINCE_WINDOWS_MOBILE
    : mLinkUp(PR_FALSE)  // assume false by default
    , mStatusKnown(PR_FALSE)
#else
    : mLinkUp(PR_TRUE)  // assume true by default on non-WinMo
    , mStatusKnown(PR_TRUE)
#endif
    , mConnectionHandle(NULL)
{
}

nsNotifyAddrListener::~nsNotifyAddrListener()
{
#ifdef WINCE_WINDOWS_MOBILE
  if (mConnectionHandle)
    ConnMgrReleaseConnection(mConnectionHandle, 0);

  if (mTimer)
    mTimer->Cancel();
#endif
}

nsresult
nsNotifyAddrListener::Init(void)
{
#ifdef WINCE_WINDOWS_MOBILE
  CONNMGR_CONNECTIONINFO conn_info;
  memset(&conn_info, 0, sizeof(conn_info));
  
  conn_info.cbSize      = sizeof(conn_info);
  conn_info.dwParams    = CONNMGR_PARAM_GUIDDESTNET;
  conn_info.dwPriority  = CONNMGR_PRIORITY_LOWBKGND;
  conn_info.guidDestNet = nal_DestNetInternet;
  conn_info.bExclusive  = FALSE;
  conn_info.bDisabled   = FALSE;
  
  ConnMgrEstablishConnection(&conn_info, 
                             &mConnectionHandle);

  mTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mTimer)
      mTimer->InitWithCallback(this,
                               15*1000, // every 15 seconds
                               nsITimer::TYPE_REPEATING_SLACK);
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsNotifyAddrListener::GetIsLinkUp(PRBool *aIsUp)
{
  *aIsUp = mLinkUp;
  return NS_OK;
}

NS_IMETHODIMP
nsNotifyAddrListener::GetLinkStatusKnown(PRBool *aIsUp)
{
  *aIsUp = mStatusKnown;
  return NS_OK;
}

NS_IMETHODIMP
nsNotifyAddrListener::Notify(nsITimer* aTimer)
{
#ifdef WINCE_WINDOWS_MOBILE
  DWORD status;
  HRESULT result = ConnMgrConnectionStatus(mConnectionHandle, &status);

  if (FAILED(result)) {
    mLinkUp = PR_FALSE;
    mStatusKnown = PR_FALSE;
  }
  else
  {
    mLinkUp = (status==CONNMGR_STATUS_CONNECTED);
    mStatusKnown = PR_TRUE;
  }

  nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");

  if (!observerService)
      return NS_ERROR_FAILURE;
  
  const char *event;
  if (!mStatusKnown)
      event = NS_NETWORK_LINK_DATA_UNKNOWN;
  else
      event = mLinkUp ? NS_NETWORK_LINK_DATA_UP
          : NS_NETWORK_LINK_DATA_DOWN;
  
  observerService->NotifyObservers(static_cast<nsINetworkLinkService*>(this),
                                   NS_NETWORK_LINK_TOPIC,
                                   NS_ConvertASCIItoUTF16(event).get());
#endif
  return NS_OK;
}
