/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 sw=4 sts=4 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nspr.h"
#include "private/pprio.h"
#include "nsString.h"
#include "nsCRT.h"

#include "nsIServiceManager.h"
#include "nsIDNSService.h"
#include "nsIDNSRecord.h"
#include "nsISOCKSSocketInfo.h"
#include "nsISocketProvider.h"
#include "nsSOCKSIOLayer.h"
#include "nsNetCID.h"
#include "nsIDNSListener.h"
#include "nsICancelable.h"
#include "nsThreadUtils.h"

static PRDescIdentity	nsSOCKSIOLayerIdentity;
static PRIOMethods	nsSOCKSIOLayerMethods;
static bool firstTime = true;
static bool ipv6Supported = true;

#if defined(PR_LOGGING)
static PRLogModuleInfo *gSOCKSLog;
#define LOGDEBUG(args) PR_LOG(gSOCKSLog, PR_LOG_DEBUG, args)
#define LOGERROR(args) PR_LOG(gSOCKSLog, PR_LOG_ERROR , args)

#else
#define LOGDEBUG(args)
#define LOGERROR(args)
#endif

class nsSOCKSSocketInfo : public nsISOCKSSocketInfo
                        , public nsIDNSListener
{
    enum State {
        SOCKS_INITIAL,
        SOCKS_DNS_IN_PROGRESS,
        SOCKS_DNS_COMPLETE,
        SOCKS_CONNECTING_TO_PROXY,
        SOCKS4_WRITE_CONNECT_REQUEST,
        SOCKS4_READ_CONNECT_RESPONSE,
        SOCKS5_WRITE_AUTH_REQUEST,
        SOCKS5_READ_AUTH_RESPONSE,
        SOCKS5_WRITE_CONNECT_REQUEST,
        SOCKS5_READ_CONNECT_RESPONSE_TOP,
        SOCKS5_READ_CONNECT_RESPONSE_BOTTOM,
        SOCKS_CONNECTED,
        SOCKS_FAILED
    };

    // A buffer of 262 bytes should be enough for any request and response
    // in case of SOCKS4 as well as SOCKS5
    static const uint32_t BUFFER_SIZE = 262;
    static const uint32_t MAX_HOSTNAME_LEN = 255;

public:
    nsSOCKSSocketInfo();
    virtual ~nsSOCKSSocketInfo() { HandshakeFinished(); }

    NS_DECL_ISUPPORTS
    NS_DECL_NSISOCKSSOCKETINFO
    NS_DECL_NSIDNSLISTENER

    void Init(int32_t version,
              int32_t family,
              const char *proxyHost,
              int32_t proxyPort,
              const char *destinationHost,
              uint32_t flags);

    void SetConnectTimeout(PRIntervalTime to);
    PRStatus DoHandshake(PRFileDesc *fd, int16_t oflags = -1);
    int16_t GetPollFlags() const;
    bool IsConnected() const { return mState == SOCKS_CONNECTED; }
    void ForgetFD() { mFD = nullptr; }

private:
    void HandshakeFinished(PRErrorCode err = 0);
    PRStatus StartDNS(PRFileDesc *fd);
    PRStatus ConnectToProxy(PRFileDesc *fd);
    void FixupAddressFamily(PRFileDesc *fd, PRNetAddr *proxy);
    PRStatus ContinueConnectingToProxy(PRFileDesc *fd, int16_t oflags);
    PRStatus WriteV4ConnectRequest();
    PRStatus ReadV4ConnectResponse();
    PRStatus WriteV5AuthRequest();
    PRStatus ReadV5AuthResponse();
    PRStatus WriteV5ConnectRequest();
    PRStatus ReadV5AddrTypeAndLength(uint8_t *type, uint32_t *len);
    PRStatus ReadV5ConnectResponseTop();
    PRStatus ReadV5ConnectResponseBottom();

    void WriteUint8(uint8_t d);
    void WriteUint16(uint16_t d);
    void WriteUint32(uint32_t d);
    void WriteNetAddr(const PRNetAddr *addr);
    void WriteNetPort(const PRNetAddr *addr);
    void WriteString(const nsACString &str);

    uint8_t ReadUint8();
    uint16_t ReadUint16();
    uint32_t ReadUint32();
    void ReadNetAddr(PRNetAddr *addr, uint16_t fam);
    void ReadNetPort(PRNetAddr *addr);

    void WantRead(uint32_t sz);
    PRStatus ReadFromSocket(PRFileDesc *fd);
    PRStatus WriteToSocket(PRFileDesc *fd);

private:
    State     mState;
    uint8_t * mData;
    uint8_t * mDataIoPtr;
    uint32_t  mDataLength;
    uint32_t  mReadOffset;
    uint32_t  mAmountToRead;
    nsCOMPtr<nsIDNSRecord>  mDnsRec;
    nsCOMPtr<nsICancelable> mLookup;
    nsresult                mLookupStatus;
    PRFileDesc             *mFD;

    nsCString mDestinationHost;
    nsCString mProxyHost;
    int32_t   mProxyPort;
    int32_t   mVersion;   // SOCKS version 4 or 5
    int32_t   mDestinationFamily;
    uint32_t  mFlags;
    PRNetAddr mInternalProxyAddr;
    PRNetAddr mExternalProxyAddr;
    PRNetAddr mDestinationAddr;
    PRIntervalTime mTimeout;
};

nsSOCKSSocketInfo::nsSOCKSSocketInfo()
    : mState(SOCKS_INITIAL)
    , mDataIoPtr(nullptr)
    , mDataLength(0)
    , mReadOffset(0)
    , mAmountToRead(0)
    , mProxyPort(-1)
    , mVersion(-1)
    , mDestinationFamily(PR_AF_INET)
    , mFlags(0)
    , mTimeout(PR_INTERVAL_NO_TIMEOUT)
{
    mData = new uint8_t[BUFFER_SIZE];
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &mInternalProxyAddr);
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &mExternalProxyAddr);
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &mDestinationAddr);
}

