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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include "mpi.h"
#include "mpi-priv.h"

/* #define OLD_WAY 1  */

/* This key is the 1024-bit test key used for speed testing of RSA private 
** key ops.
*/

#define CONST const

static CONST unsigned char default_n[128] = {
/* modulus goes here */ 0
};

static CONST unsigned char default_d[128] = {
/* exponent goes here */ 0
};



#define DEFAULT_ITERS 50

typedef clock_t timetype;
#define gettime(x) *(x) = clock()
#define subtime(a, b) a -= b
#define msec(x) ((clock_t)((double)x * 1000.0 / CLOCKS_PER_SEC))
#define sec(x) (x / CLOCKS_PER_SEC)

struct TimingContextStr {
    timetype start;
    timetype end;
    timetype interval;

    int   minutes;  
    int   seconds;  
    int   millisecs;
};

typedef struct TimingContextStr TimingContext;

TimingContext *CreateTimingContext(void) 
{
    return (TimingContext *)malloc(sizeof(TimingContext));
}

void DestroyTimingContext(TimingContext *ctx) 
{
    free(ctx);
}

void TimingBegin(TimingContext *ctx) 
{
    gettime(&ctx->start);
}

static void timingUpdate(TimingContext *ctx) 
{

    ctx->millisecs = msec(ctx->interval) % 1000;
    ctx->seconds   = sec(ctx->interval);
    ctx->minutes   = ctx->seconds / 60;
    ctx->seconds  %= 60;

}

void TimingEnd(TimingContext *ctx) 
{
    gettime(&ctx->end);
    ctx->interval = ctx->end;
    subtime(ctx->interval, ctx->start);
    timingUpdate(ctx);
}

char *TimingGenerateString(TimingContext *ctx) 
{
    static char sBuf[4096];

    sprintf(sBuf, "%d minutes, %d.%03d seconds", ctx->minutes,
				ctx->seconds, ctx->millisecs);
    return sBuf;
}

static void
dumpBytes( unsigned char * b, int l)
{
    int i;
    if (l <= 0)
        return;
    for (i = 0; i < l; ++i) {
        if (i % 16 == 0)
            printf("\t");
        printf(" %02x", b[i]);
        if (i % 16 == 15)
            printf("\n");
    }
    if ((i % 16) != 0)
        printf("\n");
    printf("\n");
}

static mp_err
testNewFuncs(const unsigned char * modulusBytes, int modulus_len)
{
    mp_err mperr	= MP_OKAY;
    mp_int modulus;
    unsigned char buf[512];

    mperr = mp_init(&modulus);
    mperr = mp_read_unsigned_octets(&modulus,  modulusBytes,  modulus_len );
    mperr = mp_to_fixlen_octets(&modulus, buf, modulus_len);
    mperr = mp_to_fixlen_octets(&modulus, buf, modulus_len+1);
    mperr = mp_to_fixlen_octets(&modulus, buf, modulus_len+4);
    mperr = mp_to_unsigned_octets(&modulus, buf, modulus_len);
    mperr = mp_to_signed_octets(&modulus, buf, modulus_len + 1);
    mp_clear(&modulus);
    return mperr;
}

int
testModExp( const unsigned char *      modulusBytes, 
	    const unsigned int         expo,
	    const unsigned char *      input, 
	          unsigned char *      output,
	          int                  modulus_len)
{
    mp_err mperr	= MP_OKAY;
    mp_int modulus;
    mp_int base;
    mp_int exponent;
    mp_int result;

    mperr  = mp_init(&modulus);
    mperr += mp_init(&base);
    mperr += mp_init(&exponent);
    mperr += mp_init(&result);
    /* we initialize all mp_ints unconditionally, even if some fail.
    ** This guarantees that the DIGITS pointer is valid (even if null).
    ** So, mp_clear will do the right thing below. 
    */
    if (mperr == MP_OKAY) {
      mperr  = mp_read_unsigned_octets(&modulus,  modulusBytes,  modulus_len );
      mperr += mp_read_unsigned_octets(&base,     input,         modulus_len );
      mp_set(&exponent, expo);
      if (mperr == MP_OKAY) {
#if OLD_WAY
	mperr = s_mp_exptmod(&base, &exponent, &modulus, &result);
#else
	mperr = mp_exptmod(&base, &exponent, &modulus, &result);
#endif
	if (mperr == MP_OKAY) {
	  mperr = mp_to_fixlen_octets(&result, output, modulus_len);
	}
      }
    }
    mp_clear(&base);
    mp_clear(&result);

    mp_clear(&modulus);
    mp_clear(&exponent);

    return (int)mperr;
}

