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

#include "primpl.h"

#include <string.h>

/*
 * On Unix, the error code for gethostbyname() and gethostbyaddr()
 * is returned in the global variable h_errno, instead of the usual
 * errno.
 */
#if defined(XP_UNIX)
#if defined(_PR_NEED_H_ERRNO)
extern int h_errno;
#endif
#define _MD_GETHOST_ERRNO() h_errno
#else
#define _MD_GETHOST_ERRNO() _MD_ERRNO()
#endif

/*
 * The meaning of the macros related to gethostbyname, gethostbyaddr,
 * and gethostbyname2 is defined below.
 * - _PR_HAVE_THREADSAFE_GETHOST: the gethostbyXXX functions return
 *   the result in thread specific storage.  For example, AIX, HP-UX,
 *   and OSF1.
 * -  _PR_HAVE_GETHOST_R: have the gethostbyXXX_r functions. See next
 *   two macros.
 * - _PR_HAVE_GETHOST_R_INT: the gethostbyXXX_r functions return an
 *   int.  For example, Linux glibc.
 * - _PR_HAVE_GETHOST_R_POINTER: the gethostbyXXX_r functions return
 *   a struct hostent* pointer.  For example, Solaris and IRIX.
 */
#if defined(_PR_NO_PREEMPT) || defined(_PR_HAVE_GETHOST_R) \
    || defined(_PR_HAVE_THREADSAFE_GETHOST)
#define _PR_NO_DNS_LOCK
#endif

#if defined(_PR_NO_DNS_LOCK)
#define LOCK_DNS()
#define UNLOCK_DNS()
#else
PRLock *_pr_dnsLock = NULL;
#define LOCK_DNS() PR_Lock(_pr_dnsLock)
#define UNLOCK_DNS() PR_Unlock(_pr_dnsLock)
#endif  /* defined(_PR_NO_DNS_LOCK) */

/*
 * Some platforms have the reentrant getprotobyname_r() and
 * getprotobynumber_r().  However, they come in two flavors.
 * Some return a pointer to struct protoent, others return
 * an int.
 */

#if defined(SOLARIS) || (defined(BSDI) && defined(_REENTRANT)) \
	|| (defined(LINUX) && defined(_REENTRANT) \
        && !(defined(__GLIBC__) && __GLIBC__ >= 2))
#define _PR_HAVE_GETPROTO_R
#define _PR_HAVE_GETPROTO_R_POINTER
#endif

#if defined(OSF1) \
        || defined(AIX4_3) || (defined(AIX) && defined(_THREAD_SAFE)) \
	|| (defined(HPUX10_10) && defined(_REENTRANT)) \
        || (defined(HPUX10_20) && defined(_REENTRANT))
#define _PR_HAVE_GETPROTO_R
#define _PR_HAVE_GETPROTO_R_INT
#endif

#if (defined(LINUX) && defined(__GLIBC__) && __GLIBC__ >= 2)
#define _PR_HAVE_GETPROTO_R
#define _PR_HAVE_5_ARG_GETPROTO_R
#endif

#if !defined(_PR_HAVE_GETPROTO_R)
PRLock* _getproto_lock = NULL;
#endif

#if defined(_PR_INET6_PROBE)
PR_EXTERN(PRBool) _pr_ipv6_is_present;
#endif

#define _PR_IN6_IS_ADDR_UNSPECIFIED(a)				\
				(((a)->pr_s6_addr32[0] == 0) &&	\
				((a)->pr_s6_addr32[1] == 0) &&		\
				((a)->pr_s6_addr32[2] == 0) &&		\
				((a)->pr_s6_addr32[3] == 0))
 
#define _PR_IN6_IS_ADDR_LOOPBACK(a)					\
               (((a)->pr_s6_addr32[0] == 0)	&&	\
               ((a)->pr_s6_addr32[1] == 0)		&&	\
               ((a)->pr_s6_addr32[2] == 0)		&&	\
               ((a)->pr_s6_addr[12] == 0)		&&	\
               ((a)->pr_s6_addr[13] == 0)		&&	\
               ((a)->pr_s6_addr[14] == 0)		&&	\
               ((a)->pr_s6_addr[15] == 0x1U))
 
const PRIPv6Addr _pr_in6addr_any =	{{{ 0, 0, 0, 0,
										0, 0, 0, 0,
										0, 0, 0, 0,
										0, 0, 0, 0 }}};

const PRIPv6Addr _pr_in6addr_loopback = {{{ 0, 0, 0, 0,
											0, 0, 0, 0,
											0, 0, 0, 0,
											0, 0, 0, 0x1U }}};
/*
 * The values at bytes 10 and 11 are compared using pointers to
 * 8-bit fields, and not 32-bit fields, to make the comparison work on
 * both big-endian and little-endian systems
 */

#define _PR_IN6_IS_ADDR_V4MAPPED(a)			\
		(((a)->pr_s6_addr32[0] == 0) 	&&	\
		((a)->pr_s6_addr32[1] == 0)	&&	\
		((a)->pr_s6_addr[8] == 0)		&&	\
		((a)->pr_s6_addr[9] == 0)		&&	\
		((a)->pr_s6_addr[10] == 0xff)	&&	\
		((a)->pr_s6_addr[11] == 0xff))

#define _PR_IN6_IS_ADDR_V4COMPAT(a)			\
		(((a)->pr_s6_addr32[0] == 0) &&	\
		((a)->pr_s6_addr32[1] == 0) &&		\
		((a)->pr_s6_addr32[2] == 0))

#define _PR_IN6_V4MAPPED_TO_IPADDR(a) ((a)->pr_s6_addr32[3])

void _PR_InitNet(void)
{
#if defined(XP_UNIX)
#ifdef HAVE_NETCONFIG
	/*
	 * This one-liner prevents the endless re-open's and re-read's of
	 * /etc/netconfig on EACH and EVERY call to accept(), connect(), etc.
	 */
	 (void)setnetconfig();
#endif
#endif
#if !defined(_PR_NO_DNS_LOCK)
	_pr_dnsLock = PR_NewLock();
#endif
#if !defined(_PR_HAVE_GETPROTO_R)
	_getproto_lock = PR_NewLock();
#endif
}

/*
** Allocate space from the buffer, aligning it to "align" before doing
** the allocation. "align" must be a power of 2.
*/
static char *Alloc(PRIntn amount, char **bufp, PRIntn *buflenp, PRIntn align)
{
	char *buf = *bufp;
	PRIntn buflen = *buflenp;

	if (align && ((long)buf & (align - 1))) {
		PRIntn skip = align - ((ptrdiff_t)buf & (align - 1));
		if (buflen < skip) {
			return 0;
		}
		buf += skip;
		buflen -= skip;
	}
	if (buflen < amount) {
		return 0;
	}
	*bufp = buf + amount;
	*buflenp = buflen - amount;
	return buf;
}

