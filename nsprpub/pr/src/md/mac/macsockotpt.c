/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* This turns on UNIX style errors in OT 1.1 headers */
#define OTUNIXERRORS 1

#include <Gestalt.h>
#include <OpenTransport.h>

#define GESTALT_OPEN_TPT_PRESENT        gestaltOpenTptPresentMask
#define GESTALT_OPEN_TPT_TCP_PRESENT    gestaltOpenTptTCPPresentMask

#include <OpenTptInternet.h>    // All the internet typedefs
#include "primpl.h"

typedef enum SndRcvOpCode {
    kSTREAM_SEND,
    kSTREAM_RECEIVE,
    kDGRAM_SEND,
    kDGRAM_RECEIVE
} SndRcvOpCode;

static struct {
	PRLock *    lock;
	InetSvcRef  serviceRef;
	PRThread *  thread;
	void *      cookie;
} dnsContext;


static pascal void  DNSNotifierRoutine(void * contextPtr, OTEventCode code, OTResult result, void * cookie);
static pascal void  NotifierRoutine(void * contextPtr, OTEventCode code, 
            OTResult result, void * cookie);

static PRBool GetState(PRFileDesc *fd, PRBool *readReady, PRBool *writeReady, PRBool *exceptReady);

extern void WaitOnThisThread(PRThread *thread, PRIntervalTime timeout);
extern void DoneWaitingOnThisThread(PRThread *thread);


void _MD_InitNetAccess()
{
    OSErr       err;
    OSStatus    errOT;
    PRBool      hasOTTCPIP = PR_FALSE;
    PRBool      hasOT = PR_FALSE;
    long        gestaltResult;
    
    err = Gestalt(gestaltOpenTpt, &gestaltResult);
    if (err == noErr)
        if (gestaltResult & GESTALT_OPEN_TPT_PRESENT)
            hasOT = PR_TRUE;
    
    if (hasOT)
        if (gestaltResult & GESTALT_OPEN_TPT_TCP_PRESENT)
            hasOTTCPIP = PR_TRUE;
        
    PR_ASSERT(hasOTTCPIP == PR_TRUE);

    errOT = InitOpenTransport();
    PR_ASSERT(err == kOTNoError);
	
	dnsContext.lock = PR_NewLock();
	PR_ASSERT(dnsContext.lock != NULL);

	dnsContext.thread = _PR_MD_CURRENT_THREAD();
	dnsContext.cookie = NULL;
	
    dnsContext.serviceRef = OTOpenInternetServices(kDefaultInternetServicesPath, NULL, &errOT);
    if (errOT != kOTNoError) return;    /* no network -- oh well */
    PR_ASSERT((dnsContext.serviceRef != NULL) && (errOT == kOTNoError));

    /* Install notify function for DNR Address To String completion */
    errOT = OTInstallNotifier(dnsContext.serviceRef, DNSNotifierRoutine, &dnsContext);
    PR_ASSERT(errOT == kOTNoError);

    /* Put us into async mode */
    errOT = OTSetAsynchronous(dnsContext.serviceRef);
    PR_ASSERT(errOT == kOTNoError);

/* XXX Does not handle absence of open tpt and tcp yet! */
}


pascal void  DNSNotifierRoutine(void * contextPtr, OTEventCode code, OTResult result, void * cookie)
{
#pragma unused(contextPtr)
    _PRCPU *    cpu    = _PR_MD_CURRENT_CPU(); 
	
	if (code == T_DNRSTRINGTOADDRCOMPLETE) {
		dnsContext.thread->md.osErrCode = result;
		dnsContext.cookie = cookie;
		if (_PR_MD_GET_INTSOFF()) {
			cpu->u.missed[cpu->where] |= _PR_MISSED_IO;
			dnsContext.thread->md.notifyPending = PR_TRUE;
			return;
		}
		DoneWaitingOnThisThread(dnsContext.thread);
	}
	// or else we don't handle the event
}


static void macsock_map_error(OSStatus err)
{
    _PR_MD_CURRENT_THREAD()->md.osErrCode = err;

    if (IsEError(err) || (err >= EPERM && err <= ELASTERRNO)) {
    switch (IsEError(err) ? OSStatus2E(err) : err) {
        case EBADF:
            PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
            break;
        case EADDRNOTAVAIL:
            PR_SetError(PR_ADDRESS_NOT_AVAILABLE_ERROR, err);
            break;
        case EINPROGRESS:
            PR_SetError(PR_IN_PROGRESS_ERROR, err);
            break;
        case EWOULDBLOCK:
        case EAGAIN:
            PR_SetError(PR_WOULD_BLOCK_ERROR, err);
            break;
        case ENOTSOCK:
            PR_SetError(PR_NOT_SOCKET_ERROR, err);
            break;
        case ETIMEDOUT:
            PR_SetError(PR_IO_TIMEOUT_ERROR, err);
            break;
        case ECONNREFUSED:
            PR_SetError(PR_CONNECT_REFUSED_ERROR, err);
            break;
        case ENETUNREACH:
            PR_SetError(PR_NETWORK_UNREACHABLE_ERROR, err);
            break;
        case EADDRINUSE:
            PR_SetError(PR_ADDRESS_IN_USE_ERROR, err);
            break;
        case EFAULT:
            PR_SetError(PR_ACCESS_FAULT_ERROR, err);
            break;
        case EINTR:
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
            break;
        case EINVAL:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
            break;
        case EIO:
            PR_SetError(PR_IO_ERROR, err);
            break;
        case ENOENT:
            PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, err);
            break;
        case ENXIO:
            PR_SetError(PR_IO_ERROR, err);
            break;
        case EPROTOTYPE:
            PR_SetError(PR_PROTOCOL_NOT_SUPPORTED_ERROR, err);
            break;
        case EOPNOTSUPP:
            PR_SetError(PR_OPERATION_NOT_SUPPORTED_ERROR, err);
            break;
        default:
            PR_SetError(PR_UNKNOWN_ERROR, err);
            break;
        }
    } else {
    PR_ASSERT(IsXTIError(err));
    switch (err) {
        case kOTNoDataErr:
        case kOTFlowErr:
            PR_SetError(PR_WOULD_BLOCK_ERROR, err);
            break;
        default:
            PR_ASSERT(0);
            PR_SetError(PR_UNKNOWN_ERROR, err);
            break;
        }
    }
}



static void PrepareForAsyncCompletion(PRThread * thread, PRFileDesc * fd)
{
    thread->io_pending       = PR_TRUE;
    thread->io_fd            = fd->secret->md.osfd;
    thread->md.osErrCode     = noErr;
}


