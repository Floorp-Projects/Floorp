/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * giowin32.c: IO Channels for Win32.
 * Copyright 1998 Owen Taylor and Tor Lillqvist
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


#include "config.h"
#include "glib.h"
#include <windows.h>
#include <winsock.h>		/* Not everybody has winsock2 */
#include <fcntl.h>
#include <io.h>
#include <errno.h>
#include <sys/types.h>

#include <stdio.h>

typedef struct _GIOWin32Channel GIOWin32Channel;
typedef struct _GIOWin32Watch GIOWin32Watch;

guint g_pipe_readable_msg;

typedef enum {
  G_IO_WINDOWS_MESSAGES,	/* Windows messages */
  G_IO_FILE_DESC,		/* Unix-like file descriptors from _open*/
  G_IO_PIPE,			/* pipe, with windows messages for signalling */
  G_IO_STREAM_SOCKET		/* Stream sockets */
} GIOWin32ChannelType;

struct _GIOWin32Channel {
  GIOChannel channel;
  gint fd;			/* Either a Unix-like file handle as provided
				 * by the Microsoft C runtime, or a SOCKET
				 * as provided by WinSock.
				 */
  GIOWin32ChannelType type;

  /* This is used by G_IO_WINDOWS_MESSAGES channels */
  HWND hwnd;			/* handle of window, or NULL */

  /* This is used by G_IO_PIPE channels */
  guint peer;			/* thread id of reader */
  guint peer_fd;		/* fd in the reader */
  guint offset;			/* counter of accumulated bytes */
  guint need_wakeups;		/* in output channels whether the
				 * reader needs wakeups
				 */
};

struct _GIOWin32Watch {
  GPollFD       pollfd;
  GIOChannel   *channel;
  GIOCondition  condition;
  GIOFunc       callback;
};

static gboolean g_io_win32_msg_prepare  (gpointer  source_data, 
					 GTimeVal *current_time,
					 gint     *timeout);
static gboolean g_io_win32_msg_check    (gpointer  source_data,
					 GTimeVal *current_time);
static gboolean g_io_win32_msg_dispatch (gpointer  source_data,
					 GTimeVal *current_time,
					 gpointer  user_data);

static gboolean g_io_win32_fd_prepare  (gpointer  source_data, 
					GTimeVal *current_time,
					gint     *timeout);
static gboolean g_io_win32_fd_check    (gpointer  source_data,
					GTimeVal *current_time);
static gboolean g_io_win32_fd_dispatch (gpointer  source_data,
					GTimeVal *current_time,
					gpointer  user_data);

static gboolean g_io_win32_pipe_prepare  (gpointer  source_data, 
					  GTimeVal *current_time,
					  gint     *timeout);
static gboolean g_io_win32_pipe_check    (gpointer  source_data,
					  GTimeVal *current_time);
static gboolean g_io_win32_pipe_dispatch (gpointer  source_data,
					  GTimeVal *current_time,
					  gpointer  user_data);
static void g_io_win32_pipe_destroy	 (gpointer source_data);

static gboolean g_io_win32_sock_prepare  (gpointer  source_data, 
					  GTimeVal *current_time,
					  gint     *timeout);
static gboolean g_io_win32_sock_check    (gpointer  source_data,
					  GTimeVal *current_time);
static gboolean g_io_win32_sock_dispatch (gpointer  source_data,
					  GTimeVal *current_time,
					  gpointer  user_data);

static void g_io_win32_destroy (gpointer source_data);

static GIOError g_io_win32_msg_read (GIOChannel *channel, 
				     gchar      *buf, 
				     guint       count,
				     guint      *bytes_written);

static GIOError g_io_win32_msg_write(GIOChannel *channel, 
				     gchar      *buf, 
				     guint       count,
				     guint      *bytes_written);
static GIOError g_io_win32_msg_seek (GIOChannel *channel,
				     gint        offset, 
				     GSeekType   type);
