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

#undef G_LOG_DOMAIN

#include <stdio.h>
#include <string.h>
#include "glib.h"



int
main (int   argc,
      char *argv[])
{
  guint16 gu16t1 = 0x44afU, gu16t2 = 0xaf44U;
  guint32 gu32t1 = 0x02a7f109U, gu32t2 = 0x09f1a702U;
#ifdef G_HAVE_GINT64
  guint64 gu64t1 = G_GINT64_CONSTANT(0x1d636b02300a7aa7U),
	  gu64t2 = G_GINT64_CONSTANT(0xa77a0a30026b631dU);
#endif

  /* type sizes */
  g_assert (sizeof (gint8) == 1);
  g_assert (sizeof (gint16) == 2);
  g_assert (sizeof (gint32) == 4);
#ifdef	G_HAVE_GINT64
  g_assert (sizeof (gint64) == 8);
#endif	/* G_HAVE_GINT64 */

  g_assert (GUINT16_SWAP_LE_BE (gu16t1) == gu16t2);
  g_assert (GUINT32_SWAP_LE_BE (gu32t1) == gu32t2);
#ifdef G_HAVE_GINT64
  g_assert (GUINT64_SWAP_LE_BE (gu64t1) == gu64t2);
#endif

  return 0;
}
