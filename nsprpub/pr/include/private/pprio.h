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

/*
** File:	pprio.h
**
** Description:	Private definitions for I/O related structures
*/

#ifndef pprio_h___
#define pprio_h___

#include "prtypes.h"
#include "prio.h"

PR_BEGIN_EXTERN_C

/*
** File descriptors of the NSPR layer can be in one of the
** following states (stored in the 'state' field of struct
** PRFilePrivate):
** - _PR_FILEDESC_OPEN: The OS fd is open.
** - _PR_FILEDESC_CLOSED: The OS fd is closed.  The PRFileDesc
**   is still open but is unusable.  The only operation allowed
**   on the PRFileDesc is PR_Close().
** - _PR_FILEDESC_FREED: The OS fd is closed and the PRFileDesc
**   structure is freed.
*/

#define _PR_FILEDESC_OPEN       0xaaaaaaaa    /* 1010101... */
#define _PR_FILEDESC_CLOSED     0x55555555    /* 0101010... */
#define _PR_FILEDESC_FREED      0x11111111

/************************************************************************/
/************************************************************************/

/* Return the method tables for files, tcp sockets and udp sockets */
PR_EXTERN(const PRIOMethods*)    PR_GetFileMethods(void);
PR_EXTERN(const PRIOMethods*)    PR_GetTCPMethods(void);
PR_EXTERN(const PRIOMethods*)    PR_GetUDPMethods(void);

/*
** Convert a NSPR Socket Handle to a Native Socket handle.
** This function will be obsoleted with the next release; avoid using it.
*/
PR_EXTERN(PRInt32)      PR_FileDesc2NativeHandle(PRFileDesc *);
PR_EXTERN(void)         PR_ChangeFileDescNativeHandle(PRFileDesc *, PRInt32);
PR_EXTERN(PRFileDesc*)  PR_AllocFileDesc(PRInt32 osfd,
                                         const PRIOMethods *methods);
PR_EXTERN(void)         PR_FreeFileDesc(PRFileDesc *fd);
/*
** Import an existing OS file to NSPR. 
*/
PR_EXTERN(PRFileDesc*)  PR_ImportFile(PRInt32 osfd);
PR_EXTERN(PRFileDesc*)  PR_ImportTCPSocket(PRInt32 osfd);
PR_EXTERN(PRFileDesc*)  PR_ImportUDPSocket(PRInt32 osfd);


/*
 *************************************************************************
 * FUNCTION: PR_CreateSocketPollFd
 * DESCRIPTION:
 *     Create a PRFileDesc wrapper for a native socket handle, for use with
 *	   PR_Poll only
 * INPUTS:
 *     None
 * OUTPUTS:
 *     None
 * RETURN: PRFileDesc*
 *     Upon successful completion, PR_CreateSocketPollFd returns a pointer
 *     to the PRFileDesc created for the native socket handle
 *     Returns a NULL pointer if the create of a new PRFileDesc failed
 *
 **************************************************************************
 */

PR_EXTERN(PRFileDesc*)	PR_CreateSocketPollFd(PRInt32 osfd);

/*
 *************************************************************************
 * FUNCTION: PR_DestroySocketPollFd
 * DESCRIPTION:
 *     Destroy the PRFileDesc wrapper created by PR_CreateSocketPollFd
 * INPUTS:
 *     None
 * OUTPUTS:
 *     None
 * RETURN: PRFileDesc*
 *     Upon successful completion, PR_DestroySocketPollFd returns
 *	   PR_SUCCESS, else PR_FAILURE
 *
 **************************************************************************
 */

PR_EXTERN(PRStatus) PR_DestroySocketPollFd(PRFileDesc *fd);


/*
** Macros for PR_Socket
**
** Socket types: PR_SOCK_STREAM, PR_SOCK_DGRAM
*/

#ifdef WIN32

#define PR_SOCK_STREAM 1
#define PR_SOCK_DGRAM 2

#else /* WIN32 */

#define PR_SOCK_STREAM SOCK_STREAM
#define PR_SOCK_DGRAM SOCK_DGRAM

#endif /* WIN32 */

/*
** Create a new Socket; this function is obsolete.
*/
PR_EXTERN(PRFileDesc*)	PR_Socket(PRInt32 domain, PRInt32 type, PRInt32 proto);

/* FUNCTION: PR_LockFile
** DESCRIPTION:
**    Lock a file for exclusive access.
** RETURNS:
**    PR_SUCCESS when the lock is held
**    PR_FAILURE otherwise
*/
PR_EXTERN(PRStatus) PR_LockFile(PRFileDesc *fd);

