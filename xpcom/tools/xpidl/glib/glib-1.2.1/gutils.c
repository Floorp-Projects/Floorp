/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1998  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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
 * MT safe for the unix part, FIXME: make the win32 part MT safe as well.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glibconfig.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifndef XP_MAC
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#endif	/* XP_MAC */

#ifdef XP_MAC
#include <unistd.h>
#endif	/* XP_MAC */

#ifdef NATIVE_WIN32
#  define STRICT			/* Strict typing, please */
#  include <windows.h>
#  include <direct.h>
#  include <errno.h>
#  include <ctype.h>
#  ifdef _MSC_VER
#    include <io.h>
#  endif /* _MSC_VER */
#endif /* NATIVE_WIN32 */

/* implement Glib's inline functions
 */
#define	G_INLINE_FUNC extern
#define	G_CAN_INLINE 1
#include "glib.h"

#ifdef	MAXPATHLEN
#define	G_PATH_LENGTH	(MAXPATHLEN + 1)
#elif	defined (PATH_MAX)
#define	G_PATH_LENGTH	(PATH_MAX + 1)
#else	/* !MAXPATHLEN */
#define G_PATH_LENGTH   (2048 + 1)
#endif	/* !MAXPATHLEN && !PATH_MAX */

const guint glib_major_version = GLIB_MAJOR_VERSION;
const guint glib_minor_version = GLIB_MINOR_VERSION;
const guint glib_micro_version = GLIB_MICRO_VERSION;
const guint glib_interface_age = GLIB_INTERFACE_AGE;
const guint glib_binary_age = GLIB_BINARY_AGE;

#if defined (NATIVE_WIN32) && defined (__LCC__)
int __stdcall 
LibMain (void         *hinstDll,
	 unsigned long dwReason,
	 void         *reserved)
{
  return 1;
}
#endif /* NATIVE_WIN32 && __LCC__ */

void
g_atexit (GVoidFunc func)
{
  gint result;
  gchar *error = NULL;

  /* keep this in sync with glib.h */

#ifdef	G_NATIVE_ATEXIT
  result = ATEXIT (func);
  if (result)
    error = g_strerror (errno);
#elif defined (HAVE_ATEXIT)
#  ifdef NeXT /* @#%@! NeXTStep */
  result = !atexit ((void (*)(void)) func);
  if (result)
    error = g_strerror (errno);
#  else
  result = atexit ((void (*)(void)) func);
  if (result)
    error = g_strerror (errno);
#  endif /* NeXT */
#elif defined (HAVE_ON_EXIT)
  result = on_exit ((void (*)(int, void *)) func, NULL);
  if (result)
    error = g_strerror (errno);
#else
  result = 0;
  error = "no implementation";
#endif /* G_NATIVE_ATEXIT */

  if (error)
    g_error ("Could not register atexit() function: %s", error);
}

gint
g_snprintf (gchar	*str,
	    gulong	 n,
	    gchar const *fmt,
	    ...)
{
#ifdef	HAVE_VSNPRINTF
  va_list args;
  gint retval;
  
  va_start (args, fmt);
  retval = vsnprintf (str, n, fmt, args);
  va_end (args);
  
  return retval;
#else	/* !HAVE_VSNPRINTF */
  gchar *printed;
  va_list args;
  
  va_start (args, fmt);
  printed = g_strdup_vprintf (fmt, args);
  va_end (args);
  
  strncpy (str, printed, n);
  str[n-1] = '\0';

  g_free (printed);
  
  return strlen (str);
#endif	/* !HAVE_VSNPRINTF */
}

gint
g_vsnprintf (gchar	 *str,
	     gulong	  n,
	     gchar const *fmt,
	     va_list      args)
{
#ifdef	HAVE_VSNPRINTF
  gint retval;
  
  retval = vsnprintf (str, n, fmt, args);
  
  return retval;
#else	/* !HAVE_VSNPRINTF */
  gchar *printed;
  
  printed = g_strdup_vprintf (fmt, args);
  strncpy (str, printed, n);
  str[n-1] = '\0';

  g_free (printed);
  
  return strlen (str);
#endif /* !HAVE_VSNPRINTF */
}