typedef enum _PRIPAddrConversion {
    _PRIPAddrNoConversion,
    _PRIPAddrIPv4Mapped,
    _PRIPAddrIPv4Compat
} _PRIPAddrConversion;

/*
** Convert an IPv4 address (v4) to an IPv4-mapped IPv6 address (v6).
*/
static void MakeIPv4MappedAddr(const char *v4, char *v6)
{
    memset(v6, 0, 10);
    memset(v6 + 10, 0xff, 2);
    memcpy(v6 + 12, v4, 4);
    PR_ASSERT(_PR_IN6_IS_ADDR_V4MAPPED(((PRIPv6Addr *) v6)));
}

/*
** Convert an IPv4 address (v4) to an IPv4-compatible IPv6 address (v6).
*/
static void MakeIPv4CompatAddr(const char *v4, char *v6)
{
    memset(v6, 0, 12);
    memcpy(v6 + 12, v4, 4);
    PR_ASSERT(_PR_IN6_IS_ADDR_V4COMPAT(((PRIPv6Addr *) v6)));
}

/*
** Copy a hostent, and all of the memory that it refers to into
** (hopefully) stacked buffers.
*/
static PRStatus CopyHostent(
    struct hostent *from,
    char **buf,
    PRIntn *bufsize,
    _PRIPAddrConversion conversion,
    PRHostEnt *to)
{
	PRIntn len, na;
	char **ap;

	if (conversion != _PRIPAddrNoConversion
			&& from->h_addrtype == AF_INET) {
		PR_ASSERT(from->h_length == 4);
		to->h_addrtype = PR_AF_INET6;
		to->h_length = 16;
	} else {
#if defined(_PR_INET6) || defined(_PR_INET6_PROBE)
		if (AF_INET6 == from->h_addrtype)
			to->h_addrtype = PR_AF_INET6;
		else
#endif
			to->h_addrtype = from->h_addrtype;
		to->h_length = from->h_length;
	}

	/* Copy the official name */
	if (!from->h_name) return PR_FAILURE;
	len = strlen(from->h_name) + 1;
	to->h_name = Alloc(len, buf, bufsize, 0);
	if (!to->h_name) return PR_FAILURE;
	memcpy(to->h_name, from->h_name, len);

	/* Count the aliases, then allocate storage for the pointers */
	if (!from->h_aliases) {
		na = 1;
	} else {
		for (na = 1, ap = from->h_aliases; *ap != 0; na++, ap++){;} /* nothing to execute */
	}
	to->h_aliases = (char**)Alloc(
	    na * sizeof(char*), buf, bufsize, sizeof(char**));
	if (!to->h_aliases) return PR_FAILURE;

	/* Copy the aliases, one at a time */
	if (!from->h_aliases) {
		to->h_aliases[0] = 0;
	} else {
		for (na = 0, ap = from->h_aliases; *ap != 0; na++, ap++) {
			len = strlen(*ap) + 1;
			to->h_aliases[na] = Alloc(len, buf, bufsize, 0);
			if (!to->h_aliases[na]) return PR_FAILURE;
			memcpy(to->h_aliases[na], *ap, len);
		}
		to->h_aliases[na] = 0;
	}

	/* Count the addresses, then allocate storage for the pointers */
	for (na = 1, ap = from->h_addr_list; *ap != 0; na++, ap++){;} /* nothing to execute */
	to->h_addr_list = (char**)Alloc(
	    na * sizeof(char*), buf, bufsize, sizeof(char**));
	if (!to->h_addr_list) return PR_FAILURE;

	/* Copy the addresses, one at a time */
	for (na = 0, ap = from->h_addr_list; *ap != 0; na++, ap++) {
		to->h_addr_list[na] = Alloc(to->h_length, buf, bufsize, 0);
		if (!to->h_addr_list[na]) return PR_FAILURE;
		if (conversion != _PRIPAddrNoConversion
				&& from->h_addrtype == AF_INET) {
			if (conversion == _PRIPAddrIPv4Mapped) {
				MakeIPv4MappedAddr(*ap, to->h_addr_list[na]);
			} else {
				PR_ASSERT(conversion == _PRIPAddrIPv4Compat);
				MakeIPv4CompatAddr(*ap, to->h_addr_list[na]);
			}
		} else {
			memcpy(to->h_addr_list[na], *ap, to->h_length);
		}
	}
	to->h_addr_list[na] = 0;
	return PR_SUCCESS;
}

#if !defined(_PR_HAVE_GETPROTO_R)
/*
** Copy a protoent, and all of the memory that it refers to into
** (hopefully) stacked buffers.
*/
static PRStatus CopyProtoent(
    struct protoent *from, char *buf, PRIntn bufsize, PRProtoEnt *to)
{
	PRIntn len, na;
	char **ap;

	/* Do the easy stuff */
	to->p_num = from->p_proto;

	/* Copy the official name */
	if (!from->p_name) return PR_FAILURE;
	len = strlen(from->p_name) + 1;
	to->p_name = Alloc(len, &buf, &bufsize, 0);
	if (!to->p_name) return PR_FAILURE;
	memcpy(to->p_name, from->p_name, len);

	/* Count the aliases, then allocate storage for the pointers */
	for (na = 1, ap = from->p_aliases; *ap != 0; na++, ap++){;} /* nothing to execute */
	to->p_aliases = (char**)Alloc(
	    na * sizeof(char*), &buf, &bufsize, sizeof(char**));
	if (!to->p_aliases) return PR_FAILURE;

	/* Copy the aliases, one at a time */
	for (na = 0, ap = from->p_aliases; *ap != 0; na++, ap++) {
		len = strlen(*ap) + 1;
		to->p_aliases[na] = Alloc(len, &buf, &bufsize, 0);
		if (!to->p_aliases[na]) return PR_FAILURE;
		memcpy(to->p_aliases[na], *ap, len);
	}
	to->p_aliases[na] = 0;

	return PR_SUCCESS;
}
#endif /* !defined(_PR_HAVE_GETPROTO_R) */

/*
 * #################################################################
 * NOTE: tmphe, tmpbuf, bufsize, h, and h_err are local variables
 * or arguments of PR_GetHostByName, PR_GetIPNodeByName, and
 * PR_GetHostByAddr.  DO NOT CHANGE THE NAMES OF THESE LOCAL 
 * VARIABLES OR ARGUMENTS.
 * #################################################################
 */
#if defined(_PR_HAVE_GETHOST_R_INT)

#define GETHOSTBYNAME(name) \
    (gethostbyname_r(name, &tmphe, tmpbuf, bufsize, &h, &h_err), h)
#define GETHOSTBYNAME2(name, af) \
    (gethostbyname2_r(name, af, &tmphe, tmpbuf, bufsize, &h, &h_err), h)
#define GETHOSTBYADDR(addr, addrlen, af) \
    (gethostbyaddr_r(addr, addrlen, af, \
    &tmphe, tmpbuf, bufsize, &h, &h_err), h)

