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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <assert.h>
#include "secrng.h"

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
    
static size_t CopyLowBits(void *dst, size_t dstlen, void *src, size_t srclen)
{
    union endianness {
	int32 i;
	char c[4];
    } u;

    if (srclen <= dstlen) {
	memcpy(dst, src, srclen);
	return srclen;
    }
    u.i = 0x01020304;
    if (u.c[0] == 0x01) {
	/* big-endian case */
	memcpy(dst, (char*)src + (srclen - dstlen), dstlen);
    } else {
	/* little-endian case */
	memcpy(dst, src, dstlen);
    }
    return dstlen;
}

#if defined(SCO) || defined(UNIXWARE) || defined(BSDI) || defined(FREEBSD) \
    || defined(NETBSD) || defined(NTO) || defined(DARWIN) || defined(OPENBSD)
#include <sys/times.h>

#define getdtablesize() sysconf(_SC_OPEN_MAX)

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    int ticks;
    struct tms buffer;

    ticks=times(&buffer);
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
#include <sys/times.h>
#include <wait.h>

int gettimeofday(struct timeval *);
int gethostname(char *, int);

#define getdtablesize() sysconf(_SC_OPEN_MAX)

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

#define getdtablesize() sysconf(_SC_OPEN_MAX)

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

#if defined(OSF1)
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/systeminfo.h>
#include <c_asm.h>

static void
GiveSystemInfo(void)
{
    char buf[BUFSIZ];
    int rv;
    int off = 0;

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

/*
 * Use the "get the cycle counter" instruction on the alpha.
 * The low 32 bits completely turn over in less than a minute.
 * The high 32 bits are some non-counter gunk that changes sometimes.
 */
static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    unsigned long t;

    t = asm("rpcc %v0");
    return CopyLowBits(buf, maxbytes, &t, sizeof(t));
}

#endif /* Alpha */

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
#include <linux/kernel.h>

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    return 0;
}

static void
GiveSystemInfo(void)
{
    /* XXX sysinfo() does not seem be implemented anywhwere */
#if 0
    struct sysinfo si;
    char hn[2000];
    if (sysinfo(&si) == 0) {
	RNG_RandomUpdate(&si, sizeof(si));
    }
#endif
}
#endif /* LINUX */

#if defined(NCR)

#include <sys/utsname.h>
#include <sys/systeminfo.h>

#define getdtablesize() sysconf(_SC_OPEN_MAX)

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

static size_t GetHighResClock(void *buf, size_t maxbuf)
{
    unsigned phys_addr, raddr, cycleval;
    static volatile unsigned *iotimer_addr = NULL;
    static int tries = 0;
    static int cntr_size;
    int mfd;
    long s0[2];
    struct timeval tv;

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
	    if (iotimer_addr == (void*)-1) {
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
    return CopyLowBits(buf, maxbuf, &s0, cntr_size);
}
#endif

#if defined(sony)
#include <sys/systeminfo.h>

#define getdtablesize() sysconf(_SC_OPEN_MAX)

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

#define getdtablesize() sysconf(_SC_OPEN_MAX)

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    int ticks;
    struct tms buffer;

    ticks=times(&buffer);
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

#if defined(VMS)
#include <c_asm.h>

static void
GiveSystemInfo(void)
{
    long si;
 
    /* 
     * This is copied from the SCO/UNIXWARE etc section. And like the comment
     * there says, what's the point? This isn't random, it generates the same
     * stuff every time its run!
     */
    si = sysconf(_SC_CHILD_MAX);
    RNG_RandomUpdate(&si, sizeof(si));
 
    si = sysconf(_SC_STREAM_MAX);
    RNG_RandomUpdate(&si, sizeof(si));
 
    si = sysconf(_SC_OPEN_MAX);
    RNG_RandomUpdate(&si, sizeof(si));
}
 
/*
 * Use the "get the cycle counter" instruction on the alpha.
 * The low 32 bits completely turn over in less than a minute.
 * The high 32 bits are some non-counter gunk that changes sometimes.
 */
static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    unsigned long t;
 
    t = asm("rpcc %v0");
    return CopyLowBits(buf, maxbytes, &t, sizeof(t));
}
 
#endif /* VMS */

#ifdef BEOS
#include <be/kernel/OS.h>

static size_t
GetHighResClock(void *buf, size_t maxbytes)
{
    bigtime_t bigtime; /* Actually an int64 */

    bigtime = real_time_clock_usecs();
    return CopyLowBits(buf, maxbytes, &bigtime, sizeof(bigtime));
}

static void
GiveSystemInfo(void)
{
    system_info *info = NULL;
    int32 val;                     
    get_system_info(info);
    if (info) {
        val = info->boot_time;
        RNG_RandomUpdate(&val, sizeof(val));
        val = info->used_pages;
        RNG_RandomUpdate(&val, sizeof(val));
        val = info->used_ports;
        RNG_RandomUpdate(&val, sizeof(val));
        val = info->used_threads;
        RNG_RandomUpdate(&val, sizeof(val));
        val = info->used_teams;
        RNG_RandomUpdate(&val, sizeof(val));
    }
}
#endif /* BEOS */

