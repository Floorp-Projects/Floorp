/*
 *
 * Some crude tests for libIDL.
 *
 * Usage: tstidl <filename> [flags]
 *
 * if given, flags is read as (output_flags << 24) | parse_flags
 *
 * gcc `libIDL-config --cflags --libs` tstidl.c -o tstidl
 *
 */
#ifdef G_LOG_DOMAIN
#  undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN		"tstidl"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef IDL_LIBRARY
#  include "IDL.h"
#else
#  include <libIDL/IDL.h>
#endif

#define IDLFP_IDENT_VISITED	(1UL << 0)

typedef struct {
	IDL_tree tree;
	IDL_ns ns;
} WalkData;

static gboolean
print_repo_id (IDL_tree_func_data *tfd, WalkData *data)
{
	char *repo_id = NULL;
	IDL_tree p;

	p = tfd->tree;

	if (IDL_NODE_TYPE (p) == IDLN_INTERFACE) {
		repo_id = IDL_IDENT_REPO_ID (IDL_INTERFACE (p).ident);
	} else if (IDL_NODE_TYPE (p) == IDLN_IDENT &&
		 IDL_NODE_UP (p) != NULL &&
		 IDL_NODE_UP (IDL_NODE_UP (p)) != NULL &&
		 IDL_NODE_TYPE (IDL_NODE_UP (IDL_NODE_UP (p))) == IDLN_ATTR_DCL)
		repo_id = IDL_IDENT_REPO_ID (p);
	if (repo_id)
		printf ("%s\n", repo_id);

	return TRUE;
}

static gboolean
print_xpidl_exts (IDL_tree_func_data *tfd, WalkData *data)
{
	IDL_tree p;

	p = tfd->tree;

	if (IDL_NODE_TYPE (p) == IDLN_IDENT &&
	    IDL_NODE_TYPE (IDL_NODE_UP (p)) == IDLN_INTERFACE) {
		const char *val;

		val = IDL_tree_property_get (p, "IID");
		if (val) printf ("\tinterface `%s' XPIDL IID:\"%s\"\n",
				 IDL_IDENT (IDL_INTERFACE (IDL_NODE_UP (p)).ident).str,
				 val);
	}

	if (IDL_NODE_TYPE (p) == IDLN_NATIVE &&
	    IDL_NATIVE (p).user_type)
		g_message ("XPIDL native type: \"%s\"", IDL_NATIVE (p).user_type);

	if (IDL_NODE_TYPE (p) == IDLN_CODEFRAG) {
		GSList *slist = IDL_CODEFRAG (p).lines;

		g_message ("XPIDL code fragment desc.: \"%s\"", IDL_CODEFRAG (p).desc);
		for (; slist; slist = slist->next)
			g_message ("XPIDL code fragment line.: \"%s\"", (char *) slist->data);
	}

	return TRUE;
}

static gboolean
print_ident_comments (IDL_tree_func_data *tfd, WalkData *data)
{
	GSList *list;
	IDL_tree p;

	p = tfd->tree;

	if (IDL_NODE_TYPE (p) == IDLN_IDENT) {
		if (!(p->flags & IDLFP_IDENT_VISITED)) {
			printf ("Identifier: %s\n", IDL_IDENT (p).str);
			for (list = IDL_IDENT (p).comments; list;
			     list = g_slist_next (list)) {
				char *comment = list->data;
				printf ("%s\n", comment);
			}
			p->flags |= IDLFP_IDENT_VISITED;
		}
	}

	return TRUE;
}

static gboolean
print_const_dcls (IDL_tree_func_data *tfd, WalkData *data)
{
	IDL_tree p;

	p = tfd->tree;

	if (IDL_NODE_TYPE (p) == IDLN_CONST_DCL) {
		GString *s;

		s = IDL_tree_to_IDL_string (p, NULL, IDLF_OUTPUT_NO_NEWLINES);
		puts (s->str);
		g_string_free (s, TRUE);

		return FALSE;
	}

	return TRUE;
}

/* Example input method... simply reads in from some file and passes it along as
 * is--warning, no actual C preprocessing performed here!  Standard C preprocessor
 * indicators should also be passed to libIDL, including # <line> "filename" (double
 * quotes expected), etcetera (do not confuse this with passing #defines to libIDL, this
 * should not be done). */

/* #define TEST_INPUT_CB */

#ifdef _WIN32
#  ifndef TEST_INPUT_CB
#    define TEST_INPUT_CB
#  endif
#endif

