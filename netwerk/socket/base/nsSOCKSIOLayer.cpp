/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Justin Bradford <jab@atdot.org>
 */

#include "nspr.h"
#include "nsString.h"

#include "nsIServiceManager.h"
#include "nsSOCKSIOLayer.h"

static PRDescIdentity	nsSOCKSIOLayerIdentity;
static PRIOMethods	nsSOCKSIOLayerMethods;
static PRBool firstTime = PR_TRUE;

class nsSOCKSSocketInfo : public nsISOCKSSocketInfo
{
public:
    nsSOCKSSocketInfo();
    virtual ~nsSOCKSSocketInfo();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROXY
    NS_DECL_NSISOCKSSOCKETINFO
    
protected:
    
    // nsIProxy
    char*	mProxyHost;
    PRInt32	mProxyPort;
    char*        mProxyType;
    
    // nsISOCKSSocketInfo
    PRNetAddr	mInternalProxyAddr;
    PRNetAddr	mExternalProxyAddr;
    PRNetAddr	mDestinationAddr;
};

nsSOCKSSocketInfo::nsSOCKSSocketInfo()
{
    NS_INIT_REFCNT();
    
    mProxyHost = nsnull;
    mProxyPort = -1;
    mProxyType = nsnull;
    
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &mInternalProxyAddr);
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &mExternalProxyAddr);
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &mDestinationAddr);
}

nsSOCKSSocketInfo::~nsSOCKSSocketInfo()
{
    PR_FREEIF(mProxyHost);
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsSOCKSSocketInfo, nsISOCKSSocketInfo, nsIProxy)

