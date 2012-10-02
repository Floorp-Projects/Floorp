/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
0xc2,0xae,0x96,0x89,0xaf,0xce,0xd0,0x7b,0x3b,0x35,0xfd,0x0f,0xb1,0xf4,0x7a,0xd1,
0x3c,0x7d,0xb5,0x86,0xf2,0x68,0x36,0xc9,0x97,0xe6,0x82,0x94,0x86,0xaa,0x05,0x39,
0xec,0x11,0x51,0xcc,0x5c,0xa1,0x59,0xba,0x29,0x18,0xf3,0x28,0xf1,0x9d,0xe3,0xae,
0x96,0x5d,0x6d,0x87,0x73,0xf6,0xf6,0x1f,0xd0,0x2d,0xfb,0x2f,0x7a,0x13,0x7f,0xc8,
0x0c,0x7a,0xe9,0x85,0xfb,0xce,0x74,0x86,0xf8,0xef,0x2f,0x85,0x37,0x73,0x0f,0x62,
0x4e,0x93,0x17,0xb7,0x7e,0x84,0x9a,0x94,0x11,0x05,0xca,0x0d,0x31,0x4b,0x2a,0xc8,
0xdf,0xfe,0xe9,0x0c,0x13,0xc7,0xf2,0xad,0x19,0x64,0x28,0x3c,0xb5,0x6a,0xc8,0x4b,
0x79,0xea,0x7c,0xce,0x75,0x92,0x45,0x3e,0xa3,0x9d,0x64,0x6f,0x04,0x69,0x19,0x17
};

static CONST unsigned char default_d[128] = {
0x13,0xcb,0xbc,0xf2,0xf3,0x35,0x8c,0x6d,0x7b,0x6f,0xd9,0xf3,0xa6,0x9c,0xbd,0x80,
0x59,0x2e,0x4f,0x2f,0x11,0xa7,0x17,0x2b,0x18,0x8f,0x0f,0xe8,0x1a,0x69,0x5f,0x6e,
0xac,0x5a,0x76,0x7e,0xd9,0x4c,0x6e,0xdb,0x47,0x22,0x8a,0x57,0x37,0x7a,0x5e,0x94,
0x7a,0x25,0xb5,0xe5,0x78,0x1d,0x3c,0x99,0xaf,0x89,0x7d,0x69,0x2e,0x78,0x9d,0x1d,
0x84,0xc8,0xc1,0xd7,0x1a,0xb2,0x6d,0x2d,0x8a,0xd9,0xab,0x6b,0xce,0xae,0xb0,0xa0,
0x58,0x55,0xad,0x5c,0x40,0x8a,0xd6,0x96,0x08,0x8a,0xe8,0x63,0xe6,0x3d,0x6c,0x20,
0x49,0xc7,0xaf,0x0f,0x25,0x73,0xd3,0x69,0x43,0x3b,0xf2,0x32,0xf8,0x3d,0x5e,0xee,
0x7a,0xca,0xd6,0x94,0x55,0xe5,0xbd,0x25,0x34,0x8d,0x63,0x40,0xb5,0x8a,0xc3,0x01
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
      mperr  = mp_read_unsigned_octets(&modulus,  
		modulusBytes + (sizeof default_n - modulus_len),  modulus_len );
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
      mperr  = mp_read_unsigned_octets(&modulus,  
		modulusBytes + (sizeof default_n - modulus_len),  modulus_len );
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

    if (argc >= 2) {
	iters = atol(argv[1]);
    }

    if (argc >= 3) {
	modulus_len = atol(argv[2]);
    } else 
	modulus_len = sizeof default_n;

    /* no library init function !? */

    memset(buf, 0x41, sizeof buf);

  if (iters < 2) {
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
  }
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
