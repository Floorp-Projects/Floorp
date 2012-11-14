/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#if defined(sgi)
#ifndef IRIX
	error - IRIX is not defined
#endif
#endif

#if defined(__sun)
#ifndef SOLARIS
	error - SOLARIS is not defined
#endif
#endif

#if defined(__hpux)
#ifndef HPUX
	error - HPUX is not defined
#endif
#endif

#if defined(__alpha) 
#if !(defined(_WIN32)) && !(defined(OSF1)) && !(defined(__linux)) && !(defined(__FreeBSD__))
	error - None of OSF1, _WIN32, __linux, or __FreeBSD__ is defined
#endif
#endif

#if defined(_IBMR2)
#ifndef AIX
	error - AIX is not defined
#endif
#endif

#if defined(linux)
#ifndef LINUX
	error - LINUX is not defined
#endif
#endif

#if defined(bsdi)
#ifndef BSDI
	error - BSDI is not defined
#endif
#endif

#if defined(M_UNIX)
#ifndef SCO
      error - SCO is not defined
#endif
#endif
#if !defined(M_UNIX) && defined(_USLC_)
#ifndef UNIXWARE
      error - UNIXWARE is not defined
#endif
#endif

#if defined(__APPLE__)
#ifndef DARWIN
      error - DARWIN is not defined
#endif
#endif

/************************************************************************/

/* Generate cpucfg.h */

#ifdef XP_PC
#ifdef WIN32
#define INT64	_PRInt64
#else
#define INT64	long
#endif
#else
#if defined(HPUX) || defined(SCO) || defined(UNIXWARE)
#define INT64	long
#else
#define INT64	long long
#endif
#endif

struct align_short {
    char c;
    short a;
};
struct align_int {
    char c;
    int a;
};
struct align_long {
    char c;
    long a;
};
struct align_PRInt64 {
    char c;
    INT64 a;
};
struct align_fakelonglong {
    char c;
    struct {
	long hi, lo;
    } a;
};
struct align_float {
    char c;
    float a;
};
struct align_double {
    char c;
    double a;
};
struct align_pointer {
    char c;
    void *a;
};

