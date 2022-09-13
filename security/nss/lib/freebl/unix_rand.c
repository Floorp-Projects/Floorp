/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include "secrng.h"
#include "secerr.h"
#include "prerror.h"
#include "prthread.h"
#include "prprf.h"
#include "prenv.h"

size_t RNG_FileUpdate(const char *fileName, size_t limit);

/*
 * When copying data to the buffer we want the least signicant bytes
 * from the input since those bits are changing the fastest. The address
 * of least significant byte depends upon whether we are running on
 * a big-endian or little-endian machine.
 *
 * Does this mean the least signicant bytes are the most significant
 * to us? :-)
 */

static size_t
CopyLowBits(void *dst, size_t dstlen, void *src, size_t srclen)
{
    union endianness {
        PRInt32 i;
        char c[4];
    } u;

    if (srclen <= dstlen) {
        memcpy(dst, src, srclen);
        return srclen;
    }
    u.i = 0x01020304;
    if (u.c[0] == 0x01) {
        /* big-endian case */
        memcpy(dst, (char *)src + (srclen - dstlen), dstlen);
    } else {
        /* little-endian case */
        memcpy(dst, src, dstlen);
    }
    return dstlen;
}

#ifdef SOLARIS

#include <kstat.h>

static const PRUint32 entropy_buf_len = 4096; /* buffer up to 4 KB */

/* Buffer entropy data, and feed it to the RNG, entropy_buf_len bytes at a time.
 * Returns error if RNG_RandomUpdate fails. Also increments *total_fed
 * by the number of bytes successfully buffered.
 */
static SECStatus
BufferEntropy(char *inbuf, PRUint32 inlen,
              char *entropy_buf, PRUint32 *entropy_buffered,
              PRUint32 *total_fed)
{
    PRUint32 tocopy = 0;
    PRUint32 avail = 0;
    SECStatus rv = SECSuccess;

    while (inlen) {
        avail = entropy_buf_len - *entropy_buffered;
        if (!avail) {
            /* Buffer is full, time to feed it to the RNG. */
            rv = RNG_RandomUpdate(entropy_buf, entropy_buf_len);
            if (SECSuccess != rv) {
                break;
            }
            *entropy_buffered = 0;
            avail = entropy_buf_len;
        }
        tocopy = PR_MIN(avail, inlen);
        memcpy(entropy_buf + *entropy_buffered, inbuf, tocopy);
        *entropy_buffered += tocopy;
        inlen -= tocopy;
        inbuf += tocopy;
        *total_fed += tocopy;
    }
    return rv;
}

/* Feed kernel statistics structures and ks_data field to the RNG.
 * Returns status as well as the number of bytes successfully fed to the RNG.
 */
static SECStatus
RNG_kstat(PRUint32 *fed)
{
    kstat_ctl_t *kc = NULL;
    kstat_t *ksp = NULL;
    PRUint32 entropy_buffered = 0;
    char *entropy_buf = NULL;
    SECStatus rv = SECSuccess;

    PORT_Assert(fed);
    if (!fed) {
        return SECFailure;
    }
    *fed = 0;

    kc = kstat_open();
    PORT_Assert(kc);
    if (!kc) {
        return SECFailure;
    }
    entropy_buf = (char *)PORT_Alloc(entropy_buf_len);
    PORT_Assert(entropy_buf);
    if (entropy_buf) {
        for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next) {
            if (-1 == kstat_read(kc, ksp, NULL)) {
                /* missing data from a single kstat shouldn't be fatal */
                continue;
            }
            rv = BufferEntropy((char *)ksp, sizeof(kstat_t),
                               entropy_buf, &entropy_buffered,
                               fed);
            if (SECSuccess != rv) {
                break;
            }

            if (ksp->ks_data && ksp->ks_data_size > 0 && ksp->ks_ndata > 0) {
                rv = BufferEntropy((char *)ksp->ks_data, ksp->ks_data_size,
                                   entropy_buf, &entropy_buffered,
                                   fed);
                if (SECSuccess != rv) {
                    break;
                }
            }
        }
        if (SECSuccess == rv && entropy_buffered) {
            /* Buffer is not empty, time to feed it to the RNG */
            rv = RNG_RandomUpdate(entropy_buf, entropy_buffered);
        }
        PORT_Free(entropy_buf);
    } else {
        rv = SECFailure;
    }
    if (kstat_close(kc)) {
        PORT_Assert(0);
        rv = SECFailure;
    }
    return rv;
}

#endif

#if defined(SCO) || defined(UNIXWARE) || defined(BSDI) || defined(FREEBSD) || defined(NETBSD) || defined(DARWIN) || defined(OPENBSD) || defined(NTO) || defined(__riscos__) || defined(__GNU__) || defined(__FreeBSD_kernel__) || defined(__NetBSD_kernel__)
#include <sys/times.h>

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    int ticks;
    struct tms buffer;

    ticks = times(&buffer);
    return CopyLowBits(buf, maxbytes, &ticks, sizeof(ticks));
}

