/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * giochannel.c: IO Channel abstraction
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

#include "config.h"

#include "glib.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

void
g_io_channel_init (GIOChannel *channel)
{
  channel->channel_flags = 0;
  channel->ref_count = 1;
}


void 
g_io_channel_ref (GIOChannel *channel)
{
  g_return_if_fail (channel != NULL);

  channel->ref_count++;
}

void 
g_io_channel_unref (GIOChannel *channel)
{
  g_return_if_fail (channel != NULL);

  channel->ref_count--;
  if (channel->ref_count == 0)
    channel->funcs->io_free (channel);
}

GIOError 
g_io_channel_read (GIOChannel *channel, 
		   gchar      *buf, 
		   guint       count,
		   guint      *bytes_read)
{
  g_return_val_if_fail (channel != NULL, G_IO_ERROR_UNKNOWN);

  return channel->funcs->io_read (channel, buf, count, bytes_read);
}

GIOError 
g_io_channel_write (GIOChannel *channel, 
		    gchar      *buf, 
		    guint       count,
		    guint      *bytes_written)
{
  g_return_val_if_fail (channel != NULL, G_IO_ERROR_UNKNOWN);

  return channel->funcs->io_write (channel, buf, count, bytes_written);
}

GIOError 
g_io_channel_seek  (GIOChannel   *channel,
		    gint        offset, 
		    GSeekType   type)
{
  g_return_val_if_fail (channel != NULL, G_IO_ERROR_UNKNOWN);

  return channel->funcs->io_seek (channel, offset, type);
}
     
void
g_io_channel_close (GIOChannel *channel)
{
  g_return_if_fail (channel != NULL);

  channel->funcs->io_close (channel);
}

guint 
g_io_add_watch_full (GIOChannel    *channel,
		     gint           priority,
		     GIOCondition   condition,
		     GIOFunc        func,
		     gpointer       user_data,
		     GDestroyNotify notify)
{
  g_return_val_if_fail (channel != NULL, 0);

  return channel->funcs->io_add_watch (channel, priority, condition,
				       func, user_data, notify);
}

guint 
g_io_add_watch (GIOChannel    *channel,
		GIOCondition   condition,
		GIOFunc        func,
		gpointer       user_data)
{
  return g_io_add_watch_full (channel, 0, condition, func, user_data, NULL);
}