#define ALIGN_OF(type) \
    (((char*)&(((struct align_##type *)0)->a)) - ((char*)0))

int bpb;

/* Used if shell doesn't support redirection. By default, assume it does. */
FILE *stream;

static int Log2(int n)
{
    int log2 = 0;

    if (n & (n-1))
	log2++;
    if (n >> 16)
	log2 += 16, n >>= 16;
    if (n >> 8)
	log2 += 8, n >>= 8;
    if (n >> 4)
	log2 += 4, n >>= 4;
    if (n >> 2)
	log2 += 2, n >>= 2;
    if (n >> 1)
	log2++;
    return log2;
}

/* We assume that int's are 32 bits */
static void do64(void)
{
    union {
	int i;
	char c[4];
    } u;

    u.i = 0x01020304;
    if (u.c[0] == 0x01) {
	fprintf(stream, "#undef  IS_LITTLE_ENDIAN\n");
	fprintf(stream, "#define IS_BIG_ENDIAN 1\n\n");
    } else {
	fprintf(stream, "#define IS_LITTLE_ENDIAN 1\n");
	fprintf(stream, "#undef  IS_BIG_ENDIAN\n\n");
    }
}

static void do32(void)
{
    union {
	long i;
	char c[4];
    } u;

    u.i = 0x01020304;
    if (u.c[0] == 0x01) {
	fprintf(stream, "#undef  IS_LITTLE_ENDIAN\n");
	fprintf(stream, "#define IS_BIG_ENDIAN 1\n\n");
    } else {
	fprintf(stream, "#define IS_LITTLE_ENDIAN 1\n");
	fprintf(stream, "#undef  IS_BIG_ENDIAN\n\n");
    }
}

/*
** Concievably this could actually be used; but there is lots of code out
** there with and's and shift's in it that assumes a byte is 8 bits, so
** forget about porting THIS code to those non 8 bit byte machines.
*/
static void BitsPerByte(void)
{
    bpb = 8;
}

int main(int argc, char **argv)
{
    BitsPerByte();

    /* If we got a command line argument, try to use it as the stream. */
    ++argv;
    if(*argv) {
        if(!(stream = fopen ( *argv, "wt" ))) {
            fprintf(stderr, "Could not write to output file %s.\n", *argv);
            return 1;
        }
    } else {
		stream = stdout;
	}

    fprintf(stream, "#ifndef nspr_cpucfg___\n");
    fprintf(stream, "#define nspr_cpucfg___\n\n");

    fprintf(stream, "/* AUTOMATICALLY GENERATED - DO NOT EDIT */\n\n");

    if (sizeof(long) == 8) {
	do64();
    } else {
	do32();
    }
    fprintf(stream, "#define PR_BYTES_PER_BYTE   %d\n", sizeof(char));
    fprintf(stream, "#define PR_BYTES_PER_SHORT  %d\n", sizeof(short));
    fprintf(stream, "#define PR_BYTES_PER_INT    %d\n", sizeof(int));
    fprintf(stream, "#define PR_BYTES_PER_INT64  %d\n", 8);
    fprintf(stream, "#define PR_BYTES_PER_LONG   %d\n", sizeof(long));
    fprintf(stream, "#define PR_BYTES_PER_FLOAT  %d\n", sizeof(float));
    fprintf(stream, "#define PR_BYTES_PER_DOUBLE %d\n\n", sizeof(double));

    fprintf(stream, "#define PR_BITS_PER_BYTE    %d\n", bpb);
    fprintf(stream, "#define PR_BITS_PER_SHORT   %d\n", bpb * sizeof(short));
    fprintf(stream, "#define PR_BITS_PER_INT     %d\n", bpb * sizeof(int));
    fprintf(stream, "#define PR_BITS_PER_INT64   %d\n", bpb * 8);
    fprintf(stream, "#define PR_BITS_PER_LONG    %d\n", bpb * sizeof(long));
    fprintf(stream, "#define PR_BITS_PER_FLOAT   %d\n", bpb * sizeof(float));
    fprintf(stream, "#define PR_BITS_PER_DOUBLE  %d\n\n", 
            bpb * sizeof(double));

    fprintf(stream, "#define PR_BITS_PER_BYTE_LOG2   %d\n", Log2(bpb));
    fprintf(stream, "#define PR_BITS_PER_SHORT_LOG2  %d\n", 
            Log2(bpb * sizeof(short)));
    fprintf(stream, "#define PR_BITS_PER_INT_LOG2    %d\n", 
            Log2(bpb * sizeof(int)));
    fprintf(stream, "#define PR_BITS_PER_INT64_LOG2  %d\n", 6);
    fprintf(stream, "#define PR_BITS_PER_LONG_LOG2   %d\n", 
            Log2(bpb * sizeof(long)));
    fprintf(stream, "#define PR_BITS_PER_FLOAT_LOG2  %d\n", 
            Log2(bpb * sizeof(float)));
    fprintf(stream, "#define PR_BITS_PER_DOUBLE_LOG2 %d\n\n", 
            Log2(bpb * sizeof(double)));

    fprintf(stream, "#define PR_ALIGN_OF_SHORT   %d\n", ALIGN_OF(short));
    fprintf(stream, "#define PR_ALIGN_OF_INT     %d\n", ALIGN_OF(int));
    fprintf(stream, "#define PR_ALIGN_OF_LONG    %d\n", ALIGN_OF(long));
    if (sizeof(INT64) < 8) {
	/* this machine doesn't actually support PRInt64's */
	fprintf(stream, "#define PR_ALIGN_OF_INT64   %d\n", 
                ALIGN_OF(fakelonglong));
    } else {
	fprintf(stream, "#define PR_ALIGN_OF_INT64   %d\n", ALIGN_OF(PRInt64));
    }
    fprintf(stream, "#define PR_ALIGN_OF_FLOAT   %d\n", ALIGN_OF(float));
    fprintf(stream, "#define PR_ALIGN_OF_DOUBLE  %d\n", ALIGN_OF(double));
    fprintf(stream, "#define PR_ALIGN_OF_POINTER %d\n\n", ALIGN_OF(pointer));

    fprintf(stream, "#endif /* nspr_cpucfg___ */\n");
    fclose(stream);

    return 0;
}
