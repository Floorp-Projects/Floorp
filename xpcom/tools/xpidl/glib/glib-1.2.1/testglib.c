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

static gboolean
node_build_string (GNode    *node,
		   gpointer  data)
{
  gchar **p = data;
  gchar *string;
  gchar c[2] = "_";

  c[0] = P2C (node->data);

  string = g_strconcat (*p ? *p : "", c, NULL);
  g_free (*p);
  *p = string;

  return FALSE;
}

static void
g_node_test (void)
{
  GNode *root;
  GNode *node;
  GNode *node_B;
  GNode *node_F;
  GNode *node_G;
  GNode *node_J;
  guint i;
  gchar *tstring;

  g_print ("checking n-way trees: ");
  failed = FALSE;

  root = g_node_new (C2P ('A'));
  TEST (NULL, g_node_depth (root) == 1 && g_node_max_height (root) == 1);

  node_B = g_node_new (C2P ('B'));
  g_node_append (root, node_B);
  TEST (NULL, root->children == node_B);

  g_node_append_data (node_B, C2P ('E'));
  g_node_prepend_data (node_B, C2P ('C'));
  g_node_insert (node_B, 1, g_node_new (C2P ('D')));

  node_F = g_node_new (C2P ('F'));
  g_node_append (root, node_F);
  TEST (NULL, root->children->next == node_F);

  node_G = g_node_new (C2P ('G'));
  g_node_append (node_F, node_G);
  node_J = g_node_new (C2P ('J'));
  g_node_prepend (node_G, node_J);
  g_node_insert (node_G, 42, g_node_new (C2P ('K')));
  g_node_insert_data (node_G, 0, C2P ('H'));
  g_node_insert (node_G, 1, g_node_new (C2P ('I')));

  TEST (NULL, g_node_depth (root) == 1);
  TEST (NULL, g_node_max_height (root) == 4);
  TEST (NULL, g_node_depth (node_G->children->next) == 4);
  TEST (NULL, g_node_n_nodes (root, G_TRAVERSE_LEAFS) == 7);
  TEST (NULL, g_node_n_nodes (root, G_TRAVERSE_NON_LEAFS) == 4);
  TEST (NULL, g_node_n_nodes (root, G_TRAVERSE_ALL) == 11);
  TEST (NULL, g_node_max_height (node_F) == 3);
  TEST (NULL, g_node_n_children (node_G) == 4);
  TEST (NULL, g_node_find_child (root, G_TRAVERSE_ALL, C2P ('F')) == node_F);
  TEST (NULL, g_node_find (root, G_LEVEL_ORDER, G_TRAVERSE_NON_LEAFS, C2P ('I')) == NULL);
  TEST (NULL, g_node_find (root, G_IN_ORDER, G_TRAVERSE_LEAFS, C2P ('J')) == node_J);

  for (i = 0; i < g_node_n_children (node_B); i++)
    {
      node = g_node_nth_child (node_B, i);
      TEST (NULL, P2C (node->data) == ('C' + i));
    }
  
  for (i = 0; i < g_node_n_children (node_G); i++)
    TEST (NULL, g_node_child_position (node_G, g_node_nth_child (node_G, i)) == i);

  /* we have built:                    A
   *                                 /   \
   *                               B       F
   *                             / | \       \
   *                           C   D   E       G
   *                                         / /\ \
   *                                       H  I  J  K
   *
   * for in-order traversal, 'G' is considered to be the "left"
   * child of 'F', which will cause 'F' to be the last node visited.
   */

  tstring = NULL;
  g_node_traverse (root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  TEST (tstring, strcmp (tstring, "ABCDEFGHIJK") == 0);
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_POST_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  TEST (tstring, strcmp (tstring, "CDEBHIJKGFA") == 0);
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_IN_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  TEST (tstring, strcmp (tstring, "CBDEAHGIJKF") == 0);
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  TEST (tstring, strcmp (tstring, "ABFCDEGHIJK") == 0);
  g_free (tstring); tstring = NULL;
  
  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_LEAFS, -1, node_build_string, &tstring);
  TEST (tstring, strcmp (tstring, "CDEHIJK") == 0);
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_PRE_ORDER, G_TRAVERSE_NON_LEAFS, -1, node_build_string, &tstring);
  TEST (tstring, strcmp (tstring, "ABFG") == 0);
  g_free (tstring); tstring = NULL;

  g_node_reverse_children (node_B);
  g_node_reverse_children (node_G);

  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  TEST (tstring, strcmp (tstring, "ABFEDCGKJIH") == 0);
  g_free (tstring); tstring = NULL;
  
  g_node_destroy (root);

  /* allocation tests */

  root = g_node_new (NULL);
  node = root;

  for (i = 0; i < 2048; i++)
    {
      g_node_append (node, g_node_new (NULL));
      if ((i%5) == 4)
	node = node->children->next;
    }
  TEST (NULL, g_node_max_height (root) > 100);
  TEST (NULL, g_node_n_nodes (root, G_TRAVERSE_ALL) == 1 + 2048);

  g_node_destroy (root);
  
  if (!failed)
    g_print ("ok\n");
}

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
    g_print ("bad!\n");
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