/* FUNCTION: PR_TLockFile
** DESCRIPTION:
**    Test and Lock a file for exclusive access.  Do not block if the
**    file cannot be locked immediately.
** RETURNS:
**    PR_SUCCESS when the lock is held
**    PR_FAILURE otherwise
*/
PR_EXTERN(PRStatus) PR_TLockFile(PRFileDesc *fd);

/* FUNCTION: PR_UnlockFile
** DESCRIPTION:
**    Unlock a file which has been previously locked successfully by this
**    process.
** RETURNS:
**    PR_SUCCESS when the lock is released
**    PR_FAILURE otherwise
*/
PR_EXTERN(PRStatus) PR_UnlockFile(PRFileDesc *fd);

#ifdef WIN32
/* FUNCTION: PR_NTFast_AcceptRead
** DESCRIPTION:
**    NT has the notion of an "accept context", which is only needed in
**    order to make certain calls.  By default, a socket connected via
**    AcceptEx can only do a limited number of things without updating
**    the acceptcontext.  The generic version of PR_AcceptRead always
**    updates the accept context.  This version does not.
**/
PR_EXTERN(PRInt32) PR_NTFast_AcceptRead(PRFileDesc *sd, PRFileDesc **nd,
              PRNetAddr **raddr, void *buf, PRInt32 amount, PRIntervalTime t);

typedef void (*_PR_AcceptTimeoutCallback)(void *);

/* FUNCTION: PR_NTFast_AcceptRead_WithTimeoutCallback
** DESCRIPTION:
**    The AcceptEx call combines the accept with the read function.  However,
**    our daemon threads need to be able to wakeup and reliably flush their
**    log buffers if the Accept times out.  However, with the current blocking
**    interface to AcceptRead, there is no way for us to timeout the Accept;
**    this is because when we timeout the Read, we can close the newly 
**    socket and continue; but when we timeout the accept itself, there is no
**    new socket to timeout.  So instead, this version of the function is
**    provided.  After the initial timeout period elapses on the accept()
**    portion of the function, it will call the callback routine and then
**    continue the accept.   If the timeout occurs on the read, it will 
**    close the connection and return error.
*/
PR_EXTERN(PRInt32) PR_NTFast_AcceptRead_WithTimeoutCallback(
              PRFileDesc *sd, 
              PRFileDesc **nd,
              PRNetAddr **raddr, 
              void *buf, 
              PRInt32 amount, 
              PRIntervalTime t,
              _PR_AcceptTimeoutCallback callback, 
              void *callback_arg);

/* FUNCTION: PR_NTFast_Accept
** DESCRIPTION:
**    NT has the notion of an "accept context", which is only needed in
**    order to make certain calls.  By default, a socket connected via
**    AcceptEx can only do a limited number of things without updating
**    the acceptcontext.  The generic version of PR_Accept always
**    updates the accept context.  This version does not.
**/
PR_EXTERN(PRFileDesc*)	PR_NTFast_Accept(PRFileDesc *fd, PRNetAddr *addr,
                                                PRIntervalTime timeout);

/* FUNCTION: PR_NTFast_Update
** DESCRIPTION:
**    For sockets accepted with PR_NTFast_Accept or PR_NTFastAcceptRead,
**    this function will update the accept context for those sockets,
**    so that the socket can make general purpose socket calls.
**    Without calling this, the only operations supported on the socket
**    Are PR_Read, PR_Write, PR_Transmitfile, and PR_Close.
*/
PR_EXTERN(void) PR_NTFast_UpdateAcceptContext(PRFileDesc *acceptSock, 
                                        PRFileDesc *listenSock);


/* FUNCTION: PR_NT_UseNonblock
** DESCRIPTION:
**    This function only works on NT.  It tells nspr to use nonblocking io
**    rather than async IO.  There is no way to use some of each.  This 
**    function should be called immediately after calling PR_Init().
**
**    WARNING: THIS FUNCTION IS A TEMPORARY HACK AND WILL BE REMOVED SHORTLY
**    (LIKE ALL FUNCTIONS IN THE PRIVATE AREA).  DO NOT USE THIS FUNCTION AT
**    ALL WITHOUT CONTACTING mbelshe@netscape.com.
*/
PR_EXTERN(void) PR_NT_UseNonblock();

#endif /* WIN32 */

PR_END_EXTERN_C

#endif /* pprio_h___ */