// Notification routine
// Async callback routine.
// A5 is OK. Cannot allocate memory here
pascal void  NotifierRoutine(void * contextPtr, OTEventCode code, OTResult result, void * cookie)
{
	PRFileDesc *  fd       = (PRFileDesc *) contextPtr;
	EndpointRef   endpoint = (EndpointRef)fd->secret->md.osfd;
    PRThread *    thread   = NULL;
    _PRCPU *      cpu      = _PR_MD_CURRENT_CPU(); 
	OSStatus      err;
    TDiscon       discon;
	OTResult	  resultOT;

    switch (code)
    {
// OTLook Events - 
        case T_LISTEN:        // A connection request is available
            PR_ASSERT(!"T_LISTEN Not implemented");  // listen or reject?
            break;
			
        case T_CONNECT:      // Confirmation of a connect request
			// cookie = sndCall parameter from OTConnect()
            err = OTRcvConnect(endpoint, NULL);
            PR_ASSERT(err == kOTNoError);

			// wake up waiting thread, if any
            thread = fd->secret->md.write.thread;
            fd->secret->md.write.thread    = NULL;
            fd->secret->md.write.cookie    = cookie;
            fd->secret->md.connectionOpen  = PR_TRUE;
            break;

        case T_DATA:        // Standard data is available
			fd->secret->md.readReady = PR_TRUE;

			// wake up waiting thread, if any
            thread = fd->secret->md.read.thread;
            fd->secret->md.read.thread    = NULL;
            fd->secret->md.read.cookie    = cookie;
			break;

        case T_EXDATA:      // Expedited data is available
            PR_ASSERT(!"T_EXDATA Not implemented");
			return;

        case T_DISCONNECT:  // A disconnect is available
            memset(&discon, 0 , sizeof(discon));
            err = OTRcvDisconnect(endpoint, &discon);
            PR_ASSERT(err == kOTNoError);
            macsock_map_error(discon.reason);
            fd->secret->md.exceptReady     = PR_TRUE;
            fd->secret->md.connectionOpen  = PR_FALSE;
            break;
		
        case T_ERROR:       // obsolete/unused in library
            PR_ASSERT(!"T_ERROR Not implemented");
			return;		
		
        case T_UDERR:       // UDP Send error; clear the error
			(void) OTRcvUDErr((EndpointRef) cookie, NULL);
            break;

        case T_ORDREL:      // An orderly release is available
            err = OTRcvOrderlyDisconnect(endpoint);
            PR_ASSERT(err == kOTNoError);
            fd->secret->md.readReady      = PR_TRUE;   // mark readable (to emulate bsd sockets)
            // remember connection is closed, so we can return 0 on read or receive
			fd->secret->md.connectionOpen = PR_FALSE;
            break;		

        case T_GODATA:   // Flow control lifted on standard data
            fd->secret->md.writeReady = PR_TRUE;
			resultOT = OTLook(endpoint);		// clear T_GODATA event
			PR_ASSERT(resultOT == T_GODATA);
			
			// wake up waiting thread, if any
            thread = fd->secret->md.write.thread;
            fd->secret->md.write.thread    = NULL;
            fd->secret->md.write.cookie    = cookie;
            break;

        case T_GOEXDATA: // Flow control lifted on expedited data
            PR_ASSERT(!"T_GOEXDATA Not implemented");
			return;

        case T_REQUEST:  // An Incoming request is available
            PR_ASSERT(!"T_REQUEST Not implemented");
            return;

        case T_REPLY:    // An Incoming reply is available
            PR_ASSERT(!"T_REPLY Not implemented");
            return;

        case T_PASSCON:  // State is now T_DATAXFER
			// OTAccept() complete, receiving endpoint in T_DATAXFER state
			// cookie = OTAccept() resRef parameter
			break;

        case T_RESET:    // Protocol has been reset
            PR_ASSERT(!"T_RESET Not implemented");
			return;
            
// Async Completion Events
        case T_BINDCOMPLETE:
        case T_UNBINDCOMPLETE:
        case T_ACCEPTCOMPLETE:
        case T_OPTMGMTCOMPLETE:
        case T_GETPROTADDRCOMPLETE:
			thread = fd->secret->md.misc.thread;
            fd->secret->md.misc.thread    = NULL;
            fd->secret->md.misc.cookie    = cookie;
            break;

//      case T_OPENCOMPLETE:			// we open endpoints in synchronous mode
//      case T_REPLYCOMPLETE:
//      case T_DISCONNECTCOMPLETE:		// we don't call OTSndDisconnect()
//      case T_RESOLVEADDRCOMPLETE:
//      case T_GETINFOCOMPLETE:
//      case T_SYNCCOMPLETE:
//      case T_MEMORYRELEASED:			// only if OTAckSends() called on endpoint
//      case T_REGNAMECOMPLETE:
//      case T_DELNAMECOMPLETE:
//      case T_LKUPNAMECOMPLETE:
//      case T_LKUPNAMERESULT:
		// OpenTptInternet.h
//      case T_DNRSTRINGTOADDRCOMPLETE:	// DNS is handled by dnsContext in DNSNotifierRoutine()
//      case T_DNRADDRTONAMECOMPLETE:
//      case T_DNRSYSINFOCOMPLETE:
//      case T_DNRMAILEXCHANGECOMPLETE:
//      case T_DNRQUERYCOMPLETE:
        default:
        	// we should probably have a bit more sophisticated handling of kOTSystemSleep, etc.
			// PR_ASSERT(code != 0);
            return;
    }

	if (thread) {
		thread->md.osErrCode = result;
		if (_PR_MD_GET_INTSOFF()) {
			cpu->u.missed[cpu->where] |= _PR_MISSED_IO;
			thread->md.notifyPending = PR_TRUE;
			return;
		}
		DoneWaitingOnThisThread(thread);
	}
}


static OSErr CreateSocket(int type, EndpointRef *endpoint)
{
    OSStatus err;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    char *  configName;
    OTConfiguration *config;
    EndpointRef ep;

	// for now we just create the endpoint
	// we'll make it asynchronous and give it a notifier routine in _MD_makenonblock()

    switch (type){
        case SOCK_STREAM:   configName = kTCPName;  break;
        case SOCK_DGRAM:    configName = kUDPName;  break;
    }
    config = OTCreateConfiguration(configName);
    ep = OTOpenEndpoint(config, 0, NULL, &err);
    if (err != kOTNoError)
        goto ErrorExit;

    *endpoint = ep;
    PR_ASSERT(*endpoint != NULL);

    return kOTNoError;

ErrorExit:
    return err;
}