static void g_io_win32_msg_close    (GIOChannel *channel);
static guint g_io_win32_msg_add_watch (GIOChannel      *channel,
				       gint             priority,
				       GIOCondition     condition,
				       GIOFunc          func,
				       gpointer         user_data,
				       GDestroyNotify   notify);

static GIOError g_io_win32_fd_read (GIOChannel *channel, 
				    gchar      *buf, 
				    guint       count,
				    guint      *bytes_written);
static GIOError g_io_win32_fd_write(GIOChannel *channel, 
				    gchar      *buf, 
				    guint       count,
				    guint      *bytes_written);
static GIOError g_io_win32_fd_seek (GIOChannel *channel,
				    gint        offset, 
				    GSeekType   type);
static void g_io_win32_fd_close (GIOChannel *channel);

static void g_io_win32_free (GIOChannel *channel);

static guint g_io_win32_fd_add_watch (GIOChannel      *channel,
				      gint             priority,
				      GIOCondition     condition,
				      GIOFunc          func,
				      gpointer         user_data,
				      GDestroyNotify   notify);

static GIOError g_io_win32_no_seek (GIOChannel *channel,
				    gint        offset, 
				    GSeekType   type);

static GIOError g_io_win32_pipe_read (GIOChannel *channel, 
				      gchar      *buf, 
				      guint       count,
				      guint      *bytes_written);
static GIOError g_io_win32_pipe_write (GIOChannel *channel, 
				       gchar      *buf, 
				       guint       count,
				       guint      *bytes_written);
static void g_io_win32_pipe_close    (GIOChannel *channel);
static guint g_io_win32_pipe_add_watch (GIOChannel      *channel,
					gint             priority,
					GIOCondition     condition,
					GIOFunc          func,
					gpointer         user_data,
					GDestroyNotify   notify);
static void g_io_win32_pipe_free (GIOChannel *channel);

static GIOError g_io_win32_sock_read (GIOChannel *channel, 
				      gchar      *buf, 
				      guint       count,
				      guint      *bytes_written);
static GIOError g_io_win32_sock_write(GIOChannel *channel, 
				      gchar      *buf, 
				      guint       count,
				      guint      *bytes_written);
static void g_io_win32_sock_close    (GIOChannel *channel);
static guint g_io_win32_sock_add_watch (GIOChannel      *channel,
					gint             priority,
					GIOCondition     condition,
					GIOFunc          func,
					gpointer         user_data,
					GDestroyNotify   notify);

GSourceFuncs win32_watch_msg_funcs = {
  g_io_win32_msg_prepare,
  g_io_win32_msg_check,
  g_io_win32_msg_dispatch,
  g_io_win32_destroy
};

GSourceFuncs win32_watch_fd_funcs = {
  g_io_win32_fd_prepare,
  g_io_win32_fd_check,
  g_io_win32_fd_dispatch,
  g_io_win32_destroy
};

GSourceFuncs win32_watch_pipe_funcs = {
  g_io_win32_pipe_prepare,
  g_io_win32_pipe_check,
  g_io_win32_pipe_dispatch,
  g_io_win32_pipe_destroy
};

GSourceFuncs win32_watch_sock_funcs = {
  g_io_win32_sock_prepare,
  g_io_win32_sock_check,
  g_io_win32_sock_dispatch,
  g_io_win32_destroy
};

GIOFuncs win32_channel_msg_funcs = {
  g_io_win32_msg_read,
  g_io_win32_msg_write,
  g_io_win32_no_seek,
  g_io_win32_msg_close,
  g_io_win32_msg_add_watch,
  g_io_win32_free
};

GIOFuncs win32_channel_fd_funcs = {
  g_io_win32_fd_read,
  g_io_win32_fd_write,
  g_io_win32_fd_seek,
  g_io_win32_fd_close,
  g_io_win32_fd_add_watch,
  g_io_win32_free
};

GIOFuncs win32_channel_pipe_funcs = {
  g_io_win32_pipe_read,
  g_io_win32_pipe_write,
  g_io_win32_no_seek,
  g_io_win32_pipe_close,
  g_io_win32_pipe_add_watch,
  g_io_win32_pipe_free
};

