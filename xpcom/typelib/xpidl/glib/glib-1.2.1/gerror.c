/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe ; except for g_on_error_stack_trace, but who wants thread safety 
 * then
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "glib.h"
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif

#ifndef XP_MAC
#include <sys/types.h>
#endif

#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

#ifdef STDC_HEADERS
#include <string.h> /* for bzero on BSD systems */
#endif

#ifdef _MSC_VER
#include <process.h>		/* For _getpid() */
#endif

#ifndef NO_FD_SET
#  define SELECT_MASK fd_set
#else
#  ifndef _AIX
     typedef long fd_mask;
#  endif
#  if defined(_IBMR2)
#    define SELECT_MASK void
#  else
#    define SELECT_MASK int
#  endif
#endif


static void stack_trace (char **args);

extern volatile gboolean glib_on_error_halt;
volatile gboolean glib_on_error_halt = TRUE;

void
g_on_error_query (const gchar *prg_name)
{
  static const gchar *query1 = "[E]xit, [H]alt";
  static const gchar *query2 = ", show [S]tack trace";
  static const gchar *query3 = " or [P]roceed";
  gchar buf[16];

  if (!prg_name)
    prg_name = g_get_prgname ();
  
 retry:
  
  if (prg_name)
    fprintf (stdout,
	     "%s (pid:%u): %s%s%s: ",
	     prg_name,
	     (guint) getpid (),
	     query1,
	     query2,
	     query3);
  else
    fprintf (stdout,
	     "(process:%u): %s%s: ",
	     (guint) getpid (),
	     query1,
	     query3);
  fflush (stdout);
  
  fgets (buf, 8, stdin);

  if ((buf[0] == 'E' || buf[0] == 'e')
      && buf[1] == '\n')
    _exit (0);
  else if ((buf[0] == 'P' || buf[0] == 'p')
	   && buf[1] == '\n')
    return;
  else if (prg_name
	   && (buf[0] == 'S' || buf[0] == 's')
	   && buf[1] == '\n')
    {
      g_on_error_stack_trace (prg_name);
      goto retry;
    }
  else if ((buf[0] == 'H' || buf[0] == 'h')
	   && buf[1] == '\n')
    {
      while (glib_on_error_halt)
	;
      glib_on_error_halt = TRUE;
      return;
    }
  else
    goto retry;
}

void
g_on_error_stack_trace (const gchar *prg_name)
{
#if !defined(NATIVE_WIN32) && !defined(XP_MAC)
  pid_t pid;
  gchar buf[16];
  gchar *args[4] = { "gdb", NULL, NULL, NULL };

  if (!prg_name)
    return;

  sprintf (buf, "%u", (guint) getpid ());

  args[1] = (gchar*) prg_name;
  args[2] = buf;

  pid = fork ();
  if (pid == 0)
    {
      stack_trace (args);
      _exit (0);
    }
  else if (pid == (pid_t) -1)
    {
      perror ("unable to fork gdb");
      return;
    }
  
  while (glib_on_error_halt)
    ;
  glib_on_error_halt = TRUE;
#else
  abort ();
#endif
}

static gboolean stack_trace_done = FALSE;

static void
stack_trace_sigchld (int signum)
{
  stack_trace_done = TRUE;
}

static void
stack_trace (char **args)
{
#if !defined(NATIVE_WIN32) && !defined(XP_MAC)
  pid_t pid;
  int in_fd[2];
  int out_fd[2];
  SELECT_MASK fdset;
  SELECT_MASK readset;
  struct timeval tv;
  int sel, index, state;
  char buffer[256];
  char c;

  stack_trace_done = FALSE;
  signal (SIGCHLD, stack_trace_sigchld);

  if ((pipe (in_fd) == -1) || (pipe (out_fd) == -1))
    {
      perror ("unable to open pipe");
      _exit (0);
    }

  pid = fork ();
  if (pid == 0)
    {
      close (0); dup (in_fd[0]);   /* set the stdin to the in pipe */
      close (1); dup (out_fd[1]);  /* set the stdout to the out pipe */
      close (2); dup (out_fd[1]);  /* set the stderr to the out pipe */

      execvp (args[0], args);      /* exec gdb */
      perror ("exec failed");
      _exit (0);
    }
  else if (pid == (pid_t) -1)
    {
      perror ("unable to fork");
      _exit (0);
    }

  FD_ZERO (&fdset);
  FD_SET (out_fd[0], &fdset);

  write (in_fd[1], "backtrace\n", 10);
  write (in_fd[1], "p x = 0\n", 8);
  write (in_fd[1], "quit\n", 5);

  index = 0;
  state = 0;

  while (1)
    {
      readset = fdset;
      tv.tv_sec = 1;
      tv.tv_usec = 0;

      sel = select (FD_SETSIZE, &readset, NULL, NULL, &tv);
      if (sel == -1)
        break;

      if ((sel > 0) && (FD_ISSET (out_fd[0], &readset)))
        {
          if (read (out_fd[0], &c, 1))
            {
              switch (state)
                {
                case 0:
                  if (c == '#')
                    {
                      state = 1;
                      index = 0;
                      buffer[index++] = c;
                    }
                  break;
                case 1:
                  buffer[index++] = c;
                  if ((c == '\n') || (c == '\r'))
                    {
                      buffer[index] = 0;
                      fprintf (stdout, "%s", buffer);
                      state = 0;
                      index = 0;
                    }
                  break;
                default:
                  break;
                }
            }
        }
      else if (stack_trace_done)
        break;
    }

  close (in_fd[0]);
  close (in_fd[1]);
  close (out_fd[0]);
  close (out_fd[1]);
  _exit (0);
#else
  abort ();
#endif
}
