/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
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

/************************************************************************/
/************************************************************************/

/* Return the method tables for files, tcp sockets and udp sockets */
NSPR_API(const PRIOMethods*)    PR_GetFileMethods(void);
NSPR_API(const PRIOMethods*)    PR_GetTCPMethods(void);
NSPR_API(const PRIOMethods*)    PR_GetUDPMethods(void);
NSPR_API(const PRIOMethods*)    PR_GetPipeMethods(void);

/*
** Convert a NSPR Socket Handle to a Native Socket handle.
** This function will be obsoleted with the next release; avoid using it.
*/
NSPR_API(PRInt32)      PR_FileDesc2NativeHandle(PRFileDesc *);
NSPR_API(void)         PR_ChangeFileDescNativeHandle(PRFileDesc *, PRInt32);
NSPR_API(PRFileDesc*)  PR_AllocFileDesc(PRInt32 osfd,
                                         const PRIOMethods *methods);
NSPR_API(void)         PR_FreeFileDesc(PRFileDesc *fd);
/*
** Import an existing OS file to NSPR. 
*/
NSPR_API(PRFileDesc*)  PR_ImportFile(PRInt32 osfd);
NSPR_API(PRFileDesc*)  PR_ImportPipe(PRInt32 osfd);
NSPR_API(PRFileDesc*)  PR_ImportTCPSocket(PRInt32 osfd);
NSPR_API(PRFileDesc*)  PR_ImportUDPSocket(PRInt32 osfd);


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

NSPR_API(PRFileDesc*)	PR_CreateSocketPollFd(PRInt32 osfd);

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

NSPR_API(PRStatus) PR_DestroySocketPollFd(PRFileDesc *fd);


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
NSPR_API(PRFileDesc*)	PR_Socket(PRInt32 domain, PRInt32 type, PRInt32 proto);

/* FUNCTION: PR_LockFile
** DESCRIPTION:
**    Lock a file for exclusive access.
** RETURNS:
**    PR_SUCCESS when the lock is held
**    PR_FAILURE otherwise
*/
NSPR_API(PRStatus) PR_LockFile(PRFileDesc *fd);

/* FUNCTION: PR_TLockFile
** DESCRIPTION:
**    Test and Lock a file for exclusive access.  Do not block if the
**    file cannot be locked immediately.
** RETURNS:
**    PR_SUCCESS when the lock is held
**    PR_FAILURE otherwise
*/
NSPR_API(PRStatus) PR_TLockFile(PRFileDesc *fd);

/* FUNCTION: PR_UnlockFile
** DESCRIPTION:
**    Unlock a file which has been previously locked successfully by this
**    process.
** RETURNS:
**    PR_SUCCESS when the lock is released
**    PR_FAILURE otherwise
*/
NSPR_API(PRStatus) PR_UnlockFile(PRFileDesc *fd);

/*
** Emulate acceptread by accept and recv.
*/
NSPR_API(PRInt32) PR_EmulateAcceptRead(PRFileDesc *sd, PRFileDesc **nd,
    PRNetAddr **raddr, void *buf, PRInt32 amount, PRIntervalTime timeout);

/*
** Emulate sendfile by reading from the file and writing to the socket.
** The file is memory-mapped if memory-mapped files are supported.
*/
NSPR_API(PRInt32) PR_EmulateSendFile(
    PRFileDesc *networkSocket, PRSendFileData *sendData,
    PRTransmitFileFlags flags, PRIntervalTime timeout);

#ifdef WIN32
/* FUNCTION: PR_NTFast_AcceptRead
** DESCRIPTION:
**    NT has the notion of an "accept context", which is only needed in
**    order to make certain calls.  By default, a socket connected via
**    AcceptEx can only do a limited number of things without updating
**    the acceptcontext.  The generic version of PR_AcceptRead always
**    updates the accept context.  This version does not.
**/
NSPR_API(PRInt32) PR_NTFast_AcceptRead(PRFileDesc *sd, PRFileDesc **nd,
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
NSPR_API(PRInt32) PR_NTFast_AcceptRead_WithTimeoutCallback(
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
NSPR_API(PRFileDesc*)	PR_NTFast_Accept(PRFileDesc *fd, PRNetAddr *addr,
                                                PRIntervalTime timeout);

/* FUNCTION: PR_NTFast_Update
** DESCRIPTION:
**    For sockets accepted with PR_NTFast_Accept or PR_NTFastAcceptRead,
**    this function will update the accept context for those sockets,
**    so that the socket can make general purpose socket calls.
**    Without calling this, the only operations supported on the socket
**    Are PR_Read, PR_Write, PR_Transmitfile, and PR_Close.
*/
NSPR_API(void) PR_NTFast_UpdateAcceptContext(PRFileDesc *acceptSock, 
                                        PRFileDesc *listenSock);


/* FUNCTION: PR_NT_CancelIo
** DESCRIPTION:
**    Cancel IO operations on fd.
*/
NSPR_API(PRStatus) PR_NT_CancelIo(PRFileDesc *fd);


#endif /* WIN32 */

/*
** Need external access to this on Mac so we can first set up our faux
** environment vars
*/
#ifdef XP_MAC
NSPR_API(void) PR_Init_Log(void);
#endif


PR_END_EXTERN_C

#endif /* pprio_h___ */
