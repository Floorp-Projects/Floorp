/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * unix-dns.c --- portable nonblocking DNS for Unix
 * Created: Jamie Zawinski <jwz@netscape.com>, 19-Dec-96.
 */

#if defined(XP_UNIX) && defined(UNIX_ASYNC_DNS)

static void dns_socks_kludge(void);

/* Todo:

   = Should we actually use the CHANGING_ARGV_WORKS code, on systems where
     that is true, or just always do the exec() hack?  If we use it, it
     needs to be debugged more, and the exact set of systems on which it
     works needs to be determined.  (Works on: SunOS 4, Linux, AIX, OSF;
     doesn't work on: Solaris, Irix, UnixWare, HPUX.)  (But HPUX has an
     undocumented ioctl that lets you change a proc's name.)   (Unix sucks
     a lot.)
 */


/* Compile-time options:
   -DDNS_EXPLICITLY_FLUSH_PIPES
   	to do ioctls() to flush out the pipes after each write().
	at one point I thought I needed this, but it doesn't
	actually seem to be necessary.

   -DGETHOSTBYNAME_DELAY=N
   	to insert an artificial delay of N seconds before each
	call to gethostbyname (in order to simulate DNS lossage.)

   -DNO_SOCKS_NS_KLUDGE
   	Set this to *disable* the $SOCKS_NS kludge.  Otherwise,
	that environment variable will be consulted for use as an
	alternate DNS root.  It's historical; don't ask me...

   -DRANDOMIZE_ADDRESSES
	This code only deals with one IP address per host; if this
	is set, then that host will be randomly selected from among
	all the addresses of the host; otherwise, it will be the
	first address.

   -DCHANGING_ARGV_WORKS
        (This option isn't fully implemented yet, and can't work
	on some systems anyway.)

	The code in this file wants to fork(), and then change the
	name of the second process as reported by `ps'.  Some systems
	let you do that by overwriting argv[0], but some do not.

	If this isn't defined, we effect this change by re-execing
	after the fork (since you can pass a new name that way.)
	This depends on exec(argv[0] ... ) working, which it might
	not if argv[0] has a non-absolute path in it, and the user
	has played stupid games with $PATH or something.  Those
	users probably deserve to lose.

   -DSTANDALONE
   	to include a main() for interactive command-line testing.

   -DPROC3_DEBUG_PRINT
	to cause proc3 (the gethostbyname() processes) to print
	diagnostics to stderr.

   -DPROC2_DEBUG_PRINT
	to cause proc2 (the looping, dispatching process) to print
	diagnostics to stderr.

   -DPROC1_DEBUG_PRINT
	to cause proc1 (the main thread) to print diagnostics to
	stderr.  */


#include "unix-dns.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <netdb.h>		/* for gethostbyname() */
#include <signal.h>		/* for kill() */
#include <sys/param.h>		/* for MAXHOSTNAMELEN */
#include <sys/wait.h>		/* for waitpid() */
#include <sys/time.h>		/* for gettimeofday() */
#include <signal.h>		/* for signal() and the signal names */

#ifdef AIX
#include <sys/select.h>  /* for fd_set */
#endif

#ifndef NO_SOCKS_NS_KLUDGE
# include <netinet/in.h>	/* for sockaddr_in (from inet.h) */
# include <arpa/inet.h>		/* for in_addr (from nameser.h) */
# include <arpa/nameser.h>	/* for MAXDNAME (from resolv.h) */
# include <resolv.h>		/* for res_init() and _res */
#endif

#if !defined(__irix)
  /* Looks like Irix is the only one that has getdtablehi()? */
# define getdtablehi() getdtablesize()
#endif

#if !defined(__irix)
  /* Looks like Irix and Solaris 5.5 are the only ones that have
     getdtablesize()... but since Solaris 5.4 doesn't have it,
     Solaris is out.  (If you find a system that doesn't even
     have FD_SETSIZE, just grab your ankles and set it to 255.)
   */
# define getdtablesize() (FD_SETSIZE)
#endif


#ifdef DNS_EXPLICITLY_FLUSH_PIPES
# include <stropts.h>		/* for I_FLUSH and FLUSHRW */
#endif

/* Rumor has it that some systems lie about this value, and actually accept
   host names longer than sys/param.h claims.  But it's definitely the case
   that some systems (and again I don't know which) crash if you pass a
   too-long hostname into gethostbyname().  So better safe than sorry.
 */
#ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN 64
#endif

#define ASSERT(x) assert(x)

#ifdef DNS_EXPLICITLY_FLUSH_PIPES
# define FLUSH(fd) ioctl(fd, I_FLUSH, FLUSHRW)
#else
# define FLUSH(x)
#endif

/* We use fdopen() to get the more convenient stdio interface on the pipes
   we use; but we don't want the stdio library to provide *any* buffering;
   just use the buffering that the pipe itself provides, so that when
   select() says there is no input available, fgetc() agrees.
 */
#define SET_BUFFERING(file) setvbuf((file), NULL, _IONBF, 0)


#ifdef RANDOMIZE_ADDRESSES
  /* Unix is Random. */
# if defined(UNIXWARE) || defined(_INCLUDE_HPUX_SOURCE) || (defined(__sun) && defined(__svr4__))
#  define RANDOM  rand
#  define SRANDOM srand
# else /* !BSD-incompatible-Unixes */
#  define RANDOM  random
#  define SRANDOM srandom
# endif /* !BSD-incompatible-Unixes */
#endif /* RANDOMIZE_ADDRESSES */


/* Way kludgy debugging/logging interface, since gdb's support
   for debugging fork'ed processes is pathetic.
 */
#ifdef DEBUG
#define LOG_PROCn(PROC,PREFIX,BUF,SUFFIX,QL)
#else
#define LOG_PROCn(PROC,PREFIX,BUF,SUFFIX,QL) do{\
    fprintf(stderr, \
	    "\t" PROC " (%lu): " PREFIX ": (ql=%ld) %s" SUFFIX, \
	    ((unsigned long) getpid()), QL, BUF); \
  } while(0)
#endif

#ifdef PROC3_DEBUG_PRINT
# define LOG_PROC3(PREFIX,BUF,SUFFIX) LOG_PROCn("proc3",PREFIX,BUF,SUFFIX,0L)
#else
# define LOG_PROC3(PREFIX,BUF,SUFFIX)
#endif

#ifdef PROC2_DEBUG_PRINT
# define LOG_PROC2(PREFIX,BUF,SUFFIX) LOG_PROCn("proc2",PREFIX,BUF,SUFFIX,\
						proc2_queue_length())
#else
# define LOG_PROC2(PREFIX,BUF,SUFFIX)
#endif