static gint
my_list_compare_one (gconstpointer a, gconstpointer b)
{
  gint one = *((const gint*)a);
  gint two = *((const gint*)b);
  return one-two;
}

static gint
my_list_compare_two (gconstpointer a, gconstpointer b)
{
  gint one = *((const gint*)a);
  gint two = *((const gint*)b);
  return two-one;
}

/* static void
my_list_print (gpointer a, gpointer b)
{
  gint three = *((gint*)a);
  g_print("%d", three);
}; */

static gint
my_compare (gconstpointer a,
	    gconstpointer b)
{
  const char *cha = a;
  const char *chb = b;

  return *cha - *chb;
}

static gint
my_traverse (gpointer key,
	     gpointer value,
	     gpointer data)
{
  char *ch = key;
  g_print ("%c ", *ch);
  return FALSE;
}

int
main (int   argc,
      char *argv[])
{
  GList *list, *t;
  GSList *slist, *st;
  GHashTable *hash_table;
  GMemChunk *mem_chunk;
  GStringChunk *string_chunk;
  GTimer *timer;
  gint nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint morenums[10] = { 8, 9, 7, 0, 3, 2, 5, 1, 4, 6};
  gchar *string;

  gchar *mem[10000], *tmp_string = NULL, *tmp_string_2;
  gint i, j;
  GArray *garray;
  GPtrArray *gparray;
  GByteArray *gbarray;
  GString *string1, *string2;
  GTree *tree;
  char chars[62];
  GRelation *relation;
  GTuples *tuples;
  gint data [1024];
  struct {
    gchar *filename;
    gchar *dirname;
  } dirname_checks[] = {
#ifndef NATIVE_WIN32
    { "/", "/" },
    { "////", "/" },
    { ".////", "." },
    { ".", "." },
    { "..", "." },
    { "../", ".." },
    { "..////", ".." },
    { "", "." },
    { "a/b", "a" },
    { "a/b/", "a/b" },
    { "c///", "c" },
#else
    { "\\", "\\" },
    { ".\\\\\\\\", "." },
    { ".", "." },
    { "..", "." },
    { "..\\", ".." },
    { "..\\\\\\\\", ".." },
    { "", "." },
    { "a\\b", "a" },
    { "a\\b\\", "a\\b" },
    { "c\\\\\\", "c" },
#endif
  };
  guint n_dirname_checks = sizeof (dirname_checks) / sizeof (dirname_checks[0]);
  guint16 gu16t1 = 0x44afU, gu16t2 = 0xaf44U;
  guint32 gu32t1 = 0x02a7f109U, gu32t2 = 0x09f1a702U;
#ifdef G_HAVE_GINT64
  guint64 gu64t1 = G_GINT64_CONSTANT(0x1d636b02300a7aa7U),
	  gu64t2 = G_GINT64_CONSTANT(0xa77a0a30026b631dU);
#endif

  g_print ("TestGLib v%u.%u.%u (i:%u b:%u)\n",
	   glib_major_version,
	   glib_minor_version,
	   glib_micro_version,
	   glib_interface_age,
	   glib_binary_age);

  string = g_get_current_dir ();
  g_print ("cwd: %s\n", string);
  g_free (string);
  g_print ("user: %s\n", g_get_user_name ());
  g_print ("real: %s\n", g_get_real_name ());
  g_print ("home: %s\n", g_get_home_dir ());
  g_print ("tmp-dir: %s\n", g_get_tmp_dir ());

  /* type sizes */
  g_print ("checking size of gint8: %d", (int)sizeof (gint8));
  TEST (NULL, sizeof (gint8) == 1);
  g_print ("\nchecking size of gint16: %d", (int)sizeof (gint16));
  TEST (NULL, sizeof (gint16) == 2);
  g_print ("\nchecking size of gint32: %d", (int)sizeof (gint32));
  TEST (NULL, sizeof (gint32) == 4);
#ifdef	G_HAVE_GINT64
  g_print ("\nchecking size of gint64: %d", (int)sizeof (gint64));
  TEST (NULL, sizeof (gint64) == 8);
#endif	/* G_HAVE_GINT64 */
  g_print ("\n");

  g_print ("checking g_dirname()...");
  for (i = 0; i < n_dirname_checks; i++)
    {
      gchar *dirname;

      dirname = g_dirname (dirname_checks[i].filename);
      if (strcmp (dirname, dirname_checks[i].dirname) != 0)
	{
	  g_print ("\nfailed for \"%s\"==\"%s\" (returned: \"%s\")\n",
		   dirname_checks[i].filename,
		   dirname_checks[i].dirname,
		   dirname);
	  n_dirname_checks = 0;
	}
      g_free (dirname);
    }
  if (n_dirname_checks)
    g_print ("ok\n");

  g_print ("checking doubly linked lists...");

  list = NULL;
  for (i = 0; i < 10; i++)
    list = g_list_append (list, &nums[i]);
  list = g_list_reverse (list);

  for (i = 0; i < 10; i++)
    {
      t = g_list_nth (list, i);
      if (*((gint*) t->data) != (9 - i))
	g_error ("Regular insert failed");
    }

  for (i = 0; i < 10; i++)
    if(g_list_position(list, g_list_nth (list, i)) != i)
      g_error("g_list_position does not seem to be the inverse of g_list_nth\n");

  g_list_free (list);
  list = NULL;

  for (i = 0; i < 10; i++)
    list = g_list_insert_sorted (list, &morenums[i], my_list_compare_one);

  /*
  g_print("\n");
  g_list_foreach (list, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      t = g_list_nth (list, i);
      if (*((gint*) t->data) != i)
         g_error ("Sorted insert failed");
    }

  g_list_free (list);
  list = NULL;

  for (i = 0; i < 10; i++)
    list = g_list_insert_sorted (list, &morenums[i], my_list_compare_two);

  /*
  g_print("\n");
  g_list_foreach (list, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      t = g_list_nth (list, i);
      if (*((gint*) t->data) != (9 - i))
         g_error ("Sorted insert failed");
    }

  g_list_free (list);
  list = NULL;

  for (i = 0; i < 10; i++)
    list = g_list_prepend (list, &morenums[i]);

  list = g_list_sort (list, my_list_compare_two);

  /*
  g_print("\n");
  g_list_foreach (list, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      t = g_list_nth (list, i);
      if (*((gint*) t->data) != (9 - i))
         g_error ("Merge sort failed");
    }

  g_list_free (list);

  g_print ("ok\n");


  g_print ("checking singly linked lists...");

  slist = NULL;
  for (i = 0; i < 10; i++)
    slist = g_slist_append (slist, &nums[i]);
  slist = g_slist_reverse (slist);

  for (i = 0; i < 10; i++)
    {
      st = g_slist_nth (slist, i);
      if (*((gint*) st->data) != (9 - i))
	g_error ("failed");
    }

  g_slist_free (slist);
  slist = NULL;

  for (i = 0; i < 10; i++)
    slist = g_slist_insert_sorted (slist, &morenums[i], my_list_compare_one);

  /*
  g_print("\n");
  g_slist_foreach (slist, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      st = g_slist_nth (slist, i);
      if (*((gint*) st->data) != i)
         g_error ("Sorted insert failed");
    }

  g_slist_free(slist);
  slist = NULL;

  for (i = 0; i < 10; i++)
    slist = g_slist_insert_sorted (slist, &morenums[i], my_list_compare_two);

  /*
  g_print("\n");
  g_slist_foreach (slist, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      st = g_slist_nth (slist, i);
      if (*((gint*) st->data) != (9 - i))
         g_error("Sorted insert failed");
    }

  g_slist_free(slist);
  slist = NULL;

  for (i = 0; i < 10; i++)
    slist = g_slist_prepend (slist, &morenums[i]);

  slist = g_slist_sort (slist, my_list_compare_two);

  /*
  g_print("\n");
  g_slist_foreach (slist, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      st = g_slist_nth (slist, i);
      if (*((gint*) st->data) != (9 - i))
         g_error("Sorted insert failed");
    }

  g_slist_free(slist);

  g_print ("ok\n");


  g_print ("checking binary trees...\n");

  tree = g_tree_new (my_compare);
  i = 0;
  for (j = 0; j < 10; j++, i++)
    {
      chars[i] = '0' + j;
      g_tree_insert (tree, &chars[i], &chars[i]);
    }
  for (j = 0; j < 26; j++, i++)
    {
      chars[i] = 'A' + j;
      g_tree_insert (tree, &chars[i], &chars[i]);
    }
  for (j = 0; j < 26; j++, i++)
    {
      chars[i] = 'a' + j;
      g_tree_insert (tree, &chars[i], &chars[i]);
    }

  g_print ("tree height: %d\n", g_tree_height (tree));
  g_print ("tree nnodes: %d\n", g_tree_nnodes (tree));

  g_print ("tree: ");
  g_tree_traverse (tree, my_traverse, G_IN_ORDER, NULL);
  g_print ("\n");

  for (i = 0; i < 10; i++)
    g_tree_remove (tree, &chars[i]);

  g_print ("tree height: %d\n", g_tree_height (tree));
  g_print ("tree nnodes: %d\n", g_tree_nnodes (tree));

  g_print ("tree: ");
  g_tree_traverse (tree, my_traverse, G_IN_ORDER, NULL);
  g_print ("\n");

  g_print ("ok\n");


  /* check n-way trees */
  g_node_test ();

  g_print ("checking mem chunks...");

  mem_chunk = g_mem_chunk_new ("test mem chunk", 50, 100, G_ALLOC_AND_FREE);

  for (i = 0; i < 10000; i++)
    {
      mem[i] = g_chunk_new (gchar, mem_chunk);

      for (j = 0; j < 50; j++)
	mem[i][j] = i * j;
    }

  for (i = 0; i < 10000; i++)
    {
      g_mem_chunk_free (mem_chunk, mem[i]);
    }

  g_print ("ok\n");


  g_print ("checking hash tables...");

  hash_table = g_hash_table_new (my_hash, my_hash_compare);
  for (i = 0; i < 10000; i++)
    {
      array[i] = i;
      g_hash_table_insert (hash_table, &array[i], &array[i]);
    }
  g_hash_table_foreach (hash_table, my_hash_callback, NULL);

  for (i = 0; i < 10000; i++)
    if (array[i] == 0)
      g_print ("%d\n", i);

  for (i = 0; i < 10000; i++)
    g_hash_table_remove (hash_table, &array[i]);

  for (i = 0; i < 10000; i++)
    {
      array[i] = i;
      g_hash_table_insert (hash_table, &array[i], &array[i]);
    }

  if (g_hash_table_foreach_remove (hash_table, my_hash_callback_remove, NULL) != 5000 ||
      g_hash_table_size (hash_table) != 5000)
    g_print ("bad!\n");

  g_hash_table_foreach (hash_table, my_hash_callback_remove_test, NULL);


  g_hash_table_destroy (hash_table);

  g_print ("ok\n");


  g_print ("checking string chunks...");

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

  g_print ("ok\n");


  g_print ("checking arrays...");

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 10000; i++)
    g_array_append_val (garray, i);

  for (i = 0; i < 10000; i++)
    if (g_array_index (garray, gint, i) != i)
      g_print ("uh oh: %d ( %d )\n", g_array_index (garray, gint, i), i);

  g_array_free (garray, TRUE);

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 100; i++)
    g_array_prepend_val (garray, i);

  for (i = 0; i < 100; i++)
    if (g_array_index (garray, gint, i) != (100 - i - 1))
      g_print ("uh oh: %d ( %d )\n", g_array_index (garray, gint, i), 100 - i - 1);

  g_array_free (garray, TRUE);

  g_print ("ok\n");


  g_print ("checking strings...");

  string1 = g_string_new ("hi pete!");
  string2 = g_string_new ("");

  g_assert (strcmp ("hi pete!", string1->str) == 0);

  for (i = 0; i < 10000; i++)
    g_string_append_c (string1, 'a'+(i%26));

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

  g_print ("string2 length = %d...\n", string2->len);
  string2->str[70] = '\0';
  g_print ("first 70 chars:\n%s\n", string2->str);
  string2->str[141] = '\0';
  g_print ("next 70 chars:\n%s\n", string2->str+71);
  string2->str[212] = '\0';
  g_print ("and next 70:\n%s\n", string2->str+142);
  g_print ("last 70 chars:\n%s\n", string2->str+string2->len - 70);

  g_print ("ok\n");

  g_print ("checking timers...\n");

  timer = g_timer_new ();
  g_print ("  spinning for 3 seconds...\n");

  g_timer_start (timer);
  while (g_timer_elapsed (timer, NULL) < 3)
    ;

  g_timer_stop (timer);
  g_timer_destroy (timer);

  g_print ("ok\n");

  g_print ("checking g_strcasecmp...");
  g_assert (g_strcasecmp ("FroboZZ", "frobozz") == 0);
  g_assert (g_strcasecmp ("frobozz", "frobozz") == 0);
  g_assert (g_strcasecmp ("frobozz", "FROBOZZ") == 0);
  g_assert (g_strcasecmp ("FROBOZZ", "froboz") != 0);
  g_assert (g_strcasecmp ("", "") == 0);
  g_assert (g_strcasecmp ("!#%&/()", "!#%&/()") == 0);
  g_assert (g_strcasecmp ("a", "b") < 0);
  g_assert (g_strcasecmp ("a", "B") < 0);
  g_assert (g_strcasecmp ("A", "b") < 0);
  g_assert (g_strcasecmp ("A", "B") < 0);
  g_assert (g_strcasecmp ("b", "a") > 0);
  g_assert (g_strcasecmp ("b", "A") > 0);
  g_assert (g_strcasecmp ("B", "a") > 0);
  g_assert (g_strcasecmp ("B", "A") > 0);

  g_print ("ok\n");

  g_print ("checking g_strdup...");
  g_assert(g_strdup(NULL) == NULL);
  string = g_strdup(GLIB_TEST_STRING);
  g_assert(string != NULL);
  g_assert(strcmp(string, GLIB_TEST_STRING) == 0);
  g_free(string);

  g_print ("ok\n");

  g_print ("checking g_strconcat...");
  string = g_strconcat(GLIB_TEST_STRING, NULL);
  g_assert(string != NULL);
  g_assert(strcmp(string, GLIB_TEST_STRING) == 0);
  g_free(string);
  string = g_strconcat(GLIB_TEST_STRING, GLIB_TEST_STRING, 
  		       GLIB_TEST_STRING, NULL);
  g_assert(string != NULL);
  g_assert(strcmp(string, GLIB_TEST_STRING GLIB_TEST_STRING
  			  GLIB_TEST_STRING) == 0);
  g_free(string);
  
  g_print ("ok\n");

  g_print ("checking g_strdup_printf...");
  string = g_strdup_printf ("%05d %-5s", 21, "test");
  g_assert (string != NULL);
  g_assert (strcmp(string, "00021 test ") == 0);
  g_free (string);

  g_print ("ok\n");

  /* g_debug (argv[0]); */

  /* Relation tests */

  g_print ("checking relations...");

  relation = g_relation_new (2);

  g_relation_index (relation, 0, g_int_hash, g_int_equal);
  g_relation_index (relation, 1, g_int_hash, g_int_equal);

  for (i = 0; i < 1024; i += 1)
    data[i] = i;

  for (i = 1; i < 1023; i += 1)
    {
      g_relation_insert (relation, data + i, data + i + 1);
      g_relation_insert (relation, data + i, data + i - 1);
    }

  for (i = 2; i < 1022; i += 1)
    {
      g_assert (! g_relation_exists (relation, data + i, data + i));
      g_assert (! g_relation_exists (relation, data + i, data + i + 2));
      g_assert (! g_relation_exists (relation, data + i, data + i - 2));
    }

  for (i = 1; i < 1023; i += 1)
    {
      g_assert (g_relation_exists (relation, data + i, data + i + 1));
      g_assert (g_relation_exists (relation, data + i, data + i - 1));
    }

  for (i = 2; i < 1022; i += 1)
    {
      g_assert (g_relation_count (relation, data + i, 0) == 2);
      g_assert (g_relation_count (relation, data + i, 1) == 2);
    }

  g_assert (g_relation_count (relation, data, 0) == 0);

  g_assert (g_relation_count (relation, data + 42, 0) == 2);
  g_assert (g_relation_count (relation, data + 43, 1) == 2);
  g_assert (g_relation_count (relation, data + 41, 1) == 2);
  g_relation_delete (relation, data + 42, 0);
  g_assert (g_relation_count (relation, data + 42, 0) == 0);
  g_assert (g_relation_count (relation, data + 43, 1) == 1);
  g_assert (g_relation_count (relation, data + 41, 1) == 1);

  tuples = g_relation_select (relation, data + 200, 0);

  g_assert (tuples->len == 2);

