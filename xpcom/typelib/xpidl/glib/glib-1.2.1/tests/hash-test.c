/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1999 The Free Software Foundation
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#if STDC_HEADERS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#endif

#include <glib.h>



int array[10000];



static gboolean
my_hash_callback_remove (gpointer key,
			 gpointer value,
			 gpointer user_data)
{
  int *d = value;

  if ((*d) % 2)
    return TRUE;

  return FALSE;
}

static void
my_hash_callback_remove_test (gpointer key,
			      gpointer value,
			      gpointer user_data)
{
  int *d = value;

  if ((*d) % 2)
    g_assert_not_reached ();
}

static void
my_hash_callback (gpointer key,
		  gpointer value,
		  gpointer user_data)
{
  int *d = value;
  *d = 1;
}

static guint
my_hash (gconstpointer key)
{
  return (guint) *((const gint*) key);
}

static gint
my_hash_compare (gconstpointer a,
		 gconstpointer b)
{
  return *((const gint*) a) == *((const gint*) b);
}



/*
 * This is a simplified version of the pathalias hashing function.
 * Thanks to Steve Belovin and Peter Honeyman
 *
 * hash a string into a long int.  31 bit crc (from andrew appel).
 * the crc table is computed at run time by crcinit() -- we could
 * precompute, but it takes 1 clock tick on a 750.
 *
 * This fast table calculation works only if POLY is a prime polynomial
 * in the field of integers modulo 2.  Since the coefficients of a
 * 32-bit polynomial won't fit in a 32-bit word, the high-order bit is
 * implicit.  IT MUST ALSO BE THE CASE that the coefficients of orders
 * 31 down to 25 are zero.  Happily, we have candidates, from
 * E. J.  Watson, "Primitive Polynomials (Mod 2)", Math. Comp. 16 (1962):
 *	x^32 + x^7 + x^5 + x^3 + x^2 + x^1 + x^0
 *	x^31 + x^3 + x^0
 *
 * We reverse the bits to get:
 *	111101010000000000000000000000001 but drop the last 1
 *         f   5   0   0   0   0   0   0
 *	010010000000000000000000000000001 ditto, for 31-bit crc
 *	   4   8   0   0   0   0   0   0
 */

#define POLY 0x48000000L	/* 31-bit polynomial (avoids sign problems) */

static guint CrcTable[128];

/*
 - crcinit - initialize tables for hash function
 */
static void crcinit(void)
{
	int i, j;
	guint sum;

	for (i = 0; i < 128; ++i) {
		sum = 0L;
		for (j = 7 - 1; j >= 0; --j)
			if (i & (1 << j))
				sum ^= POLY >> j;
		CrcTable[i] = sum;
	}
}

/*
 - hash - Honeyman's nice hashing function
 */
static guint honeyman_hash(gconstpointer key)
{
	const gchar *name = (const gchar *) key;
	gint size;
	guint sum = 0;

	g_assert (name != NULL);
	g_assert (*name != 0);

	size = strlen(name);

	while (size--) {
		sum = (sum >> 7) ^ CrcTable[(sum ^ (*name++)) & 0x7f];
	}

	return(sum);
}


static gint second_hash_cmp (gconstpointer a, gconstpointer b)
{
  gint rc = (strcmp (a, b) == 0);

  return rc;
}



static guint one_hash(gconstpointer key)
{
  return 1;
}


static void not_even_foreach (gpointer       key,
				 gpointer       value,
				 gpointer	user_data)
{
  const char *_key = (const char *) key;
  const char *_value = (const char *) value;
  int i;
  char val [20];

  g_assert (_key != NULL);
  g_assert (*_key != 0);
  g_assert (_value != NULL);
  g_assert (*_value != 0);

  i = atoi (_key);
  g_assert (atoi (_key) > 0);

  sprintf (val, "%d value", i);
  g_assert (strcmp (_value, val) == 0);

  g_assert ((i % 2) != 0);
  g_assert (i != 3);
}


static gboolean remove_even_foreach (gpointer       key,
				 gpointer       value,
				 gpointer	user_data)
{
  const char *_key = (const char *) key;
  const char *_value = (const char *) value;
  int i;
  char val [20];

  g_assert (_key != NULL);
  g_assert (*_key != 0);
  g_assert (_value != NULL);
  g_assert (*_value != 0);

  i = atoi (_key);
  g_assert (i > 0);

  sprintf (val, "%d value", i);
  g_assert (strcmp (_value, val) == 0);

  return ((i % 2) == 0) ? TRUE : FALSE;
}