#ifdef PROC1_DEBUG_PRINT
# define LOG_PROC1(PREFIX,BUF,SUFFIX) LOG_PROCn("proc1",PREFIX,BUF,SUFFIX,\
						proc1_queue_length())
#else
# define LOG_PROC1(PREFIX,BUF,SUFFIX)
#endif


/* The internal status codes that are used; these follow the basic
   SMTP/NNTP model of three-digit codes.
 */
#define DNS_STATUS_GETHOSTBYNAME_OK	101	/* proc3 -> proc2 */
#define DNS_STATUS_LOOKUP_OK		102	/* proc2 -> proc1 */
#define DNS_STATUS_KILLED_OK		103	/* proc2 -> proc1 */
#define DNS_STATUS_LOOKUP_STARTED	201	/* proc2 -> proc1 */
#define DNS_STATUS_GETHOSTBYNAME_FAILED 501	/* proc3 -> proc2 */
#define DNS_STATUS_LOOKUP_FAILED	502	/* proc2 -> proc1 */
#define DNS_STATUS_LOOKUP_NOT_STARTED	503	/* proc2 -> proc1 */
#define DNS_STATUS_KILL_FAILED		504	/* proc2 -> proc1 */
#define DNS_STATUS_UNIMPLEMENTED	601	/* unimplemented (ha ha) */
#define DNS_STATUS_INTERNAL_ERROR	602	/* assertion failure */

#define DNS_PROC2_NAME "(dns helper)"


static char *
string_trim(char *s)
{
  char *s2;
  if (!s) return 0;
  s2 = s + strlen(s) - 1;
  while (s2 > s && (*s2 == '\n' || *s2 == '\r' || *s2 == ' ' || *s2 == '\t'))
    *s2-- = 0;
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
    s++;
  return s;
}


/* Process 3.
   Inherits a string (hostname) from the spawner.
   Writes "1xx: a.b.c.d\n" or "5xx: error msg\n".
 */

static void
blocking_gethostbyname (const char *name, int out_fd,
			unsigned long random_number)
{
  char buf[MAXHOSTNAMELEN + 100];
  struct hostent *h;
  unsigned long which_addr = 0;

  static int firstTime=1;
  if (firstTime) {
    firstTime=0;
    dns_socks_kludge();
  }

#if GETHOSTBYNAME_DELAY
  {
    int i = GETHOSTBYNAME_DELAY;
    LOG_PROC3("sleeping","","\n");
    sleep(i);
  }
#endif /* GETHOSTBYNAME_DELAY */

  LOG_PROC3("gethostbyname",name,"\n");

  h = gethostbyname(name);

#ifdef RANDOMIZE_ADDRESSES
  if (h)
    {
      unsigned long n_addrs;
      for (n_addrs = 0; h->h_addr_list[n_addrs]; n_addrs++)
	;
      if (n_addrs > 0)
	which_addr = (random_number % n_addrs);
      else
	h = 0;
    }
#endif /* RANDOMIZE_ADDRESSES */

  if (h)
    sprintf(buf, "%d: %d.%d.%d.%d\n",
	    DNS_STATUS_GETHOSTBYNAME_OK,
	    ((unsigned char *) h->h_addr_list[which_addr])[0],
	    ((unsigned char *) h->h_addr_list[which_addr])[1],
	    ((unsigned char *) h->h_addr_list[which_addr])[2],
	    ((unsigned char *) h->h_addr_list[which_addr])[3]);
  else
    sprintf(buf, "%d: host %s not found\n",
	    DNS_STATUS_GETHOSTBYNAME_FAILED, name);

  LOG_PROC3("writing response",buf,"");

  write(out_fd, buf, strlen(buf));
  FLUSH(out_fd);
}



/* Process 2.
   Loops forever.

   Reads "lookup: hostname\n".
   Writes "2xx: id\n" or "5xx: error msg\n".
   Some time later, writes "1xx: id: a.b.c.d\n" or "5xx: id: error msg\n".

   -or-

   Reads "kill: id\n".
   Writes "1xx: killed" or "5xx: kill failed"
 */

typedef struct dns_lookup {
  long id;
  pid_t pid;
  FILE *fd;
  char *name;
  struct dns_lookup *next, *prev;
} dns_lookup;

static long dns_id_tick;
static dns_lookup *proc2_queue = 0;


static long
proc2_queue_length(void)
{
  dns_lookup *obj;
  long i = 0;
  for (obj = proc2_queue; obj; obj = obj->next) i++;
  return i;
}


static dns_lookup *
new_lookup_object (const char *name)
{
  dns_lookup *obj;
  char *n2;
  ASSERT(name);
  if (!name) return 0;

  n2 = strdup(name);
  if (!n2) return 0; /* MK_OUT_OF_MEMORY */

  obj = (dns_lookup *) malloc(sizeof(*obj));
  if (!obj) return 0;
  memset(obj, 0, sizeof(*obj));
  obj->id = ++dns_id_tick;
  obj->name = n2;
  obj->fd = 0;

  ASSERT(!proc2_queue || proc2_queue->prev == 0);
  obj->next = proc2_queue;
  if (proc2_queue)
    proc2_queue->prev = obj;
  proc2_queue = obj;

  return obj;
}


/* Frees the object associated with a host lookup on which proc2 is waiting,
   and writes the result to proc1.  Returns 0 if everything went ok, negative
   otherwise.  Status should be 0 (for "ok") or negative (the inverse of one
   of the three-digit response codes that *proc1* is expecting (not the codes
   that *proc3* returns.)n
 */
static int
free_lookup_object (dns_lookup *obj,
		    int out_fd,
		    int status, const char *error_msg,
		    const unsigned char ip_addr[4])
{
  char buf[MAXHOSTNAMELEN + 100];

  ASSERT(status == 0 || (status < -100 && status > -700));

  ASSERT(obj);
  if (!obj) return -1;

  if (status >= 0 && ip_addr)
    {
      sprintf(buf, "%d: %lu: %d.%d.%d.%d\n",
	      DNS_STATUS_LOOKUP_OK,
	      obj->id, ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);
    }
  else
    {
      ASSERT(error_msg);
      sprintf(buf, "%d: %lu: %s\n",
	      ((status < -100) ? -status : DNS_STATUS_INTERNAL_ERROR),
	      obj->id, (error_msg ? error_msg : "???"));
    }

  LOG_PROC2("writing response",buf,"");

  write(out_fd, buf, strlen(buf));
  FLUSH(out_fd);

  if (obj->fd)
    {
      fclose(obj->fd);
      obj->fd = 0;
    }
  if (obj->prev)
    {
      ASSERT(obj->prev->next != obj->next);
      obj->prev->next = obj->next;
    }
  if (obj->next)
    {
      ASSERT(obj->next->prev != obj->prev);
      obj->next->prev = obj->prev;
    }
  if (proc2_queue == obj)
    proc2_queue = obj->next;

  memset(obj, ~0, sizeof(obj));
  free(obj);

  return 0;
}


