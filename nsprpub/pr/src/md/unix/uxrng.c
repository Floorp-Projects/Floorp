/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


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

#elif defined(HPUX)

#ifdef __ia64
#include <ia64/sys/inline.h>

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    PRUint64 t;

#ifdef __GNUC__
    __asm__ __volatile__("mov %0 = ar.itc" : "=r" (t));
#else
    t = _Asm_mov_from_ar(_AREG44);
#endif
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

#elif defined(AIX)

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    return 0;
}

#elif (defined(LINUX) || defined(FREEBSD) || defined(__FreeBSD_kernel__) \
    || defined(NETBSD) || defined(__NetBSD_kernel__) || defined(OPENBSD) \
    || defined(__GNU__))
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int      fdDevURandom;
static PRCallOnceType coOpenDevURandom;

static PRStatus OpenDevURandom( void )
{
    fdDevURandom = open( "/dev/urandom", O_RDONLY );
    return((-1 == fdDevURandom)? PR_FAILURE : PR_SUCCESS );
} /* end OpenDevURandom() */

static size_t GetDevURandom( void *buf, size_t size )
{
    int bytesIn;
    int rc;

    rc = PR_CallOnce( &coOpenDevURandom, OpenDevURandom );
    if ( PR_FAILURE == rc ) {
        _PR_MD_MAP_OPEN_ERROR( errno );
        return(0);
    }

    bytesIn = read( fdDevURandom, buf, size );
    if ( -1 == bytesIn ) {
        _PR_MD_MAP_READ_ERROR( errno );
        return(0);
    }

    return( bytesIn );
} /* end GetDevURandom() */

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    return(GetDevURandom( buf, maxbytes ));
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
