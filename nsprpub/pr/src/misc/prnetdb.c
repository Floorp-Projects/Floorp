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

#if defined(_PR_NO_PREEMPT)
#define LOCK_DNS()
#define UNLOCK_DNS()
#else
PRLock *_pr_dnsLock = NULL;
#define LOCK_DNS() PR_Lock(_pr_dnsLock)
#define UNLOCK_DNS() PR_Unlock(_pr_dnsLock)
#endif  /* defined(_PR_NO_PREEMPT) */

#if defined(XP_UNIX)
#include <signal.h>

/*
** Unix's, as a rule, have a bug in their select code: if a timer
** interrupt occurs and you have SA_RESTART set on your signal, select
** forgets how much time has elapsed and restarts the system call from
** the beginning. This can cause a call to select to *never* time out.
**
** Because we aren't certain that select is wrapped properly in this code
** we disable the clock while a dns operation is occuring. This sucks and
** can be tossed when implement our own dns code that calls our own
** PR_Poll.
*/

static sigset_t timer_set;
#define DISABLECLOCK(_set)    sigprocmask(SIG_BLOCK, &timer_set, _set)
#define ENABLECLOCK(_set)    sigprocmask(SIG_SETMASK, _set, 0)

#endif /* XP_UNIX */

/*
 * Some platforms have the reentrant getprotobyname_r() and
 * getprotobynumber_r().  However, they come in two flavors.
 * Some return a pointer to struct protoent, others return
 * an int.
 */

#if defined(SOLARIS) \
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

#if defined(_PR_INET6)
PRBool _pr_ipv6_enabled = PR_FALSE;
#if defined(AIX)
const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;
#else
extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;
#endif /* AIX */
#endif /* _PR_INET6 */

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
	sigemptyset(&timer_set);
	sigaddset(&timer_set, SIGALRM);
#endif
#if !defined(_PR_NO_PREEMPT)
	_pr_dnsLock = PR_NewLock();
#endif
#if !defined(_PR_HAVE_GETPROTO_R)
	_getproto_lock = PR_NewLock();
#endif

}

PR_IMPLEMENT(PRStatus) PR_SetIPv6Enable(PRBool itIs)
{
#if defined(XP_MAC)
#pragma unused (itIs)
#endif

#if defined(_PR_INET6)
    _pr_ipv6_enabled = itIs;
    return PR_SUCCESS;
#else /* defined(_PR_INET6) */
    PR_SetError(PR_PROTOCOL_NOT_SUPPORTED_ERROR, 0);
    return PR_FAILURE;
#endif /* defined(_PR_INET6) */
}  /* PR_SetIPv6Enable */