static dns_lookup *
spawn_lookup_process (const char *name, int out_fd)
{
  dns_lookup *obj = new_lookup_object(name);
  pid_t forked;
  int fds[2];		/* one reads from [0] and writes to [1]. */
  unsigned long random_number = 0;

  if (pipe (fds))
    {
      free_lookup_object(obj, out_fd,
			 -DNS_STATUS_LOOKUP_NOT_STARTED,
			 "can't make pipe", 0);
      obj = 0;
      return 0;
    }

  /* Save the fd from which we should read from the forked proc. */
  obj->fd = fdopen(fds[0], "r");
  if (!obj->fd)
    {
      close(fds[0]);
      close(fds[1]);
      free_lookup_object(obj, out_fd,
			 -DNS_STATUS_LOOKUP_NOT_STARTED,
			 "out of memory", 0);
      obj = 0;
      return 0;
    }
  SET_BUFFERING(obj->fd);

#ifdef RANDOMIZE_ADDRESSES
  /* Generate the random numbers in proc2 for use by proc3, so that the
     PRNG actually gets permuted. */
  random_number = (unsigned long) RANDOM();
#endif /* RANDOMIZE_ADDRESSES */


  switch (forked = fork ())
    {
    case -1:
      {
	close (fds[1]);
	free_lookup_object(obj, out_fd,
			   -DNS_STATUS_LOOKUP_NOT_STARTED,
			   "can't fork", 0);
	obj = 0;
	return 0;
      }
    break;

    case 0:	/* This is the forked process. */
      {
	/* Close the other side of the pipe (it's used by the main fork.) */
	close (fds[0]);

	/* Call gethostbyname, then write the result down the pipe. */
	blocking_gethostbyname(name, fds[1], random_number);

	/* Now exit this process. */
	close (fds[1]);
	exit(0);
      }
    break;

    default:
      {
	/* This is the "old" process (subproc pid is in `forked'.) */
	obj->pid = forked;

	/* This is the file descriptor we created for the benefit
	   of the child process - we don't need it in the parent. */
	close (fds[1]);

	return obj;  /* ok. */
      }
    break;
    }

  /* shouldn't get here. */
  ASSERT(0);
  return 0;
}


/* Given a line which is an asynchronous response from proc3, find the
   associated lookup object, feed the response on to proc1, and free
   the dns_lookup object.
 */
static void
handle_subproc_response(dns_lookup *obj, const char *line, int out_fd)
{
  int iip[4];
  char *s;
  int code = 0;
  int i;

  i = sscanf(line, "%d: %d.%d.%d.%d\n",
	     &code, &iip[0], &iip[1], &iip[2], &iip[3]);
  if (i != 5)
    {
      i = sscanf(line, "%d:", &code);
      if (i != 1 || code <= 100)
	code = DNS_STATUS_INTERNAL_ERROR;
    }

  switch (code)
    {
    case DNS_STATUS_GETHOSTBYNAME_OK:
      {
	unsigned char ip[5];
	ip[0] = iip[0]; ip[1] = iip[1]; ip[2] = iip[2]; ip[3] = iip[3];
	ip[4] = 0;  /* just in case */
	free_lookup_object (obj, out_fd, 0, 0, ip);
	obj = 0;
	break;
      }

    case DNS_STATUS_GETHOSTBYNAME_FAILED:
    default:
      {
	ASSERT(code == DNS_STATUS_GETHOSTBYNAME_FAILED);
	if (code == DNS_STATUS_GETHOSTBYNAME_FAILED)
	  code = DNS_STATUS_LOOKUP_FAILED;
	s = strchr(line, ':');
	if (s) s++;
	free_lookup_object (obj, out_fd,
			    (code > 100
			     ? -code
			     : -DNS_STATUS_INTERNAL_ERROR),
			    (s ? string_trim(s) : "???"), 0);
	obj = 0;
	break;
      }
    }
}


static void
cancel_lookup(long id, int out_fd)
{
  dns_lookup *obj = 0;

  if (proc2_queue)
    for (obj = proc2_queue; obj; obj = obj->next)
      if (obj->id == id) break;

  if (obj)
    {
      if (obj->pid)
	{
	  pid_t pid2;

      /*
       * SIGKILL causes the browser to hang if the user clicks on the stop
       * button while a long/bogus dns lookup is in progress.  According
       * to signal guru asharma, we should use SIGQUIT instead. -re
       */
/* 	  kill(obj->pid, SIGKILL); */
	  kill(obj->pid, SIGQUIT);
	  pid2 = waitpid(obj->pid, 0, 0);
	  ASSERT(obj->pid == pid2);
	}
      free_lookup_object(obj, out_fd, -DNS_STATUS_KILLED_OK, "cancelled", 0);
      obj = 0;
    }
  else
    {
      char buf[200];
      sprintf (buf, "%d: %lu: unable to cancel\n", DNS_STATUS_KILL_FAILED, id);
      LOG_PROC2("writing (kill) response",buf,"");
      write(out_fd, buf, strlen(buf));
    }

  FLUSH(out_fd);
}


static void
dns_socks_kludge(void)
{
#ifndef NO_SOCKS_NS_KLUDGE
  /* Gross historical kludge.
     If the environment variable $SOCKS_NS is defined, stomp on the host that
     the DNS code uses for host lookup to be a specific ip address.
   */
  char *ns = getenv("SOCKS_NS");
  if (ns && *ns) 
    {
      /* Gross hack added to Gross historical kludge - need to
       * initialize resolv.h structure first with low-cost call
       * to gethostbyname() before hacking _res. Subsequent call
       * to gethostbyname() will then pick up $SOCKS_NS address.
       */
      gethostbyname("localhost");

      res_init();
      _res.nsaddr_list[0].sin_addr.s_addr = inet_addr(ns);
      _res.nscount = 1;
    }
#endif /* !NO_SOCKS_NS_KLUDGE */
}


#ifdef PROC2_DEBUG_PRINT
static void
dns_signal_handler_dfl (int sig)
{
  char buf[100];
  signal (sig, SIG_DFL);
  sprintf(buf,"caught signal %d!\n",sig);
  LOG_PROC2("dns_signal_handler",buf,"");
  kill (getpid(), sig);
}
#endif /* PROC2_DEBUG_PRINT */

static void
dns_signal_handler_ign (int sig)
{
#ifdef PROC2_DEBUG_PRINT
  char buf[100];
  sprintf(buf,"caught signal %d -- ignoring.\n",sig);
  LOG_PROC2("dns_signal_handler",buf,"");
#endif /* PROC2_DEBUG_PRINT */
  signal (sig, dns_signal_handler_ign);
}