#elif defined(_PR_HAVE_GETHOST_R_POINTER)

#define GETHOSTBYNAME(name) \
    gethostbyname_r(name, &tmphe, tmpbuf, bufsize, &h_err)
#define GETHOSTBYNAME2(name, af) \
    gethostbyname2_r(name, af, &tmphe, tmpbuf, bufsize, &h_err)
#define GETHOSTBYADDR(addr, addrlen, af) \
    gethostbyaddr_r(addr, addrlen, af, &tmphe, tmpbuf, bufsize, &h_err)

#else

#define GETHOSTBYNAME(name) gethostbyname(name)
#define GETHOSTBYNAME2(name, af) gethostbyname2(name, af)
#define GETHOSTBYADDR(addr, addrlen, af) gethostbyaddr(addr, addrlen, af)

#endif  /* definition of GETHOSTBYXXX */

PR_IMPLEMENT(PRStatus) PR_GetHostByName(
    const char *name, char *buf, PRIntn bufsize, PRHostEnt *hp)
{
	struct hostent *h;
	PRStatus rv = PR_FAILURE;
#if defined(_PR_HAVE_GETHOST_R)
    char localbuf[PR_NETDB_BUF_SIZE];
    char *tmpbuf;
    struct hostent tmphe;
    int h_err;
#endif

    if (!_pr_initialized) _PR_ImplicitInitialization();

#if defined(_PR_HAVE_GETHOST_R)
    tmpbuf = localbuf;
    if (bufsize > sizeof(localbuf))
    {
        tmpbuf = PR_Malloc(bufsize);
        if (NULL == tmpbuf)
        {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return rv;
        }
    }
#endif

	LOCK_DNS();

#ifdef XP_OS2_VACPP
	h = GETHOSTBYNAME((char *)name);
#else
    h = GETHOSTBYNAME(name);
#endif
    
	if (NULL == h)
	{
	    PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_GETHOST_ERRNO());
	}
	else
	{
		_PRIPAddrConversion conversion = _PRIPAddrNoConversion;
		rv = CopyHostent(h, &buf, &bufsize, conversion, hp);
		if (PR_SUCCESS != rv)
		    PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
	}
	UNLOCK_DNS();
#if defined(_PR_HAVE_GETHOST_R)
    if (tmpbuf != localbuf)
        PR_Free(tmpbuf);
#endif
	return rv;
}

#if defined(_PR_INET6_PROBE) && defined(_PR_HAVE_GETIPNODEBYNAME)
typedef struct hostent  * (*_pr_getipnodebyname_t)(const char *, int,
										int, int *);
typedef struct hostent  * (*_pr_getipnodebyaddr_t)(const void *, size_t,
													int, int *);
typedef void (*_pr_freehostent_t)(struct hostent *);
extern void * _pr_getipnodebyname_fp;
extern void * _pr_getipnodebyaddr_fp;
extern void * _pr_freehostent_fp;
#endif

#if defined(_PR_INET6) && defined(_PR_HAVE_GETHOSTBYNAME2)
/*
** Append the V4 addresses to the end of the list
*/
static PRStatus AppendV4AddrsToHostent(
    struct hostent *from,
    char **buf,
    PRIntn *bufsize,
    PRHostEnt *to)
{
    PRIntn na, na_old;
    char **ap;
    char **new_addr_list;
			
    /* Count the addresses, then grow storage for the pointers */
    for (na_old = 0, ap = to->h_addr_list; *ap != 0; na_old++, ap++)
        {;} /* nothing to execute */
    for (na = na_old + 1, ap = from->h_addr_list; *ap != 0; na++, ap++)
        {;} /* nothing to execute */
    new_addr_list = (char**)Alloc(
        na * sizeof(char*), buf, bufsize, sizeof(char**));
    if (!new_addr_list) return PR_FAILURE;

    /* Copy the V6 addresses, one at a time */
    for (na = 0, ap = to->h_addr_list; *ap != 0; na++, ap++) {
        new_addr_list[na] = to->h_addr_list[na];
    }
    to->h_addr_list = new_addr_list;

    /* Copy the V4 addresses, one at a time */
    for (ap = from->h_addr_list; *ap != 0; na++, ap++) {
        to->h_addr_list[na] = Alloc(to->h_length, buf, bufsize, 0);
        if (!to->h_addr_list[na]) return PR_FAILURE;
        MakeIPv4MappedAddr(*ap, to->h_addr_list[na]);
    }
    to->h_addr_list[na] = 0;
    return PR_SUCCESS;
}
#endif

PR_IMPLEMENT(PRStatus) PR_GetIPNodeByName(
    const char *name, PRUint16 af, PRIntn flags,
    char *buf, PRIntn bufsize, PRHostEnt *hp)
{
	struct hostent *h = 0;
	PRStatus rv = PR_FAILURE;
#if defined(_PR_HAVE_GETHOST_R)
    char localbuf[PR_NETDB_BUF_SIZE];
    char *tmpbuf;
    struct hostent tmphe;
    int h_err;
#endif
#if defined(_PR_HAVE_GETIPNODEBYNAME)
	PRUint16 md_af = af;
	int error_num;
	int tmp_flags = 0;
#endif
#if defined(_PR_HAVE_GETHOSTBYNAME2)
    PRBool did_af_inet = PR_FALSE;
#endif

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (af != PR_AF_INET && af != PR_AF_INET6) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

#if defined(_PR_HAVE_GETIPNODEBYNAME)
	if (flags & PR_AI_V4MAPPED)
		tmp_flags |= AI_V4MAPPED;
	if (flags & PR_AI_ADDRCONFIG)
		tmp_flags |= AI_ADDRCONFIG;
	if (flags & PR_AI_ALL)
		tmp_flags |= AI_ALL;
    if (af == PR_AF_INET6)
    	md_af = AF_INET6;
	else
    	md_af = af;
#endif

#if defined(_PR_HAVE_GETHOST_R)
    tmpbuf = localbuf;
    if (bufsize > sizeof(localbuf))
    {
        tmpbuf = PR_Malloc(bufsize);
        if (NULL == tmpbuf)
        {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return rv;
        }
    }
#endif

    /* Do not need to lock the DNS lock if getipnodebyname() is called */
#ifdef _PR_INET6
#ifdef _PR_HAVE_GETHOSTBYNAME2
    LOCK_DNS();
    if (af == PR_AF_INET6)
    {
#ifdef _PR_INET6_PROBE
      if (_pr_ipv6_is_present == PR_TRUE)
#endif
        h = GETHOSTBYNAME2(name, AF_INET6); 
        if ((NULL == h) && (flags & PR_AI_V4MAPPED))
        {
            did_af_inet = PR_TRUE;
            h = GETHOSTBYNAME2(name, AF_INET);
        }
    }
    else
    {
        did_af_inet = PR_TRUE;
        h = GETHOSTBYNAME2(name, af);
    }
#elif defined(_PR_HAVE_GETIPNODEBYNAME)
    h = getipnodebyname(name, md_af, tmp_flags, &error_num);
#else
#error "Unknown name-to-address translation function"
#endif	/* _PR_HAVE_GETHOSTBYNAME2 */
#elif defined(_PR_INET6_PROBE) && defined(_PR_HAVE_GETIPNODEBYNAME)
    if (_pr_ipv6_is_present == PR_TRUE)
    	h = (*((_pr_getipnodebyname_t)_pr_getipnodebyname_fp))(name, md_af, tmp_flags, &error_num);
    else
    {
        LOCK_DNS();
    	h = GETHOSTBYNAME(name);
    }
#else /* _PR_INET6 */
    LOCK_DNS();
#ifdef XP_OS2_VACPP
	h = GETHOSTBYNAME((char *)name);
#else
    h = GETHOSTBYNAME(name);
#endif
#endif /* _PR_INET6 */
    
	if (NULL == h)
	{
#if defined(_PR_INET6) && defined(_PR_HAVE_GETIPNODEBYNAME)
	    PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, error_num);
#elif defined(_PR_INET6_PROBE) && defined(_PR_HAVE_GETIPNODEBYNAME)
    	if (_pr_ipv6_is_present == PR_TRUE)
	    	PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, error_num);
		else
	    	PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_GETHOST_ERRNO());