guint	     
g_parse_debug_string  (const gchar *string, 
		       GDebugKey   *keys, 
		       guint	    nkeys)
{
  guint i;
  guint result = 0;
  
  g_return_val_if_fail (string != NULL, 0);
  
  if (!g_strcasecmp (string, "all"))
    {
      for (i=0; i<nkeys; i++)
	result |= keys[i].value;
    }
  else
    {
      gchar *str = g_strdup (string);
      gchar *p = str;
      gchar *q;
      gboolean done = FALSE;
      
      while (*p && !done)
	{
	  q = strchr (p, ':');
	  if (!q)
	    {
	      q = p + strlen(p);
	      done = TRUE;
	    }
	  
	  *q = 0;
	  
	  for (i=0; i<nkeys; i++)
	    if (!g_strcasecmp(keys[i].key, p))
	      result |= keys[i].value;
	  
	  p = q+1;
	}
      
      g_free (str);
    }
  
  return result;
}

gchar*
g_basename (const gchar	   *file_name)
{
  register gchar *base;
  
  g_return_val_if_fail (file_name != NULL, NULL);
  
  base = strrchr (file_name, G_DIR_SEPARATOR);
  if (base)
    return base + 1;

#ifdef NATIVE_WIN32
  if (isalpha (file_name[0]) && file_name[1] == ':')
    return (gchar*) file_name + 2;
#endif /* NATIVE_WIN32 */
  
  return (gchar*) file_name;
}

gboolean
g_path_is_absolute (const gchar *file_name)
{
  g_return_val_if_fail (file_name != NULL, FALSE);
  
  if (file_name[0] == G_DIR_SEPARATOR)
    return TRUE;

#ifdef NATIVE_WIN32
  if (isalpha (file_name[0]) && file_name[1] == ':' && file_name[2] == G_DIR_SEPARATOR)
    return TRUE;
#endif

  return FALSE;
}

gchar*
g_path_skip_root (gchar *file_name)
{
  g_return_val_if_fail (file_name != NULL, NULL);
  
  if (file_name[0] == G_DIR_SEPARATOR)
    return file_name + 1;

#ifdef NATIVE_WIN32
  if (isalpha (file_name[0]) && file_name[1] == ':' && file_name[2] == G_DIR_SEPARATOR)
    return file_name + 3;
#endif

  return NULL;
}

gchar*
g_dirname (const gchar	   *file_name)
{
  register gchar *base;
  register guint len;
  
  g_return_val_if_fail (file_name != NULL, NULL);
  
  base = strrchr (file_name, G_DIR_SEPARATOR);
  if (!base)
    return g_strdup (".");
  while (base > file_name && *base == G_DIR_SEPARATOR)
    base--;
  len = (guint) 1 + base - file_name;
  
  base = g_new (gchar, len + 1);
  g_memmove (base, file_name, len);
  base[len] = 0;
  
  return base;
}

gchar*
g_get_current_dir (void)
{
  gchar *buffer;
  gchar *dir;

  buffer = g_new (gchar, G_PATH_LENGTH);
  *buffer = 0;
  
  /* We don't use getcwd(3) on SUNOS, because, it does a popen("pwd")
   * and, if that wasn't bad enough, hangs in doing so.
   */
#if	defined (sun) && !defined (__SVR4)
  dir = getwd (buffer);
#else	/* !sun */
  dir = getcwd (buffer, G_PATH_LENGTH - 1);
#endif	/* !sun */
  
  if (!dir || !*buffer)
    {
      /* hm, should we g_error() out here?
       * this can happen if e.g. "./" has mode \0000
       */
      buffer[0] = G_DIR_SEPARATOR;
      buffer[1] = 0;
    }

  dir = g_strdup (buffer);
  g_free (buffer);
  
  return dir;
}

gchar*
g_getenv (const gchar *variable)
{
#ifndef NATIVE_WIN32
  g_return_val_if_fail (variable != NULL, NULL);

  return getenv (variable);
#else
  gchar *v;
  guint k;
  static gchar *p = NULL;
  static gint l;
  gchar dummy[2];

  g_return_val_if_fail (variable != NULL, NULL);
  
  v = getenv (variable);
  if (!v)
    return NULL;
  
  /* On Windows NT, it is relatively typical that environment variables
   * contain references to other environment variables. Handle that by
   * calling ExpandEnvironmentStrings.
   */

  /* First check how much space we need */
  k = ExpandEnvironmentStrings (v, dummy, 2);
  /* Then allocate that much, and actualy do the expansion */
  if (p == NULL)
    {
      p = g_malloc (k);
      l = k;
    }
  else if (k > l)
    {
      p = g_realloc (p, k);
      l = k;
    }
  ExpandEnvironmentStrings (v, p, k);
  return p;
#endif
}