GIOFuncs win32_channel_sock_funcs = {
  g_io_win32_sock_read,
  g_io_win32_sock_write,
  g_io_win32_no_seek,
  g_io_win32_sock_close,
  g_io_win32_sock_add_watch,
  g_io_win32_free
};

#define N_WATCHED_PIPES 4

static struct {
  gint fd;
  GIOWin32Watch *watch;
  GIOWin32Channel *channel;
  gpointer user_data;
} watched_pipes[N_WATCHED_PIPES];

static gint n_watched_pipes = 0;

static gboolean
g_io_win32_msg_prepare  (gpointer source_data, 
			 GTimeVal *current_time,
			 gint    *timeout)
{
  GIOWin32Watch *data = source_data;
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) data->channel;
  MSG msg;

  *timeout = -1;

  return PeekMessage (&msg, win32_channel->hwnd, 0, 0, PM_NOREMOVE) == TRUE;
}

static gboolean 
g_io_win32_msg_check    (gpointer source_data,
			 GTimeVal *current_time)
{
  GIOWin32Watch *data = source_data;
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) data->channel;
  MSG msg;

  return PeekMessage (&msg, win32_channel->hwnd, 0, 0, PM_NOREMOVE) == TRUE;
}

static gboolean
g_io_win32_msg_dispatch (gpointer source_data, 
			 GTimeVal *current_time,
			 gpointer user_data)

{
  GIOWin32Watch *data = source_data;

  return (*data->callback)(data->channel,
			   data->pollfd.revents & data->condition,
			   user_data);
}

static void
g_io_win32_destroy (gpointer source_data)
{
  GIOWin32Watch *data = source_data;

  g_main_remove_poll (&data->pollfd);
  g_io_channel_unref (data->channel);
  g_free (data);
}

static gboolean
g_io_win32_fd_prepare  (gpointer source_data, 
			GTimeVal *current_time,
			gint    *timeout)
{
  *timeout = -1;

  return FALSE;
}

static gboolean 
g_io_win32_fd_check    (gpointer source_data,
			GTimeVal *current_time)
{
  GIOWin32Watch *data = source_data;

  return (data->pollfd.revents & data->condition);
}

static gboolean
g_io_win32_fd_dispatch (gpointer source_data, 
			GTimeVal *current_time,
			gpointer user_data)

{
  GIOWin32Watch *data = source_data;

  return (*data->callback)(data->channel,
			   data->pollfd.revents & data->condition,
			   user_data);
}

static GIOError
g_io_win32_msg_read (GIOChannel *channel, 
		     gchar     *buf, 
		     guint      count,
		     guint     *bytes_read)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  MSG msg;			/* In case of alignment problems */

  if (count < sizeof (MSG))
    return G_IO_ERROR_INVAL;
  
  if (!PeekMessage (&msg, win32_channel->hwnd, 0, 0, PM_REMOVE))
    return G_IO_ERROR_AGAIN;

  memmove (buf, &msg, sizeof (MSG));
  *bytes_read = sizeof (MSG);
  return G_IO_ERROR_NONE;
}
		       
static GIOError 
g_io_win32_msg_write(GIOChannel *channel, 
		     gchar     *buf, 
		     guint      count,
		     guint     *bytes_written)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  MSG msg;
  gint result;

  if (count != sizeof (MSG))
    return G_IO_ERROR_INVAL;

  /* In case of alignment problems */
  memmove (&msg, buf, sizeof (MSG));
  if (!PostMessage (win32_channel->hwnd, msg.message, msg.wParam, msg.lParam))
    return G_IO_ERROR_UNKNOWN;

  *bytes_written = sizeof (MSG);
  return G_IO_ERROR_NONE; 
}

static GIOError 
g_io_win32_no_seek (GIOChannel *channel,
		    gint      offset, 
		    GSeekType type)
{
  g_warning ("g_io_win32_no_seek: unseekable IO channel type");
  return G_IO_ERROR_UNKNOWN;
}


