/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDashboard_h__
#define nsDashboard_h__

#include "nsIDashboard.h"
#include "nsIDashboardEventNotifier.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsIDNSService.h"
#include "nsIServiceManager.h"
#include "nsIThread.h"
#include "nsSocketTransport2.h"
#include "mozilla/net/DashboardTypes.h"
#include "nsHttp.h"
#include "nsITransport.h"
#include "nsITimer.h"

namespace mozilla {
namespace net {

class Dashboard:
    public nsIDashboard,
    public nsIDashboardEventNotifier,
    public nsITransportEventSink,
    public nsITimerCallback
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIDASHBOARD
    NS_DECL_NSIDASHBOARDEVENTNOTIFIER
    NS_DECL_NSITRANSPORTEVENTSINK
    NS_DECL_NSITIMERCALLBACK

    Dashboard();
    friend class DashConnStatusRunnable;
    static const char *GetErrorString(nsresult rv);
private:
    virtual ~Dashboard();

    void GetSocketsDispatch();
    void GetHttpDispatch();
    void GetDnsInfoDispatch();
    void StartTimer(uint32_t aTimeout);
    void StopTimer();
    nsresult TestNewConnection(const nsACString& aHost, uint32_t aPort,
                               const char *aProtocol, uint32_t aTimeout);

    /* Helper methods that pass the JSON to the callback function. */
    nsresult GetSockets();
    nsresult GetHttpConnections();
    nsresult GetWebSocketConnections();
    nsresult GetDNSCacheEntries();
    nsresult GetConnectionStatus(struct ConnStatus aStatus);

private:
    struct SocketData
    {
        uint64_t totalSent;
        uint64_t totalRecv;
        nsTArray<SocketInfo> data;
        nsCOMPtr<NetDashboardCallback> cb;
        nsIThread* thread;
    };

    struct HttpData {
        nsTArray<HttpRetParams> data;
        nsCOMPtr<NetDashboardCallback> cb;
        nsIThread* thread;
    };

    struct LogData
    {
        LogData(nsCString host, uint32_t serial, bool encryption):
            mHost(host),
            mSerial(serial),
            mMsgSent(0),
            mMsgReceived(0),
            mSizeSent(0),
            mSizeReceived(0),
            mEncrypted(encryption)
        { }
        nsCString mHost;
        uint32_t  mSerial;
        uint32_t  mMsgSent;
        uint32_t  mMsgReceived;
        uint64_t  mSizeSent;
        uint64_t  mSizeReceived;
        bool      mEncrypted;
        bool operator==(const LogData& a) const
        {
            return mHost.Equals(a.mHost) && (mSerial == a.mSerial);
        }
    };

    struct WebSocketData
    {
        WebSocketData():lock("Dashboard.webSocketData")
        {
        }
        uint32_t IndexOf(nsCString hostname, uint32_t mSerial)
        {
            LogData temp(hostname, mSerial, false);
            return data.IndexOf(temp);
        }
        nsTArray<LogData> data;
        mozilla::Mutex lock;
        nsCOMPtr<NetDashboardCallback> cb;
        nsIThread* thread;
    };

    struct DnsData
    {
        nsCOMPtr<nsIDNSService> serv;
        nsTArray<DNSCacheEntries> data;
        nsCOMPtr<NetDashboardCallback> cb;
        nsIThread* thread;
    };

    struct ConnectionData
    {
        nsCOMPtr<nsISocketTransport> socket;
        nsCOMPtr<nsIInputStream> streamIn;
        nsCOMPtr<nsITimer> timer;
        nsCOMPtr<NetDashboardCallback> cb;
        nsIThread* thread;
    };

    bool mEnableLogging;

    struct SocketData mSock;
    struct HttpData mHttp;
    struct WebSocketData mWs;
    struct DnsData mDns;
    struct ConnectionData mConn;
};

class DashConnStatusRunnable: public nsRunnable
{
public:
    DashConnStatusRunnable(Dashboard * aDashboard, ConnStatus aStatus)
    : mDashboard(aDashboard)
    {
        mStatus.creationSts = aStatus.creationSts;
    }

    NS_IMETHODIMP Run()
    {
        return mDashboard->GetConnectionStatus(mStatus);
    }

private:
    ConnStatus mStatus;
    Dashboard * mDashboard;
};

} } // namespace mozilla::net
#endif // nsDashboard_h__
