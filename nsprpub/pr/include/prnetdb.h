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

#ifndef prnetdb_h___
#define prnetdb_h___

#include "prtypes.h"
#include "prio.h"

PR_BEGIN_EXTERN_C


/*
 *********************************************************************
 *  Translate an Internet address to/from a character string
 *********************************************************************
 */
PR_EXTERN(PRStatus) PR_StringToNetAddr(
    const char *string, PRNetAddr *addr);

PR_EXTERN(PRStatus) PR_NetAddrToString(
    const PRNetAddr *addr, char *string, PRUint32 size);

/*
** Structures returned by network data base library.  All addresses are
** supplied in host order, and returned in network order (suitable for
** use in system calls).
*/
/*
** Beware that WINSOCK.H defines h_addrtype and h_length as short.
** Client code does direct struct copies of hostent to PRHostEnt and
** hence the ifdef.
*/
typedef struct PRHostEnt {
    char *h_name;       /* official name of host */
    char **h_aliases;   /* alias list */
#if defined(WIN32) || defined(WIN16)
    PRInt16 h_addrtype; /* host address type */
    PRInt16 h_length;   /* length of address */
#else
    PRInt32 h_addrtype; /* host address type */
    PRInt32 h_length;   /* length of address */
#endif
    char **h_addr_list; /* list of addresses from name server */
} PRHostEnt;

/* A safe size to use that will mostly work... */
#if (defined(AIX) && defined(_THREAD_SAFE)) || defined(OSF1)
#define PR_NETDB_BUF_SIZE sizeof(struct protoent_data)
#else
#define PR_NETDB_BUF_SIZE 1024
#endif

/***********************************************************************
** FUNCTION:	
** DESCRIPTION:	PR_GetHostByName()
** Lookup a host by name.
**
** INPUTS:
**  char *hostname      Character string defining the host name of interest
**  char *buf           A scratch buffer for the runtime to return result.
**                      This buffer is allocated by the caller.
**  PRIntn bufsize      Number of bytes in 'buf'. A recommnded value to
**                      use is PR_NETDB_BUF_SIZE.
** OUTPUTS:
**  PRHostEnt *hostentry
**                      This structure is filled in by the runtime if
**                      the function returns PR_SUCCESS. This structure
**                      is allocated by the caller.
** RETURN:
**  PRStatus            PR_SUCCESS if the lookup succeeds. If it fails
**                      the result will be PR_FAILURE and the reason
**                      for the failure can be retrieved by PR_GetError().
***********************************************************************/
PR_EXTERN(PRStatus) PR_GetHostByName(
    const char *hostname, char *buf, PRIntn bufsize, PRHostEnt *hostentry);

/***********************************************************************
** FUNCTION:	
** DESCRIPTION:	PR_GetHostByAddr()
** Lookup a host entry by its network address.
**
** INPUTS:
**  char *hostaddr      IP address of host in question
**  char *buf           A scratch buffer for the runtime to return result.
**                      This buffer is allocated by the caller.
**  PRIntn bufsize      Number of bytes in 'buf'. A recommnded value to
**                      use is PR_NETDB_BUF_SIZE.
** OUTPUTS:
**  PRHostEnt *hostentry
**                      This structure is filled in by the runtime if
**                      the function returns PR_SUCCESS. This structure
**                      is allocated by the caller.
** RETURN:
**  PRStatus            PR_SUCCESS if the lookup succeeds. If it fails
**                      the result will be PR_FAILURE and the reason
**                      for the failure can be retrieved by PR_GetError().
***********************************************************************/
PR_EXTERN(PRStatus) PR_GetHostByAddr(
    const PRNetAddr *hostaddr, char *buf, PRIntn bufsize, PRHostEnt *hostentry);

/***********************************************************************
** FUNCTION:	PR_EnumerateHostEnt()	
** DESCRIPTION:
**  A stateless enumerator over a PRHostEnt structure acquired from
**  PR_GetHostByName() PR_GetHostByAddr() to evaluate the possible
**  network addresses.
**
** INPUTS:
**  PRIntn  enumIndex   Index of the enumeration. The enumeration starts
**                      and ends with a value of zero.
**
**  PRHostEnt *hostEnt  A pointer to a host entry struct that was
**                      previously returned by PR_GetHostByName() or
**                      PR_GetHostByAddr().
**
**  PRUint16 port       The port number to be assigned as part of the
**                      PRNetAddr.
**
** OUTPUTS:
**  PRNetAddr *address  A pointer to an address structure that will be
**                      filled in by the call to the enumeration if the
**                      result of the call is greater than zero.
**
** RETURN:
**  PRIntn              The value that should be used for the next call
**                      of the enumerator ('enumIndex'). The enumeration
**                      is ended if this value is returned zero.
**                      If a value of -1 is returned, the enumeration
**                      has failed. The reason for the failure can be
**                      retrieved by calling PR_GetError().
***********************************************************************/
PR_EXTERN(PRIntn) PR_EnumerateHostEnt(
    PRIntn enumIndex, const PRHostEnt *hostEnt, PRUint16 port, PRNetAddr *address);