static void
GiveSystemInfo(void)
{
    long si;

    /*
     * Is this really necessary?  Why not use rand48 or something?
     */
    si = sysconf(_SC_CHILD_MAX);
    RNG_RandomUpdate(&si, sizeof(si));

    si = sysconf(_SC_STREAM_MAX);
    RNG_RandomUpdate(&si, sizeof(si));

    si = sysconf(_SC_OPEN_MAX);
    RNG_RandomUpdate(&si, sizeof(si));
}
#endif

#if defined(__sun)
#if defined(__svr4) || defined(SVR4)
#include <sys/systeminfo.h>

static void
GiveSystemInfo(void)
{
    int rv;
    char buf[2000];

    rv = sysinfo(SI_MACHINE, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
    rv = sysinfo(SI_RELEASE, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
    rv = sysinfo(SI_HW_SERIAL, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
}

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    hrtime_t t;
    t = gethrtime();
    if (t) {
        return CopyLowBits(buf, maxbytes, &t, sizeof(t));
    }
    return 0;
}
#else /* SunOS (Sun, but not SVR4) */

extern long sysconf(int name);

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    return 0;
}

static void
GiveSystemInfo(void)
{
    long si;

    /* This is not very good */
    si = sysconf(_SC_CHILD_MAX);
    RNG_RandomUpdate(&si, sizeof(si));
}
#endif
#endif /* Sun */

#if defined(__hpux)
#include <sys/unistd.h>

#if defined(__ia64)
#include <ia64/sys/inline.h>

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    PRUint64 t;

    t = _Asm_mov_from_ar(_AREG44);
    return CopyLowBits(buf, maxbytes, &t, sizeof(t));
}
#else
static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    extern int ret_cr16();
    int cr16val;

    cr16val = ret_cr16();
    return CopyLowBits(buf, maxbytes, &cr16val, sizeof(cr16val));
}
#endif

static void
GiveSystemInfo(void)
{
    long si;

    /* This is not very good */
    si = sysconf(_AES_OS_VERSION);
    RNG_RandomUpdate(&si, sizeof(si));
    si = sysconf(_SC_CPU_VERSION);
    RNG_RandomUpdate(&si, sizeof(si));
}
#endif /* HPUX */

#if defined(_IBMR2)
static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    return 0;
}

static void
GiveSystemInfo(void)
{
    /* XXX haven't found any yet! */
}
#endif /* IBM R2 */

#if defined(LINUX)
#include <sys/sysinfo.h>

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    return 0;
}

static void
GiveSystemInfo(void)
{
#ifndef NO_SYSINFO
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        RNG_RandomUpdate(&si, sizeof(si));
    }
#endif
}
#endif /* LINUX */

#if defined(NCR)

#include <sys/utsname.h>
#include <sys/systeminfo.h>

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    return 0;
}

