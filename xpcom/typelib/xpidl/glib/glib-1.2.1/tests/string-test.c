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
  GStringChunk *string_chunk;

  gchar *tmp_string = NULL, *tmp_string_2;
  gint i;
  GString *string1, *string2;

  string_chunk = g_string_chunk_new (1024);

  for (i = 0; i < 100000; i ++)
    {
      tmp_string = g_string_chunk_insert (string_chunk, "hi pete");

      if (strcmp ("hi pete", tmp_string) != 0)
	g_error ("string chunks are broken.\n");
    }

  tmp_string_2 = g_string_chunk_insert_const (string_chunk, tmp_string);

  g_assert (tmp_string_2 != tmp_string &&
	    strcmp(tmp_string_2, tmp_string) == 0);

  tmp_string = g_string_chunk_insert_const (string_chunk, tmp_string);

  g_assert (tmp_string_2 == tmp_string);

  g_string_chunk_free (string_chunk);

  string1 = g_string_new ("hi pete!");
  string2 = g_string_new ("");

  g_assert (string1 != NULL);
  g_assert (string2 != NULL);
  g_assert (strlen (string1->str) == string1->len);
  g_assert (strlen (string2->str) == string2->len);
  g_assert (string2->len == 0);
  g_assert (strcmp ("hi pete!", string1->str) == 0);
  g_assert (strcmp ("", string2->str) == 0);

  for (i = 0; i < 10000; i++)
    g_string_append_c (string1, 'a'+(i%26));

  g_assert((strlen("hi pete!") + 10000) == string1->len);
  g_assert((strlen("hi pete!") + 10000) == strlen(string1->str));

#if !(defined (_MSC_VER) || defined (__LCC__))
  /* MSVC and LCC use the same run-time C library, which doesn't like
     the %10000.10000f format... */
  g_string_sprintf (string2, "%s|%0100d|%s|%s|%0*d|%*.*f|%10000.10000f",
		    "this pete guy sure is a wuss, like he's the number ",
		    1,
		    " wuss.  everyone agrees.\n",
		    string1->str,
		    10, 666, 15, 15, 666.666666666, 666.666666666);
#else
  g_string_sprintf (string2, "%s|%0100d|%s|%s|%0*d|%*.*f|%100.100f",
		    "this pete guy sure is a wuss, like he's the number ",
		    1,
		    " wuss.  everyone agrees.\n",
		    string1->str,
		    10, 666, 15, 15, 666.666666666, 666.666666666);
#endif

  return 0;
}