NS_IMETHODIMP 
nsSOCKSSocketInfo::GetProxyHost(char * *aProxyHost)
{
    if (!aProxyHost) return NS_ERROR_NULL_POINTER;
    if (mProxyHost) 
    {
        *aProxyHost = nsCRT::strdup(mProxyHost);
        return (*aProxyHost == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    else
    {
        *aProxyHost = nsnull;
        return NS_OK;
    }
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::SetProxyHost(const char * aProxyHost)
{
    PR_FREEIF(mProxyHost);
    if (aProxyHost)
    {
        mProxyHost = nsCRT::strdup(aProxyHost);
        return (mProxyHost == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    else
    {
        mProxyHost = nsnull;
        return NS_OK;
    }
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::GetProxyPort(PRInt32 *aProxyPort)
{
    *aProxyPort = mProxyPort;
    return NS_OK;
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::SetProxyPort(PRInt32 aProxyPort)
{
    mProxyPort = aProxyPort;
    return NS_OK;
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::GetProxyType(char * *aProxyType)
{
    if (!aProxyType) return NS_ERROR_NULL_POINTER;
    if (mProxyType) 
    {
        *aProxyType = nsCRT::strdup(mProxyType);
        return (*aProxyType == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    else
    {
        *aProxyType = nsnull;
        return NS_OK;
    }
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::SetProxyType(const char * aProxyType)
{
    PR_FREEIF(mProxyType);
    if (aProxyType)
    {
        mProxyType = nsCRT::strdup(aProxyType);
        return (mProxyType == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    else
    {
        mProxyType = nsnull;
        return NS_OK;
    }
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::GetExternalProxyAddr(PRNetAddr * *aExternalProxyAddr)
{
    nsCRT::memcpy(*aExternalProxyAddr, &mExternalProxyAddr, sizeof(PRNetAddr));
    return NS_OK;
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::SetExternalProxyAddr(PRNetAddr *aExternalProxyAddr)
{
    nsCRT::memcpy(&mExternalProxyAddr, aExternalProxyAddr, sizeof(PRNetAddr));
    return NS_OK;
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::GetDestinationAddr(PRNetAddr * *aDestinationAddr)
{
    nsCRT::memcpy(*aDestinationAddr, &mDestinationAddr, sizeof(PRNetAddr));
    return NS_OK;
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::SetDestinationAddr(PRNetAddr *aDestinationAddr)
{
    nsCRT::memcpy(&mDestinationAddr, aDestinationAddr, sizeof(PRNetAddr));
    return NS_OK;
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::GetInternalProxyAddr(PRNetAddr * *aInternalProxyAddr)
{
    nsCRT::memcpy(*aInternalProxyAddr, &mInternalProxyAddr, sizeof(PRNetAddr));
    return NS_OK;
}

NS_IMETHODIMP 
nsSOCKSSocketInfo::SetInternalProxyAddr(PRNetAddr *aInternalProxyAddr)
{
    nsCRT::memcpy(&mInternalProxyAddr, aInternalProxyAddr, sizeof(PRNetAddr));
    return NS_OK;
}

static PRStatus PR_CALLBACK
nsSOCKSIOLayerConnect(PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
    PRStatus status;
    
    nsSOCKSSocketInfo * info = (nsSOCKSSocketInfo*) fd->secret;
    if (info == NULL) return PR_FAILURE;
    
    // First, we need to look up our proxy...
    char scratch[PR_NETDB_BUF_SIZE];
    PRHostEnt hostentry;
    char * proxyHost;

    nsresult rv = info->GetProxyHost(&proxyHost);

    if (NS_FAILED(rv) || !proxyHost || !(*proxyHost)) {
        return PR_FAILURE;
    }

    status = PR_GetHostByName(proxyHost, scratch, PR_NETDB_BUF_SIZE, &hostentry);
    
    if (PR_SUCCESS != status) {
        return status;
    }
    
    // Extract the proxy addr
    PRIntn entEnum = 0;
    PRNetAddr proxyAddr;
    PRInt32 proxyPort;
    info->GetProxyPort(&proxyPort);
    entEnum = PR_EnumerateHostEnt(entEnum, &hostentry, proxyPort, &proxyAddr);
    
    if (entEnum <= 0) {
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
        return status;
    }
    
    // Store blocking option
    nonblocking = sockopt.value.non_blocking;
    
    sockopt.option = PR_SockOpt_Nonblocking;
    sockopt.value.non_blocking = PR_FALSE;
    status = PR_SetSocketOption(fd, &sockopt);
    
    if (PR_SUCCESS != status) {
        return status;
    }
    
    // Now setup sockopts, so we can restore the value later.
    sockopt.option = PR_SockOpt_Nonblocking;
    sockopt.value.non_blocking = nonblocking;
    
    
    // connect to the proxy server
    status = fd->lower->methods->connect(fd->lower, &proxyAddr, timeout);
    
    if (PR_SUCCESS != status) {
        return status;
    }
    
    
    // We are now connected to the SOCKS proxy server.
    // Now we will negotiate a connection to the desired server.
    
    unsigned char request[22];
    int request_len = 0;
    unsigned char response[22];
    int response_len = 0;
    
    request[0] = 0x05; // SOCKS version 5
    request[1] = 0x01; // number of auth procotols we recognize
    // auth protocols
    request[2] = 0x00; // no authentication required
    // compliant implementations MUST implement GSSAPI
    // and SHOULD implement username/password
    // TODO: we don't
    //request[3] = 0x01; // GSSAPI
    //request[4] = 0x02; // username/password
    
    request_len = 2 + request[1];
    int write_len = PR_Write(fd, request, request_len);
    if (write_len != request_len) {
        return PR_FAILURE;
    }
    
    // get the server's response
    response_len = 22;
    response_len = PR_Read(fd, response, response_len);
    
    if (response_len <= 0) {
        return PR_FAILURE;
    }
    
    if (response[0] != 0x05) {
        // it's a either not SOCKS or not our version
        return PR_FAILURE;
    }
    switch (response[1]) {
        case 0x00:
            // no auth
            break;
        case 0x01:
            // GSSAPI
            // TODO: implement
        case 0x02:
            // username/password
            // TODO: implement
        default:
            // unrecognized auth method
            return PR_FAILURE;
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
        
        request[3] = 0x04; // encoding of destination address (1 == IPv6)
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
        return PR_FAILURE;
    }
    
    // destination port
    PRUint16 destPort = PR_NetAddrInetPort(addr);
    
    request[request_len-1] = destPort >> 8;
    request[request_len-2] = destPort;
    
    if (PR_Write(fd, request, request_len) != request_len) {
        // bad write
        return PR_FAILURE;
    }
    
    response_len = 22;
    response_len = PR_Read(fd, response, response_len);
    if (response_len <= 0) {
        // bad read
        return PR_FAILURE;
    }
    
    if (response[0] != 0x05) {
        // bad response
        return PR_FAILURE;
    }
    
    if (response[1] != 0x00) {
        // SOCKS server failed to connect
        return PR_FAILURE;
    }
    
    
    // get external bound address (this is what 
    // the outside world sees as "us")
    PRNetAddr extAddr;
    PRUint16 extPort;
    char *ip = nsnull;
    
    switch (response[3]) {
        case 0x01: // IPv4
            
            extPort = (response[8] << 8) | response[9];
            
            PR_SetNetAddr(PR_IpAddrAny, PR_AF_INET, extPort, &extAddr);
            
            ip = (char*)(&extAddr.inet.ip);
            *ip++ = response[4];
            *ip++ = response[5];
            *ip++ = response[6];
            *ip++ = response[7];
            
            break;
        case 0x04: // IPv6
            
            extPort = (response[20] << 8) | response[21];
            
            PR_SetNetAddr(PR_IpAddrAny, PR_AF_INET6, extPort, &extAddr);
            
            ip = (char*)(&extAddr.ipv6.ip.pr_s6_addr);
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
            PR_InitializeNetAddr(PR_IpAddrNull, 0, &extAddr);
            //return PR_FAILURE;
            break;
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


// create a new socket with a SOCKS IO layer
nsresult
nsSOCKSIOLayerNewSocket(const char *host, 
                        PRInt32 port,
                        const char *proxyHost,
                        PRInt32 proxyPort,
                        PRFileDesc **fd, 
                        nsISupports** info)
{
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
    }
    
    
    PRFileDesc *	sock;
    PRFileDesc *	layer;
    PRStatus	rv;
    
    /* Get a normal NSPR socket */
    sock = PR_NewTCPSocket();
    if (! sock) 
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    layer = PR_CreateIOLayerStub(nsSOCKSIOLayerIdentity, &nsSOCKSIOLayerMethods);
    if (! layer)
    {
        PR_Close(sock);
        return NS_ERROR_FAILURE;
    }
    
    nsSOCKSSocketInfo * infoObject = new nsSOCKSSocketInfo();
    if (!infoObject)
    {
        PR_Close(sock);
        // clean up IOLayerStub
        PR_DELETE(layer);
        return NS_ERROR_FAILURE;
    }
    
    NS_ADDREF(infoObject);
    infoObject->SetProxyHost(proxyHost);
    infoObject->SetProxyPort(proxyPort);
    layer->secret = (PRFilePrivate*) infoObject;
    rv = PR_PushIOLayer(sock, PR_GetLayersIdentity(sock), layer);
    
    if (NS_FAILED(rv))
    {
        PR_Close(sock);
        NS_RELEASE(infoObject);
        PR_DELETE(layer);
        return NS_ERROR_FAILURE;
    }
    
    *fd = sock;
    *info = infoObject;
    NS_ADDREF(*info);

    return NS_OK;
}

// add SOCKS IO layer to an existing socket
nsresult
nsSOCKSIOLayerAddToSocket(const char *host, 
                          PRInt32 port,
                          const char *proxyHost,
                          PRInt32 proxyPort,
                          PRFileDesc *fd, 
                          nsISupports** info)
{
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
    }
    
    
    PRFileDesc *	layer;
    PRStatus	rv;
    
    layer = PR_CreateIOLayerStub(nsSOCKSIOLayerIdentity, &nsSOCKSIOLayerMethods);
    if (! layer)
    {
        return NS_ERROR_FAILURE;
    }
    
    nsSOCKSSocketInfo * infoObject = new nsSOCKSSocketInfo();
    if (!infoObject)
    {
        // clean up IOLayerStub
        PR_DELETE(layer);
        return NS_ERROR_FAILURE;
    }
    
    NS_ADDREF(infoObject);
    infoObject->SetProxyHost(proxyHost);
    infoObject->SetProxyPort(proxyPort);
    layer->secret = (PRFilePrivate*) infoObject;
    rv = PR_PushIOLayer(fd, PR_GetLayersIdentity(fd), layer);
    
    if (NS_FAILED(rv))
    {
        NS_RELEASE(infoObject);
        PR_DELETE(layer);
        return NS_ERROR_FAILURE;
    }
    
    *info = infoObject;
    NS_ADDREF(*info);
    return NS_OK;
}


