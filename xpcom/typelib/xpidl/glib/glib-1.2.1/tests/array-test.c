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

int array[10000];
gboolean failed = FALSE;

#define	TEST(m,cond)	G_STMT_START { failed = !(cond); \
if (failed) \
  { if (!m) \
      g_print ("\n(%s:%d) failed for: %s\n", __FILE__, __LINE__, ( # cond )); \
    else \
      g_print ("\n(%s:%d) failed for: %s: (%s)\n", __FILE__, __LINE__, ( # cond ), (gchar*)m); \
  } \
else \
  g_print ("."); fflush (stdout); \
} G_STMT_END

#define	C2P(c)		((gpointer) ((long) (c)))
#define	P2C(p)		((gchar) ((long) (p)))

#define GLIB_TEST_STRING "el dorado "
#define GLIB_TEST_STRING_5 "el do"

typedef struct {
	guint age;
	gchar name[40];
} GlibTestInfo;


int
main (int   argc,
      char *argv[])
{
  gint i;
  GArray *garray;
  GPtrArray *gparray;
  GByteArray *gbarray;

  /* array tests */
  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 10000; i++)
    g_array_append_val (garray, i);

  for (i = 0; i < 10000; i++)
    g_assert (g_array_index (garray, gint, i) == i);

  g_array_free (garray, TRUE);

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 100; i++)
    g_array_prepend_val (garray, i);

  for (i = 0; i < 100; i++)
    g_assert (g_array_index (garray, gint, i) == (100 - i - 1));

  g_array_free (garray, TRUE);

  /* pointer arrays */
  gparray = g_ptr_array_new ();
  for (i = 0; i < 10000; i++)
    g_ptr_array_add (gparray, GINT_TO_POINTER (i));

  for (i = 0; i < 10000; i++)
    if (g_ptr_array_index (gparray, i) != GINT_TO_POINTER (i))
      g_print ("array fails: %p ( %p )\n", g_ptr_array_index (gparray, i), GINT_TO_POINTER (i));

  g_ptr_array_free (gparray, TRUE);

  /* byte arrays */
  gbarray = g_byte_array_new ();
  for (i = 0; i < 10000; i++)
    g_byte_array_append (gbarray, (guint8*) "abcd", 4);

  for (i = 0; i < 10000; i++)
    {
      g_assert (gbarray->data[4*i] == 'a');
      g_assert (gbarray->data[4*i+1] == 'b');
      g_assert (gbarray->data[4*i+2] == 'c');
      g_assert (gbarray->data[4*i+3] == 'd');
    }

  g_byte_array_free (gbarray, TRUE);

  return 0;
}