static void
dns_catch_signals(void)
{
#ifndef SIG_ERR
# define SIG_ERR -1
#endif

#ifdef PROC2_DEBUG_PRINT
  int sig;
  char buf[255];
  LOG_PROC2("dns_catch_signals","plugging in...\n","");
# define CATCH_SIGNAL_DFL(SIG) sig = SIG; \
  if (((int)signal(SIG, dns_signal_handler_dfl)) == ((int)SIG_ERR)) \
    goto FAIL
# define CATCH_SIGNAL_IGN(SIG) sig = SIG; \
  if (((int)signal(SIG, dns_signal_handler_ign)) == ((int)SIG_ERR)) \
    goto FAIL
#else  /* !PROC2_DEBUG_PRINT */
# define CATCH_SIGNAL_DFL(SIG) \
   if ((int)signal(SIG,SIG_DFL) == ((int)SIG_ERR)) goto FAIL
# define CATCH_SIGNAL_IGN(SIG) \
   if ((int)signal(SIG,SIG_IGN) == ((int)SIG_ERR)) goto FAIL
#endif /* !PROC2_DEBUG_PRINT */

  CATCH_SIGNAL_IGN(SIGHUP);
  CATCH_SIGNAL_IGN(SIGINT);
  CATCH_SIGNAL_IGN(SIGQUIT);
  CATCH_SIGNAL_DFL(SIGILL);
  CATCH_SIGNAL_DFL(SIGTRAP);
  CATCH_SIGNAL_DFL(SIGIOT);
  CATCH_SIGNAL_DFL(SIGABRT);
#  ifdef SIGEMT
  CATCH_SIGNAL_DFL(SIGEMT);
#  endif
  CATCH_SIGNAL_DFL(SIGFPE);
  CATCH_SIGNAL_DFL(SIGBUS);
  CATCH_SIGNAL_DFL(SIGSEGV);
#  ifdef SIGSYS
  CATCH_SIGNAL_DFL(SIGSYS);
#  endif
  CATCH_SIGNAL_DFL(SIGPIPE);
  CATCH_SIGNAL_DFL(SIGTERM);	/* ignore this one?  hmm... */
  CATCH_SIGNAL_IGN(SIGUSR1);
  CATCH_SIGNAL_IGN(SIGUSR2);
#  ifdef SIGXCPU
  CATCH_SIGNAL_DFL(SIGXCPU);
#  endif
#  ifdef SIGDANGER
  CATCH_SIGNAL_DFL(SIGDANGER);
#  endif
# undef CATCH_SIGNAL

  return;

 FAIL:
#ifdef PROC2_DEBUG_PRINT
  sprintf(buf,"\tproc2 (%lu): error handling signal %d",
	  (unsigned long) getpid(), sig);
  perror(buf);
#endif /* PROC2_DEBUG_PRINT */
  exit(1);
}


static void dns_driver_init(int argc, char **argv, int in_fd, int out_fd);
static void dns_driver_main_loop(int in_fd, int out_fd);

/* Essentially, this is the main() of the forked dns-helper process.
 */
static void
dns_driver(int argc, char **argv, int in_fd, int out_fd)
{
  /* Might not return, if we fork-and-exec. */
  dns_driver_init(argc, argv, in_fd, out_fd);

  /* Shouldn't return until proc1 has closed its end of the pipe. */
  dns_driver_main_loop(in_fd, out_fd);
}


static void
dns_driver_init(int argc, char **argv, int in_fd, int out_fd)
{
#ifdef CHANGING_ARGV_WORKS
  {
    LOG_PROC2("launched","","\n");
    ERROR! Write me!  (This should overwrite argv.)
  }
#else /* !CHANGING_ARGV_WORKS */
  {
    char proc2_name[] = DNS_PROC2_NAME "\000";

    /* If argv[0] is already the proc2 name, then we're already in proc2,
       so there's nothing else to do.

       Otherwise, re-exec the running program with a different name.
       Kludge, kludge, kludge.
     */
    if (!strcmp(argv[0], proc2_name))
      {
	LOG_PROC2("dns_driver_init","not respawning.\n","");
      }
    else
      {
	char *new_argv[2];
	new_argv[0] = proc2_name;
	new_argv[1] = 0;

	LOG_PROC2("dns_driver_init",
		  "execvp(... \"" DNS_PROC2_NAME "\")\n", "");

	/* Copy in_fd and out_fd onto stdin/stdout, in order to pass them
	   to the newly-exec'ed process. */
	dup2(in_fd, fileno(stdin));
	dup2(out_fd, fileno(stdout));

	execvp(argv[0], new_argv);

#ifdef DEBUG
	fprintf(stderr,
		"\nMozilla: execvp(\"%s\") failed!\n"
	"\tThis means that we were unable to fork() the dns-helper process,\n"
	"\tand so host-name lookups will happen in the foreground instead\n"
	"\tof in the background (and therefore won't be interruptible.)\n\n",
		argv[0]);
	exit(0);
      }
#endif
  }
#endif /* !CHANGING_ARGV_WORKS */


  /* Close any streams we're not using.
   */
  if (fileno(stdin) != in_fd && fileno(stdin) != out_fd)
    fclose(stdin);
  if (fileno(stdout) != in_fd && fileno(stdout) != out_fd)
    fclose(stdout);
  /* Never close stderr -- proc2 needs it in case the execvp() fails. */

  dns_socks_kludge();
  dns_catch_signals();

#ifdef RANDOMIZE_ADDRESSES
  {
    struct timeval tp;
    struct timezone tzp;
    int seed;
    gettimeofday(&tp, &tzp);
    seed = (999*tp.tv_sec) + (1001*tp.tv_usec) + (1003 * getpid());
    SRANDOM(seed);
  }
#endif /* RANDOMIZE_ADDRESSES */

}


