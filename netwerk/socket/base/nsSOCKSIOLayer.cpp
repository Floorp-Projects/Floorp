/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Bradford <jab@atdot.org>
 *   Bradley Baetz <bbaetz@acm.org>
 *   Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nspr.h"
#include "nsString.h"
#include "nsCRT.h"

#include "nsIServiceManager.h"
#include "nsIDNSService.h"
#include "nsISOCKSSocketInfo.h"
#include "nsSOCKSIOLayer.h"
#include "nsNetCID.h"

static PRDescIdentity	nsSOCKSIOLayerIdentity;
static PRIOMethods	nsSOCKSIOLayerMethods;
static PRBool firstTime = PR_TRUE;

#if defined(PR_LOGGING)
static PRLogModuleInfo *gSOCKSLog;
#define LOGDEBUG(args) PR_LOG(gSOCKSLog, PR_LOG_DEBUG, args)
#define LOGERROR(args) PR_LOG(gSOCKSLog, PR_LOG_ERROR , args)

#else
#define LOGDEBUG(args)
#define LOGERROR(args)
#endif

class nsSOCKSSocketInfo : public nsISOCKSSocketInfo
{
public:
    nsSOCKSSocketInfo();
    virtual ~nsSOCKSSocketInfo() {}
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSISOCKSSOCKETINFO

    void Init(PRInt32 version, const char *proxyHost, PRInt32 proxyPort);
    
    const nsCString &ProxyHost() { return mProxyHost; }
    PRInt32          ProxyPort() { return mProxyPort; }
    PRInt32          Version()   { return mVersion; }

private:
    nsCString mProxyHost;
    PRInt32	  mProxyPort;
    PRInt32   mVersion;   // SOCKS version 4 or 5
    PRNetAddr mInternalProxyAddr;
    PRNetAddr mExternalProxyAddr;
    PRNetAddr mDestinationAddr;
};

nsSOCKSSocketInfo::nsSOCKSSocketInfo()
    : mProxyPort(-1)
    , mVersion(-1)
{
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &mInternalProxyAddr);
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &mExternalProxyAddr);
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &mDestinationAddr);
}

void
nsSOCKSSocketInfo::Init(PRInt32 version, const char *proxyHost, PRInt32 proxyPort)
{
    mVersion   = version;
    mProxyHost = proxyHost;
    mProxyPort = proxyPort;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsSOCKSSocketInfo, nsISOCKSSocketInfo)

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


