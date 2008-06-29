/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "primpl.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>


#if defined(SOLARIS)

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    hrtime_t t;
    t = gethrtime();
    if (t) {
	    return _pr_CopyLowBits(buf, maxbytes, &t, sizeof(t));
    }
    return 0;
}

#elif defined(SUNOS4)

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    return 0;
}

#elif defined(HPUX)

#ifdef __ia64
#include <ia64/sys/inline.h>

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    PRUint64 t;

    t = _Asm_mov_from_ar(_AREG44);
    return _pr_CopyLowBits(buf, maxbytes, &t, sizeof(t));
}
#else
static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    extern int ret_cr16();
    int cr16val;

    cr16val = ret_cr16();
    return(_pr_CopyLowBits(buf, maxbytes, &cr16val, sizeof(cr16val)));
}
#endif

#elif defined(OSF1)

#include <c_asm.h>

/*
 * Use the "get the cycle counter" instruction on the alpha.
 * The low 32 bits completely turn over in less than a minute.
 * The high 32 bits are some non-counter gunk that changes sometimes.
 */
static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    unsigned long t;

#ifdef __GNUC__
    __asm__("rpcc %0" : "=r" (t));
#else
    t = asm("rpcc %v0");
#endif
    return _pr_CopyLowBits(buf, maxbytes, &t, sizeof(t));
}

#elif defined(VMS)

#include <ints.h>

/*
 * Use the "get the cycle counter" instruction on the alpha.
 * The low 32 bits completely turn over in less than a minute.
 * The high 32 bits are some non-counter gunk that changes sometimes.
 */
static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    uint64 t;

    t = __RPCC();
    return _pr_CopyLowBits(buf, maxbytes, &t, sizeof(t));
}

#elif defined(AIX)

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    return 0;
}

#elif (defined(LINUX) || defined(FREEBSD) || defined(__FreeBSD_kernel__) \
    || defined(NETBSD) || defined(__NetBSD_kernel__) || defined(OPENBSD))
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int      fdDevRandom;
static PRCallOnceType coOpenDevRandom;

static PRStatus OpenDevRandom( void )
{
    fdDevRandom = open( "/dev/random", O_RDONLY );
    return((-1 == fdDevRandom)? PR_FAILURE : PR_SUCCESS );
} /* end OpenDevRandom() */

static size_t GetDevRandom( void *buf, size_t size )
{
    int bytesIn;
    int rc;

    rc = PR_CallOnce( &coOpenDevRandom, OpenDevRandom );
    if ( PR_FAILURE == rc ) {
        _PR_MD_MAP_OPEN_ERROR( errno );
        return(0);
    }

    bytesIn = read( fdDevRandom, buf, size );
    if ( -1 == bytesIn ) {
        _PR_MD_MAP_READ_ERROR( errno );
        return(0);
    }

    return( bytesIn );
} /* end GetDevRandom() */

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{             
    return(GetDevRandom( buf, maxbytes ));
}

#elif defined(NCR)

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    return 0;
}

#elif defined(IRIX)
#include <fcntl.h>
#undef PRIVATE
#include <sys/mman.h>
#include <sys/syssgi.h>
#include <sys/immu.h>
#include <sys/systeminfo.h>
#include <sys/utsname.h>

static size_t GetHighResClock(void *buf, size_t maxbuf)
{
    unsigned phys_addr, raddr, cycleval;
    static volatile unsigned *iotimer_addr = NULL;
    static int tries = 0;
    static int cntr_size;
    int mfd;
    unsigned s0[2];

#ifndef SGI_CYCLECNTR_SIZE
#define SGI_CYCLECNTR_SIZE      165     /* Size user needs to use to read CC */
#endif

    if (iotimer_addr == NULL) {
	    if (tries++ > 1) {
	        /* Don't keep trying if it didn't work */
	        return 0;
	    }

	    /*
	    ** For SGI machines we can use the cycle counter, if it has one,
	    ** to generate some truly random numbers
	    */
	    phys_addr = syssgi(SGI_QUERY_CYCLECNTR, &cycleval);
	    if (phys_addr) {
	        int pgsz = getpagesize();
	        int pgoffmask = pgsz - 1;

	        raddr = phys_addr & ~pgoffmask;
	        mfd = open("/dev/mmem", O_RDONLY);
	        if (mfd < 0) {
    		    return 0;
	        }
	        iotimer_addr = (unsigned *)
		    mmap(0, pgoffmask, PROT_READ, MAP_PRIVATE, mfd, (int)raddr);
	        if (iotimer_addr == (unsigned*)-1) {
	    	    close(mfd);
		        iotimer_addr = NULL;
		        return 0;
	        }
	        iotimer_addr = (unsigned*)
		    ((__psint_t)iotimer_addr | (phys_addr & pgoffmask));
	        /*
	         * The file 'mfd' is purposefully not closed.
	         */
	        cntr_size = syssgi(SGI_CYCLECNTR_SIZE);
	        if (cntr_size < 0) {
    		    struct utsname utsinfo;

		        /* 
		         * We must be executing on a 6.0 or earlier system, since the
		         * SGI_CYCLECNTR_SIZE call is not supported.
		         * 
		         * The only pre-6.1 platforms with 64-bit counters are
		         * IP19 and IP21 (Challenge, PowerChallenge, Onyx).
		         */
		        uname(&utsinfo);
		        if (!strncmp(utsinfo.machine, "IP19", 4) ||
		            !strncmp(utsinfo.machine, "IP21", 4))
			        cntr_size = 64;
		        else
			        cntr_size = 32;
	        }
	        cntr_size /= 8;	/* Convert from bits to bytes */
	    }
    }

    s0[0] = *iotimer_addr;
    if (cntr_size > 4)
	s0[1] = *(iotimer_addr + 1);
    memcpy(buf, (char *)&s0[0], cntr_size);
    return _pr_CopyLowBits(buf, maxbuf, &s0, cntr_size);
}

#elif defined(SONY)

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    return 0;
}

#elif defined(SNI)
#include <sys/times.h>

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    int ticks;
    struct tms buffer;

    ticks=times(&buffer);
    return _pr_CopyLowBits(buf, maxbytes, &ticks, sizeof(ticks));
}

#elif defined(NEC)

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    return 0;
}
#elif defined(SCO) || defined(UNIXWARE) || defined(BSDI) || defined(NTO) \
    || defined(QNX) || defined(DARWIN) || defined(RISCOS)
#include <sys/times.h>

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    int ticks;
    struct tms buffer;

    ticks=times(&buffer);
    return _pr_CopyLowBits(buf, maxbytes, &ticks, sizeof(ticks));
}
#else
#error! Platform undefined
#endif /* defined(SOLARIS) */

extern PRSize _PR_MD_GetRandomNoise( void *buf, PRSize size )
{
    struct timeval tv;
    int n = 0;
    int s;

    n += GetHighResClock(buf, size);
    size -= n;

    GETTIMEOFDAY(&tv);

    if ( size > 0 ) {
        s = _pr_CopyLowBits((char*)buf+n, size, &tv.tv_usec, sizeof(tv.tv_usec));
        size -= s;
        n += s;
    }
    if ( size > 0 ) {
        s = _pr_CopyLowBits((char*)buf+n, size, &tv.tv_sec, sizeof(tv.tv_usec));
        size -= s;
        n += s;
    }

    return n;
} /* end _PR_MD_GetRandomNoise() */