G_LOCK_DEFINE_STATIC (g_utils_global);

static	gchar	*g_tmp_dir = NULL;
static	gchar	*g_user_name = NULL;
static	gchar	*g_real_name = NULL;
static	gchar	*g_home_dir = NULL;

/* HOLDS: g_utils_global_lock */
static void
g_get_any_init (void)
{
  if (!g_tmp_dir)
    {
      g_tmp_dir = g_strdup (g_getenv ("TMPDIR"));
      if (!g_tmp_dir)
	g_tmp_dir = g_strdup (g_getenv ("TMP"));
      if (!g_tmp_dir)
	g_tmp_dir = g_strdup (g_getenv ("TEMP"));
      
#ifdef P_tmpdir
      if (!g_tmp_dir)
	{
	  int k;
	  g_tmp_dir = g_strdup (P_tmpdir);
	  k = strlen (g_tmp_dir);
	  if (g_tmp_dir[k-1] == G_DIR_SEPARATOR)
	    g_tmp_dir[k-1] = '\0';
	}
#endif
      
      if (!g_tmp_dir)
	{
#ifndef NATIVE_WIN32
	  g_tmp_dir = g_strdup ("/tmp");
#else /* NATIVE_WIN32 */
	  g_tmp_dir = g_strdup ("C:\\");
#endif /* NATIVE_WIN32 */
	}
      
      if (!g_home_dir)
	g_home_dir = g_strdup (g_getenv ("HOME"));
      
#ifdef NATIVE_WIN32
      if (!g_home_dir)
	{
	  /* The official way to specify a home directory on NT is
	   * the HOMEDRIVE and HOMEPATH environment variables.
	   *
	   * This is inside #ifdef NATIVE_WIN32 because with the cygwin dll,
	   * HOME should be a POSIX style pathname.
	   */
	  
	  if (getenv ("HOMEDRIVE") != NULL && getenv ("HOMEPATH") != NULL)
	    {
	      gchar *homedrive, *homepath;
	      
	      homedrive = g_strdup (g_getenv ("HOMEDRIVE"));
	      homepath = g_strdup (g_getenv ("HOMEPATH"));
	      
	      g_home_dir = g_strconcat (homedrive, homepath, NULL);
	      g_free (homedrive);
	      g_free (homepath);
	    }
	}
#endif /* !NATIVE_WIN32 */
      
#ifdef HAVE_PWD_H
      {
	struct passwd *pw = NULL;
	gpointer buffer = NULL;
	
#  ifdef HAVE_GETPWUID_R
        struct passwd pwd;
        guint bufsize = 64;
        gint error;
	
        do
          {
            g_free (buffer);
            buffer = g_malloc (bufsize);
	    errno = 0;
	    
#    ifdef HAVE_GETPWUID_R_POSIX
	    error = getpwuid_r (getuid (), &pwd, buffer, bufsize, &pw);
            error = error < 0 ? errno : error;
#    else /* !HAVE_GETPWUID_R_POSIX */
#      ifdef _AIX
	    error = getpwuid_r (getuid (), &pwd, buffer, bufsize);
	    pw = error == 0 ? &pwd : NULL;
#      else /* !_AIX */
            pw = getpwuid_r (getuid (), &pwd, buffer, bufsize);
            error = pw ? 0 : errno;
#      endif /* !_AIX */            
#    endif /* !HAVE_GETPWUID_R_POSIX */
	    
	    if (!pw)
	      {
		/* we bail out prematurely if the user id can't be found
		 * (should be pretty rare case actually), or if the buffer
		 * should be sufficiently big and lookups are still not
		 * successfull.
		 */
		if (error == 0 || error == ENOENT)
		  {
		    g_warning ("getpwuid_r(): failed due to: No such user %d.",
			       getuid ());
		    break;
		  }
		if (bufsize > 32 * 1024)
		  {
		    g_warning ("getpwuid_r(): failed due to: %s.",
			       g_strerror (error));
		    break;
		  }
		
		bufsize *= 2;
	      }
	  }
	while (!pw);
#  endif /* !HAVE_GETPWUID_R */
	
	if (!pw)
	  {
	    setpwent ();
	    pw = getpwuid (getuid ());
	    endpwent ();
	  }
	if (pw)
	  {
	    g_user_name = g_strdup (pw->pw_name);
	    g_real_name = g_strdup (pw->pw_gecos);
	    if (!g_home_dir)
	      g_home_dir = g_strdup (pw->pw_dir);
	  }
	g_free (buffer);
      }
      
#else /* !HAVE_PWD_H */
      
#  ifdef NATIVE_WIN32
      {
	guint len = 17;
	gchar buffer[17];
	
	if (GetUserName (buffer, &len))
	  {
	    g_user_name = g_strdup (buffer);
	    g_real_name = g_strdup (buffer);
	  }
      }
#  endif /* NATIVE_WIN32 */
      
#endif /* !HAVE_PWD_H */
      
      if (!g_user_name)
	g_user_name = g_strdup ("somebody");
      if (!g_real_name)
	g_real_name = g_strdup ("Unknown");
      else
	{
	  gchar *p;

	  for (p = g_real_name; *p; p++)
	    if (*p == ',')
	      {
		*p = 0;
		p = g_strdup (g_real_name);
		g_free (g_real_name);
		g_real_name = p;
		break;
	      }
	}
    }
}