#if defined(nec_ews)
#include <sys/systeminfo.h>

#define getdtablesize() sysconf(_SC_OPEN_MAX)

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

size_t RNG_GetNoise(void *buf, size_t maxbytes)
{
    struct timeval tv;
    int n = 0;
    int c;

    n = GetHighResClock(buf, maxbytes);
    maxbytes -= n;

#if defined(__sun) && (defined(_svr4) || defined(SVR4)) || defined(sony)
    (void)gettimeofday(&tv);
#else
    (void)gettimeofday(&tv, 0);
#endif
    c = CopyLowBits((char*)buf+n, maxbytes, &tv.tv_usec, sizeof(tv.tv_usec));
    n += c;
    maxbytes -= c;
    c = CopyLowBits((char*)buf+n, maxbytes, &tv.tv_sec, sizeof(tv.tv_sec));
    n += c;
    return n;
}

#define SAFE_POPEN_MAXARGS	10	/* must be at least 2 */

/*
 * safe_popen is static to this module and we know what arguments it is
 * called with. Note that this version only supports a single open child
 * process at any time.
 */
static pid_t safe_popen_pid;
static struct sigaction oldact;

static FILE *
safe_popen(char *cmd)
{
    int p[2], fd, argc;
    pid_t pid;
    char *argv[SAFE_POPEN_MAXARGS + 1];
    FILE *fp;
    static char blank[] = " \t";
    static struct sigaction newact;

    if (pipe(p) < 0)
	return 0;

    /* Setup signals so that SIGCHLD is ignored as we want to do waitpid */
    newact.sa_handler = SIG_DFL;
    newact.sa_flags = 0;
    sigfillset(&newact.sa_mask);
    sigaction (SIGCHLD, &newact, &oldact);

    pid = fork();
    switch (pid) {
      case -1:
	close(p[0]);
	close(p[1]);
	sigaction (SIGCHLD, &oldact, NULL);
	return 0;

      case 0:
	/* dup write-side of pipe to stderr and stdout */
	if (p[1] != 1) dup2(p[1], 1);
	if (p[1] != 2) dup2(p[1], 2);
	close(0);
	for (fd = PR_MIN(65536,getdtablesize()); --fd > 2; close(fd))
	    ;

	/* clean up environment in the child process */
	putenv("PATH=/bin:/usr/bin:/sbin:/usr/sbin:/etc:/usr/etc");
	putenv("SHELL=/bin/sh");
	putenv("IFS= \t");

	/*
	 * The caller may have passed us a string that is in text
	 * space. It may be illegal to modify the string
	 */
	cmd = strdup(cmd);
	/* format argv */
	argv[0] = strtok(cmd, blank);
	argc = 1;
	while ((argv[argc] = strtok(0, blank)) != 0) {
	    if (++argc == SAFE_POPEN_MAXARGS) {
		argv[argc] = 0;
		break;
	    }
	}

	/* and away we go */
	execvp(argv[0], argv);
	exit(127);
	break;

      default:
	close(p[1]);
	fp = fdopen(p[0], "r");
	if (fp == 0) {
	    close(p[0]);
	    sigaction (SIGCHLD, &oldact, NULL);
	    return 0;
	}
	break;
    }

    /* non-zero means there's a cmd running */
    safe_popen_pid = pid;
    return fp;
}

static int
safe_pclose(FILE *fp)
{
    pid_t pid;
    int count, status;

    if ((pid = safe_popen_pid) == 0)
	return -1;
    safe_popen_pid = 0;

    /* if the child hasn't exited, kill it -- we're done with its output */
    count = 0;
    while (waitpid(pid, &status, WNOHANG) == 0) {
    	if (kill(pid, SIGKILL) < 0 && errno == ESRCH)
	    break;
	if (++count == 1000)
	    break;
    }

    /* Reset SIGCHLD signal hander before returning */
    sigaction(SIGCHLD, &oldact, NULL);

    fclose(fp);
    return status;
}


#if !defined(VMS)

#ifdef DARWIN
#include <crt_externs.h>
#endif