void
nsSOCKSSocketInfo::Init(int32_t version, int32_t family, const char *proxyHost, int32_t proxyPort, const char *host, uint32_t flags)
{
    mVersion         = version;
    mDestinationFamily = family;
    mProxyHost       = proxyHost;
    mProxyPort       = proxyPort;
    mDestinationHost = host;
    mFlags           = flags;
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsSOCKSSocketInfo, nsISOCKSSocketInfo, nsIDNSListener)

NS_IMETHODIMP 
nsSOCKSSocketInfo::GetExternalProxyAddr(PRNetAddr * *aExternalProxyAddr)
{
    memcpy(*aExternalProxyAddr, &mExternalProxyAddr, sizeof(PRNetAddr));
    return NS_OK;
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::SetExternalProxyAddr(PRNetAddr *aExternalProxyAddr)
{
    memcpy(&mExternalProxyAddr, aExternalProxyAddr, sizeof(PRNetAddr));
    return NS_OK;
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::GetDestinationAddr(PRNetAddr * *aDestinationAddr)
{
    memcpy(*aDestinationAddr, &mDestinationAddr, sizeof(PRNetAddr));
    return NS_OK;
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::SetDestinationAddr(PRNetAddr *aDestinationAddr)
{
    memcpy(&mDestinationAddr, aDestinationAddr, sizeof(PRNetAddr));
    return NS_OK;
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::GetInternalProxyAddr(PRNetAddr * *aInternalProxyAddr)
{
    memcpy(*aInternalProxyAddr, &mInternalProxyAddr, sizeof(PRNetAddr));
    return NS_OK;
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::SetInternalProxyAddr(PRNetAddr *aInternalProxyAddr)
{
    memcpy(&mInternalProxyAddr, aInternalProxyAddr, sizeof(PRNetAddr));
    return NS_OK;
}

// There needs to be a means of distinguishing between connection errors
// that the SOCKS server reports when it rejects a connection request, and
// connection errors that happen while attempting to connect to the SOCKS
// server. Otherwise, Firefox will report incorrectly that the proxy server
// is refusing connections when a SOCKS request is rejected by the proxy.
// When a SOCKS handshake failure occurs, the PR error is set to
// PR_UNKNOWN_ERROR, and the real error code is returned via the OS error.
void
nsSOCKSSocketInfo::HandshakeFinished(PRErrorCode err)
{
    if (err == 0) {
        mState = SOCKS_CONNECTED;
    } else {
        mState = SOCKS_FAILED;
        PR_SetError(PR_UNKNOWN_ERROR, err);
    }

    // We don't need the buffer any longer, so free it.
    delete [] mData;
    mData = nullptr;
    mDataIoPtr = nullptr;
    mDataLength = 0;
    mReadOffset = 0;
    mAmountToRead = 0;
    if (mLookup) {
        mLookup->Cancel(NS_ERROR_FAILURE);
        mLookup = nullptr;
    }
}

PRStatus
nsSOCKSSocketInfo::StartDNS(PRFileDesc *fd)
{
    NS_ABORT_IF_FALSE(!mDnsRec && mState == SOCKS_INITIAL,
                      "Must be in initial state to make DNS Lookup");

    nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
    if (!dns)
        return PR_FAILURE;

    mFD  = fd;
    nsresult rv = dns->AsyncResolve(mProxyHost, 0, this,
                                    NS_GetCurrentThread(),
                                    getter_AddRefs(mLookup));

    if (NS_FAILED(rv)) {
        LOGERROR(("socks: DNS lookup for SOCKS proxy %s failed",
                  mProxyHost.get()));
        return PR_FAILURE;
    }
    mState = SOCKS_DNS_IN_PROGRESS;
    PR_SetError(PR_IN_PROGRESS_ERROR, 0);
    return PR_FAILURE;
}

NS_IMETHODIMP
nsSOCKSSocketInfo::OnLookupComplete(nsICancelable *aRequest,
                                    nsIDNSRecord *aRecord,
                                    nsresult aStatus)
{
    NS_ABORT_IF_FALSE(aRequest == mLookup, "wrong DNS query");
    mLookup = nullptr;
    mLookupStatus = aStatus;
    mDnsRec = aRecord;
    mState = SOCKS_DNS_COMPLETE;
    if (mFD) {
      ConnectToProxy(mFD);
      ForgetFD();
    }
    return NS_OK;
}

PRStatus
nsSOCKSSocketInfo::ConnectToProxy(PRFileDesc *fd)
{
    PRStatus status;
    nsresult rv;

    NS_ABORT_IF_FALSE(mState == SOCKS_DNS_COMPLETE,
                      "Must have DNS to make connection!");

    if (NS_FAILED(mLookupStatus)) {
        PR_SetError(PR_BAD_ADDRESS_ERROR, 0);
        return PR_FAILURE;
    }

    // Try socks5 if the destination addrress is IPv6
    if (mVersion == 4 &&
        PR_NetAddrFamily(&mDestinationAddr) == PR_AF_INET6) {
        mVersion = 5;
    }

    int32_t addresses = 0;
    do {
        if (addresses++)
            mDnsRec->ReportUnusable(mProxyPort);
        
        rv = mDnsRec->GetNextAddr(mProxyPort, &mInternalProxyAddr);
        // No more addresses to try? If so, we'll need to bail
        if (NS_FAILED(rv)) {
            LOGERROR(("socks: unable to connect to SOCKS proxy, %s",
                     mProxyHost.get()));
            return PR_FAILURE;
        }

#if defined(PR_LOGGING)
        char buf[64];
        PR_NetAddrToString(&mInternalProxyAddr, buf, sizeof(buf));
        LOGDEBUG(("socks: trying proxy server, %s:%hu",
                 buf, PR_ntohs(PR_NetAddrInetPort(&mInternalProxyAddr))));
#endif
        PRNetAddr proxy = mInternalProxyAddr;
        FixupAddressFamily(fd, &proxy);
        status = fd->lower->methods->connect(fd->lower, &proxy, mTimeout);
        if (status != PR_SUCCESS) {
            PRErrorCode c = PR_GetError();
            // If EINPROGRESS, return now and check back later after polling
            if (c == PR_WOULD_BLOCK_ERROR || c == PR_IN_PROGRESS_ERROR) {
                mState = SOCKS_CONNECTING_TO_PROXY;
                return status;
            }
        }
    } while (status != PR_SUCCESS);

    // Connected now, start SOCKS
    if (mVersion == 4)
        return WriteV4ConnectRequest();
    return WriteV5AuthRequest();
}

void
nsSOCKSSocketInfo::FixupAddressFamily(PRFileDesc *fd, PRNetAddr *proxy)
{
    int32_t proxyFamily = PR_NetAddrFamily(&mInternalProxyAddr);
    // Do nothing if the address family is already matched
    if (proxyFamily == mDestinationFamily) {
        return;
    }
    // If the system does not support IPv6 and the proxy address is IPv6,
    // We can do nothing here.
    if (proxyFamily == PR_AF_INET6 && !ipv6Supported) {
        return;
    }
    // If the system does not support IPv6 and the destination address is
    // IPv6, convert IPv4 address to IPv4-mapped IPv6 address to satisfy
    // the emulation layer
    if (mDestinationFamily == PR_AF_INET6 && !ipv6Supported) {
        proxy->ipv6.family = PR_AF_INET6;
        proxy->ipv6.port = mInternalProxyAddr.inet.port;
        uint8_t *proxyp = proxy->ipv6.ip.pr_s6_addr;
        memset(proxyp, 0, 10);
        memset(proxyp + 10, 0xff, 2);
        memcpy(proxyp + 12,(char *) &mInternalProxyAddr.inet.ip, 4);
        // mDestinationFamily should not be updated
        return;
    }
    // Get an OS native handle from a specified FileDesc
    PROsfd osfd = PR_FileDesc2NativeHandle(fd);
    if (osfd == -1) {
        return;
    }
    // Create a new FileDesc with a specified family
    PRFileDesc *tmpfd = PR_OpenTCPSocket(proxyFamily);
    if (!tmpfd) {
        return;
    }
    PROsfd newsd = PR_FileDesc2NativeHandle(tmpfd);
    if (newsd == -1) {
        PR_Close(tmpfd);
        return;
    }
    // Must succeed because PR_FileDesc2NativeHandle succeeded
    fd = PR_GetIdentitiesLayer(fd, PR_NSPR_IO_LAYER);
    MOZ_ASSERT(fd);
    // Swap OS native handles
    PR_ChangeFileDescNativeHandle(fd, newsd);
    PR_ChangeFileDescNativeHandle(tmpfd, osfd);
    // Close temporary FileDesc which is now associated with
    // old OS native handle
    PR_Close(tmpfd);
    mDestinationFamily = proxyFamily;
}

PRStatus
nsSOCKSSocketInfo::ContinueConnectingToProxy(PRFileDesc *fd, int16_t oflags)
{
    PRStatus status;

    NS_ABORT_IF_FALSE(mState == SOCKS_CONNECTING_TO_PROXY,
                      "Continuing connection in wrong state!");

    LOGDEBUG(("socks: continuing connection to proxy"));

    status = fd->lower->methods->connectcontinue(fd->lower, oflags);
    if (status != PR_SUCCESS) {
        PRErrorCode c = PR_GetError();
        if (c != PR_WOULD_BLOCK_ERROR && c != PR_IN_PROGRESS_ERROR) {
            // A connection failure occured, try another address
            mState = SOCKS_DNS_COMPLETE;
            return ConnectToProxy(fd);
        }

        // We're still connecting
        return PR_FAILURE;
    }

    // Connected now, start SOCKS
    if (mVersion == 4)
        return WriteV4ConnectRequest();
    return WriteV5AuthRequest();
}

PRStatus
nsSOCKSSocketInfo::WriteV4ConnectRequest()
{
    PRNetAddr *addr = &mDestinationAddr;
    int32_t proxy_resolve;

    NS_ABORT_IF_FALSE(mState == SOCKS_CONNECTING_TO_PROXY,
                      "Invalid state!");
    
    proxy_resolve = mFlags & nsISocketProvider::PROXY_RESOLVES_HOST;

    mDataLength = 0;
    mState = SOCKS4_WRITE_CONNECT_REQUEST;

    LOGDEBUG(("socks4: sending connection request (socks4a resolve? %s)",
             proxy_resolve? "yes" : "no"));

    // Send a SOCKS 4 connect request.
    WriteUint8(0x04); // version -- 4
    WriteUint8(0x01); // command -- connect
    WriteNetPort(addr);
    if (proxy_resolve) {
        // Add the full name, null-terminated, to the request
        // according to SOCKS 4a. A fake IP address, with the first
        // four bytes set to 0 and the last byte set to something other
        // than 0, is used to notify the proxy that this is a SOCKS 4a
        // request. This request type works for Tor and perhaps others.
        WriteUint32(PR_htonl(0x00000001)); // Fake IP
        WriteUint8(0x00); // Send an emtpy username
        if (mDestinationHost.Length() > MAX_HOSTNAME_LEN) {
            LOGERROR(("socks4: destination host name is too long!"));
            HandshakeFinished(PR_BAD_ADDRESS_ERROR);
            return PR_FAILURE;
        }
        WriteString(mDestinationHost); // Hostname
        WriteUint8(0x00);
    } else if (PR_NetAddrFamily(addr) == PR_AF_INET) {
        WriteNetAddr(addr); // Add the IPv4 address
        WriteUint8(0x00); // Send an emtpy username
    } else if (PR_NetAddrFamily(addr) == PR_AF_INET6) {
        LOGERROR(("socks: SOCKS 4 can't handle IPv6 addresses!"));
        HandshakeFinished(PR_BAD_ADDRESS_ERROR);
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}

PRStatus
nsSOCKSSocketInfo::ReadV4ConnectResponse()
{
    NS_ABORT_IF_FALSE(mState == SOCKS4_READ_CONNECT_RESPONSE,
                      "Handling SOCKS 4 connection reply in wrong state!");
    NS_ABORT_IF_FALSE(mDataLength == 8,
                      "SOCKS 4 connection reply must be 8 bytes!");

    LOGDEBUG(("socks4: checking connection reply"));

    if (ReadUint8() != 0x00) {
        LOGERROR(("socks4: wrong connection reply"));
        HandshakeFinished(PR_CONNECT_REFUSED_ERROR);
        return PR_FAILURE;
    }

    // See if our connection request was granted
    if (ReadUint8() == 90) {
        LOGDEBUG(("socks4: connection successful!"));
        HandshakeFinished();
        return PR_SUCCESS;
    }

    LOGERROR(("socks4: unable to connect"));
    HandshakeFinished(PR_CONNECT_REFUSED_ERROR);
    return PR_FAILURE;
}

PRStatus
nsSOCKSSocketInfo::WriteV5AuthRequest()
{
    NS_ABORT_IF_FALSE(mVersion == 5, "SOCKS version must be 5!");

    mState = SOCKS5_WRITE_AUTH_REQUEST;

    // Send an initial SOCKS 5 greeting
    LOGDEBUG(("socks5: sending auth methods"));
    WriteUint8(0x05); // version -- 5
    WriteUint8(0x01); // # auth methods -- 1
    WriteUint8(0x00); // we don't support authentication

    return PR_SUCCESS;
}

PRStatus
nsSOCKSSocketInfo::ReadV5AuthResponse()
{
    NS_ABORT_IF_FALSE(mState == SOCKS5_READ_AUTH_RESPONSE,
                      "Handling SOCKS 5 auth method reply in wrong state!");
    NS_ABORT_IF_FALSE(mDataLength == 2,
                      "SOCKS 5 auth method reply must be 2 bytes!");

    LOGDEBUG(("socks5: checking auth method reply"));

    // Check version number
    if (ReadUint8() != 0x05) {
        LOGERROR(("socks5: unexpected version in the reply"));
        HandshakeFinished(PR_CONNECT_REFUSED_ERROR);
        return PR_FAILURE;
    }

    // Make sure our authentication choice was accepted
    if (ReadUint8() != 0x00) {
        LOGERROR(("socks5: server did not accept our authentication method"));
        HandshakeFinished(PR_CONNECT_REFUSED_ERROR);
        return PR_FAILURE;
    }

    return WriteV5ConnectRequest();
}

PRStatus
nsSOCKSSocketInfo::WriteV5ConnectRequest()
{
    // Send SOCKS 5 connect request
    PRNetAddr *addr = &mDestinationAddr;
    int32_t proxy_resolve;
    proxy_resolve = mFlags & nsISocketProvider::PROXY_RESOLVES_HOST;

    LOGDEBUG(("socks5: sending connection request (socks5 resolve? %s)",
             proxy_resolve? "yes" : "no"));

    mDataLength = 0;
    mState = SOCKS5_WRITE_CONNECT_REQUEST;

    WriteUint8(0x05); // version -- 5
    WriteUint8(0x01); // command -- connect
    WriteUint8(0x00); // reserved
   
    // Add the address to the SOCKS 5 request. SOCKS 5 supports several
    // address types, so we pick the one that works best for us.
    if (proxy_resolve) {
        // Add the host name. Only a single byte is used to store the length,
        // so we must prevent long names from being used.
        if (mDestinationHost.Length() > MAX_HOSTNAME_LEN) {
            LOGERROR(("socks5: destination host name is too long!"));
            HandshakeFinished(PR_BAD_ADDRESS_ERROR);
            return PR_FAILURE;
        }
        WriteUint8(0x03); // addr type -- domainname
        WriteUint8(mDestinationHost.Length()); // name length
        WriteString(mDestinationHost);
    } else if (PR_NetAddrFamily(addr) == PR_AF_INET) {
        WriteUint8(0x01); // addr type -- IPv4
        WriteNetAddr(addr);
    } else if (PR_NetAddrFamily(addr) == PR_AF_INET6) {
        WriteUint8(0x04); // addr type -- IPv6
        WriteNetAddr(addr);
    } else {
        LOGERROR(("socks5: destination address of unknown type!"));
        HandshakeFinished(PR_BAD_ADDRESS_ERROR);
        return PR_FAILURE;
    }

    WriteNetPort(addr); // port

    return PR_SUCCESS;
}

PRStatus
nsSOCKSSocketInfo::ReadV5AddrTypeAndLength(uint8_t *type, uint32_t *len)
{
    NS_ABORT_IF_FALSE(mState == SOCKS5_READ_CONNECT_RESPONSE_TOP ||
                      mState == SOCKS5_READ_CONNECT_RESPONSE_BOTTOM,
                      "Invalid state!");
    NS_ABORT_IF_FALSE(mDataLength >= 5,
                      "SOCKS 5 connection reply must be at least 5 bytes!");
 
    // Seek to the address location 
    mReadOffset = 3;
   
    *type = ReadUint8();

    switch (*type) {
        case 0x01: // ipv4
            *len = 4 - 1;
            break;
        case 0x04: // ipv6
            *len = 16 - 1;
            break;
        case 0x03: // fqdn
            *len = ReadUint8();
            break;
        default:   // wrong address type
            LOGERROR(("socks5: wrong address type in connection reply!"));
            return PR_FAILURE;
    }

    return PR_SUCCESS;
}

PRStatus
nsSOCKSSocketInfo::ReadV5ConnectResponseTop()
{
    uint8_t res;
    uint32_t len;

    NS_ABORT_IF_FALSE(mState == SOCKS5_READ_CONNECT_RESPONSE_TOP,
                      "Invalid state!");
    NS_ABORT_IF_FALSE(mDataLength == 5,
                      "SOCKS 5 connection reply must be exactly 5 bytes!");

    LOGDEBUG(("socks5: checking connection reply"));

    // Check version number
    if (ReadUint8() != 0x05) {
        LOGERROR(("socks5: unexpected version in the reply"));
        HandshakeFinished(PR_CONNECT_REFUSED_ERROR);
        return PR_FAILURE;
    }

    // Check response
    res = ReadUint8();
    if (res != 0x00) {
        PRErrorCode c = PR_CONNECT_REFUSED_ERROR;

        switch (res) {
            case 0x01:
                LOGERROR(("socks5: connect failed: "
                          "01, General SOCKS server failure."));
                break;
            case 0x02:
                LOGERROR(("socks5: connect failed: "
                          "02, Connection not allowed by ruleset."));
                break;
            case 0x03:
                LOGERROR(("socks5: connect failed: 03, Network unreachable."));
                c = PR_NETWORK_UNREACHABLE_ERROR;
                break;
            case 0x04:
                LOGERROR(("socks5: connect failed: 04, Host unreachable."));
                break;
            case 0x05:
                LOGERROR(("socks5: connect failed: 05, Connection refused."));
                break;
            case 0x06:  
                LOGERROR(("socks5: connect failed: 06, TTL expired."));
                c = PR_CONNECT_TIMEOUT_ERROR;
                break;
            case 0x07:
                LOGERROR(("socks5: connect failed: "
                          "07, Command not supported."));
                break;
            case 0x08:
                LOGERROR(("socks5: connect failed: "
                          "08, Address type not supported."));
                c = PR_BAD_ADDRESS_ERROR;
                break;
            default:
                LOGERROR(("socks5: connect failed."));
                break;
        }

        HandshakeFinished(c);
        return PR_FAILURE;
    }

    if (ReadV5AddrTypeAndLength(&res, &len) != PR_SUCCESS) {
        HandshakeFinished(PR_BAD_ADDRESS_ERROR);
        return PR_FAILURE;
    }

    mState = SOCKS5_READ_CONNECT_RESPONSE_BOTTOM;
    WantRead(len + 2);

    return PR_SUCCESS;
}

PRStatus
nsSOCKSSocketInfo::ReadV5ConnectResponseBottom()
{
    uint8_t type;
    uint32_t len;

    NS_ABORT_IF_FALSE(mState == SOCKS5_READ_CONNECT_RESPONSE_BOTTOM,
                      "Invalid state!");

    if (ReadV5AddrTypeAndLength(&type, &len) != PR_SUCCESS) {
        HandshakeFinished(PR_BAD_ADDRESS_ERROR);
        return PR_FAILURE;
    }

    NS_ABORT_IF_FALSE(mDataLength == 7+len,
                      "SOCKS 5 unexpected length of connection reply!");

    LOGDEBUG(("socks5: loading source addr and port"));
    // Read what the proxy says is our source address
    switch (type) {
        case 0x01: // ipv4
            ReadNetAddr(&mExternalProxyAddr, PR_AF_INET);
            break;
        case 0x04: // ipv6
            ReadNetAddr(&mExternalProxyAddr, PR_AF_INET6);
            break;
        case 0x03: // fqdn (skip)
            mReadOffset += len;
            mExternalProxyAddr.raw.family = PR_AF_INET;
            break;
    }

    ReadNetPort(&mExternalProxyAddr);

    LOGDEBUG(("socks5: connected!"));
    HandshakeFinished();

    return PR_SUCCESS;
}

void
nsSOCKSSocketInfo::SetConnectTimeout(PRIntervalTime to)
{
    mTimeout = to;
}

PRStatus
nsSOCKSSocketInfo::DoHandshake(PRFileDesc *fd, int16_t oflags)
{
    LOGDEBUG(("socks: DoHandshake(), state = %d", mState));

    switch (mState) {
        case SOCKS_INITIAL:
            return StartDNS(fd);
        case SOCKS_DNS_IN_PROGRESS:
            PR_SetError(PR_IN_PROGRESS_ERROR, 0);
            return PR_FAILURE;
        case SOCKS_DNS_COMPLETE:
            return ConnectToProxy(fd);
        case SOCKS_CONNECTING_TO_PROXY:
            return ContinueConnectingToProxy(fd, oflags);
        case SOCKS4_WRITE_CONNECT_REQUEST:
            if (WriteToSocket(fd) != PR_SUCCESS)
                return PR_FAILURE;
            WantRead(8);
            mState = SOCKS4_READ_CONNECT_RESPONSE;
            return PR_SUCCESS;
        case SOCKS4_READ_CONNECT_RESPONSE:
            if (ReadFromSocket(fd) != PR_SUCCESS)
                return PR_FAILURE;
            return ReadV4ConnectResponse();

        case SOCKS5_WRITE_AUTH_REQUEST:
            if (WriteToSocket(fd) != PR_SUCCESS)
                return PR_FAILURE;
            WantRead(2);
            mState = SOCKS5_READ_AUTH_RESPONSE;
            return PR_SUCCESS;
        case SOCKS5_READ_AUTH_RESPONSE:
            if (ReadFromSocket(fd) != PR_SUCCESS)
                return PR_FAILURE;
            return ReadV5AuthResponse();
        case SOCKS5_WRITE_CONNECT_REQUEST:
            if (WriteToSocket(fd) != PR_SUCCESS)
                return PR_FAILURE;

            // The SOCKS 5 response to the connection request is variable
            // length. First, we'll read enough to tell how long the response
            // is, and will read the rest later.
            WantRead(5);
            mState = SOCKS5_READ_CONNECT_RESPONSE_TOP;
            return PR_SUCCESS;
        case SOCKS5_READ_CONNECT_RESPONSE_TOP:
            if (ReadFromSocket(fd) != PR_SUCCESS)
                return PR_FAILURE;
            return ReadV5ConnectResponseTop();
        case SOCKS5_READ_CONNECT_RESPONSE_BOTTOM:
            if (ReadFromSocket(fd) != PR_SUCCESS)
                return PR_FAILURE;
            return ReadV5ConnectResponseBottom();

        case SOCKS_CONNECTED:
            LOGERROR(("socks: already connected"));
            HandshakeFinished(PR_IS_CONNECTED_ERROR);
            return PR_FAILURE;
        case SOCKS_FAILED:
            LOGERROR(("socks: already failed"));
            return PR_FAILURE;
    }

    LOGERROR(("socks: executing handshake in invalid state, %d", mState));
    HandshakeFinished(PR_INVALID_STATE_ERROR);

    return PR_FAILURE;
}

int16_t
nsSOCKSSocketInfo::GetPollFlags() const
{
    switch (mState) {
        case SOCKS_DNS_IN_PROGRESS:
        case SOCKS_DNS_COMPLETE:
        case SOCKS_CONNECTING_TO_PROXY:
            return PR_POLL_EXCEPT | PR_POLL_WRITE;
        case SOCKS4_WRITE_CONNECT_REQUEST:
        case SOCKS5_WRITE_AUTH_REQUEST:
        case SOCKS5_WRITE_CONNECT_REQUEST:
            return PR_POLL_WRITE;
        case SOCKS4_READ_CONNECT_RESPONSE:
        case SOCKS5_READ_AUTH_RESPONSE:
        case SOCKS5_READ_CONNECT_RESPONSE_TOP:
        case SOCKS5_READ_CONNECT_RESPONSE_BOTTOM:
            return PR_POLL_READ;
        default:
            break;
    }

    return 0;
}

inline void
nsSOCKSSocketInfo::WriteUint8(uint8_t v)
{
    NS_ABORT_IF_FALSE(mDataLength + sizeof(v) <= BUFFER_SIZE,
                      "Can't write that much data!");
    mData[mDataLength] = v;
    mDataLength += sizeof(v);
}

inline void
nsSOCKSSocketInfo::WriteUint16(uint16_t v)
{
    NS_ABORT_IF_FALSE(mDataLength + sizeof(v) <= BUFFER_SIZE,
                      "Can't write that much data!");
    memcpy(mData + mDataLength, &v, sizeof(v));
    mDataLength += sizeof(v);
}

inline void
nsSOCKSSocketInfo::WriteUint32(uint32_t v)
{
    NS_ABORT_IF_FALSE(mDataLength + sizeof(v) <= BUFFER_SIZE,
                      "Can't write that much data!");
    memcpy(mData + mDataLength, &v, sizeof(v));
    mDataLength += sizeof(v);
}

void
nsSOCKSSocketInfo::WriteNetAddr(const PRNetAddr *addr)
{
    const char *ip = NULL;
    uint32_t len = 0;

    if (PR_NetAddrFamily(addr) == PR_AF_INET) {
        ip = (const char*)&addr->inet.ip;
        len = sizeof(addr->inet.ip);
    } else if (PR_NetAddrFamily(addr) == PR_AF_INET6) {
        ip = (const char*)addr->ipv6.ip.pr_s6_addr;
        len = sizeof(addr->ipv6.ip.pr_s6_addr);
    }

    NS_ABORT_IF_FALSE(ip != NULL, "Unknown address");
    NS_ABORT_IF_FALSE(mDataLength + len <= BUFFER_SIZE,
                      "Can't write that much data!");
 
    memcpy(mData + mDataLength, ip, len);
    mDataLength += len;
}

void
nsSOCKSSocketInfo::WriteNetPort(const PRNetAddr *addr)
{
    WriteUint16(PR_NetAddrInetPort(addr));
}

void
nsSOCKSSocketInfo::WriteString(const nsACString &str)
{
    NS_ABORT_IF_FALSE(mDataLength + str.Length() <= BUFFER_SIZE,
                      "Can't write that much data!");
    memcpy(mData + mDataLength, str.Data(), str.Length());
    mDataLength += str.Length();
}

inline uint8_t
nsSOCKSSocketInfo::ReadUint8()
{
    uint8_t rv;
    NS_ABORT_IF_FALSE(mReadOffset + sizeof(rv) <= mDataLength,
                      "Not enough space to pop a uint8_t!");
    rv = mData[mReadOffset];
    mReadOffset += sizeof(rv);
    return rv;
}

inline uint16_t
nsSOCKSSocketInfo::ReadUint16()
{
    uint16_t rv;
    NS_ABORT_IF_FALSE(mReadOffset + sizeof(rv) <= mDataLength,
                      "Not enough space to pop a uint16_t!");
    memcpy(&rv, mData + mReadOffset, sizeof(rv));
    mReadOffset += sizeof(rv);
    return rv;
}

inline uint32_t
nsSOCKSSocketInfo::ReadUint32()
{
    uint32_t rv;
    NS_ABORT_IF_FALSE(mReadOffset + sizeof(rv) <= mDataLength,
                      "Not enough space to pop a uint32_t!");
    memcpy(&rv, mData + mReadOffset, sizeof(rv));
    mReadOffset += sizeof(rv);
    return rv;
}

void
nsSOCKSSocketInfo::ReadNetAddr(PRNetAddr *addr, uint16_t fam)
{
    uint32_t amt = 0;
    const uint8_t *ip = mData + mReadOffset;

    addr->raw.family = fam;
    if (fam == PR_AF_INET) {
        amt = sizeof(addr->inet.ip);
        NS_ABORT_IF_FALSE(mReadOffset + amt <= mDataLength,
                          "Not enough space to pop an ipv4 addr!");
        memcpy(&addr->inet.ip, ip, amt);
    } else if (fam == PR_AF_INET6) {
        amt = sizeof(addr->ipv6.ip.pr_s6_addr);
        NS_ABORT_IF_FALSE(mReadOffset + amt <= mDataLength,
                          "Not enough space to pop an ipv6 addr!");
        memcpy(addr->ipv6.ip.pr_s6_addr, ip, amt);
    }

    mReadOffset += amt;
}

void
nsSOCKSSocketInfo::ReadNetPort(PRNetAddr *addr)
{
    addr->inet.port = ReadUint16();
}

void
nsSOCKSSocketInfo::WantRead(uint32_t sz)
{
    NS_ABORT_IF_FALSE(mDataIoPtr == NULL,
                      "WantRead() called while I/O already in progress!");
    NS_ABORT_IF_FALSE(mDataLength + sz <= BUFFER_SIZE,
                      "Can't read that much data!");
    mAmountToRead = sz;
}

PRStatus
nsSOCKSSocketInfo::ReadFromSocket(PRFileDesc *fd)
{
    int32_t rc;
    const uint8_t *end;

    if (!mAmountToRead) {
        LOGDEBUG(("socks: ReadFromSocket(), nothing to do"));
        return PR_SUCCESS;
    }

    if (!mDataIoPtr) {
        mDataIoPtr = mData + mDataLength;
        mDataLength += mAmountToRead;
    }

    end = mData + mDataLength;

    while (mDataIoPtr < end) {
        rc = PR_Read(fd, mDataIoPtr, end - mDataIoPtr);
        if (rc <= 0) {
            if (rc == 0) {
                LOGERROR(("socks: proxy server closed connection"));
                HandshakeFinished(PR_CONNECT_REFUSED_ERROR);
                return PR_FAILURE;
            } else if (PR_GetError() == PR_WOULD_BLOCK_ERROR) {
                LOGDEBUG(("socks: ReadFromSocket(), want read"));
            }
            break;
        }

        mDataIoPtr += rc;
    }

    LOGDEBUG(("socks: ReadFromSocket(), have %u bytes total",
             unsigned(mDataIoPtr - mData)));
    if (mDataIoPtr == end) {
        mDataIoPtr = nullptr;
        mAmountToRead = 0;
        mReadOffset = 0;
        return PR_SUCCESS;
    }

    return PR_FAILURE;
}

PRStatus
nsSOCKSSocketInfo::WriteToSocket(PRFileDesc *fd)
{
    int32_t rc;
    const uint8_t *end;

    if (!mDataLength) {
        LOGDEBUG(("socks: WriteToSocket(), nothing to do"));
        return PR_SUCCESS;
    }

    if (!mDataIoPtr)
        mDataIoPtr = mData;

    end = mData + mDataLength;

    while (mDataIoPtr < end) {
        rc = PR_Write(fd, mDataIoPtr, end - mDataIoPtr);
        if (rc < 0) {
            if (PR_GetError() == PR_WOULD_BLOCK_ERROR) {
                LOGDEBUG(("socks: WriteToSocket(), want write"));
            }
            break;
        }
        
        mDataIoPtr += rc;
    }

    if (mDataIoPtr == end) {
        mDataIoPtr = nullptr;
        mDataLength = 0;
        mReadOffset = 0;
        return PR_SUCCESS;
    }
    
    return PR_FAILURE;
}

static PRStatus
nsSOCKSIOLayerConnect(PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime to)
{
    PRStatus status;
    PRNetAddr dst;

    nsSOCKSSocketInfo * info = (nsSOCKSSocketInfo*) fd->secret;
    if (info == NULL) return PR_FAILURE;

    if (PR_NetAddrFamily(addr) == PR_AF_INET6 &&
        PR_IsNetAddrType(addr, PR_IpAddrV4Mapped)) {
        const uint8_t *srcp;

        LOGDEBUG(("socks: converting ipv4-mapped ipv6 address to ipv4"));

        // copied from _PR_ConvertToIpv4NetAddr()
        PR_InitializeNetAddr(PR_IpAddrAny, 0, &dst);
        srcp = addr->ipv6.ip.pr_s6_addr;
        memcpy(&dst.inet.ip, srcp + 12, 4);
        dst.inet.family = PR_AF_INET;
        dst.inet.port = addr->ipv6.port;
    } else {
        memcpy(&dst, addr, sizeof(dst));
    }

    info->SetDestinationAddr(&dst);
    info->SetConnectTimeout(to);

    do {
        status = info->DoHandshake(fd, -1);
    } while (status == PR_SUCCESS && !info->IsConnected());

    return status;
}

static PRStatus
nsSOCKSIOLayerConnectContinue(PRFileDesc *fd, int16_t oflags)
{
    PRStatus status;

    nsSOCKSSocketInfo * info = (nsSOCKSSocketInfo*) fd->secret;
    if (info == NULL) return PR_FAILURE;

    do { 
        status = info->DoHandshake(fd, oflags);
    } while (status == PR_SUCCESS && !info->IsConnected());

    return status;
}

static int16_t
nsSOCKSIOLayerPoll(PRFileDesc *fd, int16_t in_flags, int16_t *out_flags)
{
    nsSOCKSSocketInfo * info = (nsSOCKSSocketInfo*) fd->secret;
    if (info == NULL) return PR_FAILURE;

    if (!info->IsConnected()) {
        *out_flags = 0;
        return info->GetPollFlags();
    }

    return fd->lower->methods->poll(fd->lower, in_flags, out_flags);
}

static PRStatus
nsSOCKSIOLayerClose(PRFileDesc *fd)
{
    nsSOCKSSocketInfo * info = (nsSOCKSSocketInfo*) fd->secret;
    PRDescIdentity id = PR_GetLayersIdentity(fd);

    if (info && id == nsSOCKSIOLayerIdentity)
    {
        info->ForgetFD();
        NS_RELEASE(info);
        fd->identity = PR_INVALID_IO_LAYER;
    }

    return fd->lower->methods->close(fd->lower);
}

static PRFileDesc*
nsSOCKSIOLayerAccept(PRFileDesc *fd, PRNetAddr *addr, PRIntervalTime timeout)
{
    // TODO: implement SOCKS support for accept
    return fd->lower->methods->accept(fd->lower, addr, timeout);
}

static int32_t
nsSOCKSIOLayerAcceptRead(PRFileDesc *sd, PRFileDesc **nd, PRNetAddr **raddr, void *buf, int32_t amount, PRIntervalTime timeout)
{
    // TODO: implement SOCKS support for accept, then read from it
    return sd->lower->methods->acceptread(sd->lower, nd, raddr, buf, amount, timeout);
}

static PRStatus
nsSOCKSIOLayerBind(PRFileDesc *fd, const PRNetAddr *addr)
{
    // TODO: implement SOCKS support for bind (very similar to connect)
    return fd->lower->methods->bind(fd->lower, addr);
}

static PRStatus
nsSOCKSIOLayerGetName(PRFileDesc *fd, PRNetAddr *addr)
{
    nsSOCKSSocketInfo * info = (nsSOCKSSocketInfo*) fd->secret;
    
    if (info != NULL && addr != NULL) {
        if (info->GetExternalProxyAddr(&addr) == NS_OK)
            return PR_SUCCESS;
    }

    return PR_FAILURE;
}

static PRStatus
nsSOCKSIOLayerGetPeerName(PRFileDesc *fd, PRNetAddr *addr)
{
    nsSOCKSSocketInfo * info = (nsSOCKSSocketInfo*) fd->secret;

    if (info != NULL && addr != NULL) {
        if (info->GetDestinationAddr(&addr) == NS_OK)
            return PR_SUCCESS;
    }

    return PR_FAILURE;
}

static PRStatus
nsSOCKSIOLayerListen(PRFileDesc *fd, int backlog)
{
    // TODO: implement SOCKS support for listen
    return fd->lower->methods->listen(fd->lower, backlog);
}

// add SOCKS IO layer to an existing socket
nsresult
nsSOCKSIOLayerAddToSocket(int32_t family,
                          const char *host, 
                          int32_t port,
                          const char *proxyHost,
                          int32_t proxyPort,
                          int32_t socksVersion,
                          uint32_t flags,
                          PRFileDesc *fd, 
                          nsISupports** info)
{
    NS_ENSURE_TRUE((socksVersion == 4) || (socksVersion == 5), NS_ERROR_NOT_INITIALIZED);


    if (firstTime)
    {
        //XXX hack until NSPR provides an official way to detect system IPv6
        // support (bug 388519)
        PRFileDesc *tmpfd = PR_OpenTCPSocket(PR_AF_INET6);
        if (!tmpfd) {
            ipv6Supported = false;
        } else {
            // If the system does not support IPv6, NSPR will push
            // IPv6-to-IPv4 emulation layer onto the native layer
            ipv6Supported = PR_GetIdentitiesLayer(tmpfd, PR_NSPR_IO_LAYER) == tmpfd;
            PR_Close(tmpfd);
        }

        nsSOCKSIOLayerIdentity		= PR_GetUniqueIdentity("SOCKS layer");
        nsSOCKSIOLayerMethods		= *PR_GetDefaultIOMethods();

        nsSOCKSIOLayerMethods.connect	= nsSOCKSIOLayerConnect;
        nsSOCKSIOLayerMethods.connectcontinue	= nsSOCKSIOLayerConnectContinue;
        nsSOCKSIOLayerMethods.poll	= nsSOCKSIOLayerPoll;
        nsSOCKSIOLayerMethods.bind	= nsSOCKSIOLayerBind;
        nsSOCKSIOLayerMethods.acceptread = nsSOCKSIOLayerAcceptRead;
        nsSOCKSIOLayerMethods.getsockname = nsSOCKSIOLayerGetName;
        nsSOCKSIOLayerMethods.getpeername = nsSOCKSIOLayerGetPeerName;
        nsSOCKSIOLayerMethods.accept	= nsSOCKSIOLayerAccept;
        nsSOCKSIOLayerMethods.listen	= nsSOCKSIOLayerListen;
        nsSOCKSIOLayerMethods.close	= nsSOCKSIOLayerClose;

        firstTime			= false;

#if defined(PR_LOGGING)
        gSOCKSLog = PR_NewLogModule("SOCKS");
#endif

    }

    LOGDEBUG(("Entering nsSOCKSIOLayerAddToSocket()."));

    PRFileDesc *	layer;
    PRStatus	rv;

    layer = PR_CreateIOLayerStub(nsSOCKSIOLayerIdentity, &nsSOCKSIOLayerMethods);
    if (! layer)
    {
        LOGERROR(("PR_CreateIOLayerStub() failed."));
        return NS_ERROR_FAILURE;
    }

    nsSOCKSSocketInfo * infoObject = new nsSOCKSSocketInfo();
    if (!infoObject)
    {
        // clean up IOLayerStub
        LOGERROR(("Failed to create nsSOCKSSocketInfo()."));
        PR_DELETE(layer);
        return NS_ERROR_FAILURE;
    }

    NS_ADDREF(infoObject);
    infoObject->Init(socksVersion, family, proxyHost, proxyPort, host, flags);
    layer->secret = (PRFilePrivate*) infoObject;
    rv = PR_PushIOLayer(fd, PR_GetLayersIdentity(fd), layer);

    if (rv == PR_FAILURE) {
        LOGERROR(("PR_PushIOLayer() failed. rv = %x.", rv));
        NS_RELEASE(infoObject);
        PR_DELETE(layer);
        return NS_ERROR_FAILURE;
    }

    *info = static_cast<nsISOCKSSocketInfo*>(infoObject);
    NS_ADDREF(*info);
    return NS_OK;
}