// Errors returned:
// kOTXXXX - OT returned error
// EPROTONOSUPPORT - bad socket type/protocol
// ENOBUFS - not enough space for another socket, or failure in socket creation routine
PRInt32 _MD_socket(int domain, int type, int protocol)
{
    OSStatus    err;
    EndpointRef endpoint;

    // We only deal with internet domain
    if (domain != AF_INET) {
        err = kEPROTONOSUPPORTErr;
        goto ErrorExit;
    }
    
    // We only know about tcp & udp
    if ((type != SOCK_STREAM) && (type != SOCK_DGRAM)) {
        err = kEPROTONOSUPPORTErr;
        goto ErrorExit;
    }
    
    // Convert default types to specific types.
    if (protocol == 0)  {
        if (type == SOCK_DGRAM)
            protocol = IPPROTO_UDP;
        else if (type == SOCK_STREAM)
            protocol = IPPROTO_TCP;
    }
    
    // Only support default protocol for tcp
    if ((type == SOCK_STREAM)  && (protocol != IPPROTO_TCP)) {
        err = kEPROTONOSUPPORTErr;
        goto ErrorExit;
    }
                
    // Only support default protocol for udp
    if ((type == SOCK_DGRAM)  && (protocol != IPPROTO_UDP)) {
        err = kEPROTONOSUPPORTErr;
        goto ErrorExit;
    }
        
    // Create a socket, we might run out of memory
    err = CreateSocket(type, &endpoint);
    if (err != kOTNoError)
        goto ErrorExit;

    PR_ASSERT((PRInt32)endpoint != -1);

    return ((PRInt32)endpoint);

ErrorExit:
    macsock_map_error(err);
    return -1;
}


// Errors:
// EBADF  -- bad socket id
// EFAULT -- bad address format
PRInt32 _MD_bind(PRFileDesc *fd, PRNetAddr *addr, PRUint32 addrlen)
{
    OSStatus err;
    EndpointRef endpoint = (EndpointRef) fd->secret->md.osfd;
    TBind bindReq;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRUint32 retryCount = 0;

    if (endpoint == NULL) {
        err = kEBADFErr;
        goto ErrorExit;
    }
        
    if (addr == NULL) {
        err = kEFAULTErr;
        goto ErrorExit;
    }
        
/*
 * There seems to be a bug with OT related to OTBind failing with kOTNoAddressErr even though
 * a proper legal address was supplied.  This happens very rarely and just retrying the
 * operation after a certain time (less than 1 sec. does not work) seems to succeed.
 */

TryAgain:
    // setup our request
    bindReq.addr.len = addrlen;
        
    bindReq.addr.maxlen = addrlen;
    bindReq.addr.buf = (UInt8*) addr;
    bindReq.qlen = 1;

	PR_Lock(fd->secret->md.miscLock);
    PrepareForAsyncCompletion(me, fd);
	fd->secret->md.misc.thread = me;

    err = OTBind(endpoint, &bindReq, NULL);
    if (err != kOTNoError) {
	    me->io_pending = PR_FALSE;
	    PR_Unlock(fd->secret->md.miscLock);
        goto ErrorExit;
	}

    WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);
	PR_Unlock(fd->secret->md.miscLock);

    err = me->md.osErrCode;
    if (err != kOTNoError)
        goto ErrorExit;

    return kOTNoError;

ErrorExit:
    if ((err == kOTNoAddressErr) && (++retryCount <= 4)) {
        long finalTicks;
    
        Delay(100,&finalTicks);
        goto TryAgain;
    }
    macsock_map_error(err);
    return -1;
}


// Errors:
// EBADF -- bad socket id
PRInt32 _MD_listen(PRFileDesc *fd, PRIntn backlog)
{
#if 0
    PRInt32 osfd = fd->secret->md.osfd;
    OSStatus err;
    EndpointRef endpoint = (EndpointRef) osfd;
    TBind bindReq;
    PRNetAddr addr;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (backlog == 0)
        backlog = 1;

    if (endpoint == NULL) {
        err = EBADF;
        goto ErrorExit;
    }
        
    addr.inet.port = addr.inet.ip = 0;

    bindReq.addr.maxlen = PR_NETADDR_SIZE (&addr);
    bindReq.addr.len = 0;
    bindReq.addr.buf = (UInt8*) &addr;
    bindReq.qlen = 0;
    
    PrepareForAsyncCompletion(me, osfd);    

    err = OTGetProtAddress(endpoint, &bindReq, NULL);
    if (err != kOTNoError)
        goto ErrorExit;

    WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);

    err = me->md.osErrCode;
    if (err != kOTNoError)
        goto ErrorExit;

    PrepareForAsyncCompletion(me, osfd);    

    err = OTUnbind(endpoint);
    if (err != kOTNoError)
        goto ErrorExit;

    WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);

    err = me->md.osErrCode;
    if (err != kOTNoError)
        goto ErrorExit;

    bindReq.qlen = backlog;
    
    PrepareForAsyncCompletion(me, osfd);    

    err = OTBind(endpoint, &bindReq, NULL);
    if (err != kOTNoError)
        goto ErrorExit;

    WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);

    err = me->md.osErrCode;
    if (err != kOTNoError)
        goto ErrorExit;

    PR_ASSERT(me->md.cookie == NULL);

    return kOTNoError;

ErrorExit:
    macsock_map_error(err);
    return -1;
#endif

#pragma unused (fd, backlog)    
    return kOTNoError;

}


// Errors:
// EBADF -- bad socket id
PRInt32 _MD_getsockname(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen)
{
    OSStatus err;
    EndpointRef endpoint = (EndpointRef) fd->secret->md.osfd;
    TBind bindReq;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (endpoint == NULL) {
        err = kEBADFErr;
        goto ErrorExit;
    }
        
    if (addr == NULL) {
        err = kEFAULTErr;
        goto ErrorExit;
    }

#if !defined(_PR_INET6)        
    addr->inet.family = AF_INET;
#endif
    
    PR_ASSERT(PR_NETADDR_SIZE(addr) >= (*addrlen));

    bindReq.addr.len = *addrlen;
    bindReq.addr.maxlen = *addrlen;
    bindReq.addr.buf = (UInt8*) addr;
    bindReq.qlen = 0;
    
	PR_Lock(fd->secret->md.miscLock);
    PrepareForAsyncCompletion(me, fd);    
	fd->secret->md.misc.thread = me;

    err = OTGetProtAddress(endpoint, &bindReq, NULL);
    if (err != kOTNoError) {
	    me->io_pending = PR_FALSE;
	    PR_Unlock(fd->secret->md.miscLock);
        goto ErrorExit;
	}

    WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);
	PR_Unlock(fd->secret->md.miscLock);

    err = me->md.osErrCode;
    if (err != kOTNoError)
        goto ErrorExit;

    return kOTNoError;

ErrorExit:
    macsock_map_error(err);
    return -1;
}