#else
	    PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_GETHOST_ERRNO());
#endif
	}
	else
	{
		_PRIPAddrConversion conversion = _PRIPAddrNoConversion;

		if (af == PR_AF_INET6) conversion = _PRIPAddrIPv4Mapped;
		rv = CopyHostent(h, &buf, &bufsize, conversion, hp);
		if (PR_SUCCESS != rv)
		    PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
#if defined(_PR_INET6) && defined(_PR_HAVE_GETIPNODEBYNAME)
		freehostent(h);
#elif defined(_PR_INET6_PROBE) && defined(_PR_HAVE_GETIPNODEBYNAME)
    	if (_pr_ipv6_is_present == PR_TRUE)
			(*((_pr_freehostent_t)_pr_freehostent_fp))(h);
#endif
#if defined(_PR_INET6) && defined(_PR_HAVE_GETHOSTBYNAME2)
		if ((PR_SUCCESS == rv) && (flags & PR_AI_V4MAPPED)
				&& (flags & (PR_AI_ALL|PR_AI_ADDRCONFIG))
				&& !did_af_inet && (h = GETHOSTBYNAME2(name, AF_INET)) != 0) {
			rv = AppendV4AddrsToHostent(h, &buf, &bufsize, hp);
			if (PR_SUCCESS != rv)
				PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
		}
#endif
	}

    /* Must match the convoluted logic above for LOCK_DNS() */
#ifdef _PR_INET6
#ifdef _PR_HAVE_GETHOSTBYNAME2
    UNLOCK_DNS();
#endif	/* _PR_HAVE_GETHOSTBYNAME2 */
#elif defined(_PR_INET6_PROBE) && defined(_PR_HAVE_GETIPNODEBYNAME)
    if (_pr_ipv6_is_present == PR_FALSE)
        UNLOCK_DNS();
#else /* _PR_INET6 */
    UNLOCK_DNS();
#endif /* _PR_INET6 */

#if defined(_PR_HAVE_GETHOST_R)
    if (tmpbuf != localbuf)
        PR_Free(tmpbuf);
#endif

	return rv;
}

PR_IMPLEMENT(PRStatus) PR_GetHostByAddr(
    const PRNetAddr *hostaddr, char *buf, PRIntn bufsize, PRHostEnt *hostentry)
{
	struct hostent *h;
	PRStatus rv = PR_FAILURE;
	const void *addr;
	PRUint32 tmp_ip;
	int addrlen;
	PRInt32 af;
#if defined(_PR_HAVE_GETHOST_R)
    char localbuf[PR_NETDB_BUF_SIZE];
    char *tmpbuf;
    struct hostent tmphe;
    int h_err;
#endif
#if defined(_PR_HAVE_GETIPNODEBYADDR)
	int error_num;
#endif

    if (!_pr_initialized) _PR_ImplicitInitialization();

	if (hostaddr->raw.family == PR_AF_INET6)
	{
#if defined(_PR_INET6_PROBE)
		if (_pr_ipv6_is_present == PR_TRUE)
			af = AF_INET6;
		else
			af = AF_INET;
#elif defined(_PR_INET6)
		af = AF_INET6;
#else
		af = AF_INET;
#endif
	}
	else
	{
		PR_ASSERT(hostaddr->raw.family == AF_INET);
		af = AF_INET;
	}
	if (hostaddr->raw.family == PR_AF_INET6) {
#if defined(_PR_INET6) || defined(_PR_INET6_PROBE)
		if (af == AF_INET6) {
			addr = &hostaddr->ipv6.ip;
			addrlen = sizeof(hostaddr->ipv6.ip);
		}
		else
#endif
		{
			PR_ASSERT(af == AF_INET);
			if (!_PR_IN6_IS_ADDR_V4MAPPED(&hostaddr->ipv6.ip)) {
				PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
				return rv;
			}
			tmp_ip = _PR_IN6_V4MAPPED_TO_IPADDR((PRIPv6Addr *)
												&hostaddr->ipv6.ip);
			addr = &tmp_ip;
			addrlen = sizeof(tmp_ip);
		}
	} else {
		PR_ASSERT(hostaddr->raw.family == AF_INET);
		PR_ASSERT(af == AF_INET);
		addr = &hostaddr->inet.ip;
		addrlen = sizeof(hostaddr->inet.ip);
	}

#if defined(_PR_HAVE_GETHOST_R)
    tmpbuf = localbuf;
    if (bufsize > sizeof(localbuf))
    {
        tmpbuf = PR_Malloc(bufsize);
        if (NULL == tmpbuf)
        {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return rv;
        }
    }
#endif

    /* Do not need to lock the DNS lock if getipnodebyaddr() is called */
#if defined(_PR_HAVE_GETIPNODEBYADDR) && defined(_PR_INET6)
	h = getipnodebyaddr(addr, addrlen, af, &error_num);
#elif defined(_PR_HAVE_GETIPNODEBYADDR) && defined(_PR_INET6_PROBE)
    if (_pr_ipv6_is_present == PR_TRUE)
    	h = (*((_pr_getipnodebyaddr_t)_pr_getipnodebyaddr_fp))(addr, addrlen,
				af, &error_num);
	else
    {
        LOCK_DNS();
		h = GETHOSTBYADDR(addr, addrlen, af);
    }
#else	/* _PR_HAVE_GETIPNODEBYADDR */
    LOCK_DNS();
#ifdef XP_OS2_VACPP
	h = GETHOSTBYADDR((char *)addr, addrlen, af);
#else
	h = GETHOSTBYADDR(addr, addrlen, af);
#endif
#endif /* _PR_HAVE_GETIPNODEBYADDR */
	if (NULL == h)
	{
#if defined(_PR_INET6) && defined(_PR_HAVE_GETIPNODEBYADDR)
		PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, error_num);
#elif defined(_PR_INET6_PROBE) && defined(_PR_HAVE_GETIPNODEBYADDR)
    	if (_pr_ipv6_is_present == PR_TRUE)
	    	PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, error_num);
		else
	    	PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_GETHOST_ERRNO());