#if 0
  for (i = 0; i < tuples->len; i += 1)
    {
      printf ("%d %d\n",
	      *(gint*) g_tuples_index (tuples, i, 0),
	      *(gint*) g_tuples_index (tuples, i, 1));
    }
#endif

  g_assert (g_relation_exists (relation, data + 300, data + 301));
  g_relation_delete (relation, data + 300, 0);
  g_assert (!g_relation_exists (relation, data + 300, data + 301));

  g_tuples_destroy (tuples);

  g_relation_destroy (relation);

  relation = NULL;

  g_print ("ok\n");

  g_print ("checking pointer arrays...");

  gparray = g_ptr_array_new ();
  for (i = 0; i < 10000; i++)
    g_ptr_array_add (gparray, GINT_TO_POINTER (i));

  for (i = 0; i < 10000; i++)
    if (g_ptr_array_index (gparray, i) != GINT_TO_POINTER (i))
      g_print ("array fails: %p ( %p )\n", g_ptr_array_index (gparray, i), GINT_TO_POINTER (i));

  g_ptr_array_free (gparray, TRUE);

  g_print ("ok\n");


  g_print ("checking byte arrays...");

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
  g_print ("ok\n");

  g_printerr ("g_log tests:");
  g_warning ("harmless warning with parameters: %d %s %#x", 42, "Boo", 12345);
  g_message ("the next warning is a test:");
  string = NULL;
  g_print (string);

  g_print ("checking endian macros (host is ");
#if G_BYTE_ORDER == G_BIG_ENDIAN
  g_print ("big endian)...");
#else
  g_print ("little endian)...");
#endif
  g_assert (GUINT16_SWAP_LE_BE (gu16t1) == gu16t2);  
  g_assert (GUINT32_SWAP_LE_BE (gu32t1) == gu32t2);  
#ifdef G_HAVE_GINT64
  g_assert (GUINT64_SWAP_LE_BE (gu64t1) == gu64t2);  
#endif
  g_print ("ok\n");

  return 0;
}