PRStatus _MD_getsockopt(PRFileDesc *fd, PRInt32 level, PRInt32 optname, char* optval, PRInt32* optlen)
{
    OSStatus err;
    EndpointRef endpoint = (EndpointRef) fd->secret->md.osfd;
    TOptMgmt cmd;
    TOption *opt;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    unsigned char optionBuffer[kOTOptionHeaderSize + sizeof(PRSocketOptionData)];
    
    if (endpoint == NULL) {
        err = kEBADFErr;
        goto ErrorExit;
    }
    
    /* 
    OT wants IPPROTO_IP for level and not XTI_GENERIC.  SO_REUSEADDR and SO_KEEPALIVE 
    are equated to IP level and TCP level options respectively and hence we need to set 
    the level correctly.
    */
    if (level == SOL_SOCKET) {
        if (optname == SO_REUSEADDR)
            level = IPPROTO_IP;
        else if (optname == SO_KEEPALIVE)
            level = INET_TCP;
    }

    opt = (TOption *)&optionBuffer[0];
    opt->len = sizeof(TOption);
    opt->level = level;
    opt->name = optname;
    opt->status = 0;
    
    cmd.opt.len = sizeof(TOption);
    cmd.opt.maxlen = sizeof(optionBuffer);
    cmd.opt.buf = (UInt8*)optionBuffer;
    cmd.flags = T_CURRENT;

	PR_Lock(fd->secret->md.miscLock);
    PrepareForAsyncCompletion(me, fd);    
	fd->secret->md.misc.thread = me;

    err = OTOptionManagement(endpoint, &cmd, &cmd);
    if (err != kOTNoError) {
	    me->io_pending = PR_FALSE;
	    PR_Unlock(fd->secret->md.miscLock);
        goto ErrorExit;
	}

    WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);
	PR_Unlock(fd->secret->md.miscLock);

    err = me->md.osErrCode;
    if (err != kOTNoError)
        goto ErrorExit;

    if (opt->status == T_FAILURE || opt->status == T_NOTSUPPORT){
        err = kEOPNOTSUPPErr;
        goto ErrorExit;
    }

    PR_ASSERT(opt->status == T_SUCCESS);

    switch (optname) {
        case SO_LINGER:
            *((t_linger*)optval) = *((t_linger*)&opt->value);
            *optlen = sizeof(t_linger);
            break;
        case SO_REUSEADDR:
        case TCP_NODELAY:
        case SO_KEEPALIVE:
        case SO_RCVBUF:
        case SO_SNDBUF:
            *((PRIntn*)optval) = *((PRIntn*)&opt->value);
            *optlen = sizeof(PRIntn);
            break;
        case IP_MULTICAST_LOOP:
            *((PRUint8*)optval) = *((PRIntn*)&opt->value);
            *optlen = sizeof(PRUint8);
            break;
        case IP_TTL:
            *((PRUintn*)optval) = *((PRUint8*)&opt->value);
            *optlen = sizeof(PRUintn);
            break;
        case IP_MULTICAST_TTL:
            *((PRUint8*)optval) = *((PRUint8*)&opt->value);
            *optlen = sizeof(PRUint8);
            break;
        case IP_ADD_MEMBERSHIP:
        case IP_DROP_MEMBERSHIP:
            {
            /* struct ip_mreq and TIPAddMulticast are the same size and optval 
               is pointing to struct ip_mreq */
            *((struct ip_mreq *)optval) = *((struct ip_mreq *)&opt->value);
            *optlen = sizeof(struct ip_mreq);
            break;
            }
        case IP_MULTICAST_IF:
            {
            *((PRUint32*)optval) = *((PRUint32*)&opt->value);
            *optlen = sizeof(PRUint32);
            break;
            }
        /*case IP_TOS:*/ /*IP_TOS has same value as TCP_MAXSEG */
        case TCP_MAXSEG:
            if (level == IPPROTO_TCP) { /* it is TCP_MAXSEG */
                *((PRIntn*)optval) = *((PRIntn*)&opt->value);
                *optlen = sizeof(PRIntn);
            } else { /* it is IP_TOS */
                *((PRUintn*)optval) = *((PRUint8*)&opt->value);
                *optlen = sizeof(PRUintn);
            }
            break;
        default:
            PR_ASSERT(0);
            break;    
    }
    
    return PR_SUCCESS;

ErrorExit:
    macsock_map_error(err);
    return PR_FAILURE;
}


PRStatus _MD_setsockopt(PRFileDesc *fd, PRInt32 level, PRInt32 optname, const char* optval, PRInt32 optlen)
{
    OSStatus err;
    EndpointRef endpoint = (EndpointRef) fd->secret->md.osfd;
    TOptMgmt cmd;
    TOption *opt;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    unsigned char optionBuffer[kOTOptionHeaderSize + sizeof(PRSocketOptionData) + 1];
    
    if (endpoint == NULL) {
        err = kEBADFErr;
        goto ErrorExit;
    }
    
    /* 
    OT wants IPPROTO_IP for level and not XTI_GENERIC.  SO_REUSEADDR and SO_KEEPALIVE 
    are equated to IP level and TCP level options respectively and hence we need to set 
    the level correctly.
    */
    if (level == SOL_SOCKET) {
        if (optname == SO_REUSEADDR)
            level = IPPROTO_IP;
        else if (optname == SO_KEEPALIVE)
            level = INET_TCP;
    }

    opt = (TOption *)&optionBuffer[0];
    opt->len = kOTOptionHeaderSize + optlen;

    /* special case adjustments for length follow */
    if (optname == SO_KEEPALIVE) /* we need to pass the timeout value for OT */
        opt->len = kOTOptionHeaderSize + sizeof(t_kpalive);
    if (optname == IP_MULTICAST_TTL || optname == IP_TTL) /* it is an unsigned char value */
        opt->len = kOTOneByteOptionSize;
    if (optname == IP_TOS && level == IPPROTO_IP)
        opt->len = kOTOneByteOptionSize;

    opt->level = level;
    opt->name = optname;
    opt->status = 0;
    
    cmd.opt.len = opt->len;
    cmd.opt.maxlen = sizeof(optionBuffer);
    cmd.opt.buf = (UInt8*)optionBuffer;
    
    optionBuffer[opt->len] = 0;
    
    cmd.flags = T_NEGOTIATE;

    switch (optname) {
        case SO_LINGER:
            *((t_linger*)&opt->value) = *((t_linger*)optval);
            break;
        case SO_REUSEADDR:
        case TCP_NODELAY:
        case SO_RCVBUF:
        case SO_SNDBUF:
            *((PRIntn*)&opt->value) = *((PRIntn*)optval);
            break;
        case IP_MULTICAST_LOOP:
            if (*optval != 0)
                opt->value[0] = T_YES;
            else
                opt->value[0] = T_NO;
            break;
        case SO_KEEPALIVE:
            {
            t_kpalive *kpalive = (t_kpalive *)&opt->value;
            
            kpalive->kp_onoff = *((long*)optval);
            kpalive->kp_timeout = 10; /* timeout in minutes */
            break;
            }
        case IP_TTL:
            *((unsigned char*)&opt->value) = *((PRUintn*)optval);
            break;
        case IP_MULTICAST_TTL:
            *((unsigned char*)&opt->value) = *optval;
            break;
        case IP_ADD_MEMBERSHIP:
        case IP_DROP_MEMBERSHIP:
            {
            /* struct ip_mreq and TIPAddMulticast are the same size and optval 
               is pointing to struct ip_mreq */
            *((TIPAddMulticast *)&opt->value) = *((TIPAddMulticast *)optval);
            break;
            }
        case IP_MULTICAST_IF:
            {
            *((PRUint32*)&opt->value) = *((PRUint32*)optval);
            break;
            }
        /*case IP_TOS:*/ /*IP_TOS has same value as TCP_MAXSEG */
        case TCP_MAXSEG:
            if (level == IPPROTO_TCP) { /* it is TCP_MAXSEG */
                *((PRIntn*)&opt->value) = *((PRIntn*)optval);
            } else { /* it is IP_TOS */
                *((unsigned char*)&opt->value) = *((PRUintn*)optval);
            }
            break;
        default:
            PR_ASSERT(0);
            break;    
    }
    
	PR_Lock(fd->secret->md.miscLock);
    PrepareForAsyncCompletion(me, fd);    
	fd->secret->md.misc.thread = me;

    err = OTOptionManagement(endpoint, &cmd, &cmd);
    if (err != kOTNoError) {
	    me->io_pending = PR_FALSE;
	    PR_Unlock(fd->secret->md.miscLock);
        goto ErrorExit;
	}

    WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);
	PR_Unlock(fd->secret->md.miscLock);

    err = me->md.osErrCode;
    if (err != kOTNoError)
        goto ErrorExit;

    if (opt->status == T_FAILURE || opt->status == T_NOTSUPPORT){
        err = kEOPNOTSUPPErr;
        goto ErrorExit;
    }
    
    if (level == IPPROTO_TCP && optname == TCP_MAXSEG && opt->status == T_READONLY) {
        err = kEOPNOTSUPPErr;
        goto ErrorExit;
    }

    PR_ASSERT(opt->status == T_SUCCESS);

    return PR_SUCCESS;