#else
		PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_GETHOST_ERRNO());
#endif
	}
	else
	{
		_PRIPAddrConversion conversion = _PRIPAddrNoConversion;
		if (hostaddr->raw.family == PR_AF_INET6) {
			if (af == AF_INET) {
				if (_PR_IN6_IS_ADDR_V4MAPPED((PRIPv6Addr*)
												&hostaddr->ipv6.ip)) {
					conversion = _PRIPAddrIPv4Mapped;
				} else if (_PR_IN6_IS_ADDR_V4COMPAT((PRIPv6Addr *)
													&hostaddr->ipv6.ip)) {
					conversion = _PRIPAddrIPv4Compat;
				}
			}
		}
		rv = CopyHostent(h, &buf, &bufsize, conversion, hostentry);
		if (PR_SUCCESS != rv) {
		    PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
		}
#if defined(_PR_INET6) && defined(_PR_HAVE_GETIPNODEBYADDR)
		freehostent(h);
#elif defined(_PR_INET6_PROBE) && defined(_PR_HAVE_GETIPNODEBYADDR)
    	if (_pr_ipv6_is_present == PR_TRUE)
			(*((_pr_freehostent_t)_pr_freehostent_fp))(h);
#endif
	}

    /* Must match the convoluted logic above for LOCK_DNS() */
#if defined(_PR_HAVE_GETIPNODEBYADDR) && defined(_PR_INET6)
#elif defined(_PR_HAVE_GETIPNODEBYADDR) && defined(_PR_INET6_PROBE)
    if (_pr_ipv6_is_present == PR_FALSE)
        UNLOCK_DNS();
#else	/* _PR_HAVE_GETIPNODEBYADDR */
    UNLOCK_DNS();
#endif /* _PR_HAVE_GETIPNODEBYADDR */

#if defined(_PR_HAVE_GETHOST_R)
    if (tmpbuf != localbuf)
        PR_Free(tmpbuf);
#endif

	return rv;
}

/******************************************************************************/
/*
 * Some systems define a reentrant version of getprotobyname(). Too bad
 * the signature isn't always the same. But hey, they tried. If there
 * is such a definition, use it. Otherwise, grab a lock and do it here.
 */
/******************************************************************************/

#if !defined(_PR_HAVE_GETPROTO_R)
/*
 * This may seem like a silly thing to do, but the compiler SHOULD
 * complain if getprotobyname_r() is implemented on some system and
 * we're not using it. For sure these signatures are different than
 * any usable implementation.
 */

static struct protoent *getprotobyname_r(const char* name)
{
#ifdef XP_OS2_VACPP
	return getprotobyname((char *)name);
#else
	return getprotobyname(name);
#endif
} /* getprotobyname_r */

static struct protoent *getprotobynumber_r(PRInt32 number)
{
	return getprotobynumber(number);
} /* getprotobynumber_r */

#endif /* !defined(_PR_HAVE_GETPROTO_R) */

PR_IMPLEMENT(PRStatus) PR_GetProtoByName(
    const char* name, char* buffer, PRInt32 buflen, PRProtoEnt* result)
{
	PRStatus rv = PR_SUCCESS;
#if defined(_PR_HAVE_GETPROTO_R)
	struct protoent* res = (struct protoent*)result;
#endif

    if (!_pr_initialized) _PR_ImplicitInitialization();

#if defined(_PR_HAVE_GETPROTO_R_INT)
    {
        /*
        ** The protoent_data has a pointer as the first field.
        ** That implies the buffer better be aligned, and char*
        ** doesn't promise much.
        */
        PRUptrdiff aligned = (PRUptrdiff)buffer;
        if (0 != (aligned & (sizeof(struct protoent_data*) - 1)))
        {
            aligned += sizeof(struct protoent_data*) - 1;
            aligned &= ~(sizeof(struct protoent_data*) - 1);
            buflen -= (aligned - (PRUptrdiff)buffer);
            buffer = (char*)aligned;
        }
    }
#endif  /* defined(_PR_HAVE_GETPROTO_R_INT) */

    if (PR_NETDB_BUF_SIZE > buflen)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

#if defined(_PR_HAVE_GETPROTO_R_POINTER)
    if (NULL == getprotobyname_r(name, res, buffer, buflen))
    {
        PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_ERRNO());
        return PR_FAILURE;
    }
#elif defined(_PR_HAVE_GETPROTO_R_INT)
    /*
    ** The buffer needs to be zero'd, and it should be
    ** at least the size of a struct protoent_data.
    */
    memset(buffer, 0, buflen);
	if (-1 == getprotobyname_r(name, res, (struct protoent_data*)buffer))
    {
        PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_ERRNO());
        return PR_FAILURE;
    }
#elif defined(_PR_HAVE_5_ARG_GETPROTO_R)
    /* The 5th argument for getprotobyname_r() cannot be NULL */
    if (-1 == getprotobyname_r(name, res, buffer, buflen, &res))
    {
        PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_ERRNO());
        return PR_FAILURE;
    }
#else  /* do it the hard way */
	{
		struct protoent *staticBuf;
		PR_Lock(_getproto_lock);
		staticBuf = getprotobyname_r(name);
		if (NULL == staticBuf)
		{
		    rv = PR_FAILURE;
		    PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_ERRNO());
        }
		else
		{
			rv = CopyProtoent(staticBuf, buffer, buflen, result);
			if (PR_FAILURE == rv)
			    PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
        }
		PR_Unlock(_getproto_lock);
	}
#endif  /* all that */
    return rv;
}

PR_IMPLEMENT(PRStatus) PR_GetProtoByNumber(
    PRInt32 number, char* buffer, PRInt32 buflen, PRProtoEnt* result)
{
	PRStatus rv = PR_SUCCESS;
#if defined(_PR_HAVE_GETPROTO_R)
	struct protoent* res = (struct protoent*)result;
#endif

    if (!_pr_initialized) _PR_ImplicitInitialization();

#if defined(_PR_HAVE_GETPROTO_R_INT)
    {
        /*
        ** The protoent_data has a pointer as the first field.
        ** That implies the buffer better be aligned, and char*
        ** doesn't promise much.
        */
        PRUptrdiff aligned = (PRUptrdiff)buffer;
        if (0 != (aligned & (sizeof(struct protoent_data*) - 1)))
        {
            aligned += sizeof(struct protoent_data*) - 1;
            aligned &= ~(sizeof(struct protoent_data*) - 1);
            buflen -= (aligned - (PRUptrdiff)buffer);
            buffer = (char*)aligned;
        }
    }
#endif /* defined(_PR_HAVE_GETPROTO_R_INT) */

    if (PR_NETDB_BUF_SIZE > buflen)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

#if defined(_PR_HAVE_GETPROTO_R_POINTER)
    if (NULL == getprotobynumber_r(number, res, buffer, buflen))
    {
        PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_ERRNO());
        return PR_FAILURE;
    }