// Negotiate a SOCKS 5 connection. Assumes the TCP connection to the socks 
// server port has been established.
static nsresult
ConnectSOCKS5(PRFileDesc *fd, const PRNetAddr *addr, PRNetAddr *extAddr, PRIntervalTime timeout)
{
    NS_ENSURE_TRUE(fd, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_TRUE(addr, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_TRUE(extAddr, NS_ERROR_NOT_INITIALIZED);

    unsigned char request[22];
    int request_len = 0;
    unsigned char response[22];
    int response_len = 0;

    request[0] = 0x05; // SOCKS version 5
    request[1] = 0x01; // number of auth procotols we recognize
    // auth protocols
    request[2] = 0x00; // no authentication required
    // compliant implementations MUST implement GSSAPI
    // and SHOULD implement username/password and MAY
    // implement CHAP
    // TODO: we don't implement these
    //request[3] = 0x01; // GSSAPI
    //request[4] = 0x02; // username/password
    //request[5] = 0x03; // CHAP

    request_len = 2 + request[1];
    int write_len = PR_Send(fd, request, request_len, 0, timeout);
    if (write_len != request_len) {

        LOGERROR(("PR_Send() failed. Wrote: %d bytes; Expected: %d.", write_len, request_len));
        return NS_ERROR_FAILURE;
    }
    
    // get the server's response. Use PR_Recv() instead of 
    response_len = 2;
    response_len = PR_Recv(fd, response, response_len, 0, timeout);
    
    if (response_len <= 0) {

        LOGERROR(("PR_Recv() failed. response_len = %d.", response_len));
        return NS_ERROR_FAILURE;
    }
    
    if (response[0] != 0x05) {
        // it's a either not SOCKS or not our version
        LOGERROR(("Not a SOCKS 5 reply. Expected: 5; received: %x", response[0]));
        return NS_ERROR_FAILURE;
    }
    switch (response[1]) {
        case 0x00:
            // no auth
            break;
        case 0x01:
            // GSSAPI
            // TODO: implement
            LOGERROR(("Server want to use GSSAPI to authenticate, but we don't support it."));
            return NS_ERROR_FAILURE;
        case 0x02:
            // username/password
            // TODO: implement
            LOGERROR(("Server want to use username/password to authenticate, but we don't support it."));
            return NS_ERROR_FAILURE;
        case 0x03:
            // CHAP
            // TODO: implement?
            LOGERROR(("Server want to use CHAP to authenticate, but we don't support it."));
            return NS_ERROR_FAILURE;
        default:
            // unrecognized auth method
            LOGERROR(("Uncrecognized authentication method received: %x", response[1]));
            return NS_ERROR_FAILURE;
    }
    
    // we are now authenticated, so lets tell
    // the server where to connect to
    
    request_len = 6;
    
    request[0] = 0x05; // SOCKS version 5
    request[1] = 0x01; // CONNECT command
    request[2] = 0x00; // obligatory reserved field (perfect for MS tampering!)
    
    if (PR_NetAddrFamily(addr) == PR_AF_INET) {
        
        request[3] = 0x01; // encoding of destination address (1 == IPv4)
        request_len += 4;
        
        char * ip = (char*)(&addr->inet.ip);
        request[4] = *ip++;
        request[5] = *ip++;
        request[6] = *ip++;
        request[7] = *ip++;
        
    } else if (PR_NetAddrFamily(addr) == PR_AF_INET6) {
        
        request[3] = 0x04; // encoding of destination address (4 == IPv6)
        request_len += 16;
        
        char * ip = (char*)(&addr->ipv6.ip.pr_s6_addr);
        request[4] = *ip++; request[5] = *ip++; request[6] = *ip++; request[7] = *ip++;
        request[8] = *ip++; request[9] = *ip++; request[10] = *ip++; request[11] = *ip++;
        request[12] = *ip++; request[13] = *ip++; request[14] = *ip++; request[15] = *ip++;
        request[16] = *ip++; request[17] = *ip++; request[18] = *ip++; request[19] = *ip++;
        
        // we're going to test to see if this address can
        // be mapped back into IPv4 without loss. if so,
        // we'll use IPv4 instead, as reliable SOCKS server 
        // support for IPv6 is probably questionable.
        
        if (PR_IsNetAddrType(addr, PR_IpAddrV4Mapped)) {
            
            request[3] = 0x01; // ipv4 encoding
            request[4] = request[16];
            request[5] = request[17];
            request[6] = request[18];
            request[7] = request[19];
            request_len -= 12;
            
        }
        
    } else {
        
        // Unknown address type
        LOGERROR(("Don't know what kind of IP address this is."));
        return NS_ERROR_FAILURE;
    }
    
    // destination port
    PRUint16 destPort = PR_htons(PR_NetAddrInetPort(addr));
    
    request[request_len-2] = (unsigned char)(destPort >> 8);
    request[request_len-1] = (unsigned char)destPort;
    
    write_len = PR_Send(fd, request, request_len, 0, timeout);
    if (write_len != request_len) {

        // bad write
        LOGERROR(("PR_Send() failed sending connect command. Wrote: %d bytes; Expected: %d.", write_len, request_len));
        return NS_ERROR_FAILURE;
    }
    
    response_len = 22;
    response_len = PR_Recv(fd, response, response_len, 0, timeout);
    if (response_len <= 0) {

        // bad read
        LOGERROR(("PR_Recv() failed getting connect command reply. response_len = %d.", response_len));
        return NS_ERROR_FAILURE;
    }
    
    if (response[0] != 0x05) {

        // bad response
        LOGERROR(("Not a SOCKS 5 reply. Expected: 5; received: %x", response[0]));
        return NS_ERROR_FAILURE;
    }
    
    switch(response[1]) {
        case 0x00:  break;      // success
        case 0x01:  LOGERROR(("SOCKS 5 server rejected connect request: 01, General SOCKS server failure."));
                    return NS_ERROR_FAILURE;
        case 0x02:  LOGERROR(("SOCKS 5 server rejected connect request: 02, Connection not allowed by ruleset."));
                    return NS_ERROR_FAILURE;
        case 0x03:  LOGERROR(("SOCKS 5 server rejected connect request: 03, Network unreachable."));
                    return NS_ERROR_FAILURE;
        case 0x04:  LOGERROR(("SOCKS 5 server rejected connect request: 04, Host unreachable."));
                    return NS_ERROR_FAILURE;
        case 0x05:  LOGERROR(("SOCKS 5 server rejected connect request: 05, Connection refused."));
                    return NS_ERROR_FAILURE;
        case 0x06:  LOGERROR(("SOCKS 5 server rejected connect request: 06, TTL expired."));
                    return NS_ERROR_FAILURE;
        case 0x07:  LOGERROR(("SOCKS 5 server rejected connect request: 07, Command not supported."));
                    return NS_ERROR_FAILURE;
        case 0x08:  LOGERROR(("SOCKS 5 server rejected connect request: 08, Address type not supported."));
                    return NS_ERROR_FAILURE;
        default:    LOGERROR(("SOCKS 5 server rejected connect request: %x.", response[1]));
                    return NS_ERROR_FAILURE;

        
    }
    
    
    // get external bound address (this is what 
    // the outside world sees as "us")
    char *ip = nsnull;
    PRUint16 extPort = 0;

    switch (response[3]) {
        case 0x01: // IPv4
            
            extPort = (response[8] << 8) | response[9];
            
            PR_SetNetAddr(PR_IpAddrAny, PR_AF_INET, extPort, extAddr);
            
            ip = (char*)(&extAddr->inet.ip);
            *ip++ = response[4];
            *ip++ = response[5];
            *ip++ = response[6];
            *ip++ = response[7];
            
            break;
        case 0x04: // IPv6
            
            extPort = (response[20] << 8) | response[21];
            
            PR_SetNetAddr(PR_IpAddrAny, PR_AF_INET6, extPort, extAddr);
            
            ip = (char*)(&extAddr->ipv6.ip.pr_s6_addr);
            *ip++ = response[4]; *ip++ = response[5]; *ip++ = response[6]; *ip++ = response[7];
            *ip++ = response[8]; *ip++ = response[9]; *ip++ = response[10]; *ip++ = response[11];
            *ip++ = response[12]; *ip++ = response[13]; *ip++ = response[14]; *ip++ = response[15];
            *ip++ = response[16]; *ip++ = response[17]; *ip++ = response[18]; *ip++ = response[19];
            
            break;
        case 0x03: // FQDN (should not get this back)
        default: // unknown format
            // if we get here, we don't know our external address.
            // however, as that's possibly not critical to the user,
            // we let it slide.
            PR_InitializeNetAddr(PR_IpAddrNull, 0, extAddr);
            //return NS_ERROR_FAILURE;
            break;
    }
    return NS_OK;

}

// Negotiate a SOCKS 4 connection. Assumes the TCP connection to the socks 
// server port has been established.
static nsresult
ConnectSOCKS4(PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
    NS_ENSURE_TRUE(fd, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_TRUE(addr, NS_ERROR_NOT_INITIALIZED);

    unsigned char request[12];
    int request_len = 0;
    unsigned char response[10];
    int response_len = 0;

    request[0] = 0x04; // SOCKS version 4
    request[1] = 0x01; // CD command code -- 1 for connect

    // destination port
    PRUint16 destPort = PR_htons(PR_NetAddrInetPort(addr));

    request[2] = (unsigned char)(destPort >> 8);
    request[3] = (unsigned char)destPort;

    // destination IP 
    char * ip = nsnull;

    // IPv4
    if (PR_NetAddrFamily(addr) == PR_AF_INET) 
        ip = (char*)(&addr->inet.ip);

    // IPv6
    else if (PR_NetAddrFamily(addr) == PR_AF_INET6) {

        // IPv4 address encoded in an IPv6 address
        if (PR_IsNetAddrType(addr, PR_IpAddrV4Mapped))
            ip = (char*)(&addr->ipv6.ip.pr_s6_addr[12]);
        else {
            LOGERROR(("IPv6 not supported in SOCK 4."));
            return NS_ERROR_FAILURE;	// SOCKS 4 can't do IPv6
        }
    }

    else {

        LOGERROR(("Don't know what kind of IP address this is."));
        return NS_ERROR_FAILURE;		// don't recognize this type
    }

    request[4] = *ip++;
    request[5] = *ip++;
    request[6] = *ip++;
    request[7] = *ip++;

    // username
    request[8] = 'M';
    request[9] = 'O';
    request[10] = 'Z';

    request[11] = 0x00;

    request_len = 12;
    int write_len = PR_Send(fd, request, request_len, 0, timeout);
    if (write_len != request_len) {

        LOGERROR(("PR_Send() failed. Wrote: %d bytes; Expected: %d.", write_len, request_len));
        return NS_ERROR_FAILURE;
    }
    
    // get the server's response
    response_len = 8;	// size of the response
    response_len = PR_Recv(fd, response, response_len, 0, timeout);

    if (response_len <= 0) {
        LOGERROR(("PR_Recv() failed. response_len = %d.", response_len));
        return NS_ERROR_FAILURE;
    }
    
    if ((response[0] != 0x00) && (response[0] != 0x04)) {
        // Novell BorderManager sends a response of type 4, should be zero
        // According to the spec. Cope with this brokenness.        
        // it's not a SOCKS 4 reply or version 0 of the reply code
        LOGERROR(("Not a SOCKS 4 reply. Expected: 0; received: %x.", response[0]));
        return NS_ERROR_FAILURE;
    }

    if (response[1] != 0x5A) { // = 90: request granted
        // connect request not granted
        LOGERROR(("Connection request refused. Expected: 90; received: %d.", response[1]));
        return NS_ERROR_FAILURE;
    }
 
    return NS_OK;

}


static PRStatus PR_CALLBACK
nsSOCKSIOLayerConnect(PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime /*timeout*/)
{

    PRStatus status;
    
    nsSOCKSSocketInfo * info = (nsSOCKSSocketInfo*) fd->secret;
    if (info == NULL) return PR_FAILURE;
    
    // First, we need to look up our proxy...
    const nsCString &proxyHost = info->ProxyHost();

    if (proxyHost.IsEmpty())
        return PR_FAILURE;

    PRInt32 socksVersion = info->Version();

    LOGDEBUG(("nsSOCKSIOLayerConnect SOCKS %u; proxyHost: %s.", socksVersion, proxyHost.get()));

    // Sync resolve the proxy hostname.
    PRNetAddr proxyAddr;
    {
        nsCOMPtr<nsIDNSService> dns;
        nsCOMPtr<nsIDNSRecord> rec; 
        nsresult rv;
        
        dns = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv))
            return PR_FAILURE;

        rv = dns->Resolve(proxyHost, 0, getter_AddRefs(rec));
        if (NS_FAILED(rv))
            return PR_FAILURE;

        rv = rec->GetNextAddr(info->ProxyPort(), &proxyAddr);
        if (NS_FAILED(rv))
            return PR_FAILURE;
    }

    info->SetInternalProxyAddr(&proxyAddr);
    
    // For now, we'll do this as a blocking connect,
    // but with nspr 4.1, the necessary functions to
    // do a non-blocking connect will be available
    
    // Preserve the non-blocking state of the socket
    PRBool nonblocking;
    PRSocketOptionData sockopt;
    sockopt.option = PR_SockOpt_Nonblocking;
    status = PR_GetSocketOption(fd, &sockopt);
    
    if (PR_SUCCESS != status) {
        LOGERROR(("PR_GetSocketOption() failed. status = %x.", status));
        return status;
    }
    
    // Store blocking option
    nonblocking = sockopt.value.non_blocking;
    
    sockopt.option = PR_SockOpt_Nonblocking;
    sockopt.value.non_blocking = PR_FALSE;
    status = PR_SetSocketOption(fd, &sockopt);
    
    if (PR_SUCCESS != status) {
        LOGERROR(("PR_SetSocketOption() failed. status = %x.", status));
        return status;
    }
    
    // Now setup sockopts, so we can restore the value later.
    sockopt.option = PR_SockOpt_Nonblocking;
    sockopt.value.non_blocking = nonblocking;
    

    // This connectWait should be long enough to connect to local proxy
    // servers, but not much longer. Since this protocol negotiation
    // uses blocking network calls, the app can appear to hang for a maximum
    // of this time if the user presses the STOP button during the SOCKS
    // connection negotiation. Note that this value only applies to the 
    // connecting to the SOCKS server: once the SOCKS connection has been
    // established, the value is not used anywhere else.
    PRIntervalTime connectWait = PR_SecondsToInterval(10);

    // Connect to the proxy server.
    status = fd->lower->methods->connect(fd->lower, &proxyAddr, connectWait);
    
    if (PR_SUCCESS != status) {
        LOGERROR(("Failed to TCP connect to the proxy server (%s): timeout = %d, status = %x.",proxyHost.get(), connectWait, status));
        PR_SetSocketOption(fd, &sockopt);
        return status;
    }
    
    
    // We are now connected to the SOCKS proxy server.
    // Now we will negotiate a connection to the desired server.
    
    // External IP address returned from ConnectSOCKS5(). Not supported in SOCKS4.
    PRNetAddr extAddr;	
    PR_InitializeNetAddr(PR_IpAddrNull, 0, &extAddr);

 

    NS_ASSERTION((socksVersion == 4) || (socksVersion == 5), "SOCKS Version must be selected");

    nsresult rv;

    // Try to connect via SOCKS 5.
    if (socksVersion == 5) {
        rv = ConnectSOCKS5(fd, addr, &extAddr, connectWait);

        if (NS_FAILED(rv)) {
            PR_SetSocketOption(fd, &sockopt);
            return PR_FAILURE;	
        }

    }

    // Try to connect via SOCKS 4.
    else {
        rv = ConnectSOCKS4(fd, addr, connectWait);

        if (NS_FAILED(rv)) {
            PR_SetSocketOption(fd, &sockopt);
            return PR_FAILURE;	
        }

    }

    
    info->SetDestinationAddr((PRNetAddr*)addr);
    info->SetExternalProxyAddr(&extAddr);
    
    // restore non-blocking option
    PR_SetSocketOption(fd, &sockopt);
    
    // we're set-up and connected. 
    // this socket can be used as normal now.
    
    return PR_SUCCESS;
}

static PRStatus PR_CALLBACK
nsSOCKSIOLayerClose(PRFileDesc *fd)
{
    nsSOCKSSocketInfo * info = (nsSOCKSSocketInfo*) fd->secret;
    PRDescIdentity id = PR_GetLayersIdentity(fd);
    
    if (info && id == nsSOCKSIOLayerIdentity)
    {
        NS_RELEASE(info);
        fd->identity = PR_INVALID_IO_LAYER;
    }
    
    return fd->lower->methods->close(fd->lower);
}

static PRFileDesc* PR_CALLBACK
nsSOCKSIOLayerAccept(PRFileDesc *fd, PRNetAddr *addr, PRIntervalTime timeout)
{
    // TODO: implement SOCKS support for accept
    return fd->lower->methods->accept(fd->lower, addr, timeout);
}

static PRInt32 PR_CALLBACK
nsSOCKSIOLayerAcceptRead(PRFileDesc *sd, PRFileDesc **nd, PRNetAddr **raddr, void *buf, PRInt32 amount, PRIntervalTime timeout)
{
    // TODO: implement SOCKS support for accept, then read from it
    return sd->lower->methods->acceptread(sd->lower, nd, raddr, buf, amount, timeout);
}

static PRStatus PR_CALLBACK
nsSOCKSIOLayerBind(PRFileDesc *fd, const PRNetAddr *addr)
{
    // TODO: implement SOCKS support for bind (very similar to connect)
    return fd->lower->methods->bind(fd->lower, addr);
}

static PRStatus PR_CALLBACK
nsSOCKSIOLayerGetName(PRFileDesc *fd, PRNetAddr *addr)
{
    nsSOCKSSocketInfo * info = (nsSOCKSSocketInfo*) fd->secret;
    
    if (info != NULL && addr != NULL) {
        if (info->GetExternalProxyAddr(&addr) == NS_OK)
            return PR_SUCCESS;
    }
    
    return PR_FAILURE;
}

static PRStatus PR_CALLBACK
nsSOCKSIOLayerGetPeerName(PRFileDesc *fd, PRNetAddr *addr)
{
    nsSOCKSSocketInfo * info = (nsSOCKSSocketInfo*) fd->secret;
    
    if (info != NULL && addr != NULL) {
        if (info->GetDestinationAddr(&addr) == NS_OK)
            return PR_SUCCESS;
    }
    
    return PR_FAILURE;
}

static PRStatus PR_CALLBACK
nsSOCKSIOLayerListen(PRFileDesc *fd, PRIntn backlog)
{
    // TODO: implement SOCKS support for listen
    return fd->lower->methods->listen(fd->lower, backlog);
}

// add SOCKS IO layer to an existing socket
nsresult
nsSOCKSIOLayerAddToSocket(PRInt32 family,
                          const char *host, 
                          PRInt32 port,
                          const char *proxyHost,
                          PRInt32 proxyPort,
                          PRInt32 socksVersion,
                          PRFileDesc *fd, 
                          nsISupports** info)
{
    NS_ENSURE_TRUE((socksVersion == 4) || (socksVersion == 5), NS_ERROR_NOT_INITIALIZED);


    if (firstTime)
    {
        nsSOCKSIOLayerIdentity		= PR_GetUniqueIdentity("SOCKS layer");
        nsSOCKSIOLayerMethods		= *PR_GetDefaultIOMethods();
        
        nsSOCKSIOLayerMethods.connect	= nsSOCKSIOLayerConnect;
        nsSOCKSIOLayerMethods.bind	= nsSOCKSIOLayerBind;
        nsSOCKSIOLayerMethods.acceptread = nsSOCKSIOLayerAcceptRead;
        nsSOCKSIOLayerMethods.getsockname = nsSOCKSIOLayerGetName;
        nsSOCKSIOLayerMethods.getpeername = nsSOCKSIOLayerGetPeerName;
        nsSOCKSIOLayerMethods.accept	= nsSOCKSIOLayerAccept;
        nsSOCKSIOLayerMethods.listen	= nsSOCKSIOLayerListen;
        nsSOCKSIOLayerMethods.close	= nsSOCKSIOLayerClose;
        
        firstTime			= PR_FALSE;

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
    infoObject->Init(socksVersion, proxyHost, proxyPort);
    layer->secret = (PRFilePrivate*) infoObject;
    rv = PR_PushIOLayer(fd, PR_GetLayersIdentity(fd), layer);
    
    if (NS_FAILED(rv))
    {
        LOGERROR(("PR_PushIOLayer() failed. rv = %x.", rv));
        NS_RELEASE(infoObject);
        PR_DELETE(layer);
        return NS_ERROR_FAILURE;
    }
    
    *info = infoObject;
    NS_ADDREF(*info);
    return NS_OK;
}