ErrorExit:
    macsock_map_error(err);
    return PR_FAILURE;
}


PRInt32 _MD_socketavailable(PRFileDesc *fd)
{
    PRInt32 osfd = fd->secret->md.osfd;
    OSStatus err;
    EndpointRef endpoint = (EndpointRef) osfd;
    size_t bytes;

    if (endpoint == NULL) {
        err = kEBADFErr;
        goto ErrorExit;
    }
    
    bytes = 0;
    
    err = OTCountDataBytes(endpoint, &bytes);
    if ((err == kOTLookErr) ||         // Not really errors, we just need to do a read,
        (err == kOTNoDataErr))        // or thereÕs nothing there.
        err = kOTNoError;
        
    if (err != kOTNoError)
        goto ErrorExit;
        
    return bytes;

ErrorExit:
    macsock_map_error(err);
    return -1;
}


PRInt32 _MD_accept(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen, PRIntervalTime timeout)
{
#pragma unused (fd, addr, addrlen, timeout)

    PR_ASSERT(!"_MD_accept Not implemented");
	return -1;

#if 0
    OSStatus err;
    EndpointRef endpoint = (EndpointRef) fd->secret->md.osfd;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    TBind bindReq;
    PRNetAddr bindAddr;
    PRInt32 newosfd = -1;
    EndpointRef newEndpoint;
    TCall call;
    PRNetAddr callAddr;

    if (endpoint == NULL) {
        err = kEBADFErr;
        goto ErrorExit;
    }
        
    memset(&call, 0 , sizeof(call));

    call.addr.maxlen = PR_NETADDR_SIZE(&callAddr);
    call.addr.len = PR_NETADDR_SIZE(&callAddr);
    call.addr.buf = (UInt8*) &callAddr;
    
    PrepareForAsyncCompletion(me, fd);    

    err = OTListen (endpoint, &call);
    if (err != kOTNoError && (err != kOTNoDataErr || fd->secret->nonblocking)) {
        me->io_pending = PR_FALSE;
        goto ErrorExit;
    }

    while (err == kOTNoDataErr) {
        WaitOnThisThread(me, timeout);
        err = me->md.osErrCode;
        if (err != kOTNoError)
            goto ErrorExit;

        PrepareForAsyncCompletion(me, fd);    

        err = OTListen (endpoint, &call);
        if (err == kOTNoError)
            break;

        PR_ASSERT(err == kOTNoDataErr);
    }        

    newosfd = _MD_socket(AF_INET, SOCK_STREAM, 0);
    if (newosfd == -1)
        return -1;

    newEndpoint = (EndpointRef)newosfd;
    
    // Bind to a local port; let the system assign it.

    bindAddr.inet.port = bindAddr.inet.ip = 0;

    bindReq.addr.maxlen = PR_NETADDR_SIZE (&bindAddr);
    bindReq.addr.len = 0;
    bindReq.addr.buf = (UInt8*) &bindAddr;
    bindReq.qlen = 0;
    
    PrepareThreadForAsyncIO(me, newosfd);    

    err = OTBind(newEndpoint, &bindReq, NULL);
    if (err != kOTNoError)
        goto ErrorExit;

    WaitOnThisThread(me, timeout);

    err = me->md.osErrCode;
    if (err != kOTNoError)
        goto ErrorExit;

    PrepareThreadForAsyncIO(me, newosfd);    

    err = OTAccept (endpoint, newEndpoint, &call);
    if (err != kOTNoError)
        goto ErrorExit;

    WaitOnThisThread(me, timeout);

    err = me->md.osErrCode;
    if (err != kOTNoError)
        goto ErrorExit;

    if (addr != NULL)
        *addr = callAddr;
    if (addrlen != NULL)
        *addrlen = call.addr.len;

    return newosfd;

ErrorExit:
    if (newosfd != -1)
        _MD_closesocket(newosfd);
    macsock_map_error(err);
    return -1;
#endif
}