static void
dns_driver_main_loop(int in_fd, int out_fd)
{
  int status = 0;
  FILE *in = fdopen(in_fd, "r");
  ASSERT(in);
  if (!in) return;  /* out of memory *already*?  We lost real bad... */
  SET_BUFFERING(in);

  while (1)
    {
      fd_set fdset;
      char line[MAXHOSTNAMELEN + 100];
      dns_lookup *obj = 0;
      dns_lookup *next = 0;

      FD_ZERO(&fdset);

      /* Select on our parent's input stream. */
      FD_SET(in_fd, &fdset);

      /* And select on the pipe associated with each child process. */
      if (proc2_queue)
	for (obj = proc2_queue; obj; obj = obj->next)
	  {
	    int fd;
	    ASSERT(obj->fd);
	    if (!obj->fd) continue;
	    fd = fileno(obj->fd);
	    ASSERT(fd >= 0 &&
		   fd != in_fd &&
		   fd != out_fd &&
		   !FD_ISSET(fd, &fdset));
	    FD_SET(fd, &fdset);
	  }

#ifdef PROC2_DEBUG_PRINT
      {
	int i = 0, j = 0;
	fprintf(stderr, "\tproc2 (%lu): entering select(",
		(unsigned long) getpid());
	for (i = 0; i < getdtablehi(); i++)
	  if (FD_ISSET(i, &fdset))
	    {
	      fprintf(stderr, "%s%d", (j == 0 ? "" : ", "), i);
	      j++;
	    }
	fprintf(stderr, ")\n");
      }
#endif /* PROC2_DEBUG_PRINT */

      status = select(getdtablehi(), &fdset, 0, 0, 0);

      LOG_PROC2("select returned","","\n");

      ASSERT(status > 0);
      if (status <= 0) continue;


      /* Handle input from our parent.
       */

      if (FD_ISSET (in_fd, &fdset))
	{
	  /* Read one line from the parent (blocking if necessary, which
	     it shouldn't be except for a trivial amount of time.) */
	  *line = 0;
	  fgets(line, sizeof(line)-1, in);

	  if (!*line)
	    {
	      LOG_PROC2("got EOF from proc1; exiting","","\n");
	      return;
	    }

	  LOG_PROC2("read command",line,"");

	  string_trim(line);

	  if (!strncmp(line, "kill: ", 6))
	    {
	      long id = 0;
	      status = sscanf(line+6, "%ld\n", &id);
	      ASSERT(status == 1);
	      if (id)
		cancel_lookup(id, out_fd);
	    }
	  else if (!strncmp(line, "lookup: ", 8))
	    {
	      char *name = line + 8;
	      obj = spawn_lookup_process(name, out_fd);
	      /* Write a response to the parent immediately (to assign an id.)
	       */
	      if (obj)
		sprintf(line, "%d: %lu\n",
			DNS_STATUS_LOOKUP_STARTED,
			obj->id);
	      else
		sprintf(line, "%d: %lu\n",
			DNS_STATUS_LOOKUP_NOT_STARTED,
			obj->id);

	      LOG_PROC2("writing initial",line,"");
	      write(out_fd, line, strlen(line));
	      FLUSH(out_fd);
	    }
	}


      /* Handle input from our kids.
       */

      for (obj = proc2_queue; obj; obj = next)
	{
	  next=obj->next;
	  if (FD_ISSET(fileno(obj->fd), &fdset))
	    {
	      pid_t pid = obj->pid;
	      pid_t pid2;

	      /* Read one line from the subprocess (blocking if necessary,
		 which it shouldn't be except for a trivial amount of time.) */
	      *line = 0;
	      fgets(line, sizeof(line)-1, obj->fd);

#ifdef PROC2_DEBUG_PRINT
	      fprintf(stderr, "\tproc2 (%lu): read from proc3 (%ld): %s",
		      (unsigned long) getpid(), obj->id, line);
#endif /* PROC2_DEBUG_PRINT */

	      handle_subproc_response(obj, line, out_fd);

	      /* `obj' has now been freed.  After having written one line, the
		 process should exit.  Wait for it now (we saved `pid' before
		 `obj' got freed.)
		 
		 The thing that confuses me is, if I *don't* do the waitpid()
		 here, zombies don't seem to get left around.  I expected them
		 to, and don't know why they don't.  (On Irix 6.2, at least.)
		 */
	      pid2 = waitpid(pid, 0, 0);
	    }
	}
    }

  ASSERT(0);   /* not reached */
}



/* Process 1 (the user-called code.)
 */


static FILE *dns_in_fd = 0;
static FILE *dns_out_fd = 0;


typedef struct queued_response {
  long id;
  int (*cb) (void *id, void *closure, int status, const char *result);
  void *closure;
  struct queued_response *next, *prev;
} queued_response;

static queued_response *proc1_queue = 0;


static long
proc1_queue_length(void)
{
  queued_response *obj;
  long i = 0;
  for (obj = proc1_queue; obj; obj = obj->next) i++;
  return i;
}


/* Call this from main() to initialize the async DNS library.
   Returns a file descriptor that should be selected for, or
   negative if something went wrong.  Pass it the argc/argv
   that your `main' was called with (it needs these pointers
   in order to give its forked subprocesses sensible names.)
 */
int
DNS_SpawnProcess(int argc, char **argv)
{
  static int already_spawned_p = 0;
  pid_t forked;
  int infds[2];		/* "in" and "out" are from the p.o.v. of proc2. */
  int outfds[2];	/* one reads from [0] and writes to [1]. */

  ASSERT(!already_spawned_p);
  if (already_spawned_p) return -1;
  already_spawned_p = 1;

#ifndef CHANGING_ARGV_WORKS
  if (!strcmp(argv[0], DNS_PROC2_NAME))
    {
      /* Oh look, we're already in proc2, so don't fork again.
	 Assume our side of the pipe is on stdin/stdout.
       */
      LOG_PROC2("DNS_SpawnProcess","reconnecting to stdin/stdout.\n","");

      /* doesn't return until the other side closes the pipe. */
      dns_driver(argc, argv, fileno(stdin), fileno(stdout));
      exit(0);
    }
#endif /* !CHANGING_ARGV_WORKS */

  LOG_PROC1("DNS_SpawnProcess","forking proc2.\n","");

  if (pipe (infds))
    {
      return -1; /* pipe error: return better error code? */
    }
  if (pipe (outfds))
    {
      close(infds[0]);
      close(infds[1]);
      return -1; /* pipe error: return better error code? */
    }

  switch (forked = fork ())
    {
    case -1:
      {
	close (infds[0]);
	close (infds[1]);
	close (outfds[0]);
	close (outfds[1]);
	return -1; /* fork error: return better error code? */
      }
    break;

    case 0:	/* This is the forked process. */
      {
	/* Close the other sides of the pipes (used by the main fork.) */
	close (infds[1]);
	close (outfds[0]);

	/* doesn't return until the other side closes the pipe. */
	dns_driver(argc, argv, infds[0], outfds[1]);

	close (infds[0]);
	close (outfds[1]);
	exit(-1);
      }
    break;

    default:
      {
	/* This is the "old" process (subproc pid is in `forked'.)
	   (Save that somewhere?)
	 */

	/* This is the file descriptor we created for the benefit
	   of the child process - we don't need it in the parent. */
	close (infds[0]);
	close (outfds[1]);

	/* Set up FILE objects for the proc3 side of the pipes.
	   (Note that the words "out" in `dns_out_fd' and `outfds'
	   have opposite senses here.)
	 */
	dns_out_fd = fdopen(infds[1], "w");
	dns_in_fd = fdopen(outfds[0], "r");

	SET_BUFFERING(dns_out_fd);
	SET_BUFFERING(dns_in_fd);

	return fileno(dns_in_fd);   /* ok! */
      }
    break;
    }
}


