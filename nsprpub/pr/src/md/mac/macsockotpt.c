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

/*
	Since Apple put out new headers without
	putting in a way to test for them, we found some random symbol which
	isn't defined in the "1.1" headers.
*/
#include <OpenTransport.h>
#ifdef kOTInvalidStreamRef
/* old */
#define GESTALT_OPEN_TPT_PRESENT		gestaltOpenTptPresent
#define GESTALT_OPEN_TPT_TCP_PRESENT	gestaltOpenTptTCPPresent
#else
/* new */
#define GESTALT_OPEN_TPT_PRESENT		gestaltOpenTptPresentMask
#define GESTALT_OPEN_TPT_TCP_PRESENT	gestaltOpenTptTCPPresentMask
#endif

#include <OpenTptInternet.h>	// All the internet typedefs
#include "macsocket.h"
#include "primpl.h"

typedef enum SndRcvOpCode {
    kSTREAM_SEND,
    kSTREAM_RECEIVE	,
    kDGRAM_SEND,
    kDGRAM_RECEIVE
} SndRcvOpCode;


static InetSvcRef sSvcRef;

static pascal void  NotifierRoutine(void * contextPtr, OTEventCode code, 
			OTResult result, void * cookie);

static PRBool GetState(EndpointRef endpoint, PRBool *readReady, PRBool *writeReady, PRBool *exceptReady);

extern void WaitOnThisThread(PRThread *thread, PRIntervalTime timeout);
extern void DoneWaitingOnThisThread(PRThread *thread);