PRInt32 _MD_connect(PRFileDesc *fd, PRNetAddr *addr, PRUint32 addrlen, PRIntervalTime timeout)
{
    OSStatus err;
    EndpointRef endpoint = (EndpointRef) fd->secret->md.osfd;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    TCall sndCall;
    TBind bindReq;
    PRNetAddr bindAddr;

    if (endpoint == NULL) {
        err = kEBADFErr;
        goto ErrorExit;
    }
        
    if (addr == NULL) {
        err = kEFAULTErr;
        goto ErrorExit;
    }
        
    // Bind to a local port; let the system assign it.

    bindAddr.inet.port = bindAddr.inet.ip = 0;

    bindReq.addr.maxlen = PR_NETADDR_SIZE (&bindAddr);
    bindReq.addr.len = 0;
    bindReq.addr.buf = (UInt8*) &bindAddr;
    bindReq.qlen = 0;
    
	PR_Lock(fd->secret->md.miscLock);
    PrepareForAsyncCompletion(me, fd);    
	fd->secret->md.misc.thread = me;

    err = OTBind(endpoint, &bindReq, NULL);
    if (err != kOTNoError) {
	    me->io_pending = PR_FALSE;
	    PR_Unlock(fd->secret->md.miscLock);
        goto ErrorExit;
	}

    WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);
	PR_Unlock(fd->secret->md.miscLock);

    err = me->md.osErrCode;
    if (err != kOTNoError)
        goto ErrorExit;

    memset(&sndCall, 0 , sizeof(sndCall));

    sndCall.addr.maxlen = addrlen;
    sndCall.addr.len = addrlen;
    sndCall.addr.buf = (UInt8*) addr;

	if (!fd->secret->nonblocking) {    
        PrepareForAsyncCompletion(me, fd);    
	    fd->secret->md.write.thread = me;
    }
	
    err = OTConnect (endpoint, &sndCall, NULL);
	if (err == kOTNoError) {
        PR_ASSERT(!"OTConnect returned kOTNoError in async mode!?!");	
	}
	if (fd->secret->nonblocking) {
		if (err == kOTNoDataErr)
			err = EINPROGRESS;
		goto ErrorExit;
	} else {
		if (err != kOTNoError && err != kOTNoDataErr) {
            me->io_pending = PR_FALSE;
            goto ErrorExit;
		}
	}
	
    WaitOnThisThread(me, timeout);

    err = me->md.osErrCode;
    if (err != kOTNoError)
        goto ErrorExit;

    return kOTNoError;

ErrorExit:
    macsock_map_error(err);
    return -1;
}


// Errors:
// EBADF -- bad socket id
// EFAULT -- bad buffer
static PRInt32 SendReceiveStream(PRFileDesc *fd, void *buf, PRInt32 amount, 
                               PRIntn flags, PRIntervalTime timeout, SndRcvOpCode opCode)
{
    OSStatus err;
    OTResult result;
    EndpointRef endpoint = (EndpointRef) fd->secret->md.osfd;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRInt32 bytesLeft = amount;

    PR_ASSERT(flags == 0);
    
    if (endpoint == NULL) {
        err = kEBADFErr;
        goto ErrorExit;
    }
        
    if (buf == NULL) {
        err = kEFAULTErr;
        goto ErrorExit;
    }
    
    if (opCode != kSTREAM_SEND && opCode != kSTREAM_RECEIVE) {
        err = kEINVALErr;
        goto ErrorExit;
    }
        
    while (bytesLeft > 0) {
    
        PrepareForAsyncCompletion(me, fd);    

        if (opCode == kSTREAM_SEND) {
			fd->secret->md.write.thread = me;
			fd->secret->md.writeReady = PR_FALSE;				// expect the worst
            result = OTSnd(endpoint, buf, bytesLeft, NULL);
			if (result != kOTFlowErr)							// hope for the best
				fd->secret->md.writeReady = PR_TRUE;
        } else {
			fd->secret->md.read.thread = me;
			fd->secret->md.readReady = PR_FALSE;				// expect the worst			
            result = OTRcv(endpoint, buf, bytesLeft, NULL);
			if (result != kOTNoDataErr)							// hope for the best
				fd->secret->md.readReady = PR_TRUE;
        }
		me->io_pending = PR_FALSE;

        if (result > 0) {
            buf = (void *) ( (UInt32) buf + (UInt32)result );
            bytesLeft -= result;
            if (opCode == kSTREAM_RECEIVE)
                return result;
        } else {
			switch (result) {
				case kOTOutStateErr:			// it has been closed
                    return 0;
				
				case kOTLookErr:
				    PR_ASSERT(!"call to OTLook() required after all.");
					break;
				
				case kOTFlowErr:
				case kOTNoDataErr:
				case kEAGAINErr:
				case kEWOULDBLOCKErr:
					if (fd->secret->nonblocking) {
						err = result;
						goto ErrorExit;
					}

					WaitOnThisThread(me, timeout);
					err = me->md.osErrCode;
					if (err != kOTNoError)
						goto ErrorExit;				
					break;
					
				default:
					err = result;
					goto ErrorExit;
			}
		}
    }

    return amount;

ErrorExit:
    macsock_map_error(err);
    return -1;
}                               


PRInt32 _MD_recv(PRFileDesc *fd, void *buf, PRInt32 amount, 
                               PRIntn flags, PRIntervalTime timeout)
{
    return (SendReceiveStream(fd, buf, amount, flags, timeout, kSTREAM_RECEIVE));
}                               


PRInt32 _MD_send(PRFileDesc *fd,const void *buf, PRInt32 amount, 
                               PRIntn flags, PRIntervalTime timeout)
{
    return (SendReceiveStream(fd, (void *)buf, amount, flags, timeout, kSTREAM_SEND));
}                               


// Errors:
// EBADF -- bad socket id
// EFAULT -- bad buffer
static PRInt32 SendReceiveDgram(PRFileDesc *fd, void *buf, PRInt32 amount, 
                               PRIntn flags, PRNetAddr *addr, PRUint32 *addrlen, 
                               PRIntervalTime timeout, SndRcvOpCode opCode)
{
    OSStatus err;
    EndpointRef endpoint = (EndpointRef) fd->secret->md.osfd;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRInt32 bytesLeft = amount;
    TUnitData dgram;
    OTResult	result;

    PR_ASSERT(flags == 0);
    
    if (endpoint == NULL) {
        err = kEBADFErr;
        goto ErrorExit;
    }
        
    if (buf == NULL || addr == NULL) {
        err = kEFAULTErr;
        goto ErrorExit;
    }
    
    if (opCode != kDGRAM_SEND && opCode != kDGRAM_RECEIVE) {
        err = kEINVALErr;
        goto ErrorExit;
    }
        
    memset(&dgram, 0 , sizeof(dgram));
    dgram.addr.maxlen = *addrlen;
    dgram.addr.len = *addrlen;
    dgram.addr.buf = (UInt8*) addr;
    dgram.udata.maxlen = amount;
    dgram.udata.len = amount;
    dgram.udata.buf = (UInt8*) buf;    

    while (bytesLeft > 0) {
    
        PrepareForAsyncCompletion(me, fd);    

        if (opCode == kDGRAM_SEND) {
			fd->secret->md.write.thread = me;
			fd->secret->md.writeReady = PR_FALSE;				// expect the worst
            err = OTSndUData(endpoint, &dgram);
			if (result != kOTFlowErr)							// hope for the best
				fd->secret->md.writeReady = PR_TRUE;
		} else {
			fd->secret->md.read.thread = me;
			fd->secret->md.readReady = PR_FALSE;				// expect the worst			
            err = OTRcvUData(endpoint, &dgram, NULL);
			if (result != kOTNoDataErr)							// hope for the best
				fd->secret->md.readReady = PR_TRUE;
		}

        if (err == kOTNoError) {
            buf = (void *) ( (UInt32) buf + (UInt32)dgram.udata.len );
            bytesLeft -= dgram.udata.len;
            dgram.udata.buf = (UInt8*) buf;    
            me->io_pending = PR_FALSE;
        } else {
            PR_ASSERT(err == kOTNoDataErr || err == kOTOutStateErr);
            WaitOnThisThread(me, timeout);
            err = me->md.osErrCode;
            if (err != kOTNoError)
                goto ErrorExit;
        }
    }

    if (opCode == kDGRAM_RECEIVE)
        *addrlen = dgram.addr.len;

    return amount;

ErrorExit:
    macsock_map_error(err);
    return -1;
}                               