#elif defined(_PR_HAVE_GETPROTO_R_INT)
    /*
    ** The buffer needs to be zero'd for these OS's.
    */
    memset(buffer, 0, buflen);
	if (-1 == getprotobynumber_r(number, res, (struct protoent_data*)buffer))
    {
        PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_ERRNO());
        return PR_FAILURE;
    }
#elif defined(_PR_HAVE_5_ARG_GETPROTO_R)
    /* The 5th argument for getprotobynumber_r() cannot be NULL */
    if (-1 == getprotobynumber_r(number, res, buffer, buflen, &res))
    {
        PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_ERRNO());
        return PR_FAILURE;
    }
#else  /* do it the hard way */
	{
		struct protoent *staticBuf;
		PR_Lock(_getproto_lock);
		staticBuf = getprotobynumber_r(number);
		if (NULL == staticBuf)
		{
		    rv = PR_FAILURE;
		    PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_ERRNO());
        }
		else
		{
			rv = CopyProtoent(staticBuf, buffer, buflen, result);
			if (PR_FAILURE == rv)
			    PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
        }
		PR_Unlock(_getproto_lock);
	}
#endif  /* all that crap */
    return rv;

}

PRUintn _PR_NetAddrSize(const PRNetAddr* addr)
{
    PRUintn addrsize;

    /*
     * RFC 2553 added a new field (sin6_scope_id) to
     * struct sockaddr_in6.  PRNetAddr's ipv6 member has a
     * scope_id field to match the new field.  In order to
     * work with older implementations supporting RFC 2133,
     * we take the size of struct sockaddr_in6 instead of
     * addr->ipv6.
     */
    if (AF_INET == addr->raw.family)
        addrsize = sizeof(addr->inet);
    else if (PR_AF_INET6 == addr->raw.family)
#if defined(_PR_INET6)
        addrsize = sizeof(struct sockaddr_in6);
#else
        addrsize = sizeof(addr->ipv6);
#endif
#if defined(XP_UNIX)
    else if (AF_UNIX == addr->raw.family)
        addrsize = sizeof(addr->local);
#endif
    else addrsize = 0;

    return addrsize;
}  /* _PR_NetAddrSize */

PR_IMPLEMENT(PRIntn) PR_EnumerateHostEnt(
    PRIntn enumIndex, const PRHostEnt *hostEnt, PRUint16 port, PRNetAddr *address)
{
    void *addr = hostEnt->h_addr_list[enumIndex++];
    memset(address, 0, sizeof(PRNetAddr));
    if (NULL == addr) enumIndex = 0;
    else
    {
        address->raw.family = hostEnt->h_addrtype;
        if (PR_AF_INET6 == hostEnt->h_addrtype)
        {
            address->ipv6.port = htons(port);
        	address->ipv6.flowinfo = 0;
        	address->ipv6.scope_id = 0;
            memcpy(&address->ipv6.ip, addr, hostEnt->h_length);
        }
        else
        {
            PR_ASSERT(AF_INET == hostEnt->h_addrtype);
            address->inet.port = htons(port);
            memcpy(&address->inet.ip, addr, hostEnt->h_length);
        }
    }
    return enumIndex;
}  /* PR_EnumerateHostEnt */

PR_IMPLEMENT(PRStatus) PR_InitializeNetAddr(
    PRNetAddrValue val, PRUint16 port, PRNetAddr *addr)
{
    PRStatus rv = PR_SUCCESS;
    if (!_pr_initialized) _PR_ImplicitInitialization();

	if (val != PR_IpAddrNull) memset(addr, 0, sizeof(addr->inet));
	addr->inet.family = AF_INET;
	addr->inet.port = htons(port);
	switch (val)
	{
	case PR_IpAddrNull:
		break;  /* don't overwrite the address */
	case PR_IpAddrAny:
		addr->inet.ip = htonl(INADDR_ANY);
		break;
	case PR_IpAddrLoopback:
		addr->inet.ip = htonl(INADDR_LOOPBACK);
		break;
	default:
		PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
		rv = PR_FAILURE;
	}
    return rv;
}  /* PR_InitializeNetAddr */

PR_IMPLEMENT(PRStatus) PR_SetNetAddr(
    PRNetAddrValue val, PRUint16 af, PRUint16 port, PRNetAddr *addr)
{
    PRStatus rv = PR_SUCCESS;
    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (af == PR_AF_INET6)
    {
        if (val != PR_IpAddrNull) memset(addr, 0, sizeof(addr->ipv6));
        addr->ipv6.family = af;
        addr->ipv6.port = htons(port);
        addr->ipv6.flowinfo = 0;
        addr->ipv6.scope_id = 0;
        switch (val)
        {
        case PR_IpAddrNull:
            break;  /* don't overwrite the address */
        case PR_IpAddrAny:
            addr->ipv6.ip = _pr_in6addr_any;
            break;
        case PR_IpAddrLoopback:
            addr->ipv6.ip = _pr_in6addr_loopback;
            break;
        default:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            rv = PR_FAILURE;
        }
    }
    else
    {
        if (val != PR_IpAddrNull) memset(addr, 0, sizeof(addr->inet));
        addr->inet.family = af;
        addr->inet.port = htons(port);
        switch (val)
        {
        case PR_IpAddrNull:
            break;  /* don't overwrite the address */
        case PR_IpAddrAny:
            addr->inet.ip = htonl(INADDR_ANY);
            break;
        case PR_IpAddrLoopback:
            addr->inet.ip = htonl(INADDR_LOOPBACK);
            break;
        default:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            rv = PR_FAILURE;
        }
    }
    return rv;
}  /* PR_SetNetAddr */