static queued_response *
new_queued_response (int (*cb) (void *, void *closure, int, const char *),
		     void *closure)
{
  queued_response *obj;

  obj = (queued_response *) malloc(sizeof(*obj));
  if (!obj) return 0;
  memset(obj, 0, sizeof(*obj));
  obj->id = 0;
  obj->cb = cb;
  obj->closure = closure;

  ASSERT(!proc1_queue || proc1_queue->prev == 0);
  obj->next = proc1_queue;
  if (proc1_queue)
    proc1_queue->prev = obj;
  proc1_queue = obj;

  return obj;
}


/* Frees the object associated with a host lookup on which proc1 is waiting,
   and runs the callback function.  The returned value is that of the callback.
 */
static int
free_queued_response (queued_response *obj, int status, const char *result)
{
  long id = obj->id;
  int (*cb) (void *id, void *closure, int status, const char *result)
    = obj->cb;
  void *closure = obj->closure;

  if( -1 == id ) return 0;

  if (obj->prev)
    {
      ASSERT(obj->prev->next != obj->next);
      obj->prev->next = obj->next;
    }
  if (obj->next)
    {
      ASSERT(obj->next->prev != obj->prev);
      obj->next->prev = obj->prev;
    }
  if (proc1_queue == obj)
    proc1_queue = obj->next;

  ASSERT(closure != (void *) obj);
  memset(obj, ~0, sizeof(*obj));
  free(obj);

  ASSERT(status < -100 && status > -700);
  if (status == -DNS_STATUS_LOOKUP_OK) status = 1;
  else if (status == -DNS_STATUS_KILL_FAILED) status = 0;

  if (cb)
    return cb((void *) id, closure, status, result);
  else
    return 0;
}


/* Given a line which is an asynchronous response from proc2, find the
   associated lookup object, run the callback, return the status that
   the callback returned, and free the queued_response object.
 */
static int
handle_async_response(char *line)
{
  int i;
  int code = 0;
  queued_response *obj = 0;
  long id = 0;
  int iip[4];

  ASSERT(line);
  if (!line || !*line) return -1;

  i = sscanf(line, "%d: %lu: %d.%d.%d.%d\n",
	     &code, &id,
	     &iip[0], &iip[1], &iip[2], &iip[3]);
  if (i != 6)
    {
      i = sscanf(line, "%d: %lu:", &code, &id);
      if (i != 2)
	{
	  i = sscanf(line, "%d:", &code);
	  if (i != 1)
	    code = DNS_STATUS_INTERNAL_ERROR;
	}
    }

  if (id)
    for (obj = proc1_queue; obj; obj = obj->next)
      if (obj->id == id) break;

  if (!obj)		/* got an id that we don't know about? */
    return -1;

  switch (code)
    {
    case DNS_STATUS_LOOKUP_OK:		/* "1xx: id: a.b.c.d\n" */
      {
	unsigned char ip[5];
	ip[0] = iip[0]; ip[1] = iip[1]; ip[2] = iip[2]; ip[3] = iip[3];
	ip[4] = 0;  /* just in case */
	return free_queued_response(obj, -DNS_STATUS_LOOKUP_OK, (char *) ip);
	break;
      }
    case DNS_STATUS_LOOKUP_FAILED:	/* "5xx: id: host not found\n" */
    default:
      {
	char *msg = 0;
	ASSERT(code == DNS_STATUS_LOOKUP_FAILED);
	msg = strchr(line, ':');
	if (msg)
	  {
	    char *s = strchr(msg, ':');
	    if (s) msg = s+1;
	  }
	else
	  {
	    msg = line;
	  }

	code = (code > 100 ? -code : -DNS_STATUS_INTERNAL_ERROR);
	free_queued_response(obj, code, string_trim(msg));
	return code;
	break;
      }
    }
  ASSERT(0); /* not reached */
}



/* Kick off an async DNS lookup;
   The returned value is an id representing this transaction;
    the result_callback will be run (in the main process) when we
    have a result.  Returns negative if something went wrong.
   If `status' is negative,`result' is an error message.
   If `status' is positive, `result' is a 4-character string of
   the IP address.
   If `status' is 0, then the lookup was prematurely aborted
    via a call to DNS_AbortHostLookup().
 */
int
DNS_AsyncLookupHost(const char *name,
		    int (*result_callback) (void *id,
					    void *closure,
					    int status,
					    const char *result),
		    void *closure,
		    void **id_return)
{
  char buf[MAXHOSTNAMELEN + 100];
  queued_response *obj = 0;
  int code = 0;
  int i;
  char *s;

  ASSERT(result_callback);
  if (!result_callback) return -1;
  ASSERT(id_return);
  if (!id_return) return -1;
  *id_return = 0;

  ASSERT(name);
  if (!name || !*name) return -1;  /* invalid host name: better error code? */
  if (strchr(name, '\n')) return -1; /* ditto */
  if (strchr(name, '\r')) return -1;

  if (strlen(name) > MAXHOSTNAMELEN)
    return -1; /* host name too long: return better error code? */

  obj = new_queued_response(result_callback, closure);
  if (!obj) return -1; /* out of memory: return better error code? */


  LOG_PROC1("writing: lookup",name,"\n");
  fprintf(dns_out_fd, "lookup: %s\n", name);
  fflush(dns_out_fd);
  FLUSH(fileno(dns_out_fd));

  /* We have just written a "lookup:" command to proc2.
     As soon as this command is received, proc2 will write a 2xx response
     back to us, ACKing it, and giving us an ID.  We must wait for that
     response in order to return the ID to our caller; we block waiting
     for it, knowing that it won't be long in coming.

     However, there could be other data in the pipe already -- the final
     results of previously launched commands (1xx responses or 5xx responses.)
     So we read commands, and process them, until we find the one we're
     looking for.

     Under no circumstances can there be more than one 2xx response in the
     pipe at a time (since this is the only place a command is issued that
     can cause a 2xx response to be generated, and each call waits for it.)
   */

AGAIN:

  *buf = 0;
  fgets(buf, sizeof(buf)-1, dns_in_fd);
  LOG_PROC1("read (lookup)",buf,"");

  obj->id = 0;

  if (!*buf)	/* EOF, perhaps? */
    {
      code = DNS_STATUS_INTERNAL_ERROR;
    }
  else
    {
      i = sscanf(buf, "%d:", &code);
      ASSERT(i == 1);
      if (code <= 100) code = DNS_STATUS_INTERNAL_ERROR;
    }

  switch (code)
    {
    case DNS_STATUS_LOOKUP_STARTED:	    /* 2xx: id\n" */
      i = sscanf(buf, "%d: %lu\n", &code, &obj->id);
      ASSERT(i == 2);
      *id_return = (void *) obj->id;
      return 0;
      break;

    case DNS_STATUS_LOOKUP_OK:		    /* 1xx or 5xx which is an */
    case DNS_STATUS_LOOKUP_FAILED:	    /* async response (not for us) */
      handle_async_response(buf);
      goto AGAIN;
      break;

    case DNS_STATUS_LOOKUP_NOT_STARTED:
    default:				    /* 5xx: error msg\n"
					       Presumably this is for us.
					       (If not, there's a bug...) */
      ASSERT(code == DNS_STATUS_LOOKUP_NOT_STARTED);
      s = strchr(buf, ':');
      if (s) s++;
      code = (code > 100 ? -code : -DNS_STATUS_INTERNAL_ERROR);
      code = free_queued_response(obj, code, (s ? string_trim(s) : "???"));
      obj = 0;
      return code;
      break;
    }
  ASSERT(0); /* not reached */
  return -1;
}


