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
/* acconfig.h
   This file is in the public domain.

   Descriptive text for the C preprocessor macros that
   the distributed Autoconf macros can define.
   No software package will use all of them; autoheader copies the ones
   your configure.in uses into your configuration header file templates.

   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  Although this order
   can split up related entries, it makes it easier to check whether
   a given entry is in the file.

   Leave the following blank line there!!  Autoheader needs it.  */


/* Other stuff */

#undef ENABLE_MEM_CHECK
#undef ENABLE_MEM_PROFILE

#undef REALLOC_0_WORKS

#undef G_COMPILED_WITH_DEBUGGING
#undef G_THREADS_ENABLED

#undef GLIB_SIZEOF_GMUTEX
#undef GLIB_BYTE_CONTENTS_GMUTEX

#undef HAVE_BROKEN_WCTYPE
#undef HAVE_DOPRNT
#undef HAVE_FLOAT_H
#undef HAVE_GETPWUID_R
#undef HAVE_GETPWUID_R_POSIX
#undef HAVE_LIMITS_H
#undef HAVE_LONG_DOUBLE
#undef HAVE_POLL
#undef HAVE_PTHREAD_COND_TIMEDWAIT_POSIX
#undef HAVE_PTHREAD_GETSPECIFIC_POSIX
#undef HAVE_PTHREAD_MUTEX_TRYLOCK_POSIX
#undef HAVE_PWD_H
#undef HAVE_SYS_PARAM_H
#undef HAVE_SYS_POLL_H
#undef HAVE_SYS_SELECT_H
#undef HAVE_SYS_TIME_H
#undef HAVE_SYS_TIMES_H
#undef HAVE_STRERROR
#undef HAVE_STRSIGNAL
#undef HAVE_UNISTD_H
#undef HAVE_VALUES_H
#undef HAVE_WCHAR_H
#undef HAVE_WCTYPE_H

#undef NO_FD_SET
#undef NO_SYS_ERRLIST
#undef NO_SYS_SIGLIST
#undef NO_SYS_SIGLIST_DECL

#undef SIZEOF_CHAR
#undef SIZEOF_SHORT
#undef SIZEOF_LONG
#undef SIZEOF_INT
#undef SIZEOF_VOID_P

#undef G_VA_COPY
#undef G_VA_COPY_AS_ARRAY
#undef G_HAVE___INLINE
#undef G_HAVE___INLINE__
#undef G_HAVE_INLINE

#undef GLIB_MAJOR_VERSION
#undef GLIB_MINOR_VERSION
#undef GLIB_MICRO_VERSION
#undef GLIB_INTERFACE_AGE
#undef GLIB_BINARY_AGE

#undef WIN32
#undef NATIVE_WIN32

#undef G_THREAD_SOURCE

/* #undef PACKAGE */
/* #undef VERSION */




/* Leave that blank line there!!  Autoheader needs it.
   If you're adding to this file, keep in mind:
   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  */