PRInt32 _MD_recvfrom(PRFileDesc *fd, void *buf, PRInt32 amount, 
                               PRIntn flags, PRNetAddr *addr, PRUint32 *addrlen,
                               PRIntervalTime timeout)
{
    return (SendReceiveDgram(fd, buf, amount, flags, addr, addrlen,
                            timeout, kDGRAM_RECEIVE));
}                               


PRInt32 _MD_sendto(PRFileDesc *fd,const void *buf, PRInt32 amount, 
                               PRIntn flags, PRNetAddr *addr, PRUint32 addrlen,
                               PRIntervalTime timeout)
{
    return (SendReceiveDgram(fd, (void *)buf, amount, flags, addr, &addrlen,
                            timeout, kDGRAM_SEND));
}                               


PRInt32 _MD_closesocket(PRInt32 osfd)
{
    OSStatus err;
    EndpointRef endpoint = (EndpointRef) osfd;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (endpoint == NULL) {
        err = kEBADFErr;
        goto ErrorExit;
    }
        
    if (me->io_pending && me->io_fd == osfd)
        me->io_pending = PR_FALSE;

    (void) OTSndOrderlyDisconnect(endpoint);
    err = OTCloseProvider(endpoint);
    if (err != kOTNoError)
        goto ErrorExit;

    return kOTNoError;

ErrorExit:
    macsock_map_error(err);
    return -1;
}                               


PRInt32 _MD_writev(PRFileDesc *fd, const struct PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
#pragma unused (fd, iov, iov_size, timeout)

    PR_ASSERT(0);
    _PR_MD_CURRENT_THREAD()->md.osErrCode = unimpErr;
    return -1;
}                               


static PRBool GetState(PRFileDesc *fd, PRBool *readReady, PRBool *writeReady, PRBool *exceptReady)
{
    OTResult resultOT;
    
	*readReady = fd->secret->md.readReady;
	*exceptReady = fd->secret->md.exceptReady;

    resultOT = OTGetEndpointState((EndpointRef)fd->secret->md.osfd);
    switch (resultOT)    {
        case T_DATAXFER:
        case T_INREL:
            *writeReady = PR_TRUE;
            break;
        default:
            *writeReady = PR_FALSE;
    }
    
    return  *readReady || *writeReady || *exceptReady;
}


PRInt32 _MD_poll(PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
    PRInt32 ready = 0;
    PRPollDesc *pd, *epd;
    PRIntervalTime sleepTime, timein;
    
    sleepTime = PR_MillisecondsToInterval(5UL);
    if (PR_INTERVAL_NO_TIMEOUT != timeout)
    {
        if (sleepTime > timeout) sleepTime = timeout;
        timein = PR_IntervalNow();
    }

    do
    {
        for (pd = pds, epd = pd + npds; pd < epd; pd++)
        {
            PRInt16  in_flags_read = 0,  in_flags_write = 0;
            PRInt16 out_flags_read = 0, out_flags_write = 0;

            if (NULL == pd->fd || pd->in_flags == 0) continue;

            if (pd->in_flags & PR_POLL_READ)
            {
                in_flags_read = (pd->fd->methods->poll)(
                    pd->fd, pd->in_flags & ~PR_POLL_WRITE, &out_flags_read);
            }
            if (pd->in_flags & PR_POLL_WRITE)
            {
                in_flags_write = (pd->fd->methods->poll)(
                    pd->fd, pd->in_flags & ~PR_POLL_READ, &out_flags_write);
            }
            if ((0 != (in_flags_read & out_flags_read))
            || (0 != (in_flags_write & out_flags_write)))
            {
                ready += 1;  /* some layer has buffer input */
                pd->out_flags = out_flags_read | out_flags_write;
            }
            else
            {
                PRFileDesc *bottomFD;
                PRBool readReady, writeReady, exceptReady;

                pd->out_flags = 0;  /* pre-condition */
                bottomFD = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);
                PR_ASSERT(NULL != bottomFD);
                if ((NULL != bottomFD) && (_PR_FILEDESC_OPEN == bottomFD->secret->state))
                {
                    if (GetState(bottomFD, &readReady, &writeReady, &exceptReady))
                    {
                        if (readReady)
                        {
                            if (in_flags_read & PR_POLL_READ)
                                pd->out_flags |= PR_POLL_READ;
                            if (in_flags_write & PR_POLL_READ)
                                pd->out_flags |= PR_POLL_WRITE;
                        }
                        if (writeReady)
                        {
                            if (in_flags_read & PR_POLL_WRITE)
                                pd->out_flags |= PR_POLL_READ;
                            if (in_flags_write & PR_POLL_WRITE)
                                pd->out_flags |= PR_POLL_WRITE;
                        }
                        if (exceptReady && (pd->in_flags & PR_POLL_EXCEPT))
                        {
                            pd->out_flags |= PR_POLL_EXCEPT;
                        }
                        if (0 != pd->out_flags) ready++;
                    }
                }
                else
                {
                    ready += 1;  /* this will cause an abrupt return */
                    pd->out_flags = PR_POLL_NVAL;  /* bogii */
                }
            }
        }

        if (ready > 0) return ready;

        (void) PR_Sleep(sleepTime);

    } while ((timeout == PR_INTERVAL_NO_TIMEOUT) ||
           (((PRIntervalTime)(PR_IntervalNow() - timein)) < timeout));

    return 0; /* timed out */
}


void _MD_initfiledesc(PRFileDesc *fd)
{
	// Allocate a PR_Lock to arbitrate miscellaneous OT calls for this endpoint between threads
	// We presume that only one thread will be making Read calls (Recv/Accept) and that only
	// one thread will be making Write calls (Send/Connect) on the endpoint at a time.
	if (fd->methods->file_type == PR_DESC_SOCKET_TCP ||
		fd->methods->file_type == PR_DESC_SOCKET_UDP )
	{
		PR_ASSERT(fd->secret->md.miscLock == NULL);
		fd->secret->md.miscLock = PR_NewLock();
		PR_ASSERT(fd->secret->md.miscLock != NULL);
		fd->secret->md.connectionOpen = PR_FALSE;	// starts out closed
		fd->secret->md.readReady = PR_FALSE;		// let's not presume we have data ready to read
		fd->secret->md.writeReady = PR_TRUE;		// let's presume we can write unless we hear otherwise
		fd->secret->md.exceptReady = PR_FALSE;
	}
}