/* Prematurely give up on the given host-lookup transaction.
   The `token' is what was returned by DNS_AsyncLookupHost.
   This causes the result_callback to be called with a negative
   status.
 */
int
DNS_AbortHostLookup(void *token)
{
  char buf[MAXHOSTNAMELEN + 100];
  int code = 0;
  int i;
  long id = (int) token;
  long id2 = 0;
  queued_response *obj = 0;
  char *s;

  for (obj = proc1_queue; obj; obj = obj->next)
    if (obj->id == id) break;

  if (!obj) return -1;

#ifdef PROC1_DEBUG_PRINT
  fprintf(stderr, "\tproc1 (%lu): writing: kill: %lu (ql=%ld)\n",
	  (unsigned long) getpid(), id, proc1_queue_length());
#endif /* PROC1_DEBUG_PRINT */

  fprintf(dns_out_fd, "kill: %lu\n", id);
  fflush(dns_out_fd);
  FLUSH(fileno(dns_out_fd));


  /* We have just written a "kill:" command to proc2.
     As soon as this command is received, proc2 will write a 1xx or 5xx
     response back to us, ACKing it.  We must wait for that response in
     order to return an error code to our caller; we block waiting for it,
     knowing that it won't be long in coming.

     However, there could be other data in the pipe already -- the final
     results of previously launched commands (1xx responses or 5xx responses.)
     So we read commands, and process them, until we find the one we're
     looking for.

     Under no circumstances can there be more than one kill-response in the
     pipe at a time (since this is the only place a command is issued that
     can cause a kill-response response to be generated, and each call waits
     for it.)
   */

AGAIN:

  *buf = 0;
  fgets(buf, sizeof(buf)-1, dns_in_fd);
  LOG_PROC1("read (kill)",buf,"");

  i = sscanf(buf, "%d:", &code);
  ASSERT(i == 1);
  if (code <= 100) code = DNS_STATUS_INTERNAL_ERROR;

  switch (code)
    {
    case DNS_STATUS_KILLED_OK:
      return free_queued_response(obj, -DNS_STATUS_KILLED_OK, "killed");
      break;

    case DNS_STATUS_LOOKUP_OK:		    /* 1xx or 5xx which is an */
    case DNS_STATUS_LOOKUP_FAILED:	    /* async response (not for us) */

      /* Actually, it might be for us -- if we've issued a kill, but the
	 success/failure for this same code raced in before the response
	 to the kill (a kill-failure, presumably) then ignore this response
	 (wait for the kill-failure instead.) */
      id2 = 0;
      i = sscanf(buf, "%d: %lu:", &code, &id2);
      ASSERT(i == 2);
      if (i == 2 && id2 == id) {
          LOG_PROC1("Bug #87736 (1)",buf,"");
          handle_async_response(buf);
	goto AGAIN;
      }

      /* Not for us -- let it through. */
      handle_async_response(buf);
      goto AGAIN;
      break;

    case DNS_STATUS_KILL_FAILED:
    default:				    /* 5xx: error msg\n"
					       Presumably this is for us.
					       (If not, there's a bug...) */
      ASSERT(code == DNS_STATUS_KILL_FAILED);
      s = strchr(buf, ':');
      if (s)
	{
	  char *s2 = strchr(++s, ':');
	  if (s2) s = s2+1;
	}
      code = (code > 100 ? -code : -DNS_STATUS_INTERNAL_ERROR);
      free_queued_response(obj, code, (s ? string_trim(s) : "???"));
      LOG_PROC1("Bug #87736 (2)",buf,"");
      return code;
      break;
    }
  ASSERT(0); /* not reached */
  return -1;
}



/* The main select() loop of your program should call this when the fd
   that was returned by DNS_SpawnProcess comes active.  This may cause
   some of the result_callback functions to run.

   If this returns negative, then a fatal error has happened, and you
   should close `fd' and not select() for it again.  Call gethostbyname()
   in the foreground or something.
 */
int
DNS_ServiceProcess(int fd)
{
  char buf[MAXHOSTNAMELEN + 100];
  char *s = 0;
  int status = 0;
  int final_status = 0;

AGAIN:

  ASSERT(fd == fileno(dns_in_fd));
  if (fd != fileno(dns_in_fd)) return -1;

  ASSERT(final_status <= 0);

  /* Poll to see if input is available on fd.
     NOTE: This only works because we've marked the FILE* object wrapped
     around this fd as being fully unbuffered!!
   */
  {
    fd_set fdset;
    struct timeval t = { 0L, 0L, };
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    status = select(fd+1, &fdset, 0, 0, &t);
    if (status < 0 && final_status == 0)
      final_status = status;
    if (status <= 0)  /* error, or no input ready */
      return final_status;
  }

  *buf = 0;
  s = fgets(buf, sizeof(buf)-1, dns_in_fd);

  if (s && *s)
    {
      LOG_PROC1("read (async)",buf,"");
    }
  else
    {
      /* #### If there's nothing to read, the socket must be closed? */
      LOG_PROC1("read (async)","nothing -- returning error\n","");
      return (final_status < 0 ? final_status : -1);
    }

  /* call all pending procs, but remember the first error code that
     one of them returned.
     */
  status = handle_async_response(buf);
  status = 0; /* #### */
  if (status < 0 && final_status == 0)
    final_status = status;

  /* see if there is more to read. */
  goto AGAIN;
}



#ifdef DEBUG
void
print_fdset(fd_set *fdset)
{
  int i;
  int any = 0;
  if (!fdset) return;
  for (i = 0; i < getdtablesize(); i++)
    {
      if (FD_ISSET(i, fdset))
	{
	  if (any) fprintf(stderr, ", ");
	  any = 1;
	  fprintf(stderr, "%d", i);
	}
    }
}