gchar*
g_get_user_name (void)
{
  G_LOCK (g_utils_global);
  if (!g_tmp_dir)
    g_get_any_init ();
  G_UNLOCK (g_utils_global);
  
  return g_user_name;
}

gchar*
g_get_real_name (void)
{
  G_LOCK (g_utils_global);
  if (!g_tmp_dir)
    g_get_any_init ();
  G_UNLOCK (g_utils_global);
 
  return g_real_name;
}

/* Return the home directory of the user. If there is a HOME
 * environment variable, its value is returned, otherwise use some
 * system-dependent way of finding it out. If no home directory can be
 * deduced, return NULL.
 */

gchar*
g_get_home_dir (void)
{
  G_LOCK (g_utils_global);
  if (!g_tmp_dir)
    g_get_any_init ();
  G_UNLOCK (g_utils_global);
  
  return g_home_dir;
}

/* Return a directory to be used to store temporary files. This is the
 * value of the TMPDIR, TMP or TEMP environment variables (they are
 * checked in that order). If none of those exist, use P_tmpdir from
 * stdio.h.  If that isn't defined, return "/tmp" on POSIXly systems,
 * and C:\ on Windows.
 */

gchar*
g_get_tmp_dir (void)
{
  G_LOCK (g_utils_global);
  if (!g_tmp_dir)
    g_get_any_init ();
  G_UNLOCK (g_utils_global);
  
  return g_tmp_dir;
}

static gchar *g_prgname = NULL;

gchar*
g_get_prgname (void)
{
  gchar* retval;

  G_LOCK (g_utils_global);
  retval = g_prgname;
  G_UNLOCK (g_utils_global);

  return retval;
}

void
g_set_prgname (const gchar *prgname)
{
  gchar *c;
    
  G_LOCK (g_utils_global);
  c = g_prgname;
  g_prgname = g_strdup (prgname);
  g_free (c);
  G_UNLOCK (g_utils_global);
}

guint
g_direct_hash (gconstpointer v)
{
  return GPOINTER_TO_UINT (v);
}

gint
g_direct_equal (gconstpointer v1,
		gconstpointer v2)
{
  return v1 == v2;
}

gint
g_int_equal (gconstpointer v1,
	     gconstpointer v2)
{
  return *((const gint*) v1) == *((const gint*) v2);
}

guint
g_int_hash (gconstpointer v)
{
  return *(const gint*) v;
}

#if 0 /* Old IO Channels */

GIOChannel*
g_iochannel_new (gint fd)
{
  GIOChannel *channel = g_new (GIOChannel, 1);

  channel->fd = fd;

#ifdef NATIVE_WIN32
  channel->peer = 0;
  channel->peer_fd = 0;
  channel->offset = 0;
  channel->need_wakeups = 0;
#endif /* NATIVE_WIN32 */

  return channel;
}

void
g_iochannel_free (GIOChannel *channel)
{
  g_return_if_fail (channel != NULL);

  g_free (channel);
}

void
g_iochannel_close_and_free (GIOChannel *channel)
{
  g_return_if_fail (channel != NULL);

  close (channel->fd);

  g_iochannel_free (channel);
}

#undef g_iochannel_wakeup_peer