static void 
g_io_win32_msg_close (GIOChannel *channel)
{
  /* Nothing to be done. Or should we set hwnd to some invalid value? */
}

static void 
g_io_win32_free (GIOChannel *channel)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;

  g_free (win32_channel);
}

static guint 
g_io_win32_msg_add_watch (GIOChannel    *channel,
			  gint           priority,
			  GIOCondition   condition,
			  GIOFunc        func,
			  gpointer       user_data,
			  GDestroyNotify notify)
{
  GIOWin32Watch *watch = g_new (GIOWin32Watch, 1);
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  
  watch->channel = channel;
  g_io_channel_ref (channel);

  watch->callback = func;
  watch->condition = condition;

  watch->pollfd.fd = G_WIN32_MSG_HANDLE;
  watch->pollfd.events = condition;

  g_main_add_poll (&watch->pollfd, priority);

  return g_source_add (priority, TRUE, &win32_watch_msg_funcs,
		       watch, user_data, notify);
}

static gboolean
g_io_win32_pipe_prepare  (gpointer source_data, 
			  GTimeVal *current_time,
			  gint    *timeout)
{
  *timeout = -1;

  return FALSE;
}

static gboolean 
g_io_win32_pipe_check    (gpointer source_data,
			  GTimeVal *current_time)
{
  GIOWin32Watch *data = source_data;
  return FALSE;
}

static gboolean
g_io_win32_pipe_dispatch (gpointer source_data, 
			  GTimeVal *current_time,
			  gpointer user_data)

{
  GIOWin32Watch *data = source_data;

  return (*data->callback)(data->channel,
			   data->pollfd.revents & data->condition,
			   user_data);
}

static void
g_io_win32_pipe_destroy (gpointer source_data)
{
  GIOWin32Watch *data = source_data;

  g_io_channel_unref (data->channel);
  g_free (data);
}

static gboolean
g_io_win32_sock_prepare  (gpointer source_data, 
			GTimeVal *current_time,
			gint    *timeout)
{
  *timeout = -1;

  return FALSE;
}

static gboolean 
g_io_win32_sock_check    (gpointer source_data,
			GTimeVal *current_time)
{
  GIOWin32Watch *data = source_data;

  return (data->pollfd.revents & data->condition);
}

static gboolean
g_io_win32_sock_dispatch (gpointer source_data, 
			GTimeVal *current_time,
			gpointer user_data)

{
  GIOWin32Watch *data = source_data;

  return (*data->callback)(data->channel,
			   data->pollfd.revents & data->condition,
			   user_data);
}

static GIOError
g_io_win32_fd_read (GIOChannel *channel, 
		    gchar     *buf, 
		    guint      count,
		    guint     *bytes_read)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  gint result;

  result = read (win32_channel->fd, buf, count);
  if (result < 0)
    {
      *bytes_read = 0;
      switch (errno)
	{
	case EINVAL:
	  return G_IO_ERROR_INVAL;
	case EAGAIN:
	  return G_IO_ERROR_AGAIN;
	default:
	  return G_IO_ERROR_UNKNOWN;
	}
    }
  else
    {
      *bytes_read = result;
      return G_IO_ERROR_NONE;
    }
}
		       
static GIOError 
g_io_win32_fd_write(GIOChannel *channel, 
		    gchar     *buf, 
		    guint      count,
		    guint     *bytes_written)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  gint result;

  result = write (win32_channel->fd, buf, count);
      
  if (result < 0)
    {
      *bytes_written = 0;
      switch (errno)
	{
	case EINVAL:
	  return G_IO_ERROR_INVAL;
	case EAGAIN:
	  return G_IO_ERROR_AGAIN;
	default:
	  return G_IO_ERROR_UNKNOWN;
	}
    }
  else
    {
      *bytes_written = result;
      return G_IO_ERROR_NONE;
    }
}

