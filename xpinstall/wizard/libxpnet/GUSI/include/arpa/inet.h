/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * [¤3 Deleted as of 22Jul99, see
 *     ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change
 *	   for details]
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)inet.h	8.1 (Berkeley) 6/2/93
 */

#ifndef _INET_H_
#define	_INET_H_

/* Adapted for GUSI by Matthias Neeracher <neeri@iis.ee.ethz.ch> */

/* External definitions for functions in inet(3) */

#include <sys/cdefs.h>

/* XNS mandates availability of xtonx() and uintx_t -- neeri */

#include <netinet/in.h>
#include <machine/endian.h>

__BEGIN_DECLS
in_addr_t	 	inet_addr __P((const char *));
int		 		inet_aton __P((const char *, struct in_addr *));
in_addr_t	 	inet_lnaof __P((struct in_addr));
struct in_addr	inet_makeaddr __P((uint32_t , in_addr_t));
in_addr_t	 	inet_netof __P((struct in_addr));
in_addr_t		inet_network __P((const char *));
char		     *inet_ntoa __P((struct in_addr));
__END_DECLS

#endif /* !_INET_H_ */
