/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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
** A collection of things thought to be obsolete
*/

#if defined(PROBSLET_H)
#else
#define PROBSLET_H

#include "prio.h"

PR_BEGIN_EXTERN_C

/*
** Yield the current thread.  The proper function to use in place of
** PR_Yield() is PR_Sleep() with an argument of PR_INTERVAL_NO_WAIT.
*/
PR_EXTERN(PRStatus) PR_Yield(void);

/*
** These are obsolete and are replaced by PR_GetSocketOption() and
** PR_SetSocketOption().
*/

PR_EXTERN(PRStatus) PR_GetSockOpt(
    PRFileDesc *fd, PRSockOption optname, void* optval, PRInt32* optlen);

PR_EXTERN(PRStatus) PR_SetSockOpt(
    PRFileDesc *fd, PRSockOption optname, const void* optval, PRInt32 optlen);

/************************************************************************/
/************* The following definitions are for select *****************/
/************************************************************************/

/*
** The following is obsolete and will be deleted in the next release!
** These are provided for compatibility, but are GUARANTEED to be slow.
**
** Override PR_MAX_SELECT_DESC if you need more space in the select set.
*/
#ifndef PR_MAX_SELECT_DESC
#define PR_MAX_SELECT_DESC 1024
#endif
typedef struct PR_fd_set {
    PRUint32      hsize;
    PRFileDesc   *harray[PR_MAX_SELECT_DESC];
    PRUint32      nsize;
    PRInt32       narray[PR_MAX_SELECT_DESC];
} PR_fd_set;

/*
*************************************************************************
** FUNCTION:    PR_Select
** DESCRIPTION:
**
** The call returns as soon as I/O is ready on one or more of the underlying
** file/socket descriptors or an exceptional condition is pending. A count of the 
** number of ready descriptors is returned unless a timeout occurs in which case 
** zero is returned.  On return, PR_Select replaces the given descriptor sets with 
** subsets consisting of those descriptors that are ready for the requested condition.
** The total number of ready descriptors in all the sets is the return value.
**
** INPUTS:
**   PRInt32 num             
**       This argument is unused but is provided for select(unix) interface
**       compatability.  All input PR_fd_set arguments are self-describing
**       with its own maximum number of elements in the set.
**                               
**   PR_fd_set *readfds
**       A set describing the io descriptors for which ready for reading
**       condition is of interest.  
**                               
**   PR_fd_set *writefds
**       A set describing the io descriptors for which ready for writing
**       condition is of interest.  
**                               
**   PR_fd_set *exceptfds
**       A set describing the io descriptors for which exception pending
**       condition is of interest.  
**
**   Any of the above readfds, writefds or exceptfds may be given as NULL 
**   pointers if no descriptors are of interest for that particular condition.                          
**   
**   PRIntervalTime timeout  
**       Amount of time the call will block waiting for I/O to become ready. 
**       If this time expires without any I/O becoming ready, the result will
**       be zero.
**
** OUTPUTS:    
**   PR_fd_set *readfds
**       A set describing the io descriptors which are ready for reading.
**                               
**   PR_fd_set *writefds
**       A set describing the io descriptors which are ready for writing.
**                               
**   PR_fd_set *exceptfds
**       A set describing the io descriptors which have pending exception.
**
** RETURN:PRInt32
**   Number of io descriptors with asked for conditions or zero if the function
**   timed out or -1 on failure.  The reason for the failure is obtained by 
**   calling PR_GetError().
** XXX can we implement this on windoze and mac?
**************************************************************************
*/
PR_EXTERN(PRInt32) PR_Select(
    PRInt32 num, PR_fd_set *readfds, PR_fd_set *writefds,
    PR_fd_set *exceptfds, PRIntervalTime timeout);