PR_IMPLEMENT(PRBool)
PR_IsNetAddrType(const PRNetAddr *addr, PRNetAddrValue val)
{
    if (addr->raw.family == PR_AF_INET6) {
        if (val == PR_IpAddrAny) {
			if (_PR_IN6_IS_ADDR_UNSPECIFIED((PRIPv6Addr *)&addr->ipv6.ip)) {
            	return PR_TRUE;
			} else if (_PR_IN6_IS_ADDR_V4MAPPED((PRIPv6Addr *)&addr->ipv6.ip)
					&& _PR_IN6_V4MAPPED_TO_IPADDR((PRIPv6Addr *)&addr->ipv6.ip)
							== htonl(INADDR_ANY)) {
            	return PR_TRUE;
			}
        } else if (val == PR_IpAddrLoopback) {
            if (_PR_IN6_IS_ADDR_LOOPBACK((PRIPv6Addr *)&addr->ipv6.ip)) {
            	return PR_TRUE;
			} else if (_PR_IN6_IS_ADDR_V4MAPPED((PRIPv6Addr *)&addr->ipv6.ip)
					&& _PR_IN6_V4MAPPED_TO_IPADDR((PRIPv6Addr *)&addr->ipv6.ip)
							== htonl(INADDR_LOOPBACK)) {
            	return PR_TRUE;
			}
        } else if (val == PR_IpAddrV4Mapped
                && _PR_IN6_IS_ADDR_V4MAPPED((PRIPv6Addr *)&addr->ipv6.ip)) {
            return PR_TRUE;
        }
    } else {
        if (addr->raw.family == AF_INET) {
            if (val == PR_IpAddrAny && addr->inet.ip == htonl(INADDR_ANY)) {
                return PR_TRUE;
            } else if (val == PR_IpAddrLoopback
                    && addr->inet.ip == htonl(INADDR_LOOPBACK)) {
                return PR_TRUE;
            }
        }
    }
    return PR_FALSE;
}

#ifndef _PR_INET6
#define XX 127
static const unsigned char index_hex[256] = {
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
     0, 1, 2, 3,  4, 5, 6, 7,  8, 9,XX,XX, XX,XX,XX,XX,
    XX,10,11,12, 13,14,15,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,10,11,12, 13,14,15,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};

/*
 * StringToV6Addr() returns 1 if the conversion succeeds,
 * or 0 if the input is not a valid IPv6 address string.
 * (Same as inet_pton(AF_INET6, string, addr).)
 */
static int StringToV6Addr(const char *string, PRIPv6Addr *addr)
{
    const unsigned char *s = (const unsigned char *)string;
    int section = 0;        /* index of the current section (a 16-bit
                             * piece of the address */
    int double_colon = -1;  /* index of the section after the first
                             * 16-bit group of zeros represented by
                             * the double colon */
    unsigned int val;
    int len;

    /* Handle initial (double) colon */
    if (*s == ':') {
        if (s[1] != ':') return 0;
        s += 2;
        addr->pr_s6_addr16[0] = 0;
        section = double_colon = 1;
    }

    while (*s) {
        if (section == 8) return 0; /* too long */
        if (*s == ':') {
            if (double_colon != -1) return 0; /* two double colons */
            addr->pr_s6_addr16[section++] = 0;
            double_colon = section;
            s++;
            continue;
        }
        for (len = val = 0; len < 4 && index_hex[*s] != XX; len++) {
            val = (val << 4) + index_hex[*s++];
        }
        if (*s == '.') {
            if (len == 0) return 0; /* nothing between : and . */
            break;
        }
        if (*s == ':') {
            s++;
            if (!*s) return 0; /* cannot end with single colon */
        } else if (*s) {
            return 0; /* bad character */
        }
        addr->pr_s6_addr16[section++] = htons((unsigned short)val);
    }
    
    if (*s == '.') {
        /* Have a trailing v4 format address */
        if (section > 6) return 0; /* not enough room */

        /*
         * The number before the '.' is decimal, but we parsed it
         * as hex.  That means it is in BCD.  Check it for validity
         * and convert it to binary.
         */
        if (val > 0x0255 || (val & 0xf0) > 0x90 || (val & 0xf) > 9) return 0;
        val = (val >> 8) * 100 + ((val >> 4) & 0xf) * 10 + (val & 0xf);
        addr->pr_s6_addr[2 * section] = val;

        s++;
        val = index_hex[*s++];
        if (val > 9) return 0;
        while (*s >= '0' && *s <= '9') {
            val = val * 10 + *s++ - '0';
            if (val > 255) return 0;
        }
        if (*s != '.') return 0; /* must have exactly 4 decimal numbers */
        addr->pr_s6_addr[2 * section + 1] = val;
        section++;

        s++;
        val = index_hex[*s++];
        if (val > 9) return 0;
        while (*s >= '0' && *s <= '9') {
            val = val * 10 + *s++ - '0';
            if (val > 255) return 0;
        }
        if (*s != '.') return 0; /* must have exactly 4 decimal numbers */
        addr->pr_s6_addr[2 * section] = val;

        s++;
        val = index_hex[*s++];
        if (val > 9) return 0;
        while (*s >= '0' && *s <= '9') {
            val = val * 10 + *s++ - '0';
            if (val > 255) return 0;
        }
        if (*s) return 0; /* must have exactly 4 decimal numbers */
        addr->pr_s6_addr[2 * section + 1] = val;
        section++;
    }
    
    if (double_colon != -1) {
        /* Stretch the double colon */
        int tosection;
        int ncopy = section - double_colon;
        for (tosection = 7; ncopy--; tosection--) {
            addr->pr_s6_addr16[tosection] = 
                addr->pr_s6_addr16[double_colon + ncopy];
        }
        while (tosection >= double_colon) {
            addr->pr_s6_addr16[tosection--] = 0;
        }
    } else if (section != 8) {
        return 0; /* too short */
    }
    return 1;
}
#undef XX
            
static const char *basis_hex = "0123456789abcdef";

/*
 * V6AddrToString() returns a pointer to the buffer containing
 * the text string if the conversion succeeds, and NULL otherwise.
 * (Same as inet_ntop(AF_INET6, addr, buf, size), except that errno
 * is not set on failure.)
 */