int
doModExp(   const unsigned char *      modulusBytes, 
	    const unsigned char *      exponentBytes, 
	    const unsigned char *      input, 
	          unsigned char *      output,
	          int                  modulus_len)
{
    mp_err mperr	= MP_OKAY;
    mp_int modulus;
    mp_int base;
    mp_int exponent;
    mp_int result;

    mperr  = mp_init(&modulus);
    mperr += mp_init(&base);
    mperr += mp_init(&exponent);
    mperr += mp_init(&result);
    /* we initialize all mp_ints unconditionally, even if some fail.
    ** This guarantees that the DIGITS pointer is valid (even if null).
    ** So, mp_clear will do the right thing below. 
    */
    if (mperr == MP_OKAY) {
      mperr  = mp_read_unsigned_octets(&modulus,  modulusBytes,  modulus_len );
      mperr += mp_read_unsigned_octets(&exponent, exponentBytes, modulus_len );
      mperr += mp_read_unsigned_octets(&base,     input,         modulus_len );
      if (mperr == MP_OKAY) {
#if OLD_WAY
	mperr = s_mp_exptmod(&base, &exponent, &modulus, &result);
#else
	mperr = mp_exptmod(&base, &exponent, &modulus, &result);
#endif
	if (mperr == MP_OKAY) {
	  mperr = mp_to_fixlen_octets(&result, output, modulus_len);
	}
      }
    }
    mp_clear(&base);
    mp_clear(&result);

    mp_clear(&modulus);
    mp_clear(&exponent);

    return (int)mperr;
}

int
main(int argc, char **argv)
{
    TimingContext *	  timeCtx;
    char *		  progName;
    long 		  iters 	= DEFAULT_ITERS;
    unsigned int 	  modulus_len;
    int		 	  i;
    int 		  rv;
    unsigned char 	  buf [1024];
    unsigned char 	  buf2[1024];

    progName = strrchr(argv[0], '/');
    if (!progName)
	progName = strrchr(argv[0], '\\');
    progName = progName ? progName+1 : argv[0];

    if (argc == 2) {
	iters = atol(argv[1]);
    }

    /* no library init function !? */

    modulus_len = sizeof default_n;
    memset(buf, 0x41, sizeof buf);

    testNewFuncs( default_n, modulus_len);
    testNewFuncs( default_n+1, modulus_len - 1);
    testNewFuncs( default_n+2, modulus_len - 2);
    testNewFuncs( default_n+3, modulus_len - 3);

    printf("%lu allocations, %lu frees, %lu copies\n", mp_allocs, mp_frees, mp_copies);
    rv = testModExp(default_n, 0, buf, buf2, modulus_len);
    dumpBytes((unsigned char *)buf2, modulus_len);

    printf("%lu allocations, %lu frees, %lu copies\n", mp_allocs, mp_frees, mp_copies);
    rv = testModExp(default_n, 1, buf, buf2, modulus_len);
    dumpBytes((unsigned char *)buf2, modulus_len);

    printf("%lu allocations, %lu frees, %lu copies\n", mp_allocs, mp_frees, mp_copies);
    rv = testModExp(default_n, 2, buf, buf2, modulus_len);
    dumpBytes((unsigned char *)buf2, modulus_len);

    printf("%lu allocations, %lu frees, %lu copies\n", mp_allocs, mp_frees, mp_copies);
    rv = testModExp(default_n, 3, buf, buf2, modulus_len);
    dumpBytes((unsigned char *)buf2, modulus_len);

    printf("%lu allocations, %lu frees, %lu copies\n", mp_allocs, mp_frees, mp_copies);
    rv = doModExp(default_n, default_d, buf, buf2, modulus_len);
    if (rv != 0) {
	fprintf(stderr, "Error in modexp operation:\n");
	exit(1);
    }
    dumpBytes((unsigned char *)buf2, modulus_len);
    printf("%lu allocations, %lu frees, %lu copies\n", mp_allocs, mp_frees, mp_copies);

    timeCtx = CreateTimingContext();
    TimingBegin(timeCtx);
    i = iters;
    while (i--) {
	rv = doModExp(default_n, default_d, buf, buf2, modulus_len);
	if (rv != 0) {
	    fprintf(stderr, "Error in modexp operation\n");
	    exit(1);
	}
    }
    TimingEnd(timeCtx);
    printf("%ld iterations in %s\n", iters, TimingGenerateString(timeCtx));
    printf("%lu allocations, %lu frees, %lu copies\n", mp_allocs, mp_frees, mp_copies);

    return 0;
}