void RNG_SystemInfoForRNG(void)
{
    FILE *fp;
    char buf[BUFSIZ];
    size_t bytes;
    const char * const *cp;
    char *randfile;
#ifdef DARWIN
    char **environ = *_NSGetEnviron();
#else
    extern char **environ;
#endif
#ifdef BEOS
    static const char * const files[] = {
	"/boot/var/swap",
	"/boot/var/log/syslog",
	"/boot/var/tmp",
	"/boot/home/config/settings",
	"/boot/home",
	0
    };
#else
    static const char * const files[] = {
	"/etc/passwd",
	"/etc/utmp",
	"/tmp",
	"/var/tmp",
	"/usr/tmp",
	0
    };
#endif

#ifdef DO_PS
For now it is considered that it is too expensive to run the ps command
for the small amount of entropy it provides.
#if defined(__sun) && (!defined(__svr4) && !defined(SVR4)) || defined(bsdi) || defined(LINUX)
    static char ps_cmd[] = "ps aux";
#else
    static char ps_cmd[] = "ps -el";
#endif
#endif /* DO_PS */
#if defined(BSDI)
    static char netstat_ni_cmd[] = "netstat -nis";
#else
    static char netstat_ni_cmd[] = "netstat -ni";
#endif

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
        cp = environ;
        while (*cp) {
	    RNG_RandomUpdate(*cp, strlen(*cp));
	    cp++;
        }
        RNG_RandomUpdate(environ, (char*)cp - (char*)environ);
    }

    /* Give in system information */
    if (gethostname(buf, sizeof(buf)) > 0) {
	RNG_RandomUpdate(buf, strlen(buf));
    }
    GiveSystemInfo();

    /* grab some data from system's PRNG before any other files. */
    bytes = RNG_FileUpdate("/dev/urandom", 1024);

    /* If the user points us to a random file, pass it through the rng */
    randfile = getenv("NSRANDFILE");
    if ( ( randfile != NULL ) && ( randfile[0] != '\0') ) {
	RNG_FileForRNG(randfile);
    }

    /* pass other files through */
    for (cp = files; *cp; cp++)
	RNG_FileForRNG(*cp);

/*
 * Bug 100447: On BSD/OS 4.2 and 4.3, we have problem calling safe_popen
 * in a pthreads environment.  Therefore, we call safe_popen last and on
 * BSD/OS we do not call safe_popen when we succeeded in getting data
 * from /dev/urandom.
 */

#ifdef BSDI
    if (bytes)
        return;
#endif

#ifdef DO_PS
    fp = safe_popen(ps_cmd);
    if (fp != NULL) {
	while ((bytes = fread(buf, 1, sizeof(buf), fp)) > 0)
	    RNG_RandomUpdate(buf, bytes);
	safe_pclose(fp);
    }
#endif
    fp = safe_popen(netstat_ni_cmd);
    if (fp != NULL) {
	while ((bytes = fread(buf, 1, sizeof(buf), fp)) > 0)
	    RNG_RandomUpdate(buf, bytes);
	safe_pclose(fp);
    }

}
#else
void RNG_SystemInfoForRNG(void)
{
    FILE *fp;
    char buf[BUFSIZ];
    size_t bytes;
    int extra;
    char **cp;
    extern char **environ;
    char *randfile;
 
    GiveSystemInfo();
 
    bytes = RNG_GetNoise(buf, sizeof(buf));
    RNG_RandomUpdate(buf, bytes);
 
    /*
     * Pass the C environment and the addresses of the pointers to the
     * hash function. This makes the random number function depend on the
     * execution environment of the user and on the platform the program
     * is running on.
     */
    cp = environ;
    while (*cp) {
	RNG_RandomUpdate(*cp, strlen(*cp));
	cp++;
    }
    RNG_RandomUpdate(environ, (char*)cp - (char*)environ);
 
    /* Give in system information */
    if (gethostname(buf, sizeof(buf)) > 0) {
	RNG_RandomUpdate(buf, strlen(buf));
    }
    GiveSystemInfo();
 
    /* If the user points us to a random file, pass it through the rng */
    randfile = getenv("NSRANDFILE");
    if ( ( randfile != NULL ) && ( randfile[0] != '\0') ) {
	RNG_FileForRNG(randfile);
    }

    /*
    ** We need to generate at least 1024 bytes of seed data. Since we don't
    ** do the file stuff for VMS, and because the environ list is so short
    ** on VMS, we need to make sure we generate enough. So do another 1000
    ** bytes to be sure.
    */
    extra = 1000;
    while (extra > 0) {
        cp = environ;
        while (*cp) {
	    int n = strlen(*cp);
	    RNG_RandomUpdate(*cp, n);
	    extra -= n;
	    cp++;
        }
    }
}
#endif

#define TOTAL_FILE_LIMIT 1000000	/* one million */

size_t RNG_FileUpdate(const char *fileName, size_t limit)
{
    FILE *        file;
    size_t        bytes;
    size_t        fileBytes = 0;
    struct stat   stat_buf;
    unsigned char buffer[BUFSIZ];
    static size_t totalFileBytes = 0;
    
    if (stat((char *)fileName, &stat_buf) < 0)
	return fileBytes;
    RNG_RandomUpdate(&stat_buf, sizeof(stat_buf));
    
    file = fopen((char *)fileName, "r");
    if (file != NULL) {
	while (limit > fileBytes) {
	    bytes = PR_MIN(sizeof buffer, limit - fileBytes);
	    bytes = fread(buffer, 1, bytes, file);
	    if (bytes == 0) 
		break;
	    RNG_RandomUpdate(buffer, bytes);
	    fileBytes      += bytes;
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

void RNG_FileForRNG(const char *fileName)
{
    RNG_FileUpdate(fileName, TOTAL_FILE_LIMIT);
}
