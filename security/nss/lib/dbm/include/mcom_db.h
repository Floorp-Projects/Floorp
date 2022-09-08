/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*-
 * Copyright (c) 1990, 1993, 1994
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. ***REMOVED*** - see
 *    ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change
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
 *  @(#)db.h    8.7 (Berkeley) 6/16/94
 */

#ifndef _DB_H_
#define _DB_H_

#ifndef macintosh
#include <sys/types.h>
#endif
#include "prtypes.h"

#if !defined(XP_OS2) && !defined(XP_UNIX) || defined(NTO)
typedef PRUintn uint;
#endif
typedef PRUint8 uint8;
typedef PRUint16 uint16;
/* On AIX 5.2, sys/inttypes.h (which is included by sys/types.h)
 * defines the types int8, int16, int32, and int64. */
#if !defined(AIX)
typedef PRInt32 int32;
#endif
typedef PRUint32 uint32;

#include <limits.h>

#ifdef __DBINTERFACE_PRIVATE

#ifdef HAVE_SYS_BYTEORDER_H
#include <sys/byteorder.h>
#endif

#if defined(__linux)
#include <endian.h>
#ifndef BYTE_ORDER
#define BYTE_ORDER __BYTE_ORDER
#define BIG_ENDIAN __BIG_ENDIAN
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#endif
#endif /* __linux */

#ifdef __sgi
#define BYTE_ORDER BIG_ENDIAN
#define BIG_ENDIAN 4321
#define LITTLE_ENDIAN 1234 /* LSB first: i386, vax, all NT risc */
#endif

#ifdef __sun
#define BIG_ENDIAN 4321
#define LITTLE_ENDIAN 1234 /* LSB first: i386, vax, all NT risc */

#ifndef __SVR4
/* compat.h is only in 4.1.3 machines. - dp */
#include <compat.h>
#endif

/* XXX - dp
 * Need to find a general way of defining endian-ness in SunOS 5.3
 * SunOS 5.4 defines _BIG_ENDIAN and _LITTLE_ENDIAN
 * SunOS 5.3 does nothing like this.
 */

#ifndef BYTE_ORDER

#if defined(_BIG_ENDIAN)
#define BYTE_ORDER BIG_ENDIAN
#elif defined(_LITTLE_ENDIAN)
#define BYTE_ORDER LITTLE_ENDIAN
#elif !defined(__SVR4)
/* 4.1.3 is always BIG_ENDIAN as it was released only on sparc platforms. */
#define BYTE_ORDER BIG_ENDIAN
#elif !defined(vax) && !defined(ntohl) && !defined(lint) && !defined(i386)
/* 5.3 big endian. Copied this above line from sys/byteorder.h */
/* Now we are in a 5.3 SunOS rather non 5.4 or above SunOS  */
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#endif /* !BYTE_ORDER */
#endif /* __sun */

#if defined(__hpux) || defined(__hppa)
#define BYTE_ORDER BIG_ENDIAN
#define BIG_ENDIAN 4321
#define LITTLE_ENDIAN 1234 /* LSB first: i386, vax, all NT risc */
#endif

#if defined(AIXV3) || defined(AIX)
/* BYTE_ORDER, LITTLE_ENDIAN, BIG_ENDIAN are all defined here */
#include <sys/machine.h>
#endif

/* Digital Unix */
#ifdef __osf__
#include <machine/endian.h>
#endif

#ifdef __alpha
#ifndef WIN32
#else
/* Alpha NT */
#define BYTE_ORDER LITTLE_ENDIAN
#define BIG_ENDIAN 4321
#define LITTLE_ENDIAN 1234
#endif
#endif

#ifdef NCR
#include <sys/endian.h>
#endif

#ifdef __QNX__
#ifdef __QNXNTO__
#include <sys/param.h>
#else
#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN 4321
#define BYTE_ORDER LITTLE_ENDIAN
#endif
#endif

#ifdef SNI
/* #include <sys/hetero.h> */
#define BYTE_ORDER BIG_ENDIAN
#define BIG_ENDIAN 4321
#define LITTLE_ENDIAN 1234
#endif

#ifdef _WINDOWS
#ifdef BYTE_ORDER
#undef BYTE_ORDER
#endif

#define BYTE_ORDER LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234 /* LSB first: i386, vax, all NT risc */
#define BIG_ENDIAN 4321
#endif

#ifdef macintosh
#define BIG_ENDIAN 4321
#define LITTLE_ENDIAN 1234
#define BYTE_ORDER BIG_ENDIAN
#endif

#endif /* __DBINTERFACE_PRIVATE */

#ifdef SCO
#define MAXPATHLEN 1024
#endif

#include <fcntl.h>

#if defined(_WINDOWS) || defined(XP_OS2)
#include <stdio.h>
#include <io.h>

#ifndef XP_OS2
#define MAXPATHLEN 1024
#endif

#define EFTYPE EINVAL /* POSIX 1003.1 format errno. */

#ifndef STDERR_FILENO
#define STDIN_FILENO 0 /* ANSI C #defines */
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

#ifndef O_ACCMODE /* POSIX 1003.1 access mode mask. */
#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)
#endif
#endif

#ifdef macintosh
#include <stdio.h>
#include "xp_mcom.h"
#define O_ACCMODE 3 /* Mask for file access modes */
#define EFTYPE 2000
PR_BEGIN_EXTERN_C
int mkstemp(const char *path);
PR_END_EXTERN_C
#endif /* MACINTOSH */

#if !defined(_WINDOWS) && !defined(macintosh)
#include <sys/stat.h>
#include <errno.h>
#endif

/* define EFTYPE since most don't */
#ifndef EFTYPE
#define EFTYPE EINVAL /* POSIX 1003.1 format errno. */
#endif

#define RET_ERROR -1 /* Return values. */
#define RET_SUCCESS 0
#define RET_SPECIAL 1

#define MAX_PAGE_NUMBER 0xffffffff /* >= # of pages in a file */

#ifndef __sgi
typedef uint32 pgno_t;
#endif

#define MAX_PAGE_OFFSET 65535 /* >= # of bytes in a page */
typedef uint16 indx_t;
#define MAX_REC_NUMBER 0xffffffff /* >= # of records in a tree */
typedef uint32 recno_t;

/* Key/data structure -- a Data-Base Thang. */
typedef struct {
    void *data;  /* data */
    size_t size; /* data length */
} DBT;

/* Routine flags. */
#define R_CURSOR 1      /* del, put, seq */
#define __R_UNUSED 2    /* UNUSED */
#define R_FIRST 3       /* seq */
#define R_IAFTER 4      /* put (RECNO) */
#define R_IBEFORE 5     /* put (RECNO) */
#define R_LAST 6        /* seq (BTREE, RECNO) */
#define R_NEXT 7        /* seq */
#define R_NOOVERWRITE 8 /* put */
#define R_PREV 9        /* seq (BTREE, RECNO) */
#define R_SETCURSOR 10  /* put (RECNO) */
#define R_RECNOSYNC 11  /* sync (RECNO) */

typedef enum { DB_BTREE,
               DB_HASH,
               DB_RECNO } DBTYPE;

typedef enum { LockOutDatabase,
               UnlockDatabase } DBLockFlagEnum;

/*
 * !!!
 * The following flags are included in the dbopen(3) call as part of the
 * open(2) flags.  In order to avoid conflicts with the open flags, start
 * at the top of the 16 or 32-bit number space and work our way down.  If
 * the open flags were significantly expanded in the future, it could be
 * a problem.  Wish I'd left another flags word in the dbopen call.
 *
 * !!!
 * None of this stuff is implemented yet.  The only reason that it's here
 * is so that the access methods can skip copying the key/data pair when
 * the DB_LOCK flag isn't set.
 */
#if UINT_MAX > 65535
#define DB_LOCK 0x20000000  /* Do locking. */
#define DB_SHMEM 0x40000000 /* Use shared memory. */
#define DB_TXN 0x80000000   /* Do transactions. */
#else
#define DB_LOCK 0x2000  /* Do locking. */
#define DB_SHMEM 0x4000 /* Use shared memory. */
#define DB_TXN 0x8000   /* Do transactions. */
#endif

/* Access method description structure. */
typedef struct dbm_db {
    DBTYPE type; /* Underlying db type. */
    int (*close)(struct dbm_db *);
    int (*del)(const struct dbm_db *, const DBT *, uint);
    int (*get)(const struct dbm_db *, const DBT *, DBT *, uint);
    int (*put)(const struct dbm_db *, DBT *, const DBT *, uint);
    int (*seq)(const struct dbm_db *, DBT *, DBT *, uint);
    int (*sync)(const struct dbm_db *, uint);
    void *internal; /* Access method private. */
    int (*fd)(const struct dbm_db *);
} DB;

#define BTREEMAGIC 0x053162
#define BTREEVERSION 3

/* Structure used to pass parameters to the btree routines. */
typedef struct {
#define R_DUP 0x01 /* duplicate keys */
    uint32 flags;
    uint cachesize; /* bytes to cache */
    int maxkeypage; /* maximum keys per page */
    int minkeypage; /* minimum keys per page */
    uint psize;     /* page size */
    int(*compare)   /* comparison function */
        (const DBT *, const DBT *);
    size_t(*prefix) /* prefix function */
        (const DBT *, const DBT *);
    int lorder; /* byte order */
} BTREEINFO;

#define HASHMAGIC 0x061561
#define HASHVERSION 2

/* Structure used to pass parameters to the hashing routines. */
typedef struct {
    uint bsize;     /* bucket size */
    uint ffactor;   /* fill factor */
    uint nelem;     /* number of elements */
    uint cachesize; /* bytes to cache */
    uint32          /* hash function */
        (*hash)(const void *, size_t);
    int lorder; /* byte order */
} HASHINFO;

/* Structure used to pass parameters to the record routines. */
typedef struct {
#define R_FIXEDLEN 0x01 /* fixed-length records */
#define R_NOKEY 0x02    /* key not required */
#define R_SNAPSHOT 0x04 /* snapshot the input */
    uint32 flags;
    uint cachesize; /* bytes to cache */
    uint psize;     /* page size */
    int lorder;     /* byte order */
    size_t reclen;  /* record length (fixed-length records) */
    uint8 bval;     /* delimiting byte (variable-length records */
    char *bfname;   /* btree file name */
} RECNOINFO;

#ifdef __DBINTERFACE_PRIVATE
/*
 * Little endian <==> big endian 32-bit swap macros.
 *  M_32_SWAP   swap a memory location
 *  P_32_SWAP   swap a referenced memory location
 *  P_32_COPY   swap from one location to another
 */
#define M_32_SWAP(a)                          \
    {                                         \
        uint32 _tmp = a;                      \
        ((char *)&a)[0] = ((char *)&_tmp)[3]; \
        ((char *)&a)[1] = ((char *)&_tmp)[2]; \
        ((char *)&a)[2] = ((char *)&_tmp)[1]; \
        ((char *)&a)[3] = ((char *)&_tmp)[0]; \
    }
#define P_32_SWAP(a)                         \
    {                                        \
        uint32 _tmp = *(uint32 *)a;          \
        ((char *)a)[0] = ((char *)&_tmp)[3]; \
        ((char *)a)[1] = ((char *)&_tmp)[2]; \
        ((char *)a)[2] = ((char *)&_tmp)[1]; \
        ((char *)a)[3] = ((char *)&_tmp)[0]; \
    }
#define P_32_COPY(a, b)                        \
    {                                          \
        ((char *)&(b))[0] = ((char *)&(a))[3]; \
        ((char *)&(b))[1] = ((char *)&(a))[2]; \
        ((char *)&(b))[2] = ((char *)&(a))[1]; \
        ((char *)&(b))[3] = ((char *)&(a))[0]; \
    }

/*
 * Little endian <==> big endian 16-bit swap macros.
 *  M_16_SWAP   swap a memory location
 *  P_16_SWAP   swap a referenced memory location
 *  P_16_COPY   swap from one location to another
 */
#define M_16_SWAP(a)                          \
    {                                         \
        uint16 _tmp = a;                      \
        ((char *)&a)[0] = ((char *)&_tmp)[1]; \
        ((char *)&a)[1] = ((char *)&_tmp)[0]; \
    }
#define P_16_SWAP(a)                         \
    {                                        \
        uint16 _tmp = *(uint16 *)a;          \
        ((char *)a)[0] = ((char *)&_tmp)[1]; \
        ((char *)a)[1] = ((char *)&_tmp)[0]; \
    }
#define P_16_COPY(a, b)                        \
    {                                          \
        ((char *)&(b))[0] = ((char *)&(a))[1]; \
        ((char *)&(b))[1] = ((char *)&(a))[0]; \
    }
#endif

PR_BEGIN_EXTERN_C

extern DB *
dbopen(const char *, int, int, DBTYPE, const void *);

/* set or unset a global lock flag to disable the
 * opening of any DBM file
 */
void dbSetOrClearDBLock(DBLockFlagEnum type);

#ifdef __DBINTERFACE_PRIVATE
DB *dbm_bt_open(const char *, int, int, const BTREEINFO *, int);
DB *dbm_hash_open(const char *, int, int, const HASHINFO *, int);
DB *dbm_rec_open(const char *, int, int, const RECNOINFO *, int);
void dbm_dbpanic(DB *dbp);
#endif

PR_END_EXTERN_C

#endif /* !_DB_H_ */