static const char *V6AddrToString(
    const PRIPv6Addr *addr, char *buf, PRUint32 size)
{
#define STUFF(c) do { \
    if (!size--) return NULL; \
    *buf++ = (c); \
} while (0)

    int double_colon = -1;          /* index of the first 16-bit
                                     * group of zeros represented
                                     * by the double colon */
    int double_colon_length = 1;    /* use double colon only if
                                     * there are two or more 16-bit
                                     * groups of zeros */
    int zero_length;
    int section;
    unsigned int val;
    const char *bufcopy = buf;

    /* Scan to find the placement of the double colon */
    for (section = 0; section < 8; section++) {
        if (addr->pr_s6_addr16[section] == 0) {
            zero_length = 1;
            section++;
            while (section < 8 && addr->pr_s6_addr16[section] == 0) {
                zero_length++;
                section++;
            }
            /* Select the longest sequence of zeros */
            if (zero_length > double_colon_length) {
                double_colon = section - zero_length;
                double_colon_length = zero_length;
            }
        }
    }

    /* Now start converting to a string */
    section = 0;

    if (double_colon == 0) {
        if (double_colon_length == 6 ||
            (double_colon_length == 5 && addr->pr_s6_addr16[5] == 0xffff)) {
            /* ipv4 format address */
            STUFF(':');
            STUFF(':');
            if (double_colon_length == 5) {
                STUFF('f');
                STUFF('f');
                STUFF('f');
                STUFF('f');
                STUFF(':');
            }
            if (addr->pr_s6_addr[12] > 99) STUFF(addr->pr_s6_addr[12]/100 + '0');
            if (addr->pr_s6_addr[12] > 9) STUFF((addr->pr_s6_addr[12]%100)/10 + '0');
            STUFF(addr->pr_s6_addr[12]%10 + '0');
            STUFF('.');
            if (addr->pr_s6_addr[13] > 99) STUFF(addr->pr_s6_addr[13]/100 + '0');
            if (addr->pr_s6_addr[13] > 9) STUFF((addr->pr_s6_addr[13]%100)/10 + '0');
            STUFF(addr->pr_s6_addr[13]%10 + '0');
            STUFF('.');
            if (addr->pr_s6_addr[14] > 99) STUFF(addr->pr_s6_addr[14]/100 + '0');
            if (addr->pr_s6_addr[14] > 9) STUFF((addr->pr_s6_addr[14]%100)/10 + '0');
            STUFF(addr->pr_s6_addr[14]%10 + '0');
            STUFF('.');
            if (addr->pr_s6_addr[15] > 99) STUFF(addr->pr_s6_addr[15]/100 + '0');
            if (addr->pr_s6_addr[15] > 9) STUFF((addr->pr_s6_addr[15]%100)/10 + '0');
            STUFF(addr->pr_s6_addr[15]%10 + '0');
            STUFF('\0');
            return bufcopy;
        }
    }

    while (section < 8) {
        if (section == double_colon) {
            STUFF(':');
            STUFF(':');
            section += double_colon_length;
            continue;
        }
        val = ntohs(addr->pr_s6_addr16[section]);
        if (val > 0xfff) {
            STUFF(basis_hex[val >> 12]);
        }
        if (val > 0xff) {
            STUFF(basis_hex[(val >> 8) & 0xf]);
        }
        if (val > 0xf) {
            STUFF(basis_hex[(val >> 4) & 0xf]);
        }
        STUFF(basis_hex[val & 0xf]);
        section++;
        if (section < 8 && section != double_colon) STUFF(':');
    }
    STUFF('\0');
    return bufcopy;
#undef STUFF    
}

#endif /* !_PR_INET6 */

PR_IMPLEMENT(PRStatus) PR_StringToNetAddr(const char *string, PRNetAddr *addr)
{
    PRStatus status = PR_SUCCESS;
    PRIntn rv;

#if defined(_PR_INET6)
    rv = inet_pton(AF_INET6, string, &addr->ipv6.ip);
    if (1 == rv)
    {
        addr->raw.family = PR_AF_INET6;
    }
    else
    {
        PR_ASSERT(0 == rv);
        /* clean up after the failed inet_pton() call */
        memset(&addr->ipv6.ip, 0, sizeof(addr->ipv6.ip));
        rv = inet_pton(AF_INET, string, &addr->inet.ip);
        if (1 == rv)
        {
            addr->raw.family = AF_INET;
        }
        else
        {
            PR_ASSERT(0 == rv);
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            status = PR_FAILURE;
        }
    }
#else /* _PR_INET6 */
    rv = StringToV6Addr(string, &addr->ipv6.ip);
    if (1 == rv) {
        addr->raw.family = PR_AF_INET6;
        return PR_SUCCESS;
    }
    PR_ASSERT(0 == rv);
    /* clean up after the failed StringToV6Addr() call */
    memset(&addr->ipv6.ip, 0, sizeof(addr->ipv6.ip));

    addr->inet.family = AF_INET;
#ifdef XP_OS2_VACPP
    addr->inet.ip = inet_addr((char *)string);
#else
    addr->inet.ip = inet_addr(string);
#endif
    if ((PRUint32) -1 == addr->inet.ip)
    {
        /*
         * The string argument is a malformed address string.
         */
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        status = PR_FAILURE;
    }
#endif /* _PR_INET6 */

    return status;
}

PR_IMPLEMENT(PRStatus) PR_NetAddrToString(
    const PRNetAddr *addr, char *string, PRUint32 size)
{
    if (PR_AF_INET6 == addr->raw.family)
    {
#if defined(_PR_INET6)
        if (NULL == inet_ntop(AF_INET6, &addr->ipv6.ip, string, size))
#else
        if (NULL == V6AddrToString(&addr->ipv6.ip, string, size))
#endif
        {
            /* the size of the result buffer is inadequate */
            PR_SetError(PR_BUFFER_OVERFLOW_ERROR, 0);
            return PR_FAILURE;
        }
    }
    else
    {
        if (size < 16) goto failed;
        if (AF_INET != addr->raw.family) goto failed;
        else
        {
            unsigned char *byte = (unsigned char*)&addr->inet.ip;
            PR_snprintf(string, size, "%u.%u.%u.%u",
                byte[0], byte[1], byte[2], byte[3]);
        }
    }

    return PR_SUCCESS;

failed:
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return PR_FAILURE;

}  /* PR_NetAddrToString */

/*
 * Convert an IPv4 addr to an (IPv4-mapped) IPv6 addr
 */
PR_IMPLEMENT(void) PR_ConvertIPv4AddrToIPv6(PRUint32 v4addr, PRIPv6Addr *v6addr)
{
    PRUint8 *dstp;
    dstp = v6addr->pr_s6_addr;
    memset(dstp, 0, 10);
    memset(dstp + 10, 0xff, 2);
    memcpy(dstp + 12,(char *) &v4addr, 4);
}

PR_IMPLEMENT(PRUint16) PR_ntohs(PRUint16 n) { return ntohs(n); }
PR_IMPLEMENT(PRUint32) PR_ntohl(PRUint32 n) { return ntohl(n); }
PR_IMPLEMENT(PRUint16) PR_htons(PRUint16 n) { return htons(n); }
PR_IMPLEMENT(PRUint32) PR_htonl(PRUint32 n) { return htonl(n); }
PR_IMPLEMENT(PRUint64) PR_ntohll(PRUint64 n)
{
#ifdef IS_BIG_ENDIAN
    return n;
#else
    PRUint64 tmp;
    PRUint32 hi, lo;
    LL_L2UI(lo, n);
    LL_SHR(tmp, n, 32);
    LL_L2UI(hi, tmp);
    hi = PR_ntohl(hi);
    lo = PR_ntohl(lo);
    LL_UI2L(n, lo);
    LL_SHL(n, n, 32);
    LL_UI2L(tmp, hi);
    LL_ADD(n, n, tmp);
    return n;
#endif
}  /* ntohll */

PR_IMPLEMENT(PRUint64) PR_htonll(PRUint64 n)
{
#ifdef IS_BIG_ENDIAN
    return n;
#else
    PRUint64 tmp;
    PRUint32 hi, lo;
    LL_L2UI(lo, n);
    LL_SHR(tmp, n, 32);
    LL_L2UI(hi, tmp);
    hi = htonl(hi);
    lo = htonl(lo);
    LL_UI2L(n, lo);
    LL_SHL(n, n, 32);
    LL_UI2L(tmp, hi);
    LL_ADD(n, n, tmp);
    return n;
#endif
}  /* htonll */