void _MD_freefiledesc(PRFileDesc *fd)
{
	if (fd->secret->md.miscLock)
	{
		PR_ASSERT(fd->methods->file_type == PR_DESC_SOCKET_TCP || fd->methods->file_type == PR_DESC_SOCKET_UDP);
		PR_DestroyLock(fd->secret->md.miscLock);
		fd->secret->md.miscLock = NULL;
	} else {
		PR_ASSERT(fd->methods->file_type != PR_DESC_SOCKET_TCP && PR_DESC_SOCKET_TCP != PR_DESC_SOCKET_UDP);
	}
}

void _MD_makenonblock(PRFileDesc *fd)
{
	// We simulate non-blocking mode using async mode rather
	// than put the endpoint in non-blocking mode.
	// We need to install the PRFileDesc as the contextPtr for the NotifierRoutine, but it
	// didn't exist at the time the endpoint was created.  It does now though...
	ProviderRef	endpointRef = (ProviderRef)fd->secret->md.osfd;
	OSStatus	err;
	
	// Install the PRFileDesc as the contextPtr for the Notifier function associated with this Endpoint.
	err = OTInstallNotifier(endpointRef, NotifierRoutine, fd);
	PR_ASSERT(err == kOTNoError);
	
	// Now that we have a NotifierRoutine installed, we can make the endpoint asynchronous
	err = OTSetAsynchronous(endpointRef);
	PR_ASSERT(err == kOTNoError);
}


PR_IMPLEMENT(PRInt32) _MD_shutdown(PRFileDesc *fd, PRIntn how)
{
#pragma unused (fd, how)

/* Just succeed silently!!! */
return (0);
}                               


PR_IMPLEMENT(PRStatus) _MD_getpeername(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen)
{
#pragma unused (fd, addr, addrlen)

    PR_ASSERT(0);
    _PR_MD_CURRENT_THREAD()->md.osErrCode = unimpErr;
    return PR_FAILURE;
}                               


PR_IMPLEMENT(unsigned long) inet_addr(const char *cp)
{
    OSStatus err;
    InetHost host;    

    err = OTInetStringToHost((char*) cp, &host);
    PR_ASSERT(err == kOTNoError);
    
    return host;
}


static char *sAliases[1] = {NULL};
static struct hostent sHostEnt = {NULL, &sAliases[0], AF_INET, sizeof (long), NULL};
static InetHostInfo sHostInfo;
static InetHost *sAddresses[kMaxHostAddrs+1];


PR_IMPLEMENT(struct hostent *) gethostbyname(const char * name)
{
    OSStatus err;
    PRUint32 index;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    me->io_pending       = PR_TRUE;
    me->io_fd            = NULL;
    me->md.osErrCode     = noErr;
	
	PR_Lock(dnsContext.lock);	// so we can safely store our thread ptr in dnsContext
	dnsContext.thread = me;		// so we know what thread to wake up when OTInetStringToAddress completes

    err = OTInetStringToAddress(dnsContext.serviceRef, (char *)name, &sHostInfo);
    if (err != kOTNoError) {
        me->io_pending = PR_FALSE;
        me->md.osErrCode = err;
        goto ErrorExit;
    }

    WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);
	PR_Unlock(dnsContext.lock);

    if (me->md.osErrCode != kOTNoError)
        goto ErrorExit;

    sHostEnt.h_name = sHostInfo.name;
    for (index=0; index<kMaxHostAddrs && sHostInfo.addrs[index] != NULL; index++)
        sAddresses[index] = &sHostInfo.addrs[index];
    sAddresses[index] = NULL;    
    sHostEnt.h_addr_list = (char **)sAddresses;

    return (&sHostEnt);

ErrorExit:
    return NULL;
}


PR_IMPLEMENT(struct hostent *) gethostbyaddr(const void *addr, int addrlen, int type)
{
    PR_ASSERT(type == AF_INET);
    PR_ASSERT(addrlen == sizeof(struct in_addr));

    OTInetHostToString((InetHost)addr, sHostInfo.name);
    
    return (gethostbyname(sHostInfo.name));
}


PR_IMPLEMENT(char *) inet_ntoa(struct in_addr addr)
{
    OTInetHostToString((InetHost)addr.s_addr, sHostInfo.name);
    
    return sHostInfo.name;
}


PRStatus _MD_gethostname(char *name, int namelen)
{
    OSStatus err;
    InetInterfaceInfo info;

    /*
     *    On a Macintosh, we don't have the concept of a local host name.
     *    We do though have an IP address & everyone should be happy with
     *     a string version of that for a name.
     *    The alternative here is to ping a local DNS for our name, they
     *    will often know it.  This is the cheap, easiest, and safest way out.
     */

    /* Make sure the string is as long as the longest possible address */
    if (namelen < strlen("123.123.123.123")) {
        err = kEINVALErr;
        goto ErrorExit;
    }

    err = OTInetGetInterfaceInfo(&info, kDefaultInetInterface);
    if (err != kOTNoError)
        goto ErrorExit;
    
    OTInetHostToString(info.fAddress, name);
    
    return PR_SUCCESS;

ErrorExit:
    macsock_map_error(err);
    return PR_FAILURE;
}


#define kIPName        "ip"
static struct protoent sIPProto = {kIPName, NULL, INET_IP};
static struct protoent sTCPProto = {kTCPName, NULL, INET_TCP};
static struct protoent sUDPProto = {kUDPName, NULL, INET_UDP};

PR_IMPLEMENT(struct protoent *) getprotobyname(const char * name)
{
    if (strcmp(name, kIPName) == 0)
        return (&sIPProto);
        
    if (strcmp(name, kTCPName) == 0)
        return (&sTCPProto);
        
    if (strcmp(name, kUDPName) == 0)
        return (&sUDPProto);
        
ErrorExit:
    macsock_map_error(kEINVALErr);
    return NULL;
}


PR_IMPLEMENT(struct protoent *) getprotobynumber(int number)
{
    if (number == INET_IP)
        return (&sIPProto);
        
    if (number == INET_TCP)
        return (&sTCPProto);
        
    if (number == INET_UDP)
        return (&sUDPProto);
        
ErrorExit:
    macsock_map_error(kEINVALErr);
    return NULL;
}


int _MD_mac_get_nonblocking_connect_error(PRInt32 osfd)
{
    OTResult resultOT;
    EndpointRef endpoint = (EndpointRef) osfd;

    resultOT = OTGetEndpointState(endpoint);
    switch (resultOT)    {
        case T_OUTCON:
            macsock_map_error(EINPROGRESS);
            return -1;
        case T_DATAXFER:
            return 0;
        case T_IDLE:
            return -1;
        default:
            PR_ASSERT(0);
            return -1;
    }
}