void _MD_InitNetAccess()
{
	OSErr		err;
	OSStatus    errOT;
	PRBool 		hasOTTCPIP = PR_FALSE;
	PRBool 		hasOT = PR_FALSE;
	long 		gestaltResult;
	PRThread *me = _PR_MD_CURRENT_THREAD();
	
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

	sSvcRef = OTOpenInternetServices(kDefaultInternetServicesPath, NULL, &errOT);
	if (errOT != kOTNoError) return;	/* no network -- oh well */
	PR_ASSERT((sSvcRef != NULL) && (errOT == kOTNoError));

    /* Install notify function for DNR Address To String completion */
	errOT = OTInstallNotifier(sSvcRef, NotifierRoutine, me);
	PR_ASSERT(errOT == kOTNoError);

    /* Put us into async mode */
	errOT = OTSetAsynchronous(sSvcRef);
	PR_ASSERT(errOT == kOTNoError);

/* XXX Does not handle absence of open tpt and tcp yet! */
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

static void PrepareThreadForAsyncIO(PRThread *thread, EndpointRef endpoint, PRInt32 osfd)
{
	OSStatus err;

    thread->io_pending = PR_TRUE;
    thread->io_fd = osfd;
    thread->md.osErrCode = noErr;

	OTRemoveNotifier(endpoint);
	err = OTInstallNotifier(endpoint, NotifierRoutine, thread);
	PR_ASSERT(err == kOTNoError);
}

// Notification routine
// Async callback routine.
// A5 is OK. Cannot allocate memory here
pascal void  NotifierRoutine(void * contextPtr, OTEventCode code, OTResult result, void * cookie)
{
	PRThread * thread = (PRThread *) contextPtr;
    _PRCPU *cpu = _PR_MD_CURRENT_CPU();	
	
	switch (code)
	{
// Async Completion Event
		case T_OPENCOMPLETE:
		case T_BINDCOMPLETE:
		case T_UNBINDCOMPLETE:
		case T_GETPROTADDRCOMPLETE:
		case T_ACCEPTCOMPLETE:
// Connect callback
		case T_CONNECT:
// Standard or expedited data is available
		case T_DATA:
		case T_EXDATA:
// Standard or expedited data Flow control lifted
		case T_GODATA:
		case T_GOEXDATA:
// Asynchronous Listen Event
		case T_LISTEN:
// DNR String To Address Complete Event
		case T_DNRSTRINGTOADDRCOMPLETE:
// Option Management Request Complete Event
		case T_OPTMGMTCOMPLETE:
			thread->md.osErrCode = result;
			thread->md.cookie = cookie;
		    if (_PR_MD_GET_INTSOFF()) {
		        cpu->u.missed[cpu->where] |= _PR_MISSED_IO;
		        thread->md.notifyPending = PR_TRUE;
				return;
		    }
			DoneWaitingOnThisThread(thread);
			break;

// T_ORDREL orderly release is available;  nothing to do
		case T_ORDREL:
			break;

// T_PASSCON; nothing to do
		case T_PASSCON:
			break;

// T_DISCONNECT; disconnect is available; nothing to do
		case T_DISCONNECT:
			break;

// UDP Send error; clear the error
		case T_UDERR:
			(void) OTRcvUDErr((EndpointRef) cookie, NULL);
			break;
			
		default:
			PR_ASSERT(0);
			break;
	}
}


static OSErr CreateSocket(int type, EndpointRef *endpoint)
{
	OSStatus err;
	PRThread *me = _PR_MD_CURRENT_THREAD();

	switch (type){
		case SOCK_STREAM:
			err = OTAsyncOpenEndpoint(OTCreateConfiguration(kTCPName), 0, NULL, 
					NotifierRoutine, me);
			break;
		case SOCK_DGRAM:
			err = OTAsyncOpenEndpoint(OTCreateConfiguration(kUDPName), 0, NULL,
					NotifierRoutine, me);
			break;
	}
	if (err != kOTNoError)
		goto ErrorExit;

	WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);

	err = me->md.osErrCode;
	if (err != kOTNoError)
		goto ErrorExit;

	*endpoint = me->md.cookie;
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
	OSStatus	err;
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
// EBADF -- bad socket id
// EFAULT -- bad address format
PRInt32 _MD_bind(PRFileDesc *fd, PRNetAddr *addr, PRUint32 addrlen)
{
	PRInt32 osfd = fd->secret->md.osfd;
	OSStatus err;
	EndpointRef endpoint = (EndpointRef) osfd;
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
		
	// setup our request
#if 0
	if ((addr->inet.port == 0) || (addr->inet.ip == 0))
		bindReq.addr.len = 0;
	else
#endif
/*
 * There seems to be a bug with OT ralted to OTBind failing with kOTNoAddressErr eventhough
 * a proper legal address was supplied.  This happens very rarely and just retrying the
 * operation after a certain time (less than 1 sec. does not work) seems to succeed.
 */

TryAgain:
	bindReq.addr.len = addrlen;
		
	bindReq.addr.maxlen = addrlen;
	bindReq.addr.buf = (UInt8*) addr;
	bindReq.qlen = 1;
	
	PrepareThreadForAsyncIO(me, endpoint, osfd);    

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
	
	PrepareThreadForAsyncIO(me, endpoint, osfd);    

	err = OTGetProtAddress(endpoint, &bindReq, NULL);
	if (err != kOTNoError)
		goto ErrorExit;

	WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);

	err = me->md.osErrCode;
	if (err != kOTNoError)
		goto ErrorExit;

	PrepareThreadForAsyncIO(me, endpoint, osfd);    

	err = OTUnbind(endpoint);
	if (err != kOTNoError)
		goto ErrorExit;

	WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);

	err = me->md.osErrCode;
	if (err != kOTNoError)
		goto ErrorExit;

	bindReq.qlen = backlog;
	
	PrepareThreadForAsyncIO(me, endpoint, osfd);    

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
	PRInt32 osfd = fd->secret->md.osfd;
	OSStatus err;
	EndpointRef endpoint = (EndpointRef) osfd;
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
	
	PrepareThreadForAsyncIO(me, endpoint, osfd);    

	err = OTGetProtAddress(endpoint, &bindReq, NULL);
	if (err != kOTNoError)
		goto ErrorExit;

	WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);

	err = me->md.osErrCode;
	if (err != kOTNoError)
		goto ErrorExit;

	PR_ASSERT(me->md.cookie == &bindReq);

	return kOTNoError;

ErrorExit:
	macsock_map_error(err);
    return -1;
}