#ifdef TEST_INPUT_CB
typedef struct {
	FILE *in;
} InputData;

static int
my_input_cb (IDL_input_reason reason, union IDL_input_data *cb_data, gpointer user_data)
{
	InputData *my_data = user_data;
	int rv;
	static int linecount;

	switch (reason) {
	case IDL_INPUT_REASON_INIT:
		g_message ("my_input_cb: filename: %s", cb_data->init.filename);
		my_data->in = fopen (cb_data->init.filename, "r");
		/* If failed, should know that it is implied to libIDL that errno is set
		 * appropriately by a C library function or otherwise. Return 0 upon
		 * success. */
		linecount = 1;
		IDL_file_set (cb_data->init.filename, 1);
		return my_data->in ? 0 : -1;

	case IDL_INPUT_REASON_FILL:
		/* Fill the buffer here... return number of bytes read (maximum of
		   cb_data->fill.max_size), 0 for EOF, negative value upon error. */
		rv = fread (cb_data->fill.buffer, 1, cb_data->fill.max_size, my_data->in);
		IDL_queue_new_ident_comment ("Queue some comment...");
		g_message ("my_input_cb: fill, max size %d, got %d",
			   cb_data->fill.max_size, rv);
		if (rv == 0 && ferror (my_data->in))
			return -1;
		IDL_file_set (NULL, ++linecount);
		return rv;

	case IDL_INPUT_REASON_ABORT:
	case IDL_INPUT_REASON_FINISH:
		/* Called after parsing to indicate success or failure */
		g_message ("my_input_cb: abort or finish");
		fclose (my_data->in);
		break;
	}

	return 0;
}
#endif

int
main (int argc, char *argv[])
{
	int rv;
	IDL_tree tree;
	IDL_ns ns;
	char *fn;
	WalkData data;
	unsigned long parse_flags = 0;

#ifndef _WIN32
	{ extern int __IDL_debug;
	__IDL_debug = argc >= 4 ? TRUE : FALSE; }
#endif

	IDL_check_cast_enable (TRUE);

	g_message ("libIDL version %s", IDL_get_libver_string ());

	if (argc < 2) {
		fprintf (stderr, "usage: tstidl <filename> [parse flags, hex]\n");
		exit (1);
	}

	fn = argv[1];
	if (argc >= 3)
		sscanf (argv[2], "%lx", &parse_flags);

#ifdef TEST_INPUT_CB
	{ InputData input_cb_data;
	g_message ("IDL_parse_filename_with_input");
	rv = IDL_parse_filename_with_input (
		fn, my_input_cb, &input_cb_data,
		NULL, &tree, &ns, parse_flags,
		IDL_WARNING1);
	}
#else
	g_message ("IDL_parse_filename");
	rv = IDL_parse_filename (
		fn, NULL, NULL, &tree, &ns,
		parse_flags, IDL_WARNING1);
#endif

	if (rv == IDL_ERROR) {
		g_message ("IDL_ERROR");
		exit (1);	
	} else if (rv < 0) {
		perror (fn);
		exit (1);
	}

	/* rv == IDL_SUCCESS */

	data.tree = tree;
	data.ns = ns;

	g_print ("--------------------------------------\n");
	g_message ("Repository IDs");
	g_print ("--------------------------------------\n");
	IDL_tree_walk_in_order (tree, (IDL_tree_func) print_repo_id, &data);
	g_print ("\n--------------------------------------\n");
	g_message ("XPIDL extensions");
	g_print ("--------------------------------------\n");
	IDL_tree_walk_in_order (tree, (IDL_tree_func) print_xpidl_exts, &data);
	g_print ("\n--------------------------------------\n");
	g_message ("Constant Declarations");
	g_print ("--------------------------------------\n");
	IDL_tree_walk_in_order (tree, (IDL_tree_func) print_const_dcls, &data);
	g_print ("\n--------------------------------------\n");
	g_message ("Identifiers");
	g_print ("--------------------------------------\n");
	IDL_tree_walk_in_order (tree, (IDL_tree_func) print_ident_comments, &data);
	g_print ("\n--------------------------------------\n");
	g_message ("IDL_tree_to_IDL");
	g_print ("--------------------------------------\n");
	IDL_tree_to_IDL (tree, ns, stdout, parse_flags >> 24);
	IDL_ns_free (ns);
	IDL_tree_free (tree);

	return 0;
}