static GIOError 
g_io_win32_fd_seek (GIOChannel *channel,
		    gint      offset, 
		    GSeekType type)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  int whence;
  off_t result;

  switch (type)
    {
    case G_SEEK_SET:
      whence = SEEK_SET;
      break;
    case G_SEEK_CUR:
      whence = SEEK_CUR;
      break;
    case G_SEEK_END:
      whence = SEEK_END;
      break;
    default:
      g_warning ("g_io_win32_fd_seek: unknown seek type");
      return G_IO_ERROR_UNKNOWN;
    }
  
  result = lseek (win32_channel->fd, offset, whence);
  
  if (result < 0)
    {
      switch (errno)
	{
	case EINVAL:
	  return G_IO_ERROR_INVAL;
	default:
	  return G_IO_ERROR_UNKNOWN;
	}
    }
  else
    return G_IO_ERROR_NONE;
}

static void 
g_io_win32_fd_close (GIOChannel *channel)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;

  close (win32_channel->fd);
  return;
}

static guint 
g_io_win32_fd_add_watch (GIOChannel    *channel,
			 gint           priority,
			 GIOCondition   condition,
			 GIOFunc        func,
			 gpointer       user_data,
			 GDestroyNotify notify)
{
  GIOWin32Watch *watch = g_new (GIOWin32Watch, 1);
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  
  watch->channel = channel;
  g_io_channel_ref (channel);

  watch->callback = func;
  watch->condition = condition;

  /* This probably does not work, except for CONIN$. */
  watch->pollfd.fd = _get_osfhandle (win32_channel->fd);
  watch->pollfd.events = condition;

  g_main_add_poll (&watch->pollfd, priority);

  return g_source_add (priority, TRUE, &win32_watch_fd_funcs,
		       watch, user_data, notify);
}

static GIOError
g_io_win32_pipe_read (GIOChannel *channel, 
		      gchar     *buf, 
		      guint      count,
		      guint     *bytes_read)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  HANDLE handle;
  DWORD avail;
  gint result;

  handle = (HANDLE) _get_osfhandle (win32_channel->fd);
  if (!PeekNamedPipe (handle, NULL, 0, NULL, &avail, NULL))
    {
      return G_IO_ERROR_UNKNOWN;
    }

  count = MIN (count, avail);

  count = MAX (count, 1);	/* Must read at least one byte, or
				 * caller will think it's EOF.
				 */
  /* g_print ("g_io_win32_pipe_read: %d %d\n", win32_channel->fd, count); */
  if (count == 0)
    result = 0;
  else
    result = read (win32_channel->fd, buf, count);
  if (result < 0)
    {
      *bytes_read = 0;
      switch (errno)
	{
	case EINVAL:
	  return G_IO_ERROR_INVAL;
	case EAGAIN:
	  return G_IO_ERROR_AGAIN;
	default:
	  return G_IO_ERROR_UNKNOWN;
	}
    }
  else
    {
      *bytes_read = result;
      win32_channel->offset += result;
      /* g_print ("=%d (%d)\n", result, win32_channel->offset); */
      return G_IO_ERROR_NONE;
    }
}
		       
static GIOError 
g_io_win32_pipe_write(GIOChannel *channel, 
		      gchar     *buf, 
		      guint      count,
		      guint     *bytes_written)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  LONG prevcnt;
  gint result;

  /* g_print ("g_io_win32_pipe_write: %d %d\n", win32_channel->fd, count); */
  result = write (win32_channel->fd, buf, count);
  if (result < 0)
    {
      *bytes_written = 0;
      switch (errno)
	{
	case EINVAL:
	  return G_IO_ERROR_INVAL;
	case EAGAIN:
	  return G_IO_ERROR_AGAIN;
	default:
	  return G_IO_ERROR_UNKNOWN;
	}
    }
  else
    {
      if (g_pipe_readable_msg == 0)
	g_pipe_readable_msg = RegisterWindowMessage ("g-pipe-readable");

      win32_channel->offset += result;
      /* g_print ("=%d (%d)\n", result, win32_channel->offset); */
      if (win32_channel->need_wakeups)
	{
	  PostThreadMessage (win32_channel->peer,
			     g_pipe_readable_msg,
			     win32_channel->peer_fd,
			     win32_channel->offset);
	}
      *bytes_written = result;
      return G_IO_ERROR_NONE;
    }
}