PRStatus _MD_getsockopt(PRFileDesc *fd, PRInt32 level, PRInt32 optname, char* optval, PRInt32* optlen)
{
	OSStatus err;
	PRInt32 osfd = fd->secret->md.osfd;
	EndpointRef endpoint = (EndpointRef) osfd;
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

	PrepareThreadForAsyncIO(me, endpoint, osfd);    

	err = OTOptionManagement(endpoint, &cmd, &cmd);
	if (err != kOTNoError)
		goto ErrorExit;

	WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);

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
	PRInt32 osfd = fd->secret->md.osfd;
	EndpointRef endpoint = (EndpointRef) osfd;
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
	
	PrepareThreadForAsyncIO(me, endpoint, osfd);    

	err = OTOptionManagement(endpoint, &cmd, &cmd);
	if (err != kOTNoError)
		goto ErrorExit;

	WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);

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
	if ((err == kOTLookErr) || 		// Not really errors, we just need to do a read,
	    (err == kOTNoDataErr))		// or thereÕs nothing there.
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
	PRInt32 osfd = fd->secret->md.osfd;
	OSStatus err;
	EndpointRef endpoint = (EndpointRef) osfd;
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
	
	PrepareThreadForAsyncIO(me, endpoint, osfd);    

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

		PrepareThreadForAsyncIO(me, endpoint, osfd);    

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
	
	PrepareThreadForAsyncIO(me, newEndpoint, newosfd);    

	err = OTBind(newEndpoint, &bindReq, NULL);
	if (err != kOTNoError)
		goto ErrorExit;

	WaitOnThisThread(me, timeout);

	err = me->md.osErrCode;
	if (err != kOTNoError)
		goto ErrorExit;

	PrepareThreadForAsyncIO(me, endpoint, newosfd);    

	err = OTAccept (endpoint, newEndpoint, &call);
	if (err != kOTNoError)
		goto ErrorExit;

	WaitOnThisThread(me, timeout);

	err = me->md.osErrCode;
	if (err != kOTNoError)
		goto ErrorExit;

	PR_ASSERT(me->md.cookie != NULL);

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
}