static void
GiveSystemInfo(void)
{
    int rv;
    char buf[2000];

    rv = sysinfo(SI_MACHINE, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
    rv = sysinfo(SI_RELEASE, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
    rv = sysinfo(SI_HW_SERIAL, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
}

#endif /* NCR */

#if defined(sgi)
#include <fcntl.h>
#undef PRIVATE
#include <sys/mman.h>
#include <sys/syssgi.h>
#include <sys/immu.h>
#include <sys/systeminfo.h>
#include <sys/utsname.h>
#include <wait.h>

static void
GiveSystemInfo(void)
{
    int rv;
    char buf[4096];

    rv = syssgi(SGI_SYSID, &buf[0]);
    if (rv > 0) {
        RNG_RandomUpdate(buf, MAXSYSIDSIZE);
    }
#ifdef SGI_RDUBLK
    rv = syssgi(SGI_RDUBLK, getpid(), &buf[0], sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, sizeof(buf));
    }
#endif /* SGI_RDUBLK */
    rv = syssgi(SGI_INVENT, SGI_INV_READ, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, sizeof(buf));
    }
    rv = sysinfo(SI_MACHINE, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
    rv = sysinfo(SI_RELEASE, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
    rv = sysinfo(SI_HW_SERIAL, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
}

static size_t
GetHighResClock(void *buf, size_t maxbuf)
{
    unsigned phys_addr, raddr, cycleval;
    static volatile unsigned *iotimer_addr = NULL;
    static int tries = 0;
    static int cntr_size;
    int mfd;
    long s0[2];
    struct timeval tv;

#ifndef SGI_CYCLECNTR_SIZE
#define SGI_CYCLECNTR_SIZE 165 /* Size user needs to use to read CC */
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
            if (iotimer_addr == (void *)-1) {
                close(mfd);
                iotimer_addr = NULL;
                return 0;
            }
            iotimer_addr = (unsigned *)((__psint_t)iotimer_addr | (phys_addr & pgoffmask));
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
            cntr_size /= 8; /* Convert from bits to bytes */
        }
    }

    s0[0] = *iotimer_addr;
    if (cntr_size > 4)
        s0[1] = *(iotimer_addr + 1);
    memcpy(buf, (char *)&s0[0], cntr_size);
    return CopyLowBits(buf, maxbuf, &s0, cntr_size);
}
#endif

#if defined(sony)
#include <sys/systeminfo.h>

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    return 0;
}

static void
GiveSystemInfo(void)
{
    int rv;
    char buf[2000];

    rv = sysinfo(SI_MACHINE, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
    rv = sysinfo(SI_RELEASE, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
    rv = sysinfo(SI_HW_SERIAL, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
}
#endif /* sony */

#if defined(sinix)
#include <sys/systeminfo.h>
#include <sys/times.h>

int gettimeofday(struct timeval *, struct timezone *);
int gethostname(char *, int);

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    int ticks;
    struct tms buffer;

    ticks = times(&buffer);
    return CopyLowBits(buf, maxbytes, &ticks, sizeof(ticks));
}

static void
GiveSystemInfo(void)
{
    int rv;
    char buf[2000];

    rv = sysinfo(SI_MACHINE, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
    rv = sysinfo(SI_RELEASE, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
    rv = sysinfo(SI_HW_SERIAL, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
}
#endif /* sinix */

#if defined(nec_ews)
#include <sys/systeminfo.h>

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    return 0;
}

static void
GiveSystemInfo(void)
{
    int rv;
    char buf[2000];

    rv = sysinfo(SI_MACHINE, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
    rv = sysinfo(SI_RELEASE, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
    rv = sysinfo(SI_HW_SERIAL, buf, sizeof(buf));
    if (rv > 0) {
        RNG_RandomUpdate(buf, rv);
    }
}
#endif /* nec_ews */

size_t
RNG_GetNoise(void *buf, size_t maxbytes)
{
    struct timeval tv;
    int n = 0;
    int c;

    n = GetHighResClock(buf, maxbytes);
    maxbytes -= n;

    (void)gettimeofday(&tv, 0);
    c = CopyLowBits((char *)buf + n, maxbytes, &tv.tv_usec, sizeof(tv.tv_usec));
    n += c;
    maxbytes -= c;
    c = CopyLowBits((char *)buf + n, maxbytes, &tv.tv_sec, sizeof(tv.tv_sec));
    n += c;
    return n;
}

#ifdef DARWIN
#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE
#include <crt_externs.h>
#endif
#endif

void
RNG_SystemInfoForRNG(void)
{
    char buf[BUFSIZ];
    size_t bytes;
    const char *const *cp;
    char *randfile;
#ifdef DARWIN
#if TARGET_OS_IPHONE
    /* iOS does not expose a way to access environ. */
    char **environ = NULL;
#else
    char **environ = *_NSGetEnviron();
#endif
#else
    extern char **environ;
#endif
    static const char *const files[] = {
        "/etc/passwd",
        "/etc/utmp",
        "/tmp",
        "/var/tmp",
        "/usr/tmp",
        0
    };

    GiveSystemInfo();

    bytes = RNG_GetNoise(buf, sizeof(buf));
    RNG_RandomUpdate(buf, bytes);

    /*
     * Pass the C environment and the addresses of the pointers to the
     * hash function. This makes the random number function depend on the
     * execution environment of the user and on the platform the program
     * is running on.
     */
    if (environ != NULL) {
        cp = (const char *const *)environ;
        while (*cp) {
            RNG_RandomUpdate(*cp, strlen(*cp));
            cp++;
        }
        RNG_RandomUpdate(environ, (char *)cp - (char *)environ);
    }

    /* Give in system information */
    if (gethostname(buf, sizeof(buf)) == 0) {
        RNG_RandomUpdate(buf, strlen(buf));
    }

    /* grab some data from system's PRNG before any other files. */
    bytes = RNG_FileUpdate("/dev/urandom", SYSTEM_RNG_SEED_COUNT);
    if (!bytes) {
        PORT_SetError(SEC_ERROR_NEED_RANDOM);
    }

    /* If the user points us to a random file, pass it through the rng */
    randfile = PR_GetEnvSecure("NSRANDFILE");
    if ((randfile != NULL) && (randfile[0] != '\0')) {
        char *randCountString = PR_GetEnvSecure("NSRANDCOUNT");
        int randCount = randCountString ? atoi(randCountString) : 0;
        if (randCount != 0) {
            RNG_FileUpdate(randfile, randCount);
        } else {
            RNG_FileForRNG(randfile);
        }
    }

    /* pass other files through */
    for (cp = files; *cp; cp++)
        RNG_FileForRNG(*cp);

#if defined(BSDI) || defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD) || defined(DARWIN) || defined(LINUX) || defined(HPUX)
    if (bytes)
        return;
#endif

#ifdef SOLARIS
    if (!bytes) {
        /* On Solaris 8, /dev/urandom isn't available, so we use libkstat. */
        PRUint32 kstat_bytes = 0;
        if (SECSuccess != RNG_kstat(&kstat_bytes)) {
            PORT_Assert(0);
        }
        bytes += kstat_bytes;
        PORT_Assert(bytes);
    }
#endif
}

#define TOTAL_FILE_LIMIT 1000000 /* one million */

size_t
RNG_FileUpdate(const char *fileName, size_t limit)
{
    FILE *file;
    int fd;
    int bytes;
    size_t fileBytes = 0;
    struct stat stat_buf;
    unsigned char buffer[BUFSIZ];
    static size_t totalFileBytes = 0;

    /* suppress valgrind warnings due to holes in struct stat */
    memset(&stat_buf, 0, sizeof(stat_buf));

    if (stat((char *)fileName, &stat_buf) < 0)
        return fileBytes;
    RNG_RandomUpdate(&stat_buf, sizeof(stat_buf));

    file = fopen(fileName, "r");
    if (file != NULL) {
        /* Read from the underlying file descriptor directly to bypass stdio
         * buffering and avoid reading more bytes than we need from
         * /dev/urandom. NOTE: we can't use fread with unbuffered I/O because
         * fread may return EOF in unbuffered I/O mode on Android.
         *
         * Moreover, we read into a buffer of size BUFSIZ, so buffered I/O
         * has no performance advantage. */
        fd = fileno(file);
        /* 'file' was just opened, so this should not fail. */
        PORT_Assert(fd != -1);
        while (limit > fileBytes && fd != -1) {
            bytes = PR_MIN(sizeof buffer, limit - fileBytes);
            bytes = read(fd, buffer, bytes);
            if (bytes <= 0)
                break;
            RNG_RandomUpdate(buffer, bytes);
            fileBytes += bytes;
            totalFileBytes += bytes;
            /* after TOTAL_FILE_LIMIT has been reached, only read in first
            ** buffer of data from each subsequent file.
            */
            if (totalFileBytes > TOTAL_FILE_LIMIT)
                break;
        }
        fclose(file);
    }
    /*
     * Pass yet another snapshot of our highest resolution clock into
     * the hash function.
     */
    bytes = RNG_GetNoise(buffer, sizeof(buffer));
    RNG_RandomUpdate(buffer, bytes);
    return fileBytes;
}

void
RNG_FileForRNG(const char *fileName)
{
    RNG_FileUpdate(fileName, TOTAL_FILE_LIMIT);
}

#define _POSIX_PTHREAD_SEMANTICS
#include <dirent.h>

PRBool
ReadFileOK(char *dir, char *file)
{
    struct stat stat_buf;
    char filename[PATH_MAX];
    int count = snprintf(filename, sizeof filename, "%s/%s", dir, file);

    if (count <= 0) {
        return PR_FALSE; /* name too long, can't read it anyway */
    }

    if (stat(filename, &stat_buf) < 0)
        return PR_FALSE; /* can't stat, probably can't read it then as well */
    return S_ISREG(stat_buf.st_mode) ? PR_TRUE : PR_FALSE;
}

size_t
RNG_SystemRNG(void *dest, size_t maxLen)
{
    FILE *file;
    int fd;
    int bytes;
    size_t fileBytes = 0;
    unsigned char *buffer = dest;

    file = fopen("/dev/urandom", "r");
    if (file == NULL) {
        PORT_SetError(SEC_ERROR_NEED_RANDOM);
        return 0;
    }
    /* Read from the underlying file descriptor directly to bypass stdio
     * buffering and avoid reading more bytes than we need from /dev/urandom.
     * NOTE: we can't use fread with unbuffered I/O because fread may return
     * EOF in unbuffered I/O mode on Android.
     */
    fd = fileno(file);
    /* 'file' was just opened, so this should not fail. */
    PORT_Assert(fd != -1);
    while (maxLen > fileBytes && fd != -1) {
        bytes = maxLen - fileBytes;
        bytes = read(fd, buffer, bytes);
        if (bytes <= 0)
            break;
        fileBytes += bytes;
        buffer += bytes;
    }
    fclose(file);
    if (fileBytes != maxLen) {
        PORT_SetError(SEC_ERROR_NEED_RANDOM); /* system RNG failed */
        fileBytes = 0;
    }
    return fileBytes;
}