static void 
g_io_win32_pipe_close (GIOChannel *channel)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;

  /* g_print ("g_io_win32_pipe_close: %#x %d\n", channel, win32_channel->fd); */

  close (win32_channel->fd);
  return;
}

static guint 
g_io_win32_pipe_add_watch (GIOChannel    *channel,
			   gint           priority,
			   GIOCondition   condition,
			   GIOFunc        func,
			   gpointer       user_data,
			   GDestroyNotify notify)
{
  GIOWin32Watch *watch = g_new (GIOWin32Watch, 1);
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  gint i;
  
  /* g_print ("g_io_win32_pipe_add_watch: %d\n", win32_channel->fd); */

  watch->channel = channel;
  g_io_channel_ref (channel);

  watch->callback = func;
  watch->condition = condition;

  watch->pollfd.fd = win32_channel->fd;
  watch->pollfd.events = condition;

  for (i = 0; i < n_watched_pipes; i++)
    if (watched_pipes[i].fd == -1)
      break;
  if (i == N_WATCHED_PIPES)
    g_error ("Too many watched pipes");
  else
    {
      watched_pipes[i].fd = win32_channel->fd;
      watched_pipes[i].watch = watch;
      watched_pipes[i].channel = win32_channel;
      watched_pipes[i].user_data = user_data;
      n_watched_pipes = MAX (i + 1, n_watched_pipes);
    }
  return g_source_add (priority, FALSE, &win32_watch_pipe_funcs, watch, user_data, notify);
}

static void
g_io_win32_pipe_free (GIOChannel *channel)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  gint i;

  /* g_print ("g_io_win32_pipe_free: %#x %#x\n", channel, channel->channel_data); */

  for (i = 0; i < n_watched_pipes; i++)
    if (watched_pipes[i].fd == win32_channel->fd)
      {
	watched_pipes[i].fd = -1;
	break;
      }
  g_io_win32_free (channel);
}

static GIOError 
g_io_win32_sock_read (GIOChannel *channel, 
		      gchar      *buf, 
		      guint       count,
		      guint      *bytes_read)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  gint result;

  result = recv (win32_channel->fd, buf, count, 0);
  if (result == SOCKET_ERROR)
    {
      *bytes_read = 0;
      switch (WSAGetLastError ())
	{
	case WSAEINVAL:
	  return G_IO_ERROR_INVAL;
	case WSAEWOULDBLOCK:
	case WSAEINTR:
	  return G_IO_ERROR_AGAIN;
	default:
	  return G_IO_ERROR_UNKNOWN;
	}
    }
  else
    {
      *bytes_read = result;
      return G_IO_ERROR_NONE;
    }
}
		       
static GIOError 
g_io_win32_sock_write(GIOChannel *channel, 
		      gchar      *buf, 
		      guint       count,
		      guint      *bytes_written)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  gint result;

  result = send (win32_channel->fd, buf, count, 0);
      
  if (result == SOCKET_ERROR)
    {
      *bytes_written = 0;
      switch (WSAGetLastError ())
	{
	case WSAEINVAL:
	  return G_IO_ERROR_INVAL;
	case WSAEWOULDBLOCK:
	case WSAEINTR:
	  return G_IO_ERROR_AGAIN;
	default:
	  return G_IO_ERROR_UNKNOWN;
	}
    }
  else
    {
      *bytes_written = result;
      return G_IO_ERROR_NONE;
    }
}

static void 
g_io_win32_sock_close (GIOChannel *channel)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;

  closesocket (win32_channel->fd);
  return;
}

static guint 
g_io_win32_sock_add_watch (GIOChannel    *channel,
			   gint           priority,
			   GIOCondition   condition,
			   GIOFunc        func,
			   gpointer       user_data,
			   GDestroyNotify notify)
{
  GIOWin32Watch *watch = g_new (GIOWin32Watch, 1);
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  
  watch->channel = channel;
  g_io_channel_ref (channel);

  watch->callback = func;
  watch->condition = condition;

  watch->pollfd.fd = win32_channel->fd;
  watch->pollfd.events = condition;

  g_main_add_poll (&watch->pollfd, priority);

  return g_source_add (priority, TRUE, &win32_watch_sock_funcs, watch, user_data, notify);
}