PRInt32 _MD_connect(PRFileDesc *fd, PRNetAddr *addr, PRUint32 addrlen, PRIntervalTime timeout)
{
	PRInt32 osfd = fd->secret->md.osfd;
	OSStatus err;
	EndpointRef endpoint = (EndpointRef) osfd;
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
	
	PrepareThreadForAsyncIO(me, endpoint, osfd);    

	err = OTBind(endpoint, &bindReq, NULL);
	if (err != kOTNoError)
		goto ErrorExit;

	WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);

	err = me->md.osErrCode;
	if (err != kOTNoError)
		goto ErrorExit;

	memset(&sndCall, 0 , sizeof(sndCall));

	sndCall.addr.maxlen = addrlen;
	sndCall.addr.len = addrlen;
	sndCall.addr.buf = (UInt8*) addr;
	
	PrepareThreadForAsyncIO(me, endpoint, osfd);    

	err = OTConnect (endpoint, &sndCall, NULL);
	if (err != kOTNoError && err != kOTNoDataErr)
		goto ErrorExit;
	if (err == kOTNoDataErr && fd->secret->nonblocking) {
		err = kEINPROGRESSErr;
    	me->io_pending = PR_FALSE;
		goto ErrorExit;
	}

	WaitOnThisThread(me, timeout);

	err = me->md.osErrCode;
	if (err != kOTNoError)
		goto ErrorExit;

	PR_ASSERT(me->md.cookie != NULL);

	err = OTRcvConnect(endpoint, NULL);
	PR_ASSERT(err == kOTNoError);

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
	PRInt32 osfd = fd->secret->md.osfd;
	EndpointRef endpoint = (EndpointRef) osfd;
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
	
		PrepareThreadForAsyncIO(me, endpoint, osfd);    

		if (opCode == kSTREAM_SEND)
			result = OTSnd(endpoint, buf, bytesLeft, NULL);
		else
			result = OTRcv(endpoint, buf, bytesLeft, NULL);

		if (result > 0) {
			buf = (void *) ( (UInt32) buf + (UInt32)result );
			bytesLeft -= result;
    		me->io_pending = PR_FALSE;
    		if (opCode == kSTREAM_RECEIVE)
    			return result;
		} else {
			if (result == kOTOutStateErr) { /* it has been closed */
				me->io_pending = PR_FALSE;
				return 0;
			}
			if (result == kOTLookErr) {
				PRBool readReady,writeReady,exceptReady;
				/* process the event and then continue the operation */
				(void) GetState(endpoint, &readReady, &writeReady, &exceptReady);
				continue;
			}
			if (result != kOTNoDataErr && result != kOTFlowErr && 
			    result != kEAGAINErr && result != kEWOULDBLOCKErr) {
			    me->io_pending = PR_FALSE;
				err = result;
				goto ErrorExit;
			} else if (fd->secret->nonblocking) {
    			me->io_pending = PR_FALSE;
				err = result;
				goto ErrorExit;
			}
			WaitOnThisThread(me, timeout);
			me->io_pending = PR_FALSE;
			err = me->md.osErrCode;
			if (err != kOTNoError)
				goto ErrorExit;

			PR_ASSERT(me->md.cookie != NULL);
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
	PRInt32 osfd = fd->secret->md.osfd;
	EndpointRef endpoint = (EndpointRef) osfd;
	PRThread *me = _PR_MD_CURRENT_THREAD();
	PRInt32 bytesLeft = amount;
	TUnitData dgram;

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
	
		PrepareThreadForAsyncIO(me, endpoint, osfd);    

		if (opCode == kDGRAM_SEND)
			err = OTSndUData(endpoint, &dgram);
		else
			err = OTRcvUData(endpoint, &dgram, NULL);

		if (err == kOTNoError) {
			buf = (void *) ( (UInt32) buf + (UInt32)dgram.udata.len );
			bytesLeft -= dgram.udata.len;
			dgram.udata.buf = (UInt8*) buf;	
    		me->io_pending = PR_FALSE;
		}
		else {
			PR_ASSERT(err == kOTNoDataErr || err == kOTOutStateErr);
			WaitOnThisThread(me, timeout);
			me->io_pending = PR_FALSE;
			err = me->md.osErrCode;
			if (err != kOTNoError)
				goto ErrorExit;

			PR_ASSERT(me->md.cookie != NULL);
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

#if 0
	{
	OTResult state;
	state = OTGetEndpointState(endpoint);
	
	err = OTSndOrderlyDisconnect(endpoint);
	if (err != kOTNoError && err != kOTOutStateErr)
		goto ErrorExit;

	state = OTGetEndpointState(endpoint);
	
	err = OTUnbind(endpoint);
	if (err != kOTNoError && err != kOTOutStateErr)
		goto ErrorExit;

	state = OTGetEndpointState(endpoint);

	err = OTSetSynchronous(endpoint);
	if (err != kOTNoError)
		goto ErrorExit;

	err = OTSetBlocking(endpoint);
	if (err != kOTNoError)
		goto ErrorExit;
	}
#endif

	(void) OTSndOrderlyDisconnect(endpoint);

	err = OTCloseProvider(endpoint);
	if (err != kOTNoError)
		goto ErrorExit;

	return kOTNoError;

ErrorExit:
	macsock_map_error(err);
    return -1;
}                               

PRInt32 _MD_writev(PRFileDesc *fd, struct PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
#pragma unused (fd, iov, iov_size, timeout)

	PR_ASSERT(0);
	_PR_MD_CURRENT_THREAD()->md.osErrCode = unimpErr;
    return -1;
}                               

static PRBool GetState(EndpointRef endpoint, PRBool *readReady, PRBool *writeReady, PRBool *exceptReady)
{
	OSStatus err;
	OTResult resultOT;
	TDiscon discon;
	PRBool result = PR_FALSE;
	
	*readReady = *writeReady = *exceptReady = PR_FALSE;

	resultOT = OTLook(endpoint);
	switch (resultOT) {
		case T_DATA:
		case T_LISTEN:
			*readReady = PR_TRUE;
			break;
		case T_CONNECT:
			err = OTRcvConnect(endpoint, NULL);
			PR_ASSERT(err == kOTNoError);
			break;		
		case T_DISCONNECT:
			memset(&discon, 0 , sizeof(discon));
			err = OTRcvDisconnect(endpoint, &discon);
			PR_ASSERT(err == kOTNoError);
			macsock_map_error(discon.reason);
			*exceptReady = PR_TRUE;
			break;		
		case T_ORDREL:
			*readReady = PR_TRUE;
			err = OTRcvOrderlyDisconnect(endpoint);
			PR_ASSERT(err == kOTNoError);
			break;
	}
	resultOT = OTGetEndpointState(endpoint);
	switch (resultOT)	{
		case T_DATAXFER:
		case T_INREL:
			*writeReady = PR_TRUE;
			break;
		default:
			*writeReady = PR_FALSE;
	}
	
	if ((*readReady == PR_TRUE) || (*writeReady==PR_TRUE) || (*exceptReady==PR_TRUE))
		result = PR_TRUE;

	return result;
}

PRInt32 _MD_poll(PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
    PRPollDesc *pd, *epd;
    PRInt32 n = 0;
	PRThread *me = _PR_MD_CURRENT_THREAD();
	PRIntervalTime sleepTime;
    PRIntervalTime timein = PR_IntervalNow();
	
	sleepTime = PR_MillisecondsToInterval(5UL);
	if (sleepTime > timeout)
		sleepTime = timeout;

	do {

    for (pd = pds, epd = pd + npds; pd < epd; pd++) {
        PRInt32 osfd;
        PRInt16 in_flags = pd->in_flags;
        PRFileDesc *bottom = pd->fd;
		EndpointRef endpoint;
		PRInt16 out_flags = 0;
		PRBool readReady, writeReady, exceptReady;

		pd->out_flags = 0;
        if (NULL == bottom || in_flags == 0) {
            continue;
        }
        while (bottom->lower != NULL) {
            bottom = bottom->lower;
        }
        osfd = bottom->secret->md.osfd;
		endpoint = (EndpointRef) osfd;

		if (GetState(endpoint, &readReady, &writeReady, &exceptReady)) {
		
			if ((in_flags & PR_POLL_READ) && (readReady))  {
				out_flags |= PR_POLL_READ;
			}
			if ((in_flags & PR_POLL_WRITE) && (writeReady)) {
				out_flags |= PR_POLL_WRITE;
			}
			if ((in_flags & PR_POLL_EXCEPT) && (exceptReady)) {
				out_flags |= PR_POLL_EXCEPT;
			}
			pd->out_flags = out_flags;
			if (out_flags) {
				n++;
			}
		}
    }

	if (n > 0)
		return n;

	(void) PR_Sleep(sleepTime);

	} while ((timeout == PR_INTERVAL_NO_TIMEOUT) ||
		   (((PRIntervalTime)(PR_IntervalNow() - timein)) < timeout));

	return 0; /* timed out */
}

void _MD_makenonblock(PRFileDesc *fd)
{
	OSStatus err;
	PRInt32 osfd = fd->secret->md.osfd;
	EndpointRef endpoint = (EndpointRef) osfd;

    err = OTSetNonBlocking(endpoint);
	PR_ASSERT(err == kOTNoError || err == kOTOutStateErr);
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

	PrepareThreadForAsyncIO(me, sSvcRef, NULL);    

    err = OTInetStringToAddress(sSvcRef, (char *)name, &sHostInfo);
	if (err != kOTNoError) {
		me->io_pending = PR_FALSE;
		me->md.osErrCode = err;
		goto ErrorExit;
	}

	WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);

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
	 *	On a Macintosh, we donÕt have the concept of a local host name.
	 *	We do though have an IP address & everyone should be happy with
	 * 	a string version of that for a name.
	 *	The alternative here is to ping a local DNS for our name, they
	 *	will often know it.  This is the cheap, easiest, and safest way out.
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


#define kIPName		"ip"
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
	switch (resultOT)	{
		case T_OUTCON:
			macsock_map_error(kEINPROGRESSErr);
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