static void second_hash_test (gboolean simple_hash)
{
     int       i;
     char      key[20] = "", val[20]="", *v, *orig_key, *orig_val;
     GHashTable     *h;
     gboolean found;

     crcinit ();

     h = g_hash_table_new (simple_hash ? one_hash : honeyman_hash,
     			   second_hash_cmp);
     g_assert (h != NULL);
     for (i=0; i<20; i++)
          {
          sprintf (key, "%d", i);
	  g_assert (atoi (key) == i);

	  sprintf (val, "%d value", i);
	  g_assert (atoi (val) == i);

          g_hash_table_insert (h, g_strdup (key), g_strdup (val));
          }

     g_assert (g_hash_table_size (h) == 20);

     for (i=0; i<20; i++)
          {
          sprintf (key, "%d", i);
	  g_assert (atoi(key) == i);

          v = (char *) g_hash_table_lookup (h, key);

	  g_assert (v != NULL);
	  g_assert (*v != 0);
	  g_assert (atoi (v) == i);
          }

     /**** future test stuff, yet to be debugged 
     sprintf (key, "%d", 3);
     g_hash_table_remove (h, key);
     g_hash_table_foreach_remove (h, remove_even_foreach, NULL);
     g_hash_table_foreach (h, not_even_foreach, NULL);
     */

     for (i=0; i<20; i++)
          {
	  if (((i % 2) == 0) || (i == 3))
	    i++;

          sprintf (key, "%d", i);
	  g_assert (atoi(key) == i);

	  sprintf (val, "%d value", i);
	  g_assert (atoi (val) == i);

	  orig_key = orig_val = NULL;
          found = g_hash_table_lookup_extended (h, key,
	  					(gpointer)&orig_key,
						(gpointer)&orig_val);
	  g_assert (found);

	  g_assert (orig_key != NULL);
	  g_assert (strcmp (key, orig_key) == 0);
	  g_free (orig_key);

	  g_assert (orig_val != NULL);
	  g_assert (strcmp (val, orig_val) == 0);
	  g_free (orig_val);
          }

    g_hash_table_destroy (h);
}


static void direct_hash_test (void)
{
     gint       i, rc;
     GHashTable     *h;

     h = g_hash_table_new (NULL, NULL);
     g_assert (h != NULL);
     for (i=1; i<=20; i++)
          {
          g_hash_table_insert (h, GINT_TO_POINTER (i),
	  		       GINT_TO_POINTER (i + 42));
          }

     g_assert (g_hash_table_size (h) == 20);

     for (i=1; i<=20; i++)
          {
          rc = GPOINTER_TO_INT (
	  	g_hash_table_lookup (h, GINT_TO_POINTER (i)));

	  g_assert (rc != 0);
	  g_assert ((rc - 42) == i);
          }

    g_hash_table_destroy (h);
}



int
main (int   argc,
      char *argv[])
{
  GHashTable *hash_table;
  gint i;

  hash_table = g_hash_table_new (my_hash, my_hash_compare);
  for (i = 0; i < 10000; i++)
    {
      array[i] = i;
      g_hash_table_insert (hash_table, &array[i], &array[i]);
    }
  g_hash_table_foreach (hash_table, my_hash_callback, NULL);

  for (i = 0; i < 10000; i++)
    if (array[i] == 0)
      g_assert_not_reached();

  for (i = 0; i < 10000; i++)
    g_hash_table_remove (hash_table, &array[i]);

  for (i = 0; i < 10000; i++)
    {
      array[i] = i;
      g_hash_table_insert (hash_table, &array[i], &array[i]);
    }

  if (g_hash_table_foreach_remove (hash_table, my_hash_callback_remove, NULL) != 5000 ||
      g_hash_table_size (hash_table) != 5000)
    g_assert_not_reached();

  g_hash_table_foreach (hash_table, my_hash_callback_remove_test, NULL);


  g_hash_table_destroy (hash_table);

  second_hash_test (TRUE);
  second_hash_test (FALSE);
  direct_hash_test ();

  return 0;

}