GIOChannel *
g_io_channel_win32_new_messages (guint hwnd)
{
  GIOWin32Channel *win32_channel = g_new (GIOWin32Channel, 1);
  GIOChannel *channel = (GIOChannel *) win32_channel;

  g_io_channel_init (channel);
  channel->funcs = &win32_channel_msg_funcs;
  win32_channel->fd = -1;
  win32_channel->type = G_IO_WINDOWS_MESSAGES;
  win32_channel->hwnd = (HWND) hwnd;

  return channel;
}

GIOChannel *
g_io_channel_unix_new (gint fd)
{
  GIOWin32Channel *win32_channel = g_new (GIOWin32Channel, 1);
  GIOChannel *channel = (GIOChannel *) win32_channel;

  g_io_channel_init (channel);
  channel->funcs = &win32_channel_fd_funcs;
  win32_channel->fd = fd;
  win32_channel->type = G_IO_FILE_DESC;

  return channel;
}

gint
g_io_channel_unix_get_fd (GIOChannel *channel)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;

  return win32_channel->fd;
}

GIOChannel *
g_io_channel_win32_new_pipe_with_wakeups (int   fd,
					  guint peer,
					  int   peer_fd)
{
  GIOWin32Channel *win32_channel = g_new (GIOWin32Channel, 1);
  GIOChannel *channel = (GIOChannel *) win32_channel;

  /* g_print ("g_io_channel_win32_new_pipe_with_wakeups %d %#x %d\n", fd, peer, peer_fd); */

  g_io_channel_init (channel);
  channel->funcs = &win32_channel_pipe_funcs;
  win32_channel->fd = fd;
  win32_channel->type = G_IO_PIPE;
  win32_channel->peer = peer;
  win32_channel->peer_fd = peer_fd;
  win32_channel->offset = 0;
  win32_channel->need_wakeups = TRUE;

  return channel;
}

GIOChannel *
g_io_channel_win32_new_pipe (int fd)
{
  GIOWin32Channel *win32_channel = g_new (GIOWin32Channel, 1);
  GIOChannel *channel = (GIOChannel *) win32_channel;

  g_io_channel_init (channel);
  channel->funcs = &win32_channel_pipe_funcs;
  win32_channel->fd = fd;
  win32_channel->type = G_IO_PIPE;
  win32_channel->offset = 0;
  win32_channel->need_wakeups = FALSE;

  return channel;
}

GIOChannel *
g_io_channel_win32_new_stream_socket (int socket)
{
  GIOWin32Channel *win32_channel = g_new (GIOWin32Channel, 1);
  GIOChannel *channel = (GIOChannel *) win32_channel;

  g_io_channel_init (channel);
  channel->funcs = &win32_channel_sock_funcs;
  win32_channel->fd = socket;
  win32_channel->type = G_IO_STREAM_SOCKET;

  return channel;
}

gint
g_io_channel_win32_get_fd (GIOChannel *channel)
{
  return g_io_channel_unix_get_fd (channel);
}

void
g_io_channel_win32_pipe_request_wakeups (GIOChannel *channel,
					 guint       peer,
					 int         peer_fd)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;

  win32_channel->peer = peer;
  win32_channel->peer_fd = peer_fd;
  win32_channel->need_wakeups = TRUE;
}

void
g_io_channel_win32_pipe_readable (gint  fd,
				  guint offset)
{
  gint i;

  for (i = 0; i < n_watched_pipes; i++)
    if (watched_pipes[i].fd == fd)
      {
	if (watched_pipes[i].channel->offset < offset)
	  (*watched_pipes[i].watch->callback) (watched_pipes[i].watch->channel,
					       G_IO_IN,
					       watched_pipes[i].user_data);
	break;
      }
}