PR_IMPLEMENT(PRStatus) PR_GetHostName(char *name, PRUint32 namelen)
{
#if defined(DEBUG)
    static PRBool warn = PR_TRUE;
    if (warn) warn = _PR_Obsolete("PR_GetHostName()", "PR_GetSystemInfo()");
#endif
    return PR_GetSystemInfo(PR_SI_HOSTNAME, name, namelen);
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

/*
** Copy a hostent, and all of the memory that it refers to into
** (hopefully) stacked buffers.
*/
static PRStatus CopyHostent(
    struct hostent *from, char *buf, PRIntn bufsize, PRHostEnt *to)
{
	PRIntn len, na;
	char **ap;

	/* Do the easy stuff */
	to->h_addrtype = from->h_addrtype;
	to->h_length = from->h_length;

	/* Copy the official name */
	if (!from->h_name) return PR_FAILURE;
	len = strlen(from->h_name) + 1;
	to->h_name = Alloc(len, &buf, &bufsize, 0);
	if (!to->h_name) return PR_FAILURE;
	memcpy(to->h_name, from->h_name, len);

	/* Count the aliases, then allocate storage for the pointers */
	for (na = 1, ap = from->h_aliases; *ap != 0; na++, ap++){;} /* nothing to execute */
	to->h_aliases = (char**)Alloc(
	    na * sizeof(char*), &buf, &bufsize, sizeof(char**));
	if (!to->h_aliases) return PR_FAILURE;

	/* Copy the aliases, one at a time */
	for (na = 0, ap = from->h_aliases; *ap != 0; na++, ap++) {
		len = strlen(*ap) + 1;
		to->h_aliases[na] = Alloc(len, &buf, &bufsize, 0);
		if (!to->h_aliases[na]) return PR_FAILURE;
		memcpy(to->h_aliases[na], *ap, len);
	}
	to->h_aliases[na] = 0;

	/* Count the addresses, then allocate storage for the pointers */
	for (na = 1, ap = from->h_addr_list; *ap != 0; na++, ap++){;} /* nothing to execute */
	to->h_addr_list = (char**)Alloc(
	    na * sizeof(char*), &buf, &bufsize, sizeof(char**));
	if (!to->h_addr_list) return PR_FAILURE;

	/* Copy the addresses, one at a time */
	for (na = 0, ap = from->h_addr_list; *ap != 0; na++, ap++) {
		to->h_addr_list[na] = Alloc(to->h_length, &buf, &bufsize, 0);
		if (!to->h_addr_list[na]) return PR_FAILURE;
		memcpy(to->h_addr_list[na], *ap, to->h_length);
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

PR_IMPLEMENT(PRStatus) PR_GetHostByName(
    const char *name, char *buf, PRIntn bufsize, PRHostEnt *hp)
{
	struct hostent *h;
	PRStatus rv = PR_FAILURE;
#ifdef XP_UNIX
	sigset_t oldset;
#endif

    if (!_pr_initialized) _PR_ImplicitInitialization();

#ifdef XP_UNIX
	DISABLECLOCK(&oldset);
#endif
	LOCK_DNS();

#ifdef _PR_INET6
    if (_pr_ipv6_enabled)
    {
        h = gethostbyname2(name, AF_INET6);
        if (NULL == h)
        {
            h = gethostbyname2(name, AF_INET);
        }
    }
    else
    {
#ifdef XP_OS2_VACPP
	    h = gethostbyname((char *)name);
#else
        h = gethostbyname(name);
#endif
    }
#else
#ifdef XP_OS2_VACPP
	h = gethostbyname((char *)name);
#else
    h = gethostbyname(name);
#endif
#endif /* _PR_INET6 */
    
	if (NULL == h)
	    PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_GETHOST_ERRNO());
	else
	{
		rv = CopyHostent(h, buf, bufsize, hp);
		if (PR_SUCCESS != rv)
		    PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
	}
	UNLOCK_DNS();
#ifdef XP_UNIX
	ENABLECLOCK(&oldset);
#endif
	return rv;
}

PR_IMPLEMENT(PRStatus) PR_GetHostByAddr(
    const PRNetAddr *hostaddr, char *buf, PRIntn bufsize, PRHostEnt *hostentry)
{
	struct hostent *h;
	PRStatus rv = PR_FAILURE;
	const void *addr;
	int addrlen;
#ifdef XP_UNIX
	sigset_t oldset;
#endif

    if (!_pr_initialized) _PR_ImplicitInitialization();

#ifdef XP_UNIX
	DISABLECLOCK(&oldset);
#endif
	LOCK_DNS();
#if defined(_PR_INET6)
	if (hostaddr->raw.family == AF_INET6)
	{
		addr = &hostaddr->ipv6.ip;
		addrlen = sizeof(hostaddr->ipv6.ip);
	}
	else
#endif /* defined(_PR_INET6) */
	{
		PR_ASSERT(hostaddr->raw.family == AF_INET);
		addr = &hostaddr->inet.ip;
		addrlen = sizeof(hostaddr->inet.ip);
	}
#ifdef XP_OS2_VACPP
	h = gethostbyaddr((char *)addr, addrlen, hostaddr->raw.family);
#else
	h = gethostbyaddr(addr, addrlen, hostaddr->raw.family);
#endif
	if (NULL == h) PR_SetError(PR_DIRECTORY_LOOKUP_ERROR, _MD_GETHOST_ERRNO());
	else
	{
		rv = CopyHostent(h, buf, bufsize, hostentry);
		if (PR_SUCCESS != rv) {
		    PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
		}
	}
	UNLOCK_DNS();
#ifdef XP_UNIX
	ENABLECLOCK(&oldset);
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

	PR_ASSERT(PR_NETDB_BUF_SIZE <= buflen);
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

	PR_ASSERT(PR_NETDB_BUF_SIZE <= buflen);
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

PR_IMPLEMENT(PRUintn) PR_NetAddrSize(const PRNetAddr* addr)
{
    PRUintn addrsize;

    if (AF_INET == addr->raw.family)
        addrsize = sizeof(addr->inet);
#if defined(_PR_INET6)
    else if (AF_INET6 == addr->raw.family)
        addrsize = sizeof(addr->ipv6);
#endif
#if defined(XP_UNIX)
    else if (AF_UNIX == addr->raw.family)
        addrsize = sizeof(addr->local);
#endif
    else addrsize = 0;

    return addrsize;
}  /* PR_NetAddrSize */

PR_IMPLEMENT(PRIntn) PR_EnumerateHostEnt(
    PRIntn enumIndex, const PRHostEnt *hostEnt, PRUint16 port, PRNetAddr *address)
{
    void *addr = hostEnt->h_addr_list[enumIndex++];
    memset(address, 0, sizeof(PRNetAddr));
    if (NULL == addr) enumIndex = 0;
    else
    {
#if defined(_PR_INET6)
        if (_pr_ipv6_enabled)
        {
            address->ipv6.family = AF_INET6;
            address->ipv6.port = htons(port);
            if (AF_INET6 == hostEnt->h_addrtype)
                memcpy(&address->ipv6.ip, addr, hostEnt->h_length);
            else
            {
                unsigned char *start = (unsigned char *) &address->ipv6.ip;
                PR_ASSERT(AF_INET == hostEnt->h_addrtype);
                memset(start, 0, 10);
                memset(start + 10, 0xff, 2);
                memcpy(start + 12, addr, hostEnt->h_length);
                PR_ASSERT(IN6_IS_ADDR_V4MAPPED(&address->ipv6.ip));
            }
        }
        else
#endif /* defined(_PR_INET6) */
        {
            PR_ASSERT(AF_INET == hostEnt->h_addrtype);
            address->inet.family = hostEnt->h_addrtype;
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

#if defined(_PR_INET6)
    if (_pr_ipv6_enabled)
    {
        addr->ipv6.family = AF_INET6;
        addr->ipv6.port = htons(port);
        switch (val)
        {
        case PR_IpAddrNull:
            break;  /* don't overwrite the address */
        case PR_IpAddrAny:
            addr->ipv6.ip = in6addr_any;
            break;
        case PR_IpAddrLoopback:
            addr->ipv6.ip = in6addr_loopback;
            break;
        default:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            rv = PR_FAILURE;
        }
    }
    else
#endif  /* defined(_PR_INET6) */
    {
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
    }
    return rv;
}  /* PR_InitializeNetAddr */

PR_IMPLEMENT(PRNetAddr*) PR_CreateNetAddr(PRNetAddrValue val, PRUint16 port)
{
    PRNetAddr *addr = NULL;
    if ((PR_IpAddrAny == val) || (PR_IpAddrLoopback == val))
    {
        addr = PR_NEWZAP(PRNetAddr);
        if (NULL == addr)
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        else
            if (PR_FAILURE == PR_InitializeNetAddr(val, port, addr))
                PR_DELETE(addr);  /* and that will make 'addr' == NULL */
    }
    else
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return addr;
}  /* PR_CreateNetAddr */

PR_IMPLEMENT(PRStatus) PR_DestroyNetAddr(PRNetAddr *addr)
{
    PR_Free(addr);
    return PR_SUCCESS;
}  /* PR_DestroyNetAddr */

PR_IMPLEMENT(PRStatus) PR_StringToNetAddr(const char *string, PRNetAddr *addr)
{
    /*
    ** If we're built to support IPv6 addressing AND it's currently enabled,
    ** then all addresses are of the IPv6 addressing family. Both are required
    ** before anything overt happens.
    */
    PRStatus status = PR_InitializeNetAddr(PR_IpAddrNull, 0, addr);

    PR_ASSERT(PR_SUCCESS == status);
    if (PR_SUCCESS != status) return status;

#if defined(_PR_INET6)

    if (_pr_ipv6_enabled)
    {
        /*
        ** Okay, we're doing it.
        */
        PRIntn rv = inet_pton(AF_INET6, string, &addr->ipv6.ip);
        if (1 != rv)
        {
            /*
             * rv is 0 if the string argument is not a valid IPv4 or IPv6
	         * address string.
             * rv is -1 with errno set to EADNOSUPPORT if the af argument is
             * not a known address family.
             */
            PRIntn syserrno = (-1 == rv) ? errno : 0;
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, syserrno);
            status = PR_FAILURE;
        }
    }
    else
#endif
    {
	    PRUint32 *ip = (PRUint32*)&addr->inet.ip;

#ifdef XP_OS2_VACPP
        *ip = inet_addr((char *)string);
#else
        *ip = inet_addr(string);
#endif
	    if ((PRUint32) -1 == *ip)
        {
            /*
             * Either the af argument is not AF_INET, or the string argument
             * is a malformed address string.
             */
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            status = PR_FAILURE;
        }
    }

	return status;
}

PR_IMPLEMENT(PRStatus) PR_NetAddrToString(
    const PRNetAddr *addr, char *string, PRUint32 size)
{
    PR_ASSERT(size >= 16);
    if (size < 16) goto failed;

#if defined(_PR_INET6)
    if (_pr_ipv6_enabled)
    {
        PR_ASSERT(AF_INET6 == addr->ipv6.family);
        if ((AF_INET6 != addr->ipv6.family)
        || (NULL == inet_ntop(AF_INET6, &addr->ipv6.ip, string, size)))
            goto failed;
    }
    else
#endif  /* defined(_PR_INET6) */
    {
        PR_ASSERT(AF_INET == addr->inet.family);
        if (AF_INET != addr->inet.family) goto failed;
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

PR_IMPLEMENT(PRUint16) PR_ntohs(PRUint16 n) { return ntohs(n); }
PR_IMPLEMENT(PRUint32) PR_ntohl(PRUint32 n) { return ntohl(n); }
PR_IMPLEMENT(PRUint16) PR_htons(PRUint16 n) { return htons(n); }
PR_IMPLEMENT(PRUint32) PR_htonl(PRUint32 n) { return htonl(n); }
PR_IMPLEMENT(PRUint64) PR_ntohll(PRUint64 n)
{
    /*
    ** There is currently no attempt to optomize out depending
    ** on the host' byte order. That would be easy enough to
    ** do.
    */
    PRUint64 tmp;
    PRUint32 hi, lo;
    LL_L2UI(lo, n);
    LL_SHR(tmp, n, 32);
    LL_L2UI(hi, tmp);
    hi = PR_ntohl(hi);
    lo = PR_ntohl(lo);
    LL_UI2L(n, hi);
    LL_SHL(n, n, 32);
    LL_UI2L(tmp, lo);
    LL_ADD(n, n, tmp);
    return n;
}  /* ntohll */

PR_IMPLEMENT(PRUint64) PR_htonll(PRUint64 n)
{
    /*
    ** There is currently no attempt to optomize out depending
    ** on the host' byte order. That would be easy enough to
    ** do.
    */
    PRUint64 tmp;
    PRUint32 hi, lo;
    LL_L2UI(lo, n);
    LL_SHR(tmp, n, 32);
    LL_L2UI(hi, tmp);
    hi = htonl(hi);
    lo = htonl(lo);
    LL_UI2L(n, hi);
    LL_SHL(n, n, 32);
    LL_UI2L(tmp, lo);
    LL_ADD(n, n, tmp);
    return n;
}  /* htonll */

PR_IMPLEMENT(PRUint16) PR_FamilyInet(void)
{
#ifdef _PR_INET6
    return (_pr_ipv6_enabled ? AF_INET6 : AF_INET);
#else
    return AF_INET;
#endif
}