void
g_iochannel_wakeup_peer (GIOChannel *channel)
{
#ifdef NATIVE_WIN32
  static guint message = 0;
#endif

  g_return_if_fail (channel != NULL);

#ifdef NATIVE_WIN32
  if (message == 0)
    message = RegisterWindowMessage ("gdk-pipe-readable");

#  if 0
  g_print ("g_iochannel_wakeup_peer: calling PostThreadMessage (%#x, %d, %d, %d)\n",
	   channel->peer, message, channel->peer_fd, channel->offset);
#  endif
  PostThreadMessage (channel->peer, message,
		     channel->peer_fd, channel->offset);
#endif /* NATIVE_WIN32 */
}

#endif /* Old IO Channels */

#ifdef NATIVE_WIN32
#ifdef _MSC_VER

int
gwin_ftruncate (gint  fd,
		guint size)
{
  HANDLE hfile;
  guint curpos;

  g_return_val_if_fail (fd >= 0, -1);
  
  hfile = (HANDLE) _get_osfhandle (fd);
  curpos = SetFilePointer (hfile, 0, NULL, FILE_CURRENT);
  if (curpos == 0xFFFFFFFF
      || SetFilePointer (hfile, size, NULL, FILE_BEGIN) == 0xFFFFFFFF
      || !SetEndOfFile (hfile))
    {
      gint error = GetLastError ();

      switch (error)
	{
	case ERROR_INVALID_HANDLE:
	  errno = EBADF;
	  break;
	default:
	  errno = EIO;
	  break;
	}

      return -1;
    }

  return 0;
}

DIR*
gwin_opendir (const char *dirname)
{
  DIR *result;
  gchar *mask;
  guint k;

  g_return_val_if_fail (dirname != NULL, NULL);

  result = g_new0 (DIR, 1);
  result->find_file_data = g_new0 (WIN32_FIND_DATA, 1);
  result->dir_name = g_strdup (dirname);
  
  k = strlen (result->dir_name);
  if (k && result->dir_name[k - 1] == '\\')
    {
      result->dir_name[k - 1] = '\0';
      k--;
    }
  mask = g_strdup_printf ("%s\\*", result->dir_name);

  result->find_file_handle = (guint) FindFirstFile (mask,
					     (LPWIN32_FIND_DATA) result->find_file_data);
  g_free (mask);

  if (result->find_file_handle == (guint) INVALID_HANDLE_VALUE)
    {
      int error = GetLastError ();

      g_free (result->dir_name);
      g_free (result->find_file_data);
      g_free (result);
      switch (error)
	{
	default:
	  errno = EIO;
	  return NULL;
	}
    }
  result->just_opened = TRUE;

  return result;
}

struct dirent*
gwin_readdir (DIR *dir)
{
  static struct dirent result;

  g_return_val_if_fail (dir != NULL, NULL);

  if (dir->just_opened)
    dir->just_opened = FALSE;
  else
    {
      if (!FindNextFile ((HANDLE) dir->find_file_handle,
			 (LPWIN32_FIND_DATA) dir->find_file_data))
	{
	  int error = GetLastError ();

	  switch (error)
	    {
	    case ERROR_NO_MORE_FILES:
	      return NULL;
	    default:
	      errno = EIO;
	      return NULL;
	    }
	}
    }
  strcpy (result.d_name, g_basename (((LPWIN32_FIND_DATA) dir->find_file_data)->cFileName));
      
  return &result;
}

void
gwin_rewinddir (DIR *dir)
{
  gchar *mask;

  g_return_if_fail (dir != NULL);

  if (!FindClose ((HANDLE) dir->find_file_handle))
    g_warning ("gwin_rewinddir(): FindClose() failed\n");

  mask = g_strdup_printf ("%s\\*", dir->dir_name);
  dir->find_file_handle = (guint) FindFirstFile (mask,
					  (LPWIN32_FIND_DATA) dir->find_file_data);
  g_free (mask);

  if (dir->find_file_handle == (guint) INVALID_HANDLE_VALUE)
    {
      int error = GetLastError ();

      switch (error)
	{
	default:
	  errno = EIO;
	  return;
	}
    }
  dir->just_opened = TRUE;
}  

gint
gwin_closedir (DIR *dir)
{
  g_return_val_if_fail (dir != NULL, -1);

  if (!FindClose ((HANDLE) dir->find_file_handle))
    {
      int error = GetLastError ();

      switch (error)
	{
	default:
	  errno = EIO; return -1;
	}
    }

  g_free (dir->dir_name);
  g_free (dir->find_file_data);
  g_free (dir);

  return 0;
}

#endif /* _MSC_VER */

#endif /* NATIVE_WIN32 */
