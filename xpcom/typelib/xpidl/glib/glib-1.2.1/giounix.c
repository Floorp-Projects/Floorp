/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * giounix.c: IO Channels using unix file descriptors
 * Copyright 1998 Owen Taylor
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
 * MT safe
 */

#include "glib.h"
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

/*
 * Unix IO Channels
 */

typedef struct _GIOUnixChannel GIOUnixChannel;
typedef struct _GIOUnixWatch GIOUnixWatch;

struct _GIOUnixChannel {
  GIOChannel channel;
  gint fd;
};

struct _GIOUnixWatch {
  GPollFD       pollfd;
  GIOChannel   *channel;
  GIOCondition  condition;
  GIOFunc       callback;
};


static GIOError g_io_unix_read (GIOChannel *channel, 
		       gchar     *buf, 
		       guint      count,
		       guint     *bytes_written);
		       
static GIOError g_io_unix_write(GIOChannel *channel, 
				gchar     *buf, 
				guint      count,
				guint     *bytes_written);
static GIOError g_io_unix_seek (GIOChannel *channel,
				gint      offset, 
				GSeekType type);
static void g_io_unix_close    (GIOChannel *channel);
static void g_io_unix_free     (GIOChannel *channel);
static guint  g_io_unix_add_watch (GIOChannel      *channel,
				   gint           priority,
				   GIOCondition   condition,
				   GIOFunc        func,
				   gpointer       user_data,
				   GDestroyNotify notify);
static gboolean g_io_unix_prepare  (gpointer source_data, 
				    GTimeVal *current_time,
				    gint *timeout,
				    gpointer user_data);
static gboolean g_io_unix_check    (gpointer source_data,
				    GTimeVal *current_time,
				    gpointer user_data);
static gboolean g_io_unix_dispatch (gpointer source_data,
				    GTimeVal *current_time,
				    gpointer user_data);
static void g_io_unix_destroy  (gpointer source_data);

GSourceFuncs unix_watch_funcs = {
  g_io_unix_prepare,
  g_io_unix_check,
  g_io_unix_dispatch,
  g_io_unix_destroy
};

GIOFuncs unix_channel_funcs = {
  g_io_unix_read,
  g_io_unix_write,
  g_io_unix_seek,
  g_io_unix_close,
  g_io_unix_add_watch,
  g_io_unix_free,
};

static gboolean 
g_io_unix_prepare  (gpointer source_data, 
		    GTimeVal *current_time,
		    gint     *timeout,
		    gpointer user_data)
{
  *timeout = -1;
  return FALSE;
}

static gboolean 
g_io_unix_check    (gpointer source_data,
		    GTimeVal *current_time,
		    gpointer user_data)
{
  GIOUnixWatch *data = source_data;

  return (data->pollfd.revents & data->condition);
}

static gboolean
g_io_unix_dispatch (gpointer source_data, 
		    GTimeVal *current_time,
		    gpointer user_data)

{
  GIOUnixWatch *data = source_data;

  return (*data->callback)(data->channel,
			   data->pollfd.revents & data->condition,
			   user_data);
}

static void 
g_io_unix_destroy (gpointer source_data)
{
  GIOUnixWatch *data = source_data;

  g_main_remove_poll (&data->pollfd);
  g_io_channel_unref (data->channel);
  g_free (data);
}

static GIOError 
g_io_unix_read (GIOChannel *channel, 
		gchar     *buf, 
		guint      count,
		guint     *bytes_read)
{
  GIOUnixChannel *unix_channel = (GIOUnixChannel *)channel;
  gint result;

  result = read (unix_channel->fd, buf, count);

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
g_io_unix_write(GIOChannel *channel, 
		gchar     *buf, 
		guint      count,
		guint     *bytes_written)
{
  GIOUnixChannel *unix_channel = (GIOUnixChannel *)channel;
  gint result;

  result = write (unix_channel->fd, buf, count);

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
g_io_unix_seek (GIOChannel *channel,
		gint      offset, 
		GSeekType type)
{
  GIOUnixChannel *unix_channel = (GIOUnixChannel *)channel;
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
      g_warning ("g_io_unix_seek: unknown seek type");
      return G_IO_ERROR_UNKNOWN;
    }

  result = lseek (unix_channel->fd, offset, whence);

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
g_io_unix_close (GIOChannel *channel)
{
  GIOUnixChannel *unix_channel = (GIOUnixChannel *)channel;

  close (unix_channel->fd);
}

static void 
g_io_unix_free (GIOChannel *channel)
{
  GIOUnixChannel *unix_channel = (GIOUnixChannel *)channel;

  g_free (unix_channel);
}

static guint 
g_io_unix_add_watch (GIOChannel    *channel,
		     gint           priority,
		     GIOCondition   condition,
		     GIOFunc        func,
		     gpointer       user_data,
		     GDestroyNotify notify)
{
  GIOUnixWatch *watch = g_new (GIOUnixWatch, 1);
  GIOUnixChannel *unix_channel = (GIOUnixChannel *)channel;
  
  watch->channel = channel;
  g_io_channel_ref (channel);

  watch->callback = func;
  watch->condition = condition;

  watch->pollfd.fd = unix_channel->fd;
  watch->pollfd.events = condition;

  g_main_add_poll (&watch->pollfd, priority);

  return g_source_add (priority, TRUE, &unix_watch_funcs, watch, user_data, notify);
}

GIOChannel *
g_io_channel_unix_new (gint fd)
{
  GIOUnixChannel *unix_channel = g_new (GIOUnixChannel, 1);
  GIOChannel *channel = (GIOChannel *)unix_channel;

  g_io_channel_init (channel);
  channel->funcs = &unix_channel_funcs;

  unix_channel->fd = fd;
  return channel;
}

gint
g_io_channel_unix_get_fd (GIOChannel *channel)
{
  GIOUnixChannel *unix_channel = (GIOUnixChannel *)channel;
  return unix_channel->fd;
}