void
print_fd(int fd)
{
  int status;
  fd_set fdset;
  struct timeval t = { 0L, 0L, };

  fprintf(stderr, "  %d:\n", fd);

  FD_ZERO(&fdset);
  FD_SET(fd, &fdset);
  status = select(fd+1, &fdset, 0, 0, &t);
  if (status == 0)
    fprintf(stderr, "    readable (no input)\n");
  else if (status == 1)
    fprintf(stderr, "    readable (pending)\n");
  else
    fprintf(stderr, "    unreadable (error %d)\n", status);

  FD_ZERO(&fdset);
  FD_SET(fd, &fdset);
  status = select(fd+1, 0, &fdset, 0, &t);
  if (status == 0)
    fprintf(stderr, "    writeable (no input)\n");
  else if (status == 1)
    fprintf(stderr, "    writeable (pending)\n");
  else
    fprintf(stderr, "    unwriteable (error %d)\n", status);

  FD_ZERO(&fdset);
  FD_SET(fd, &fdset);
  status = select(fd+1, 0, 0, &fdset, &t);
  if (status == 0)
    fprintf(stderr, "    exceptionable (no input)\n");
  else if (status == 1)
    fprintf(stderr, "    exceptionable (pending)\n");
  else
    fprintf(stderr, "    unexceptionable (error %d)\n", status);
}


void
print_fdsets(fd_set *read, fd_set *write, fd_set *except)
{
  int i;
  fd_set all;
  FD_ZERO(&all);

  fprintf(stderr, "rd: "); print_fdset(read);
  fprintf(stderr, "\nwr: "); print_fdset(write);
  fprintf(stderr, "\nex: "); print_fdset(except);
  fprintf(stderr, "\n");

  if (read)
    for (i = 0; i < getdtablesize(); i++)
      if (FD_ISSET(i, read)) FD_SET(i, &all);
  if (write)
    for (i = 0; i < getdtablesize(); i++)
      if (FD_ISSET(i, write)) FD_SET(i, &all);
  if (except)
    for (i = 0; i < getdtablesize(); i++)
      if (FD_ISSET(i, except)) FD_SET(i, &all);

  for (i = 0; i < getdtablesize(); i++)
    if (FD_ISSET(i, &all)) print_fd(i);
}

#endif /* DEBUG */




#ifdef STANDALONE

typedef struct test_id_cons {
  char *name;
  void *id;
  struct test_id_cons *next;
} test_id_cons;
static test_id_cons *test_list = 0;

static int
test_cb (void *id, void *closure, int status, const char *result)
{
  const char *argv0 = (char *) closure;
  test_id_cons *cons = test_list;

  while(cons)
    {
      if (cons->id == id) break;
      cons = cons->next;
      ASSERT(cons);
    }

  if (status == 1)
    {
      unsigned char *ip = (unsigned char *) result;
      fprintf(stderr, "%s: %s = %d.%d.%d.%d\n",
	      argv0, cons->name, ip[0], ip[1], ip[2], ip[3]);
    }
  else
    {
      fprintf(stderr, "%s: %s: error %d: %s\n",
	      argv0, cons->name, status, result);
    }
  return 0;
}

int
main (int argc, char **argv)
{
  char buf[MAXHOSTNAMELEN + 100];
  int fd = DNS_SpawnProcess(argc, argv);

  if (fd <= 0)
    {
      fprintf(stderr, "%s: dns init failed\n", argv[0]);
      exit(-1);
    }

  fprintf(stderr,
	  "Type host names to spawn lookups.\n"
	  "Type `abort ID' to kill one off.\n"
	  "Type `quit' when done.\n\n");

  while(1)
    {
      int status = 0;
      fd_set fdset;

      FD_ZERO(&fdset);
      FD_SET(fd, &fdset);
      FD_SET(fileno(stdin), &fdset);

      status = select(getdtablehi(), &fdset, 0, 0, 0);
      if (status <= 0)
	{
	  fprintf(stderr, "%s: select() returned %d\n", argv[0], status);
	  exit(-1);
	}

      if (!FD_ISSET(fd, &fdset) &&
	  !FD_ISSET(fileno(stdin), &fdset))
	{
	  fprintf(stderr, "%s: neither fd is set?\n", argv[0]);
	  exit(-1);
	}

      if (FD_ISSET(fd, &fdset))
	{
	  status = DNS_ServiceProcess(fd);
	  if (status < 0)
	    {
	      fprintf(stderr, "%s: DNS_ServiceProcess() returned %d\n",
		      argv[0], status);
	      exit(-1);
	    }
	}

      if (FD_ISSET(fileno(stdin), &fdset))
	{
	  /* Read a line from the user. */
	  char *line = fgets(buf, sizeof(buf)-1, stdin);

	  line = string_trim(line);

	  if (!strcmp(line, "quit") || !strcmp(line, "exit"))
	    {
	      fprintf(stderr, "buh bye now\n");
	      exit(0);
	    }
	  else if (!strncmp(line, "abort ", 6))
	    {
	      long id;
	      status = sscanf(line+6, "%lu\n", &id);
	      if (status != 1)
		{
		  test_id_cons *cons = test_list;
		  while(cons)
		    {
		      if (!strcmp(cons->name, line+6)) break;
		      cons = cons->next;
		    }
		  if (cons)
		    id = (long) cons->id;
		  else
		    {
		      fprintf(stderr, "%s: %s is not a known host or id.\n",
			      argv[0], line+6);
		      continue;
		    }
		}

	      status = DNS_AbortHostLookup((void *)id);
	      if (status < 0)
		fprintf(stderr, "%s: DNS_AbortHostLookup(%ld) returned %d\n",
			argv[0], id, status);
	    }
	  else if (strchr(line, ' ') || strchr(line, '\t'))
	    {
	      fprintf(stderr, "%s: unrecognized command %s.\n",
			      argv[0], line);
	    }
	  else
	    {
	      void *id = 0;
	      test_id_cons *cons = 0;

	      fprintf(stderr, "%s: looking up %s...", argv[0], line);
	      status = DNS_AsyncLookupHost(line, test_cb, argv[0], &id);
	      if (status == 0)
		fprintf(stderr, " id = %lu.\n", (long)id);
	      else
		fprintf(stderr,
		      "\n%s: DNS_AsyncLookupHost(%s) returned %d (id = %lu)\n",
			argv[0], line, status, (long)id);

	      cons = (test_id_cons *) malloc(sizeof(*cons));
	      cons->name = strdup(line);
	      cons->id = id;
	      cons->next = test_list;
	      test_list = cons;
	    }
	}
    }
}

#endif /* STANDALONE */
#endif /* UNIX_ASYNC_DNS && XP_UNIX */