/* 
** The following are not thread safe for two threads operating on them at the
** same time.
**
** The following routines are provided for manipulating io descriptor sets.
** PR_FD_ZERO(&fdset) initializes a descriptor set fdset to the null set.
** PR_FD_SET(fd, &fdset) includes a particular file descriptor fd in fdset.
** PR_FD_CLR(fd, &fdset) removes a file descriptor fd from fdset.  
** PR_FD_ISSET(fd, &fdset) is nonzero if file descriptor fd is a member of 
** fdset, zero otherwise.
**
** PR_FD_NSET(osfd, &fdset) includes a particular native file descriptor osfd
** in fdset.
** PR_FD_NCLR(osfd, &fdset) removes a native file descriptor osfd from fdset.  
** PR_FD_NISSET(osfd, &fdset) is nonzero if native file descriptor osfd is a member of 
** fdset, zero otherwise.
*/

PR_EXTERN(void)        PR_FD_ZERO(PR_fd_set *set);
PR_EXTERN(void)        PR_FD_SET(PRFileDesc *fd, PR_fd_set *set);
PR_EXTERN(void)        PR_FD_CLR(PRFileDesc *fd, PR_fd_set *set);
PR_EXTERN(PRInt32)     PR_FD_ISSET(PRFileDesc *fd, PR_fd_set *set);
PR_EXTERN(void)        PR_FD_NSET(PRInt32 osfd, PR_fd_set *set);
PR_EXTERN(void)        PR_FD_NCLR(PRInt32 osfd, PR_fd_set *set);
PR_EXTERN(PRInt32)     PR_FD_NISSET(PRInt32 osfd, PR_fd_set *set);

#ifndef NO_NSPR_10_SUPPORT
#ifdef XP_MAC
#include <stat.h>
#else
#include <sys/stat.h>
#endif

PR_EXTERN(PRInt32) PR_Stat(const char *path, struct stat *buf);
#endif /* NO_NSPR_10_SUPPORT */

/***********************************************************************
** FUNCTION: PR_CreateNetAddr(), PR_DestroyNetAddr()
** DESCRIPTION:
**  Create an instance of a PRNetAddr, assigning well known values as
**  appropriate.
**
** INPUTS
**  PRNetAddrValue val  The value to be assigned to the IP Address portion
**                      of the network address. This can only specify the
**                      special well known values that are equivalent to
**                      INADDR_ANY and INADDR_LOOPBACK.
**
**  PRUInt16 port       The port number to be assigned in the structure.
**
** OUTPUTS:
**  None
**
** RETURN:
**  PRNetAddr *addr     The initialized address that has been allocated
**                      from the heap. It must be freed by the caller.
**                      A returned value of zero indicates an error. The
**                      cause of the error (most likely low memory) may
**                      be retrieved by calling PR_GetError().
**
***********************************************************************/
PR_EXTERN(PRNetAddr*) PR_CreateNetAddr(PRNetAddrValue val, PRUint16 port);
PR_EXTERN(PRStatus) PR_DestroyNetAddr(PRNetAddr *addr);



/***********************************************************************
** FUNCTION: PR_GetHostName() **OBSOLETE**
**   Use PR_GetSystemInfo() (prsystem.h) with an argument of
**   PR_SI_HOSTNAME instead.
** DESCRIPTION:	
** Get the DNS name of the hosting computer.
**
** INPUTS:
**  char *name          The location where the host's name is stored.
**  PRIntn bufsize      Number of bytes in 'name'.
** OUTPUTS:
**  PRHostEnt *name
**                      This string is filled in by the runtime if the
**                      function returns PR_SUCCESS. The string must be
**                      allocated by the caller
** RETURN:
**  PRStatus            PR_SUCCESS if the  succeeds. If it fails the
**                      result will be PR_FAILURE and the reason for
**                      the failure can be retrieved by PR_GetError().
***********************************************************************/
PR_EXTERN(PRStatus) PR_GetHostName(char *name, PRUint32 namelen);

/*
** Return the current thread's last error string.
** obsoleted by PR_GetErrorText().
*/
PR_EXTERN(const char *) PR_GetErrorString(void);

PR_END_EXTERN_C

#endif /* defined(PROBSLET_H) */

/* probslet.h */