/***********************************************************************
** FUNCTION: PR_InitializeNetAddr(), 
** DESCRIPTION:
**  Initialize the fields of a PRNetAddr, assigning well known values as
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
**  PRNetAddr *addr     The address to be manipulated.
**
** RETURN:
**  PRStatus            To indicate success or failure. If the latter, the
**                      reason for the failure can be retrieved by calling
**                      PR_GetError();
***********************************************************************/
typedef enum PRNetAddrValue
{
    PR_IpAddrNull,      /* do NOT overwrite the IP address */
    PR_IpAddrAny,       /* assign logical INADDR_ANY to IP address */
    PR_IpAddrLoopback   /* assign logical INADDR_LOOPBACK */
} PRNetAddrValue;

PR_EXTERN(PRStatus) PR_InitializeNetAddr(
    PRNetAddrValue val, PRUint16 port, PRNetAddr *addr);

/***********************************************************************
** FUNCTION:	
** DESCRIPTION:	PR_GetProtoByName()
** Lookup a protocol entry based on protocol's name
**
** INPUTS:
**  char *protocolname  Character string of the protocol's name.
**  char *buf           A scratch buffer for the runtime to return result.
**                      This buffer is allocated by the caller.
**  PRIntn bufsize      Number of bytes in 'buf'. A recommnded value to
**                      use is PR_NETDB_BUF_SIZE.
** OUTPUTS:
**  PRHostEnt *PRProtoEnt
**                      This structure is filled in by the runtime if
**                      the function returns PR_SUCCESS. This structure
**                      is allocated by the caller.
** RETURN:
**  PRStatus            PR_SUCCESS if the lookup succeeds. If it fails
**                      the result will be PR_FAILURE and the reason
**                      for the failure can be retrieved by PR_GetError().
***********************************************************************/

typedef struct PRProtoEnt {
    char *p_name;       /* official protocol name */
    char **p_aliases;   /* alias list */
#if defined(WIN32) || defined(WIN16)
    PRInt16 p_num;      /* protocol # */
#else
    PRInt32 p_num;      /* protocol # */
#endif
} PRProtoEnt;

PR_EXTERN(PRStatus) PR_GetProtoByName(
    const char* protocolname, char* buffer, PRInt32 bufsize, PRProtoEnt* result);

/***********************************************************************
** FUNCTION:	
** DESCRIPTION:	PR_GetProtoByNumber()
** Lookup a protocol entry based on protocol's number
**
** INPUTS:
**  PRInt32 protocolnumber
**                      Number assigned to the protocol.
**  char *buf           A scratch buffer for the runtime to return result.
**                      This buffer is allocated by the caller.
**  PRIntn bufsize      Number of bytes in 'buf'. A recommnded value to
**                      use is PR_NETDB_BUF_SIZE.
** OUTPUTS:
**  PRHostEnt *PRProtoEnt
**                      This structure is filled in by the runtime if
**                      the function returns PR_SUCCESS. This structure
**                      is allocated by the caller.
** RETURN:
**  PRStatus            PR_SUCCESS if the lookup succeeds. If it fails
**                      the result will be PR_FAILURE and the reason
**                      for the failure can be retrieved by PR_GetError().
***********************************************************************/
PR_EXTERN(PRStatus) PR_GetProtoByNumber(
    PRInt32 protocolnumber, char* buffer, PRInt32 bufsize, PRProtoEnt* result);

/***********************************************************************
** FUNCTION:	
** DESCRIPTION:	PR_SetIPv6Enable()
**  Enable IPv6 capability on a platform that supports the architecture.
**
**  Note: IPv6 must first be enabled for the host platform. If it is not,
**        the function will always return PR_FAILURE on any attempt to
**        change the setting.
**
** INPUTS:
**  PRBool itIs
**                      Assign it a value of PR_TRUE to turn on IPv6
**                      addressing, PR_FALSE to turn it off.
** RETURN:
**  PRStatus            PR_SUCCESS if the IPv6 is enabled for this particular
**                      host. Otherwise it will return failure and GetError()
**                      will confirm the result by indicating that the
**                      protocol is not supported
**                      (PR_PROTOCOL_NOT_SUPPORTED_ERROR) 
***********************************************************************/
PR_EXTERN(PRStatus) PR_SetIPv6Enable(PRBool itIs);

/***********************************************************************
** FUNCTIONS: PR_ntohs, PR_ntohl, PR_ntohll, PR_htons, PR_htonl, PR_htonll
**
** DESCRIPTION: API entries for the common byte ordering routines.
**
**      PR_ntohs        16 bit conversion from network to host
**      PR_ntohl        32 bit conversion from network to host
**      PR_ntohll       64 bit conversion from network to host
**      PR_htons        16 bit conversion from host to network
**      PR_htonl        32 bit conversion from host to network
**      PR_ntonll       64 bit conversion from host to network
**
***********************************************************************/
PR_EXTERN(PRUint16) PR_ntohs(PRUint16);
PR_EXTERN(PRUint32) PR_ntohl(PRUint32);
PR_EXTERN(PRUint64) PR_ntohll(PRUint64);
PR_EXTERN(PRUint16) PR_htons(PRUint16);
PR_EXTERN(PRUint32) PR_htonl(PRUint32);
PR_EXTERN(PRUint64) PR_htonll(PRUint64);

/***********************************************************************
** FUNCTION: PR_FamilyInet
**
** DESCRIPTION: Routine to get value of address family for Internet Protocol
**
***********************************************************************/
PR_EXTERN(PRUint16) PR_FamilyInet(void);

PR_END_EXTERN_C

#endif /* prnetdb_h___ */
