/**************************************************************************

    util.c

    Copyright (C) 1998, 1999 Andrew T. Veliath

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id: util.c,v 1.1 1999/05/06 15:06:09 beard%netscape.com Exp $

***************************************************************************/
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#ifndef _WIN32
#  include <unistd.h>
#endif
#include "rename.h"
#include "util.h"

IDL_EXPORT const char *IDL_tree_type_names[] = {
	"IDLN_NONE",
	"IDLN_ANY",
	"IDLN_LIST",
	"IDLN_GENTREE",
	"IDLN_INTEGER",
	"IDLN_STRING",
	"IDLN_WIDE_STRING",
	"IDLN_CHAR",
	"IDLN_WIDE_CHAR",
	"IDLN_FIXED",
	"IDLN_FLOAT",
	"IDLN_BOOLEAN",
	"IDLN_IDENT",
	"IDLN_TYPE_DCL",
	"IDLN_CONST_DCL",
	"IDLN_EXCEPT_DCL",
	"IDLN_ATTR_DCL",
	"IDLN_OP_DCL",
	"IDLN_PARAM_DCL",
	"IDLN_FORWARD_DCL",
	"IDLN_TYPE_INTEGER",
	"IDLN_TYPE_FLOAT",
	"IDLN_TYPE_FIXED",
	"IDLN_TYPE_CHAR",
	"IDLN_TYPE_WIDE_CHAR",
	"IDLN_TYPE_STRING",
	"IDLN_TYPE_WIDE_STRING",
	"IDLN_TYPE_BOOLEAN",
	"IDLN_TYPE_OCTET",
	"IDLN_TYPE_ANY",
	"IDLN_TYPE_OBJECT",
	"IDLN_TYPE_TYPECODE",
	"IDLN_TYPE_ENUM",
	"IDLN_TYPE_SEQUENCE",
	"IDLN_TYPE_ARRAY",
	"IDLN_TYPE_STRUCT",
	"IDLN_TYPE_UNION",
	"IDLN_MEMBER",
	"IDLN_NATIVE",
	"IDLN_CASE_STMT",
	"IDLN_INTERFACE",
	"IDLN_MODULE",
	"IDLN_BINOP",
	"IDLN_UNARYOP",
	"IDLN_CODEFRAG",
	/* IDLN_LAST */
};

IDL_EXPORT int				__IDL_check_type_casts = FALSE;
#ifndef HAVE_CPP_PIPE_STDIN
char *					__IDL_tmp_filename = NULL;
#endif
const char *				__IDL_real_filename = NULL;
char *					__IDL_cur_filename = NULL;
int					__IDL_cur_line;
GHashTable *				__IDL_filename_hash;
IDL_fileinfo *				__IDL_cur_fileinfo;
GHashTable *				__IDL_structunion_ht;
int					__IDL_inhibits;
IDL_tree				__IDL_root;
IDL_ns					__IDL_root_ns;
int					__IDL_is_okay;
int					__IDL_is_parsing;
unsigned long				__IDL_flags;
unsigned long				__IDL_flagsi;
gpointer				__IDL_inputcb_user_data;
IDL_input_callback			__IDL_inputcb;
GSList *				__IDL_new_ident_comments;
static int				__IDL_max_msg_level;
static int				__IDL_nerrors, __IDL_nwarnings;
static IDL_msg_callback			__IDL_msgcb;

/* Case-insensitive version of g_str_hash */
guint IDL_strcase_hash (gconstpointer v)
{
	const char *p;
	guint h = 0, g;

	for (p = (char *) v; *p != '\0'; ++p) {
		h = (h << 4) + isupper (*p) ? tolower (*p) : *p;
		if ((g = h & 0xf0000000)) {
			h = h ^ (g >> 24);
			h = h ^ g;
		}
	}

	return h /* % M */;
}

gint IDL_strcase_equal (gconstpointer a, gconstpointer b)
{
	return g_strcasecmp (a, b) == 0;
}

gint IDL_strcase_cmp (gconstpointer a, gconstpointer b)
{
	return g_strcasecmp (a, b);
}

static int my_strcmp (IDL_tree p, IDL_tree q)
{
	const char *a = IDL_IDENT (p).str;
	const char *b = IDL_IDENT (q).str;
	int cmp = IDL_strcase_cmp (a, b);

	if (__IDL_is_parsing &&
	    cmp == 0 &&
	    strcmp (a, b) != 0 &&
	    !(IDL_IDENT (p)._flags & IDLF_IDENT_CASE_MISMATCH_HIT ||
	      IDL_IDENT (q)._flags & IDLF_IDENT_CASE_MISMATCH_HIT)) {
		IDL_tree_warning (p, IDL_WARNING1, "Case mismatch between `%s'", a);
		IDL_tree_warning (q, IDL_WARNING1, "and `%s'", b);
		yywarning (IDL_WARNING1,
			   "(Identifiers should be case-consistent after initial declaration)");
		IDL_IDENT (p)._flags |= IDLF_IDENT_CASE_MISMATCH_HIT;
		IDL_IDENT (q)._flags |= IDLF_IDENT_CASE_MISMATCH_HIT;
	}

	return cmp;
}

guint IDL_ident_hash (gconstpointer v)
{
	return IDL_strcase_hash (IDL_IDENT ((IDL_tree) v).str);
}

gint IDL_ident_equal (gconstpointer a, gconstpointer b)
{
	return my_strcmp ((IDL_tree) a, (IDL_tree) b) == 0;
}

gint IDL_ident_cmp (gconstpointer a, gconstpointer b)
{
	return my_strcmp ((IDL_tree) a, (IDL_tree) b);
}

const char *IDL_get_libver_string (void)
{
	return LIBIDL_LIBRARY_VERSION;
}

const char *IDL_get_IDLver_string (void)
{
	return "2.2";
}

static void IDL_tree_optimize (IDL_tree *p, IDL_ns ns)
{
	if (!(__IDL_flags & IDLF_IGNORE_FORWARDS))
		IDL_tree_process_forward_dcls (p, ns);
	IDL_tree_remove_inhibits (p, ns);
	IDL_tree_remove_empty_modules (p, ns);
}

int IDL_parse_filename (const char *filename, const char *cpp_args,
			IDL_msg_callback msg_cb, IDL_tree *tree, IDL_ns *ns,
			unsigned long parse_flags, int max_msg_level)
{
	extern void __IDL_lex_init (void);
	extern void __IDL_lex_cleanup (void);
	extern int yyparse (void);
	extern FILE *__IDL_in;
	FILE *input;
	char *cmd;
	size_t cmd_len;
#ifdef HAVE_CPP_PIPE_STDIN
	char *fmt = CPP_PROGRAM " - %s%s %s < \"%s\" 2>/dev/null";
	char *wd = "", *dirend;
#else
	char *fmt = CPP_PROGRAM " -I- -I%s %s \"%s\" 2>/dev/null";
	char *s, *tmpfilename;
	char cwd[2048];
	gchar *linkto;
#endif
	GSList *slist;
	int rv;

	if (!filename ||
	    !tree ||
	    (tree == NULL && ns != NULL)) {
		errno = EINVAL;
		return -1;
	}

#if !defined(_WIN32) && !defined(XP_MAC)
	if (access (filename, R_OK))
		return -1;
#endif

#ifdef HAVE_CPP_PIPE_STDIN
	if ((dirend = strrchr (filename, '/'))) {
		int len = dirend - filename + 1;
		wd = g_malloc (len);
		strncpy (wd, filename, len - 1);
		wd[len - 1] = 0;
	}

	cmd_len = (strlen (filename) + (*wd ? 2 : 0) + strlen (wd) +
		  (cpp_args ? strlen (cpp_args) : 0) +
		  strlen (fmt) - 8 + 1);
	cmd = g_malloc (cmd_len);
	if (!cmd) {
		errno = ENOMEM;
		return -1;
	}

	g_snprintf (cmd, cmd_len, fmt, *wd ? "-I" : "", wd,
		    cpp_args ? cpp_args : "", filename);

	if (dirend)
		g_free (wd);
#else
	s = tmpnam (NULL);
	if (s == NULL)
		return -1;

	if (!getcwd (cwd, sizeof (cwd)))
		return -1;

	if (*filename == '/') {
		linkto = g_strdup (filename);
	} else {
		linkto = g_malloc (strlen (cwd) + strlen (filename) + 2);
		if (!linkto) {
			errno = ENOMEM;
			return -1;
		}
		strcpy (linkto, cwd);
		strcat (linkto, "/");
		strcat (linkto, filename);
	}

	tmpfilename = g_malloc (strlen (s) + 3);
	if (!tmpfilename) {
		g_free (linkto);
		errno = ENOMEM;
		return -1;
	}
	strcpy (tmpfilename, s);
	strcat (tmpfilename, ".c");
#ifndef XP_MAC
	if (symlink (linkto, tmpfilename) < 0) {
		g_free (linkto);
		g_free (tmpfilename);
		return -1;
	}
#endif
	g_free (linkto);

	cmd_len = (strlen (tmpfilename) + strlen (cwd) +
		   (cpp_args ? strlen (cpp_args) : 0) +
		   strlen (fmt) - 6 + 1);
	cmd = g_malloc (cmd_len);
	if (!cmd) {
		g_free (tmpfilename);
		errno = ENOMEM;
		return -1;
	}

	g_snprintf (cmd, cmd_len, fmt,
		    cwd, cpp_args ? cpp_args : "", tmpfilename);
#endif

#ifdef XP_MAC
	input = fopen (cmd, "r");
#else
	input = popen (cmd, "r");
#endif
	g_free (cmd);

	if (input == NULL || ferror (input)) {
#ifndef HAVE_CPP_PIPE_STDIN
		g_free (tmpfilename);
#endif
		return IDL_ERROR;
	}

	if (parse_flags & IDLF_XPIDL)
		parse_flags |= IDLF_PROPERTIES;

	__IDL_max_msg_level = max_msg_level;
	__IDL_nerrors = __IDL_nwarnings = 0;
	__IDL_in = input;
	__IDL_msgcb = msg_cb;
	__IDL_inhibits = 0;
	__IDL_flags = parse_flags;
	__IDL_flagsi = 0;
	__IDL_root_ns = IDL_ns_new ();

	__IDL_is_parsing = TRUE;
	__IDL_is_okay = TRUE;
	__IDL_lex_init ();
	__IDL_new_ident_comments = NULL;

	__IDL_real_filename = filename;
#ifndef HAVE_CPP_PIPE_STDIN
	__IDL_tmp_filename = tmpfilename;
#endif
	__IDL_filename_hash = IDL_NS (__IDL_root_ns).filename_hash;
	__IDL_structunion_ht = g_hash_table_new (g_direct_hash, g_direct_equal);
	rv = yyparse ();
	g_hash_table_destroy (__IDL_structunion_ht);
	__IDL_is_parsing = FALSE;
	__IDL_lex_cleanup ();
	__IDL_real_filename = NULL;
#ifndef HAVE_CPP_PIPE_STDIN
	__IDL_tmp_filename = NULL;
#endif

#ifdef XP_MAC
	fclose(input);
#else
	pclose (input);
#endif

#ifndef HAVE_CPP_PIPE_STDIN
	unlink (tmpfilename);
	g_free (tmpfilename);
#endif
	for (slist = __IDL_new_ident_comments; slist; slist = slist->next)
		g_free (slist->data);
	g_slist_free (__IDL_new_ident_comments);

	if (__IDL_root != NULL) {
		IDL_tree_optimize (&__IDL_root, __IDL_root_ns);

		if (__IDL_root == NULL)
			yyerror ("File empty after optimization");
	}

	__IDL_msgcb = NULL;

	if (rv != 0 || !__IDL_is_okay) {
		if (tree)
			*tree = NULL;

		if (ns)
			*ns = NULL;

		return IDL_ERROR;
	}

	if (__IDL_flags & IDLF_PREFIX_FILENAME)
		IDL_ns_prefix (__IDL_root_ns, filename);

	if (tree)
		*tree = __IDL_root;
	else
		IDL_tree_free (__IDL_root);

	if (ns)
		*ns = __IDL_root_ns;
	else
		IDL_ns_free (__IDL_root_ns);

	return IDL_SUCCESS;
}

int IDL_parse_filename_with_input (const char *filename,
				   IDL_input_callback input_cb,
				   gpointer input_cb_user_data,
				   IDL_msg_callback msg_cb,
				   IDL_tree *tree, IDL_ns *ns,
				   unsigned long parse_flags,
				   int max_msg_level)
{
	extern void __IDL_lex_init (void);
	extern void __IDL_lex_cleanup (void);
	extern int yyparse (void);
	union IDL_input_data data;
	GSList *slist;
	int rv;

	if (!filename || !input_cb || !tree ||
	    (tree == NULL && ns != NULL)) {
		errno = EINVAL;
		return -1;
	}

	if (parse_flags & IDLF_XPIDL)
		parse_flags |= IDLF_PROPERTIES;

	__IDL_max_msg_level = max_msg_level;
	__IDL_nerrors = __IDL_nwarnings = 0;
	__IDL_msgcb = msg_cb;
	__IDL_inhibits = 0;
	__IDL_flags = parse_flags;
	__IDL_flagsi = 0;
	__IDL_root_ns = IDL_ns_new ();

	__IDL_is_parsing = TRUE;
	__IDL_is_okay = TRUE;
	__IDL_lex_init ();
	__IDL_inputcb = input_cb;
	__IDL_inputcb_user_data = input_cb_user_data;
	__IDL_new_ident_comments = NULL;

	__IDL_real_filename = filename;
#ifndef HAVE_CPP_PIPE_STDIN
	__IDL_tmp_filename = NULL;
#endif
	__IDL_filename_hash = IDL_NS (__IDL_root_ns).filename_hash;
	data.init.filename = filename;
	if ((*__IDL_inputcb) (
		IDL_INPUT_REASON_INIT, &data, __IDL_inputcb_user_data)) {
		IDL_ns_free (__IDL_root_ns);
		__IDL_lex_cleanup ();
		__IDL_real_filename = NULL;
		return -1;
	}
	__IDL_structunion_ht = g_hash_table_new (g_direct_hash, g_direct_equal);
	rv = yyparse ();
	g_hash_table_destroy (__IDL_structunion_ht);
	__IDL_is_parsing = FALSE;
	__IDL_lex_cleanup ();
	__IDL_real_filename = NULL;
	for (slist = __IDL_new_ident_comments; slist; slist = slist->next)
		g_free (slist->data);
	g_slist_free (__IDL_new_ident_comments);

	if (__IDL_root != NULL) {
		IDL_tree_optimize (&__IDL_root, __IDL_root_ns);

		if (__IDL_root == NULL)
			yyerror ("File empty after optimization");
	}

	__IDL_msgcb = NULL;

	if (rv != 0 || !__IDL_is_okay) {
		if (tree)
			*tree = NULL;

		if (ns)
			*ns = NULL;

		(*__IDL_inputcb) (
			IDL_INPUT_REASON_ABORT, NULL, __IDL_inputcb_user_data);

		return IDL_ERROR;
	}

	(*__IDL_inputcb) (IDL_INPUT_REASON_FINISH, NULL, __IDL_inputcb_user_data);

	if (__IDL_flags & IDLF_PREFIX_FILENAME)
		IDL_ns_prefix (__IDL_root_ns, filename);

	if (tree)
		*tree = __IDL_root;
	else
		IDL_tree_free (__IDL_root);

	if (ns)
		*ns = __IDL_root_ns;
	else
		IDL_ns_free (__IDL_root_ns);

	return IDL_SUCCESS;
}

void yyerrorl (const char *s, int ofs)
{
	int line = __IDL_cur_line - 1 + ofs;
	gchar *filename = NULL;

	if (__IDL_cur_filename) {
#ifdef HAVE_CPP_PIPE_STDIN
		filename = __IDL_cur_filename;
#else
		filename = g_basename (__IDL_cur_filename);
#endif
	} else
		line = -1;

	++__IDL_nerrors;
	__IDL_is_okay = FALSE;

	/* Errors are counted, even if not printed */
	if (__IDL_max_msg_level < IDL_ERROR)
		return;

	if (__IDL_msgcb)
		(*__IDL_msgcb)(IDL_ERROR, __IDL_nerrors, line, filename, s);
	else {
		if (line > 0)
			fprintf (stderr, "%s:%d: Error: %s\n", filename, line, s);
		else
			fprintf (stderr, "Error: %s\n", s);
	}
}

void yywarningl (int level, const char *s, int ofs)
{
	int line = __IDL_cur_line - 1 + ofs;
	gchar *filename = NULL;

	/* Unprinted warnings are not counted */
	if (__IDL_max_msg_level < level)
		return;

	if (__IDL_cur_filename) {
#ifdef HAVE_CPP_PIPE_STDIN
		filename = __IDL_cur_filename;
#else
		filename = g_basename (__IDL_cur_filename);
#endif
	} else
		line = -1;

	++__IDL_nwarnings;

	if (__IDL_msgcb)
		(*__IDL_msgcb)(level, __IDL_nwarnings, line, filename, s);
	else {
		if (line > 0)
			fprintf (stderr, "%s:%d: Warning: %s\n", filename, line, s);
		else
			fprintf (stderr, "Warning: %s\n", s);
	}
}

void yyerror (const char *s)
{
	yyerrorl (s, 0);
}

void yywarning (int level, const char *s)
{
	yywarningl (level, s, 0);
}

void yyerrorlv (const char *fmt, int ofs, ...)
{
	gchar *msg;
	va_list args;

	va_start (args, ofs);

	msg = g_strdup_vprintf (fmt, args);
	yyerrorl (msg, ofs);

	va_end (args);

	g_free (msg);
}

void yywarninglv (int level, const char *fmt, int ofs, ...)
{
	gchar *msg;
	va_list args;

	va_start (args, ofs);

	msg = g_strdup_vprintf (fmt, args);
	yywarningl (level, msg, ofs);

	va_end (args);

	g_free (msg);
}

void yyerrorv (const char *fmt, ...)
{
	gchar *msg;
	va_list args;

	va_start (args, fmt);

	msg = g_strdup_vprintf (fmt, args);
	yyerror (msg);

	va_end (args);

	g_free (msg);
}

void yywarningv (int level, const char *fmt, ...)
{
	gchar *msg;
	va_list args;

	va_start (args, fmt);

	msg = g_strdup_vprintf (fmt, args);
	yywarning (level, msg);

	va_end (args);

	g_free (msg);
}

void IDL_tree_error (IDL_tree p, const char *fmt, ...)
{
	char *file_save = __IDL_cur_filename;
	int line_save = __IDL_cur_line;
	gchar *msg;
	va_list args;

	if (p) {
		__IDL_cur_filename = p->_file;
		__IDL_cur_line = p->_line;
	} else {
		__IDL_cur_filename = NULL;
		__IDL_cur_line = -1;
	}

	va_start (args, fmt);

	msg = g_strdup_vprintf (fmt, args);
	yyerror (msg);

	va_end (args);

	g_free (msg);

	__IDL_cur_filename = file_save;
	__IDL_cur_line = line_save;
}

void IDL_tree_warning (IDL_tree p, int level, const char *fmt, ...)
{
	char *file_save = __IDL_cur_filename;
	int line_save = __IDL_cur_line;
	gchar *msg;
	va_list args;

	if (p) {
		__IDL_cur_filename = p->_file;
		__IDL_cur_line = p->_line;
	} else {
		__IDL_cur_filename = NULL;
		__IDL_cur_line = -1;
	}

	va_start (args, fmt);

	msg = g_strdup_vprintf (fmt, args);
	yywarning (level, msg);

	va_end (args);

	g_free (msg);

	__IDL_cur_filename = file_save;
	__IDL_cur_line = line_save;
}

int IDL_tree_get_node_info (IDL_tree p, char **what, char **who)
{
	int dienow = 0;

	assert (what != NULL);
	assert (who != NULL);

	switch (IDL_NODE_TYPE (p)) {
	case IDLN_TYPE_STRUCT:
		*what = "structure definition";
		*who = IDL_IDENT (IDL_TYPE_STRUCT (p).ident).str;
		break;

	case IDLN_TYPE_UNION:
		*what = "union definition";
		*who = IDL_IDENT (IDL_TYPE_UNION (p).ident).str;
		break;

	case IDLN_TYPE_ARRAY:
		*what = "array";
		*who = IDL_IDENT (IDL_TYPE_ARRAY (p).ident).str;
		break;

	case IDLN_TYPE_ENUM:
		*what = "enumeration definition";
		*who = IDL_IDENT (IDL_TYPE_ENUM (p).ident).str;
		break;

	case IDLN_IDENT:
		*what = "identifier";
		*who = IDL_IDENT (p).str;
		break;

	case IDLN_TYPE_DCL:
		*what = "type definition";
		assert (IDL_TYPE_DCL (p).dcls != NULL);
		assert (IDL_NODE_TYPE (IDL_TYPE_DCL (p).dcls) == IDLN_LIST);
		assert (IDL_LIST (IDL_TYPE_DCL (p).dcls)._tail != NULL);
		assert (IDL_NODE_TYPE (IDL_LIST (IDL_TYPE_DCL (p).dcls)._tail) == IDLN_LIST);
		*who = IDL_IDENT (IDL_LIST (IDL_LIST (IDL_TYPE_DCL (p).dcls)._tail).data).str;
		break;

	case IDLN_MEMBER:
		*what = "member declaration";
		assert (IDL_MEMBER (p).dcls != NULL);
		assert (IDL_NODE_TYPE (IDL_MEMBER (p).dcls) == IDLN_LIST);
		assert (IDL_LIST (IDL_MEMBER (p).dcls)._tail != NULL);
		assert (IDL_NODE_TYPE (IDL_LIST (IDL_MEMBER (p).dcls)._tail) == IDLN_LIST);
		*who = IDL_IDENT (IDL_LIST (IDL_LIST (IDL_MEMBER (p).dcls)._tail).data).str;
		break;

	case IDLN_NATIVE:
		*what = "native declaration";
		assert (IDL_NATIVE (p).ident != NULL);
		assert (IDL_NODE_TYPE (IDL_NATIVE (p).ident) == IDLN_IDENT);
		*who = IDL_IDENT (IDL_NATIVE (p).ident).str;
		break;

	case IDLN_LIST:
		if (!IDL_LIST (p).data)
			break;
		dienow = IDL_tree_get_node_info (IDL_LIST (p).data, what, who);
		break;

	case IDLN_ATTR_DCL:
		*what = "interface attribute";
		assert (IDL_ATTR_DCL (p).simple_declarations != NULL);
		assert (IDL_NODE_TYPE (IDL_ATTR_DCL (p).simple_declarations) == IDLN_LIST);
		assert (IDL_LIST (IDL_ATTR_DCL (p).simple_declarations)._tail != NULL);
		assert (IDL_NODE_TYPE (IDL_LIST (
			IDL_ATTR_DCL (p).simple_declarations)._tail) == IDLN_LIST);
		*who = IDL_IDENT (IDL_LIST (IDL_LIST (
			IDL_ATTR_DCL (p).simple_declarations)._tail).data).str;
		break;

	case IDLN_PARAM_DCL:
		*what = "operation parameter";
		assert (IDL_PARAM_DCL (p).simple_declarator != NULL);
		assert (IDL_NODE_TYPE (IDL_PARAM_DCL (p).simple_declarator) = IDLN_IDENT);
		*who = IDL_IDENT (IDL_PARAM_DCL (p).simple_declarator).str;
		break;

	case IDLN_CONST_DCL:
		*what = "constant declaration for";
		*who = IDL_IDENT (IDL_CONST_DCL (p).ident).str;
		break;

	case IDLN_EXCEPT_DCL:
		*what = "exception";
		*who = IDL_IDENT (IDL_EXCEPT_DCL (p).ident).str;
		break;

	case IDLN_OP_DCL:
		*what = "interface operation";
		*who = IDL_IDENT (IDL_OP_DCL (p).ident).str;
		break;

	case IDLN_MODULE:
		*what = "module";
		*who = IDL_IDENT (IDL_MODULE (p).ident).str;
		break;

	case IDLN_FORWARD_DCL:
		*what = "forward declaration";
		*who = IDL_IDENT (IDL_FORWARD_DCL (p).ident).str;
		break;

	case IDLN_INTERFACE:
		*what = "interface";
		*who = IDL_IDENT (IDL_INTERFACE (p).ident).str;
		break;

	default:
		g_warning ("Node type: %s\n", IDL_NODE_TYPE_NAME (p));
		*what = "unknown (internal error)";
		break;
	}

	return dienow;
}

static IDL_tree IDL_node_new (IDL_tree_type type)
{
	IDL_tree p;

	p = g_new0 (IDL_tree_node, 1);
	if (p == NULL) {
		yyerror ("IDL_node_new: memory exhausted");
		return NULL;
	}

	IDL_NODE_TYPE (p) = type;
	IDL_NODE_REFS (p) = 1;

	p->_file = __IDL_cur_filename;
	p->_line = __IDL_cur_line;

	return p;
}

void __IDL_assign_up_node (IDL_tree up, IDL_tree node)
{
	if (node == NULL)
		return;

	assert (node != up);

	switch (IDL_NODE_TYPE (node)) {
	case IDLN_LIST:
		if (IDL_NODE_UP (node) == NULL)
			for (; node != NULL; node = IDL_LIST (node).next)
				IDL_NODE_UP (node) = up;
		break;

	default:
		if (IDL_NODE_UP (node) == NULL)
			IDL_NODE_UP (node) = up;
		break;
	}
}

void __IDL_assign_location (IDL_tree node, IDL_tree from_node)
{
	assert (node != NULL);

	if (from_node) {
		node->_file = from_node->_file;
		node->_line = from_node->_line;
	}
}

void __IDL_assign_this_location (IDL_tree node, char *filename, int line)
{
	assert (node != NULL);

	node->_file = filename;
	node->_line = line;
}

IDL_tree IDL_list_new (IDL_tree data)
{
	IDL_tree p = IDL_node_new (IDLN_LIST);

	__IDL_assign_up_node (p, data);
	IDL_LIST (p).data = data;
	IDL_LIST (p)._tail = p;

	return p;
}

IDL_tree IDL_list_concat (IDL_tree orig, IDL_tree append)
{
	IDL_tree p;

	if (orig == NULL)
		return append;

	if (append == NULL)
		return orig;

	IDL_LIST (IDL_LIST (orig)._tail).next = append;
	IDL_LIST (append).prev = IDL_LIST (orig)._tail;
	IDL_LIST (orig)._tail = IDL_LIST (append)._tail;

	/* Set tails on original */
	for (p = IDL_LIST (orig).next; p && p != append; p = IDL_LIST (p).next)
		IDL_LIST (p)._tail = IDL_LIST (orig)._tail;

	/* Set up nodes on appended list */
	for (p = append; p; p = IDL_LIST (p).next)
		IDL_NODE_UP (p) = IDL_NODE_UP (orig);

	return orig;
}

IDL_tree IDL_list_remove (IDL_tree list, IDL_tree p)
{
	IDL_tree new_list = list;

	if (IDL_LIST (p).prev == NULL) {
		assert (list == p);
		new_list = IDL_LIST (p).next;
		if (new_list)
			IDL_LIST (new_list).prev = NULL;
	} else {
		IDL_tree prev = IDL_LIST (p).prev;
		IDL_tree next = IDL_LIST (p).next;

		IDL_LIST (prev).next = next;
		if (next)
			IDL_LIST (next).prev = prev;
	}

	IDL_LIST (p).prev = NULL;
	IDL_LIST (p).next = NULL;
	IDL_LIST (p)._tail = p;

	/* Not all tails updated... */

	return new_list;
}

IDL_tree IDL_gentree_new (GHashFunc hash_func, GCompareFunc key_compare_func, IDL_tree data)
{
	IDL_tree p = IDL_node_new (IDLN_GENTREE);

	__IDL_assign_up_node (p, data);
	IDL_GENTREE (p).data = data;
	IDL_GENTREE (p).hash_func = hash_func;
	IDL_GENTREE (p).key_compare_func = key_compare_func;
	IDL_GENTREE (p).siblings = g_hash_table_new (hash_func, key_compare_func);
	IDL_GENTREE (p).children = g_hash_table_new (hash_func, key_compare_func);

	g_hash_table_insert (IDL_GENTREE (p).siblings, data, p);

	return p;
}

IDL_tree IDL_gentree_new_sibling (IDL_tree from, IDL_tree data)
{
	IDL_tree p = IDL_node_new (IDLN_GENTREE);

	__IDL_assign_up_node (p, data);
	IDL_GENTREE (p).data = data;
	IDL_GENTREE (p).hash_func = IDL_GENTREE (from).hash_func;
	IDL_GENTREE (p).key_compare_func = IDL_GENTREE (from).key_compare_func;
	IDL_GENTREE (p).siblings = IDL_GENTREE (from).siblings;
	IDL_GENTREE (p).children = g_hash_table_new (IDL_GENTREE (from).hash_func,
						     IDL_GENTREE (from).key_compare_func);

	return p;
}

IDL_tree IDL_integer_new (IDL_longlong_t value)
{
	IDL_tree p = IDL_node_new (IDLN_INTEGER);

	IDL_INTEGER (p).value = value;

	return p;
}

IDL_tree IDL_string_new (char *value)
{
	IDL_tree p = IDL_node_new (IDLN_STRING);

	IDL_STRING (p).value = value;

	return p;
}

IDL_tree IDL_wide_string_new (wchar_t *value)
{
	IDL_tree p = IDL_node_new (IDLN_WIDE_STRING);

	IDL_WIDE_STRING (p).value = value;

	return p;
}

IDL_tree IDL_char_new (char *value)
{
	IDL_tree p = IDL_node_new (IDLN_CHAR);

	IDL_CHAR (p).value = value;

	return p;
}

IDL_tree IDL_wide_char_new (wchar_t *value)
{
	IDL_tree p = IDL_node_new (IDLN_WIDE_CHAR);

	IDL_WIDE_CHAR (p).value = value;

	return p;
}

IDL_tree IDL_fixed_new (char *value)
{
	IDL_tree p = IDL_node_new (IDLN_FIXED);

	IDL_FIXED (p).value = value;

	return p;
}

IDL_tree IDL_float_new (double value)
{
	IDL_tree p = IDL_node_new (IDLN_FLOAT);

	IDL_FLOAT (p).value = value;

	return p;
}

IDL_tree IDL_boolean_new (unsigned value)
{
	IDL_tree p = IDL_node_new (IDLN_BOOLEAN);

	IDL_BOOLEAN (p).value = value;

	return p;
}

IDL_tree IDL_ident_new (char *str)
{
	IDL_tree p = IDL_node_new (IDLN_IDENT);

	IDL_IDENT (p).str = str;

	return p;
}

IDL_tree IDL_member_new (IDL_tree type_spec, IDL_tree dcls)
{
	IDL_tree p = IDL_node_new (IDLN_MEMBER);

	__IDL_assign_up_node (p, type_spec);
	__IDL_assign_up_node (p, dcls);
	IDL_MEMBER (p).type_spec = type_spec;
	IDL_MEMBER (p).dcls = dcls;

	return p;
}

IDL_tree IDL_native_new (IDL_tree ident)
{
	IDL_tree p = IDL_node_new (IDLN_NATIVE);

	__IDL_assign_up_node (p, ident);
	__IDL_assign_location (p, ident);
	IDL_NATIVE (p).ident = ident;

	return p;
}

IDL_tree IDL_type_dcl_new (IDL_tree type_spec, IDL_tree dcls)
{
	IDL_tree p = IDL_node_new (IDLN_TYPE_DCL);

	__IDL_assign_up_node (p, type_spec);
	__IDL_assign_up_node (p, dcls);
	__IDL_assign_location (p, IDL_LIST (dcls).data);
	IDL_TYPE_DCL (p).type_spec = type_spec;
	IDL_TYPE_DCL (p).dcls = dcls;

	return p;
}

IDL_tree IDL_type_float_new (enum IDL_float_type f_type)
{
	IDL_tree p = IDL_node_new (IDLN_TYPE_FLOAT);

	IDL_TYPE_FLOAT (p).f_type = f_type;

	return p;
}

IDL_tree IDL_type_fixed_new (IDL_tree positive_int_const,
			     IDL_tree integer_lit)
{
	IDL_tree p = IDL_node_new (IDLN_TYPE_FIXED);

	__IDL_assign_up_node (p, positive_int_const);
	__IDL_assign_up_node (p, integer_lit);
	IDL_TYPE_FIXED (p).positive_int_const = positive_int_const;
	IDL_TYPE_FIXED (p).integer_lit = integer_lit;

	return p;
}

IDL_tree IDL_type_integer_new (unsigned f_signed, enum IDL_integer_type f_type)
{
	IDL_tree p = IDL_node_new (IDLN_TYPE_INTEGER);

	IDL_TYPE_INTEGER (p).f_signed = f_signed;
	IDL_TYPE_INTEGER (p).f_type = f_type;

	return p;
}

IDL_tree IDL_type_char_new (void)
{
	return IDL_node_new (IDLN_TYPE_CHAR);
}

IDL_tree IDL_type_wide_char_new (void)
{
	return IDL_node_new (IDLN_TYPE_WIDE_CHAR);
}

IDL_tree IDL_type_boolean_new (void)
{
	return IDL_node_new (IDLN_TYPE_BOOLEAN);
}

IDL_tree IDL_type_octet_new (void)
{
	return IDL_node_new (IDLN_TYPE_OCTET);
}

IDL_tree IDL_type_any_new (void)
{
	return IDL_node_new (IDLN_TYPE_ANY);
}

IDL_tree IDL_type_object_new (void)
{
	return IDL_node_new (IDLN_TYPE_OBJECT);
}

IDL_tree IDL_type_typecode_new (void)
{
	return IDL_node_new (IDLN_TYPE_TYPECODE);
}

IDL_tree IDL_type_string_new (IDL_tree positive_int_const)
{
	IDL_tree p = IDL_node_new (IDLN_TYPE_STRING);

	__IDL_assign_up_node (p, positive_int_const);
	IDL_TYPE_STRING (p).positive_int_const = positive_int_const;

	return p;
}

IDL_tree IDL_type_wide_string_new (IDL_tree positive_int_const)
{
	IDL_tree p = IDL_node_new (IDLN_TYPE_WIDE_STRING);

	__IDL_assign_up_node (p, positive_int_const);
	IDL_TYPE_WIDE_STRING (p).positive_int_const = positive_int_const;

	return p;
}

IDL_tree IDL_type_array_new (IDL_tree ident,
			     IDL_tree size_list)
{
	IDL_tree p = IDL_node_new (IDLN_TYPE_ARRAY);

	__IDL_assign_up_node (p, ident);
	__IDL_assign_up_node (p, size_list);
	__IDL_assign_location (p, ident);
	IDL_TYPE_ARRAY (p).ident = ident;
	IDL_TYPE_ARRAY (p).size_list = size_list;

	return p;
}

IDL_tree IDL_type_sequence_new (IDL_tree simple_type_spec,
				IDL_tree positive_int_const)
{
	IDL_tree p = IDL_node_new (IDLN_TYPE_SEQUENCE);

	__IDL_assign_up_node (p, simple_type_spec);
	__IDL_assign_up_node (p, positive_int_const);
	IDL_TYPE_SEQUENCE (p).simple_type_spec = simple_type_spec;
	IDL_TYPE_SEQUENCE (p).positive_int_const = positive_int_const;

	return p;
}

IDL_tree IDL_type_struct_new (IDL_tree ident, IDL_tree member_list)
{
	IDL_tree p = IDL_node_new (IDLN_TYPE_STRUCT);

	__IDL_assign_up_node (p, ident);
	__IDL_assign_up_node (p, member_list);
	__IDL_assign_location (p, ident);
	IDL_TYPE_STRUCT (p).ident = ident;
	IDL_TYPE_STRUCT (p).member_list = member_list;

	return p;
}

IDL_tree IDL_type_union_new (IDL_tree ident, IDL_tree switch_type_spec, IDL_tree switch_body)
{
	IDL_tree p = IDL_node_new (IDLN_TYPE_UNION);

	__IDL_assign_up_node (p, ident);
	__IDL_assign_up_node (p, switch_type_spec);
	__IDL_assign_up_node (p, switch_body);
	__IDL_assign_location (p, ident);
	IDL_TYPE_UNION (p).ident = ident;
	IDL_TYPE_UNION (p).switch_type_spec = switch_type_spec;
	IDL_TYPE_UNION (p).switch_body = switch_body;

	return p;
}

IDL_tree IDL_type_enum_new (IDL_tree ident, IDL_tree enumerator_list)
{
	IDL_tree p = IDL_node_new (IDLN_TYPE_ENUM);

	__IDL_assign_up_node (p, ident);
	__IDL_assign_up_node (p, enumerator_list);
	__IDL_assign_location (p, ident);
	IDL_TYPE_ENUM (p).ident = ident;
	IDL_TYPE_ENUM (p).enumerator_list = enumerator_list;

	return p;
}

IDL_tree IDL_case_stmt_new (IDL_tree labels, IDL_tree element_spec)
{
	IDL_tree p = IDL_node_new (IDLN_CASE_STMT);

	__IDL_assign_up_node (p, labels);
	__IDL_assign_up_node (p, element_spec);
	IDL_CASE_STMT (p).labels = labels;
	IDL_CASE_STMT (p).element_spec = element_spec;

	return p;
}

IDL_tree IDL_interface_new (IDL_tree ident, IDL_tree inheritance_spec, IDL_tree body)
{
	IDL_tree p = IDL_node_new (IDLN_INTERFACE);

	/* Make sure the up node points to the interface */
	if (ident && IDL_NODE_UP (ident) &&
	    IDL_NODE_TYPE (IDL_NODE_UP (ident)) != IDLN_INTERFACE)
		IDL_NODE_UP (ident) = NULL;
	__IDL_assign_up_node (p, ident);
	__IDL_assign_up_node (p, inheritance_spec);
	__IDL_assign_up_node (p, body);
	IDL_INTERFACE (p).ident = ident;
	IDL_INTERFACE (p).inheritance_spec = inheritance_spec;
	IDL_INTERFACE (p).body = body;

	return p;
}

IDL_tree IDL_module_new (IDL_tree ident, IDL_tree definition_list)
{
	IDL_tree p = IDL_node_new (IDLN_MODULE);

	__IDL_assign_up_node (p, ident);
	__IDL_assign_up_node (p, definition_list);
	__IDL_assign_location (p, ident);
	IDL_MODULE (p).ident = ident;
	IDL_MODULE (p).definition_list = definition_list;

	return p;
}

IDL_tree IDL_binop_new (enum IDL_binop op, IDL_tree left, IDL_tree right)
{
	IDL_tree p = IDL_node_new (IDLN_BINOP);

	__IDL_assign_up_node (p, left);
	__IDL_assign_up_node (p, right);
	IDL_BINOP (p).op = op;
	IDL_BINOP (p).left = left;
	IDL_BINOP (p).right = right;

	return p;
}

IDL_tree IDL_unaryop_new (enum IDL_unaryop op, IDL_tree operand)
{
	IDL_tree p = IDL_node_new (IDLN_UNARYOP);

	__IDL_assign_up_node (p, operand);
	IDL_UNARYOP (p).op = op;
	IDL_UNARYOP (p).operand = operand;

	return p;
}

IDL_tree IDL_codefrag_new (char *desc, GSList *lines)
{
	IDL_tree p = IDL_node_new (IDLN_CODEFRAG);

	IDL_CODEFRAG (p).desc = desc;
	IDL_CODEFRAG (p).lines = lines;

	return p;
}

IDL_tree IDL_const_dcl_new (IDL_tree const_type, IDL_tree ident, IDL_tree const_exp)
{
	IDL_tree p = IDL_node_new (IDLN_CONST_DCL);

	__IDL_assign_up_node (p, const_type);
	__IDL_assign_up_node (p, ident);
	__IDL_assign_up_node (p, const_exp);
	__IDL_assign_location (p, ident);
	IDL_CONST_DCL (p).const_type = const_type;
	IDL_CONST_DCL (p).ident = ident;
	IDL_CONST_DCL (p).const_exp = const_exp;

	return p;
}

IDL_tree IDL_except_dcl_new (IDL_tree ident, IDL_tree members)
{
	IDL_tree p = IDL_node_new (IDLN_EXCEPT_DCL);

	__IDL_assign_up_node (p, ident);
	__IDL_assign_up_node (p, members);
	__IDL_assign_location (p, ident);
	IDL_EXCEPT_DCL (p).ident = ident;
	IDL_EXCEPT_DCL (p).members = members;

	return p;
}

IDL_tree IDL_attr_dcl_new (unsigned f_readonly,
			   IDL_tree param_type_spec,
			   IDL_tree simple_declarations)
{
	IDL_tree p = IDL_node_new (IDLN_ATTR_DCL);

	__IDL_assign_up_node (p, param_type_spec);
	__IDL_assign_up_node (p, simple_declarations);
	__IDL_assign_location (p, IDL_LIST (simple_declarations).data);
	IDL_ATTR_DCL (p).f_readonly = f_readonly;
	IDL_ATTR_DCL (p).param_type_spec = param_type_spec;
	IDL_ATTR_DCL (p).simple_declarations = simple_declarations;

	return p;
}

IDL_tree IDL_op_dcl_new (unsigned f_oneway,
			 IDL_tree op_type_spec,
			 IDL_tree ident,
			 IDL_tree parameter_dcls,
			 IDL_tree raises_expr,
			 IDL_tree context_expr)
{
	IDL_tree p = IDL_node_new (IDLN_OP_DCL);

	__IDL_assign_up_node (p, op_type_spec);
	__IDL_assign_up_node (p, ident);
	__IDL_assign_up_node (p, parameter_dcls);
	__IDL_assign_up_node (p, raises_expr);
	__IDL_assign_up_node (p, context_expr);
	__IDL_assign_location (p, ident);
	IDL_OP_DCL (p).f_oneway = f_oneway;
	IDL_OP_DCL (p).op_type_spec = op_type_spec;
	IDL_OP_DCL (p).ident = ident;
	IDL_OP_DCL (p).parameter_dcls = parameter_dcls;
	IDL_OP_DCL (p).raises_expr = raises_expr;
	IDL_OP_DCL (p).context_expr = context_expr;

	return p;
}

IDL_tree IDL_param_dcl_new (enum IDL_param_attr attr,
			    IDL_tree param_type_spec,
			    IDL_tree simple_declarator)
{
	IDL_tree p = IDL_node_new (IDLN_PARAM_DCL);

	__IDL_assign_up_node (p, param_type_spec);
	__IDL_assign_up_node (p, simple_declarator);
	__IDL_assign_location (p, simple_declarator);
	IDL_PARAM_DCL (p).attr = attr;
	IDL_PARAM_DCL (p).param_type_spec = param_type_spec;
	IDL_PARAM_DCL (p).simple_declarator = simple_declarator;

	return p;
}

IDL_tree IDL_forward_dcl_new (IDL_tree ident)
{
	IDL_tree p = IDL_node_new (IDLN_FORWARD_DCL);

	__IDL_assign_up_node (p, ident);
	__IDL_assign_location (p, ident);
	IDL_FORWARD_DCL (p).ident = ident;

	return p;
}

IDL_tree IDL_check_type_cast (const IDL_tree tree, IDL_tree_type type,
			      const char *file, int line, const char *function)
{
	if (__IDL_check_type_casts) {
		if (tree == NULL) {
			g_warning ("file %s: line %d: (%s) invalid type cast attempt,"
				   " NULL tree to %s\n",
				   file, line, function,
				   IDL_tree_type_names[type]);
		}
		else if (IDL_NODE_TYPE (tree) != type) {
			g_warning ("file %s: line %d: (%s) expected IDL tree type %s,"
				   " but got %s\n",
				   file, line, function,
				   IDL_tree_type_names[type], IDL_NODE_TYPE_NAME (tree));

		}
	}
	return tree;
}

IDL_tree IDL_gentree_chain_sibling (IDL_tree from, IDL_tree data)
{
	IDL_tree p;

	if (from == NULL)
		return NULL;

	p = IDL_gentree_new_sibling (from, data);
	IDL_NODE_UP (p) = IDL_NODE_UP (from);

	return p;
}

IDL_tree IDL_gentree_chain_child (IDL_tree from, IDL_tree data)
{
	IDL_tree p;

	if (from == NULL)
		return NULL;

	p = IDL_gentree_new (IDL_GENTREE (from).hash_func,
			     IDL_GENTREE (from).key_compare_func,
			     data);
	IDL_NODE_UP (p) = from;

	g_hash_table_insert (IDL_GENTREE (from).children, data, p);

	return p;
}

IDL_tree IDL_get_parent_node (IDL_tree p, IDL_tree_type type, int *levels)
{
	int count = 0;

	if (p == NULL)
		return NULL;

	if (type == IDLN_ANY)
		return IDL_NODE_UP (p);

	while (p != NULL && IDL_NODE_TYPE (p) != type) {

		if (IDL_NODE_IS_SCOPED (p))
			++count;

		p = IDL_NODE_UP (p);
	}

	if (p != NULL)
		if (levels != NULL)
			*levels = count;

	return p;
}

IDL_tree IDL_tree_get_scope (IDL_tree p)
{
	g_return_val_if_fail (p != NULL, NULL);

	if (IDL_NODE_TYPE (p) == IDLN_GENTREE)
		return p;

	if (!IDL_NODE_IS_SCOPED (p)) {
		g_warning ("Node type %s isn't scoped", IDL_NODE_TYPE_NAME (p));
		return NULL;
	}

	switch (IDL_NODE_TYPE (p)) {
	case IDLN_IDENT:
		return IDL_IDENT_TO_NS (p);

	case IDLN_INTERFACE:
		return IDL_IDENT_TO_NS (IDL_INTERFACE (p).ident);

	case IDLN_MODULE:
		return IDL_IDENT_TO_NS (IDL_MODULE (p).ident);

	case IDLN_EXCEPT_DCL:
		return IDL_IDENT_TO_NS (IDL_EXCEPT_DCL (p).ident);

	case IDLN_OP_DCL:
		return IDL_IDENT_TO_NS (IDL_OP_DCL (p).ident);

	case IDLN_TYPE_ENUM:
		return IDL_IDENT_TO_NS (IDL_TYPE_ENUM (p).ident);

	case IDLN_TYPE_STRUCT:
		return IDL_IDENT_TO_NS (IDL_TYPE_STRUCT (p).ident);

	case IDLN_TYPE_UNION:
		return IDL_IDENT_TO_NS (IDL_TYPE_UNION (p).ident);

	default:
		return NULL;
	}
}

typedef struct {
	IDL_tree_func pre_tree_func;
	IDL_tree_func post_tree_func;
	gpointer user_data;
} IDLTreeWalkRealData;

static void IDL_tree_walk_real (IDL_tree_func_data *tfd, IDLTreeWalkRealData *data)
{
	IDL_tree_func_data down_tfd;
	gboolean recurse = TRUE;
	IDL_tree p, q;

	if (tfd->tree == NULL)
		return;

	tfd->state->bottom = tfd;
	tfd->step = 0;
	tfd->data = NULL;

	if (data->pre_tree_func)
		recurse = (*data->pre_tree_func) (tfd, data->user_data);
	++tfd->step;

	down_tfd.state = tfd->state;
	down_tfd.up = tfd;

	p = tfd->tree;

	if (recurse) switch (IDL_NODE_TYPE (p)) {
	case IDLN_INTEGER:
	case IDLN_STRING:
	case IDLN_CHAR:
	case IDLN_FIXED:
	case IDLN_FLOAT:
	case IDLN_BOOLEAN:
	case IDLN_IDENT:
	case IDLN_TYPE_WIDE_CHAR:
	case IDLN_TYPE_BOOLEAN:
	case IDLN_TYPE_OCTET:
	case IDLN_TYPE_ANY:
	case IDLN_TYPE_OBJECT:
	case IDLN_TYPE_TYPECODE:
	case IDLN_TYPE_FLOAT:
	case IDLN_TYPE_INTEGER:
	case IDLN_TYPE_CHAR:
	case IDLN_CODEFRAG:
		break;

	case IDLN_LIST:
		for (q = p; q; q = IDL_LIST (q).next) {
			down_tfd.tree = IDL_LIST (q).data;
			IDL_tree_walk_real (&down_tfd, data);
		}
		break;

	case IDLN_GENTREE:
		g_error ("IDLN_GENTREE walk not implemented!");
		break;

	case IDLN_MEMBER:
		down_tfd.tree = IDL_MEMBER (p).type_spec;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_MEMBER (p).dcls;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_NATIVE:
		down_tfd.tree = IDL_NATIVE (p).ident;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_TYPE_DCL:
		down_tfd.tree = IDL_TYPE_DCL (p).type_spec;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_TYPE_DCL (p).dcls;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_CONST_DCL:
		down_tfd.tree = IDL_CONST_DCL (p).const_type;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_CONST_DCL (p).ident;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_CONST_DCL (p).const_exp;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_EXCEPT_DCL:
		down_tfd.tree = IDL_EXCEPT_DCL (p).ident;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_EXCEPT_DCL (p).members;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_ATTR_DCL:
		down_tfd.tree = IDL_ATTR_DCL (p).param_type_spec;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_ATTR_DCL (p).simple_declarations;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_OP_DCL:
		down_tfd.tree = IDL_OP_DCL (p).op_type_spec;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_OP_DCL (p).ident;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_OP_DCL (p).parameter_dcls;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_OP_DCL (p).raises_expr;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_OP_DCL (p).context_expr;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_PARAM_DCL:
		down_tfd.tree = IDL_PARAM_DCL (p).param_type_spec;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_PARAM_DCL (p).simple_declarator;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_FORWARD_DCL:
		down_tfd.tree = IDL_FORWARD_DCL (p).ident;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_TYPE_FIXED:
		down_tfd.tree = IDL_TYPE_FIXED (p).positive_int_const;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_TYPE_FIXED (p).integer_lit;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_TYPE_STRING:
		down_tfd.tree = IDL_TYPE_STRING (p).positive_int_const;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_TYPE_WIDE_STRING:
		down_tfd.tree = IDL_TYPE_WIDE_STRING (p).positive_int_const;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_TYPE_ENUM:
		down_tfd.tree = IDL_TYPE_ENUM (p).ident;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_TYPE_ENUM (p).enumerator_list;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_TYPE_SEQUENCE:
		down_tfd.tree = IDL_TYPE_SEQUENCE (p).simple_type_spec;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_TYPE_SEQUENCE (p).positive_int_const;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_TYPE_ARRAY:
		down_tfd.tree = IDL_TYPE_ARRAY (p).ident;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_TYPE_ARRAY (p).size_list;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_TYPE_STRUCT:
		down_tfd.tree = IDL_TYPE_STRUCT (p).ident;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_TYPE_STRUCT (p).member_list;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_TYPE_UNION:
		down_tfd.tree = IDL_TYPE_UNION (p).ident;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_TYPE_UNION (p).switch_type_spec;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_TYPE_UNION (p).switch_body;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_CASE_STMT:
		down_tfd.tree = IDL_CASE_STMT (p).labels;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_CASE_STMT (p).element_spec;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_INTERFACE:
		down_tfd.tree = IDL_INTERFACE (p).ident;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_INTERFACE (p).inheritance_spec;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_INTERFACE (p).body;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_MODULE:
		down_tfd.tree = IDL_MODULE (p).ident;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_MODULE (p).definition_list;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_BINOP:
		down_tfd.tree = IDL_BINOP (p).left;
		IDL_tree_walk_real (&down_tfd, data);
		down_tfd.tree = IDL_BINOP (p).right;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	case IDLN_UNARYOP:
		down_tfd.tree = IDL_UNARYOP (p).operand;
		IDL_tree_walk_real (&down_tfd, data);
		break;

	default:
		g_warning ("IDL_tree_walk_real: unknown node type %s\n",
			   IDL_NODE_TYPE_NAME (p));
		break;
	}

	if (data->post_tree_func)
			(void) (*data->post_tree_func) (tfd, data->user_data);

	tfd->state->bottom = tfd->up;
}

void IDL_tree_walk (IDL_tree p, IDL_tree_func_data *current,
		    IDL_tree_func pre_tree_func, IDL_tree_func post_tree_func,
		    gpointer user_data)
{
	IDLTreeWalkRealData data;
	IDL_tree_func_state tfs;
	IDL_tree_func_data tfd;

	g_return_if_fail (!(pre_tree_func == NULL && post_tree_func == NULL));

	data.pre_tree_func = pre_tree_func;
	data.post_tree_func = post_tree_func;
	data.user_data = user_data;

	tfs.up = current ? current->state : NULL;
	tfs.start = p;

	if (current) tfd = *current;
	tfd.state = &tfs;
	tfd.up = current;
	tfd.tree = p;

	IDL_tree_walk_real (&tfd, &data);
}

void IDL_tree_walk_in_order (IDL_tree p, IDL_tree_func tree_func, gpointer user_data)
{
	IDL_tree_walk (p, NULL, tree_func, NULL, user_data);
}

static void __IDL_tree_free (IDL_tree p);

static int tree_free_but_this (IDL_tree data, IDL_tree p, IDL_tree this_one)
{
	if (p == this_one)
		return TRUE;

	__IDL_tree_free (p);

	return TRUE;
}

static void property_free (char *key, char *value)
{
	g_free (key);
	g_free (value);
}

void __IDL_free_properties (GHashTable *table)
{
	if (table) {
		g_hash_table_foreach (table, (GHFunc) property_free, NULL);
		g_hash_table_destroy (table);
	}
}

/* Free associated node data, regardless of refcounts */
static void IDL_tree_free_real (IDL_tree p)
{
	GSList *slist;

	assert (p != NULL);

	switch (IDL_NODE_TYPE (p)) {
	case IDLN_GENTREE:
		g_hash_table_foreach (IDL_GENTREE (p).children,
				      (GHFunc) tree_free_but_this, NULL);
		g_hash_table_destroy (IDL_GENTREE (p).children);
		break;

	case IDLN_FIXED:
		g_free (IDL_FIXED (p).value);
		break;

	case IDLN_STRING:
		g_free (IDL_STRING (p).value);
		break;

	case IDLN_CHAR:
		g_free (IDL_CHAR (p).value);
		break;

	case IDLN_IDENT:
		g_free (IDL_IDENT (p).str);
		g_free (IDL_IDENT_REPO_ID (p));
		for (slist = IDL_IDENT (p).comments; slist; slist = slist->next)
			g_free (slist->data);
		g_slist_free (IDL_IDENT (p).comments);
		break;

	case IDLN_NATIVE:
		g_free (IDL_NATIVE (p).user_type);
		break;

	case IDLN_INTERFACE:
		break;

	case IDLN_CODEFRAG:
		g_free (IDL_CODEFRAG (p).desc);
		for (slist = IDL_CODEFRAG (p).lines; slist; slist = slist->next)
			g_free (slist->data);
		g_slist_free (IDL_CODEFRAG (p).lines);
		break;

	default:
		break;
	}

	__IDL_free_properties (IDL_NODE_PROPERTIES (p));

	g_free (p);
}

/* Free node taking into account refcounts */
static void __IDL_tree_free (IDL_tree p)
{
	if (p == NULL)
		return;

	if (--IDL_NODE_REFS (p) <= 0)
		IDL_tree_free_real (p);
}

/* Free a set of references of an entire tree */
void IDL_tree_free (IDL_tree p)
{
	IDL_tree q;

	if (p == NULL)
		return;

	switch (IDL_NODE_TYPE (p)) {
	case IDLN_INTEGER:
	case IDLN_FLOAT:
	case IDLN_BOOLEAN:
	case IDLN_TYPE_FLOAT:
	case IDLN_TYPE_INTEGER:
	case IDLN_TYPE_CHAR:
	case IDLN_TYPE_WIDE_CHAR:
	case IDLN_TYPE_BOOLEAN:
	case IDLN_TYPE_OCTET:
	case IDLN_TYPE_ANY:
	case IDLN_TYPE_OBJECT:
	case IDLN_TYPE_TYPECODE:
	case IDLN_FIXED:
	case IDLN_STRING:
	case IDLN_CHAR:
	case IDLN_IDENT:
	case IDLN_CODEFRAG:
		__IDL_tree_free (p);
		break;

	case IDLN_LIST:
		while (p) {
			IDL_tree_free (IDL_LIST (p).data);
			q = IDL_LIST (p).next;
			__IDL_tree_free (p);
			p = q;
		}
		break;

	case IDLN_GENTREE:
		g_hash_table_foreach (IDL_GENTREE (p).siblings,
				      (GHFunc) tree_free_but_this, p);
		g_hash_table_destroy (IDL_GENTREE (p).siblings);
		__IDL_tree_free (p);
		break;

	case IDLN_MEMBER:
		IDL_tree_free (IDL_MEMBER (p).type_spec);
		IDL_tree_free (IDL_MEMBER (p).dcls);
		__IDL_tree_free (p);
		break;

	case IDLN_NATIVE:
		IDL_tree_free (IDL_NATIVE (p).ident);
		__IDL_tree_free (p);
		break;

	case IDLN_TYPE_ENUM:
		IDL_tree_free (IDL_TYPE_ENUM (p).ident);
		IDL_tree_free (IDL_TYPE_ENUM (p).enumerator_list);
		__IDL_tree_free (p);
		break;

	case IDLN_TYPE_SEQUENCE:
		IDL_tree_free (IDL_TYPE_SEQUENCE (p).simple_type_spec);
		IDL_tree_free (IDL_TYPE_SEQUENCE (p).positive_int_const);
		__IDL_tree_free (p);
		break;

	case IDLN_TYPE_ARRAY:
		IDL_tree_free (IDL_TYPE_ARRAY (p).ident);
		IDL_tree_free (IDL_TYPE_ARRAY (p).size_list);
		__IDL_tree_free (p);
		break;

	case IDLN_TYPE_STRUCT:
		IDL_tree_free (IDL_TYPE_STRUCT (p).ident);
		IDL_tree_free (IDL_TYPE_STRUCT (p).member_list);
		__IDL_tree_free (p);
		break;

	case IDLN_TYPE_UNION:
		IDL_tree_free (IDL_TYPE_UNION (p).ident);
		IDL_tree_free (IDL_TYPE_UNION (p).switch_type_spec);
		IDL_tree_free (IDL_TYPE_UNION (p).switch_body);
		__IDL_tree_free (p);
		break;

	case IDLN_TYPE_DCL:
		IDL_tree_free (IDL_TYPE_DCL (p).type_spec);
		IDL_tree_free (IDL_TYPE_DCL (p).dcls);
		__IDL_tree_free (p);
		break;

	case IDLN_CONST_DCL:
		IDL_tree_free (IDL_CONST_DCL (p).const_type);
		IDL_tree_free (IDL_CONST_DCL (p).ident);
		IDL_tree_free (IDL_CONST_DCL (p).const_exp);
		__IDL_tree_free (p);
		break;

	case IDLN_EXCEPT_DCL:
		IDL_tree_free (IDL_EXCEPT_DCL (p).ident);
		IDL_tree_free (IDL_EXCEPT_DCL (p).members);
		__IDL_tree_free (p);
		break;

	case IDLN_ATTR_DCL:
		IDL_tree_free (IDL_ATTR_DCL (p).param_type_spec);
		IDL_tree_free (IDL_ATTR_DCL (p).simple_declarations);
		__IDL_tree_free (p);
		break;

	case IDLN_OP_DCL:
		IDL_tree_free (IDL_OP_DCL (p).op_type_spec);
		IDL_tree_free (IDL_OP_DCL (p).ident);
		IDL_tree_free (IDL_OP_DCL (p).parameter_dcls);
		IDL_tree_free (IDL_OP_DCL (p).raises_expr);
		IDL_tree_free (IDL_OP_DCL (p).context_expr);
		__IDL_tree_free (p);
		break;

	case IDLN_PARAM_DCL:
		IDL_tree_free (IDL_PARAM_DCL (p).param_type_spec);
		IDL_tree_free (IDL_PARAM_DCL (p).simple_declarator);
		__IDL_tree_free (p);
		break;

	case IDLN_FORWARD_DCL:
		IDL_tree_free (IDL_FORWARD_DCL (p).ident);
		__IDL_tree_free (p);
		break;

	case IDLN_TYPE_STRING:
		IDL_tree_free (IDL_TYPE_STRING (p).positive_int_const);
		__IDL_tree_free (p);
		break;

	case IDLN_TYPE_WIDE_STRING:
		IDL_tree_free (IDL_TYPE_WIDE_STRING (p).positive_int_const);
		__IDL_tree_free (p);
		break;

	case IDLN_TYPE_FIXED:
		IDL_tree_free (IDL_TYPE_FIXED (p).positive_int_const);
		IDL_tree_free (IDL_TYPE_FIXED (p).integer_lit);
		__IDL_tree_free (p);
		break;

	case IDLN_CASE_STMT:
		IDL_tree_free (IDL_CASE_STMT (p).labels);
		IDL_tree_free (IDL_CASE_STMT (p).element_spec);
		__IDL_tree_free (p);
		break;

	case IDLN_INTERFACE:
		IDL_tree_free (IDL_INTERFACE (p).ident);
		IDL_tree_free (IDL_INTERFACE (p).inheritance_spec);
		IDL_tree_free (IDL_INTERFACE (p).body);
		__IDL_tree_free (p);
		break;

	case IDLN_MODULE:
		IDL_tree_free (IDL_MODULE (p).ident);
		IDL_tree_free (IDL_MODULE (p).definition_list);
		__IDL_tree_free (p);
		break;

	case IDLN_BINOP:
		IDL_tree_free (IDL_BINOP (p).left);
		IDL_tree_free (IDL_BINOP (p).right);
		__IDL_tree_free (p);
		break;

	case IDLN_UNARYOP:
		IDL_tree_free (IDL_UNARYOP (p).operand);
		__IDL_tree_free (p);
		break;

	default:
		g_warning ("Free unknown node: %d\n", IDL_NODE_TYPE (p));
		break;
	}
}

#define C_ESC(a,b)				case a: *p++ = b; ++s; break
gchar *IDL_do_escapes (const char *s)
{
	char *p, *q;

	if (!s)
		return NULL;

	p = q = g_malloc (strlen (s) + 1);

	while (*s) {
		if (*s != '\\') {
			*p++ = *s++;
			continue;
		}
		++s;
		if (*s == 'x') {
			char hex[3];
			int n;
			hex[0] = 0;
			++s;
			sscanf (s, "%2[0-9a-fA-F]", hex);
 			s += strlen (hex);
			sscanf (hex, "%x", &n);
			*p++ = n;
			continue;
		}
		if (*s >= '0' && *s <= '7') {
			char oct[4];
			int n;
			oct[0] = 0;
			sscanf (s, "%3[0-7]", oct);
 			s += strlen (oct);
			sscanf (oct, "%o", &n);
			*p++ = n;
			continue;
		}
		switch (*s) {
			C_ESC ('n','\n');
			C_ESC ('t','\t');
			C_ESC ('v','\v');
			C_ESC ('b','\b');
			C_ESC ('r','\r');
			C_ESC ('f','\f');
			C_ESC ('a','\a');
			C_ESC ('\\','\\');
			C_ESC ('?','?');
			C_ESC ('\'','\'');
			C_ESC ('"','"');
		}
	}
	*p = 0;

	return q;
}

int IDL_list_length (IDL_tree list)
{
	IDL_tree curitem;
	int length;

	for (curitem = list, length = 0; curitem;
	     curitem = IDL_LIST (curitem).next)
		length++;

	return length;
}

IDL_tree IDL_list_nth (IDL_tree list, int n)
{
	IDL_tree curitem;
	int i;

	for (curitem = list, i = 0; i < n && curitem;
	     curitem = IDL_LIST (curitem).next, i++) ;

	return curitem;
}

const char *IDL_tree_property_get (IDL_tree tree, const char *key)
{
	g_return_val_if_fail (tree != NULL, NULL);
	g_return_val_if_fail (key != NULL, NULL);

	if (!IDL_NODE_PROPERTIES (tree))
		return NULL;

	return g_hash_table_lookup (IDL_NODE_PROPERTIES (tree), key);
}

void IDL_tree_property_set (IDL_tree tree, const char *key, const char *value)
{
	g_return_if_fail (tree != NULL);
	g_return_if_fail (key != NULL);

	if (!IDL_NODE_PROPERTIES (tree))
		IDL_NODE_PROPERTIES (tree) = g_hash_table_new (
			IDL_strcase_hash, IDL_strcase_equal);
	else if (IDL_tree_property_get (tree, key))
		IDL_tree_property_remove (tree, key);

	g_hash_table_insert (IDL_NODE_PROPERTIES (tree), g_strdup (key), g_strdup (value));
}

gboolean IDL_tree_property_remove (IDL_tree tree, const char *key)
{
	gboolean removed = FALSE;
	char *val;

	g_return_val_if_fail (tree != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);

	if (!IDL_NODE_PROPERTIES (tree))
		return FALSE;

	if ((val = g_hash_table_lookup (IDL_NODE_PROPERTIES (tree), key))) {
		g_hash_table_remove (IDL_NODE_PROPERTIES (tree), key);
		g_free (val);
		removed = TRUE;
	}

	return removed;
}

static void property_set (char *key, char *value, IDL_tree tree)
{
	IDL_tree_property_set (tree, key, value);
}

void IDL_tree_properties_copy (IDL_tree from_tree, IDL_tree to_tree)
{
	g_return_if_fail (from_tree != NULL);
	g_return_if_fail (to_tree != NULL);

	if (IDL_NODE_PROPERTIES (from_tree))
		g_hash_table_foreach (IDL_NODE_PROPERTIES (from_tree),
				      (GHFunc) property_set, to_tree);
}

typedef struct {
	IDL_tree *root;
	GHashTable *removed_nodes;
} RemoveListNodeData;

static int remove_list_node (IDL_tree p, IDL_tree *list_head, RemoveListNodeData *data)
{
	assert (p != NULL);
	assert (IDL_NODE_TYPE (p) == IDLN_LIST);

	if (list_head)
		*list_head = IDL_list_remove (*list_head, p);
	else
		*data->root = IDL_list_remove (*data->root, p);

	if (data->removed_nodes) {
		if (!g_hash_table_lookup_extended (data->removed_nodes, p, NULL, NULL))
			g_hash_table_insert (data->removed_nodes, p, p);
		/*
		  We shouldn't need this since we have removed it from the tree,
		  but we might need it for multiple declspec (inhibits) in the same
		  subtree.
		  IDL_tree_walk_in_order (p, (IDL_tree_func) inc_node_ref, NULL);
		*/
	} else
		IDL_tree_free (p);

	return TRUE;
}

/* Forward Declaration Resolution */
static int load_forward_dcls (IDL_tree_func_data *tfd, GHashTable *table)
{
	if (IDL_NODE_TYPE (tfd->tree) == IDLN_FORWARD_DCL) {
		char *s = IDL_ns_ident_to_qstring (IDL_FORWARD_DCL (tfd->tree).ident, "::", 0);

		if (!g_hash_table_lookup_extended (table, s, NULL, NULL))
			g_hash_table_insert (table, s, tfd->tree);
		else
			g_free (s);
	}

	return TRUE;
}

static int resolve_forward_dcls (IDL_tree_func_data *tfd, GHashTable *table)
{
	if (IDL_NODE_TYPE (tfd->tree) == IDLN_INTERFACE) {
		char *orig, *s = IDL_ns_ident_to_qstring (IDL_INTERFACE (tfd->tree).ident, "::", 0);

		if (g_hash_table_lookup_extended (table, s, (gpointer)&orig, NULL)) {
			g_hash_table_remove (table, orig);
			g_free (orig);
		}
		g_free (s);
	}

	return TRUE;
}

static int print_unresolved_forward_dcls (char *s, IDL_tree p)
{
	if (__IDL_flags & IDLF_PEDANTIC)
		IDL_tree_error (p, "Unresolved forward declaration `%s'", s);
	else
		IDL_tree_warning (p,
			IDL_WARNING1, "Unresolved forward declaration `%s'", s);
	g_free (s);

	return TRUE;
}

void IDL_tree_process_forward_dcls (IDL_tree *p, IDL_ns ns)
{
	GHashTable *table = g_hash_table_new (IDL_strcase_hash, IDL_strcase_equal);
	gint total, resolved;

	IDL_tree_walk_in_order (*p, (IDL_tree_func) load_forward_dcls, table);
	total = g_hash_table_size (table);
	IDL_tree_walk_in_order (*p, (IDL_tree_func) resolve_forward_dcls, table);
	resolved = total - g_hash_table_size (table);
	g_hash_table_foreach (table, (GHFunc) print_unresolved_forward_dcls, NULL);
	g_hash_table_destroy (table);
	if (__IDL_flags & IDLF_VERBOSE)
		g_message ("Forward declarations resolved: %d of %d", resolved, total);
}

/* Inhibit Creation Removal */
static int load_inhibits (IDL_tree_func_data *tfd, GHashTable *table)
{
	IDL_tree p, q, *list_head;

	p = tfd->tree;

	if (p != NULL &&
	    IDL_NODE_UP (p) &&
	    IDL_NODE_TYPE (IDL_NODE_UP (p)) == IDLN_LIST &&
	    IDL_NODE_DECLSPEC (p) & IDLF_DECLSPEC_INHIBIT &&
	    !g_hash_table_lookup_extended (table, IDL_NODE_UP (p), NULL, NULL)) {

		list_head = NULL;
		q = IDL_NODE_UP (IDL_NODE_UP (p));
		if (q) {
			switch (IDL_NODE_TYPE (q)) {
			case IDLN_MODULE:
				list_head = &IDL_MODULE (q).definition_list;
				break;

			case IDLN_INTERFACE:
				list_head = &IDL_INTERFACE (q).body;
				break;

			default:
				g_warning ("Unhandled node %s in load_inhibits",
					   IDL_NODE_TYPE_NAME (q));
				break;
			}
		}
		g_hash_table_insert (table, IDL_NODE_UP (p), list_head);
	}

	return TRUE;
}

void IDL_tree_remove_inhibits (IDL_tree *p, IDL_ns ns)
{
	RemoveListNodeData data;
	GHashTable *table = g_hash_table_new (g_direct_hash, g_direct_equal);
	gint removed;

	IDL_tree_walk_in_order (*p, (IDL_tree_func) load_inhibits, table);
	removed = g_hash_table_size (table);
	data.root = p;
	data.removed_nodes = IDL_NS (ns).inhibits;
	g_hash_table_foreach (table, (GHFunc) remove_list_node, &data);
	g_hash_table_destroy (table);
	if (__IDL_flags & IDLF_VERBOSE)
		g_message ("Inhibited nodes removed: %d", removed);
}

/* Multi-Pass Empty Module Removal */
static int load_empty_modules (IDL_tree_func_data *tfd, GHashTable *table)
{
	IDL_tree p, q, *list_head;

	p = tfd->tree;

	if (IDL_NODE_TYPE (p) == IDLN_MODULE &&
	    IDL_MODULE (p).definition_list == NULL &&
	    IDL_NODE_UP (p) &&
	    IDL_NODE_TYPE (IDL_NODE_UP (p)) == IDLN_LIST &&
	    !g_hash_table_lookup_extended (table, IDL_NODE_UP (p), NULL, NULL)) {

		list_head = NULL;
		q = IDL_NODE_UP (IDL_NODE_UP (p));
		if (q) {
			assert (IDL_NODE_TYPE (q) == IDLN_MODULE);
			list_head = &IDL_MODULE (q).definition_list;
		}
		g_hash_table_insert (table, IDL_NODE_UP (p), list_head);
	}

	return TRUE;
}

void IDL_tree_remove_empty_modules (IDL_tree *p, IDL_ns ns)
{
	RemoveListNodeData data;
	gboolean done = FALSE;
	gint removed = 0;

	data.root = p;
	data.removed_nodes = NULL;

	while (!done) {
		GHashTable *table = g_hash_table_new (g_direct_hash, g_direct_equal);
		IDL_tree_walk_in_order (*p, (IDL_tree_func) load_empty_modules, table);
		removed += g_hash_table_size (table);
		done = g_hash_table_size (table) == 0;
		g_hash_table_foreach (table, (GHFunc) remove_list_node, &data);
		g_hash_table_destroy (table);
	}
	if (__IDL_flags & IDLF_VERBOSE)
		g_message ("Empty modules removed: %d", removed);
}

/*
 * IDL_tree to IDL backend
 */

#define DELIM_COMMA		", "
#define DELIM_ARRAY		"]["
#define DELIM_SPACE		" "

#define indent()		++data->ilev
#define unindent()		--data->ilev
#define doindent()		do {					\
	int i;								\
	if (!(data->flags & IDLF_OUTPUT_NO_NEWLINES))			\
		for (i = 0; i < data->ilev; ++i) {			\
			switch (data->mode) {				\
			case OUTPUT_FILE:				\
				fputc ('\t', data->u.o);		\
				break;					\
									\
			case OUTPUT_STRING:				\
				g_string_append_c (data->u.s, '\t');	\
				break;					\
									\
			default:					\
				break;					\
			}						\
		}							\
	else if (data->ilev > 0)					\
		dataf (data, DELIM_SPACE);				\
} while (0)
#define nl()			do {				\
	if (!(data->flags & IDLF_OUTPUT_NO_NEWLINES)) {		\
		switch (data->mode) {				\
		case OUTPUT_FILE:				\
			fputc ('\n', data->u.o);		\
			break;					\
								\
		case OUTPUT_STRING:				\
			g_string_append_c (data->u.s, '\n');	\
			break;					\
								\
		default:					\
			break;					\
		}						\
	}							\
} while (0)
#define save_flag(flagbit,val)	do {				\
	tfd->data = GUINT_TO_POINTER (				\
 		GPOINTER_TO_UINT (tfd->data) |			\
 		(data->flagbit ? (1U << flagbit##bit) : 0));	\
	data->flagbit = val;					\
} while (0)
#define restore_flag(flagbit)	do {			\
	data->flagbit = (GPOINTER_TO_UINT (		\
		tfd->data) >> flagbit##bit) & 1U;	\
} while (0)

typedef struct {
	IDL_ns ns;
	enum {
		OUTPUT_FILE,
		OUTPUT_STRING
	} mode;
	union {
		FILE *o;
		GString *s;
	} u;
	int ilev;
	unsigned long flags;

#define identsbit		0
	guint idents : 1;

#define literalsbit		1
	guint literals : 1;

#define inline_propsbit		2
	guint inline_props : 1;

#define su_defbit		3
	guint su_def : 1;
} IDL_output_data;

static void dataf (IDL_output_data *data, const char *fmt, ...)
G_GNUC_PRINTF (2, 3);

static void idataf (IDL_output_data *data, const char *fmt, ...)
G_GNUC_PRINTF (2, 3);

static void dataf (IDL_output_data *data, const char *fmt, ...)
{
	gchar *buffer;
	va_list args;

	va_start (args, fmt);
	switch (data->mode) {
	case OUTPUT_FILE:
		vfprintf (data->u.o, fmt, args);
		break;

	case OUTPUT_STRING:
		buffer = g_strdup_vprintf (fmt, args);
		g_string_append (data->u.s, buffer);
		g_free (buffer);
		break;

	default:
		break;
	}
	va_end (args);
}

static void idataf (IDL_output_data *data, const char *fmt, ...)
{
	gchar *buffer;
	va_list args;

	va_start (args, fmt);
	doindent ();
	switch (data->mode) {
	case OUTPUT_FILE:
		vfprintf (data->u.o, fmt, args);
		break;

	case OUTPUT_STRING:
		buffer = g_strdup_vprintf (fmt, args);
		g_string_append (data->u.s, buffer);
		g_free (buffer);
		break;

	default:
		break;
	}
	va_end (args);
}

static gboolean IDL_emit_node_pre_func (IDL_tree_func_data *tfd, IDL_output_data *data);
static gboolean IDL_emit_node_post_func (IDL_tree_func_data *tfd, IDL_output_data *data);

typedef struct {
	IDL_tree_func pre_func;
	IDL_tree_func post_func;
	IDL_tree_type type, type2;
	gboolean limit;
	IDL_output_data *data;
	const char *delim;
	gboolean hit;
} IDL_output_delim_data;

static gboolean IDL_output_delim_match (IDL_tree_func_data *tfd, IDL_output_delim_data *delim)
{
	return delim->type == IDLN_ANY ||
		IDL_NODE_TYPE (tfd->tree) == delim->type ||
		IDL_NODE_TYPE (tfd->tree) == delim->type2;
}

static gboolean IDL_output_delim_pre (IDL_tree_func_data *tfd, IDL_output_delim_data *delim)
{
	if (IDL_output_delim_match (tfd, delim)) {
		if (delim->hit)
			dataf (delim->data, delim->delim);
		else
			delim->hit = TRUE;
		return delim->pre_func
			? (*delim->pre_func) (tfd, delim->data)
			: TRUE;
	} else {
		if (!delim->limit)
			return delim->pre_func
				? (*delim->pre_func) (tfd, delim->data)
				: TRUE;
		else
			return TRUE;
	}
}

static gboolean IDL_output_delim_post (IDL_tree_func_data *tfd, IDL_output_delim_data *delim)
{
	if (delim->limit && !IDL_output_delim_match (tfd, delim))
		return TRUE;

	return delim->post_func
		? (*delim->post_func) (tfd, delim->data)
		: TRUE;
}

static void IDL_output_delim (IDL_tree p, IDL_tree_func_data *current,
			      IDL_output_data *data,
			      IDL_tree_func pre_func, IDL_tree_func post_func,
			      IDL_tree_type type, IDL_tree_type type2,
			      gboolean limit,
			      const char *str)
{
	IDL_output_delim_data delim;

	delim.pre_func = pre_func;
	delim.post_func = post_func;
	delim.type = type;
	delim.type2 = type2;
	delim.limit = limit;
	delim.data = data;
	delim.hit = FALSE;
	delim.delim = str;

	IDL_tree_walk (p, current,
		       (IDL_tree_func) IDL_output_delim_pre,
		       (IDL_tree_func) IDL_output_delim_post,
		       &delim);
}

typedef struct {
	IDL_output_data *data;
	gboolean hit;
} IDL_property_emit_data;

static void IDL_emit_IDL_property (const char *key, const char *value,
				   IDL_property_emit_data *emit_data)
{
	IDL_output_data *data = emit_data->data;

	if (!emit_data->hit)
		emit_data->hit = TRUE;
	else
		dataf (emit_data->data, DELIM_COMMA);
	if (!data->inline_props) {
		nl ();
		doindent ();
	}
	if (value && *value)
		dataf (emit_data->data, "%s%s(%s)",
		       key, DELIM_SPACE, value);
	else
		dataf (emit_data->data, "%s", key);
}

static gboolean IDL_emit_IDL_properties (IDL_tree p, IDL_output_data *data)
{
	IDL_property_emit_data emit_data;

	if (IDL_NODE_PROPERTIES (p) &&
	    data->flags & IDLF_OUTPUT_PROPERTIES &&
	    g_hash_table_size (IDL_NODE_PROPERTIES (p)) > 0) {
		emit_data.data = data;
		emit_data.hit = FALSE;
		if (!data->inline_props)
			idataf (data, "[" DELIM_SPACE);
		else
			dataf (data, "[");
		indent ();
		g_hash_table_foreach (IDL_NODE_PROPERTIES (p),
				      (GHFunc) IDL_emit_IDL_property,
				      &emit_data);
		unindent ();
		if (!data->inline_props) {
			nl ();
			doindent ();
		}
		dataf (data, "]");
		if (!data->inline_props)
			nl ();
		else
			dataf (data, DELIM_SPACE);
	}

	return TRUE;
}

static gboolean IDL_emit_IDL_sc (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	dataf (data, ";"); nl ();

	return TRUE;
}

static gboolean IDL_emit_IDL_indent (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	doindent ();

	return TRUE;
}

static gboolean IDL_emit_IDL_curly_brace_open (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	dataf (data, "{"); nl (); indent ();

	return TRUE;
}

static gboolean IDL_emit_IDL_curly_brace_close (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	unindent (); idataf (data, "}");

	return TRUE;
}

static gboolean IDL_emit_IDL_curly_brace_close_sc (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	IDL_emit_IDL_curly_brace_close (tfd, data);
	IDL_emit_IDL_sc (tfd, data);

	return TRUE;
}

static gboolean IDL_emit_IDL_ident_real (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	IDL_tree_func_data *up_path;
	IDL_tree up_real, scope;
	char *s;
	int levels;

	up_path = tfd;
	up_real = tfd->tree;
	while (up_path && up_real) {
		if (IDL_NODE_TYPE (up_path->tree) != IDL_NODE_TYPE (up_real))
			break;
		up_path = up_path->up;
		up_real = IDL_NODE_UP (up_real);
	}

	assert (IDL_NODE_TYPE (tfd->tree) == IDLN_IDENT);

	if (!up_real || data->flags & IDLF_OUTPUT_NO_QUALIFY_IDENTS)
		dataf (data, "%s", IDL_IDENT (tfd->tree).str);
	else {
		/* Determine minimal required levels of scoping */
		assert (up_path != NULL);
		scope = up_path->tree ? up_path->tree : up_real;
		levels = IDL_ns_scope_levels_from_here (data->ns, tfd->tree, scope);
		s = IDL_ns_ident_to_qstring (IDL_IDENT_TO_NS (tfd->tree), "::", levels);
		dataf (data, "%s", s);
		g_free (s);
	}

	return TRUE;
}

static gboolean IDL_emit_IDL_ident_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	if (data->idents)
		IDL_emit_IDL_ident_real (tfd, data);

	return TRUE;
}

static gboolean IDL_emit_IDL_ident_force_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	IDL_emit_IDL_ident_real (tfd, data);

	return TRUE;
}

static gboolean IDL_emit_IDL_ident (IDL_tree ident, IDL_tree_func_data *tfd, IDL_output_data *data)
{
	IDL_tree_walk (ident, tfd,
		       (IDL_tree_func) IDL_emit_IDL_ident_real, NULL,
		       data);

	return TRUE;
}

static gboolean IDL_emit_IDL_literal (IDL_tree p, IDL_output_data *data)
{
	switch (IDL_NODE_TYPE (p)) {
	case IDLN_FLOAT:
		dataf (data, "%f", IDL_FLOAT (p).value);
		break;

	case IDLN_INTEGER:
		/* FIXME: sign */
		dataf (data, "%" IDL_LL "d", IDL_INTEGER (p).value);
		break;

	case IDLN_FIXED:
		dataf (data, "%s", IDL_FIXED (p).value);
		break;

	case IDLN_CHAR:
		dataf (data, "'%s'", IDL_CHAR (p).value);
		break;

	case IDLN_WIDE_CHAR:
/*		dataf (data, "'%s'", IDL_WIDE_CHAR (p).value); */
		g_warning ("IDL_emit_IDL_literal: %s is currently unhandled",
			   "Wide character output");
		break;

	case IDLN_BOOLEAN:
		dataf (data, "%s", IDL_BOOLEAN (p).value ? "TRUE" : "FALSE");
		break;

	case IDLN_STRING:
		dataf (data, "\"%s\"", IDL_STRING (p).value);
		break;

	case IDLN_WIDE_STRING:
/*		dataf (data, "\"%s\"", IDL_STRING (p).value); */
		g_warning ("IDL_emit_IDL_literal: %s is currently unhandled",
			   "Wide string output");
		break;

	default:
		g_warning ("Unhandled literal: %s", IDL_NODE_TYPE_NAME (p));
		break;
	}

	return TRUE;
}

static gboolean IDL_emit_IDL_literal_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	if (data->literals)
		IDL_emit_IDL_literal (tfd->tree, data);

	return TRUE;
}

static gboolean IDL_emit_IDL_literal_force_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	IDL_emit_IDL_literal (tfd->tree, data);

	return TRUE;
}

static gboolean IDL_emit_IDL_type_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	IDL_tree p, q;

	p = tfd->tree;

	switch (IDL_NODE_TYPE (p)) {
	case IDLN_IDENT:
		IDL_emit_IDL_ident (p, tfd, data);
		break;

	case IDLN_TYPE_CHAR:
		dataf (data, "char");
		break;

	case IDLN_TYPE_WIDE_CHAR:
		dataf (data, "wchar");
		break;

	case IDLN_TYPE_BOOLEAN:
		dataf (data, "boolean");
		break;

	case IDLN_TYPE_OCTET:
		dataf (data, "octet");
		break;

	case IDLN_TYPE_ANY:
		dataf (data, "any");
		break;

	case IDLN_TYPE_OBJECT:
		dataf (data, "Object");
		break;

	case IDLN_TYPE_TYPECODE:
		dataf (data, "TypeCode");
		break;

	case IDLN_TYPE_FLOAT:
		switch (IDL_TYPE_FLOAT (p).f_type) {
		case IDL_FLOAT_TYPE_FLOAT: dataf (data, "float"); break;
		case IDL_FLOAT_TYPE_DOUBLE: dataf (data, "double"); break;
		case IDL_FLOAT_TYPE_LONGDOUBLE: dataf (data, "long" DELIM_SPACE "double"); break;
		}
		break;

	case IDLN_TYPE_FIXED:
		dataf (data, "fixed<");
		IDL_emit_IDL_literal (IDL_TYPE_FIXED (p).positive_int_const, data);
		dataf (data, DELIM_COMMA);
		IDL_emit_IDL_literal (IDL_TYPE_FIXED (p).integer_lit, data);
		dataf (data, ">");
		return FALSE;

	case IDLN_TYPE_INTEGER:
		if (!IDL_TYPE_INTEGER (p).f_signed)
			dataf (data, "unsigned" DELIM_SPACE);
		switch (IDL_TYPE_INTEGER (p).f_type) {
		case IDL_INTEGER_TYPE_SHORT: dataf (data, "short"); break;
		case IDL_INTEGER_TYPE_LONG: dataf (data, "long"); break;
		case IDL_INTEGER_TYPE_LONGLONG: dataf (data, "long" DELIM_SPACE "long"); break;
		}
		break;

	case IDLN_TYPE_STRING:
	case IDLN_TYPE_WIDE_STRING:
		if (IDL_NODE_TYPE (p) == IDLN_TYPE_STRING) {
			dataf (data, "string");
			q = IDL_TYPE_STRING (p).positive_int_const;
		} else {
			dataf (data, "wstring");
			q = IDL_TYPE_WIDE_STRING (p).positive_int_const;
		}
		if (q) {
			dataf (data, "<");
			if (IDL_NODE_TYPE (p) == IDLN_TYPE_STRING)
				IDL_emit_IDL_literal (
					IDL_TYPE_STRING (p).positive_int_const, data);
			else
				IDL_emit_IDL_literal (
					IDL_TYPE_WIDE_STRING (p).positive_int_const, data);
			dataf (data, ">");
		}
		return FALSE;

	case IDLN_TYPE_ENUM:
		IDL_emit_IDL_indent (tfd, data);
		data->inline_props = TRUE;
		IDL_emit_IDL_properties (IDL_TYPE_ENUM (tfd->tree).ident, data);
		dataf (data, "enum" DELIM_SPACE);
		IDL_emit_IDL_ident (IDL_TYPE_ENUM (p).ident, tfd, data);
		dataf (data, DELIM_SPACE "{" DELIM_SPACE);
		IDL_output_delim (IDL_TYPE_ENUM (p).enumerator_list, tfd, data,
				  (IDL_tree_func) IDL_emit_IDL_ident_force_pre, NULL,
				  IDLN_IDENT, IDLN_NONE, TRUE, DELIM_COMMA);
		dataf (data, DELIM_SPACE "};"); nl ();
		return FALSE;

	case IDLN_TYPE_ARRAY:
		IDL_emit_IDL_ident (IDL_TYPE_ARRAY (p).ident, tfd, data);
		dataf (data, "[");
		IDL_output_delim (IDL_TYPE_ARRAY (p).size_list, tfd, data,
				  (IDL_tree_func) IDL_emit_IDL_literal_force_pre, NULL,
				  IDLN_INTEGER, IDLN_NONE, TRUE, DELIM_ARRAY);
		dataf (data, "]");
		return FALSE;

	case IDLN_TYPE_SEQUENCE:
		dataf (data, "sequence<");
		save_flag (idents, TRUE);
		IDL_tree_walk (IDL_TYPE_SEQUENCE (tfd->tree).simple_type_spec, tfd,
			       (IDL_tree_func) IDL_emit_node_pre_func,
			       (IDL_tree_func) IDL_emit_node_post_func,
			       data);
		restore_flag (idents);
		if (IDL_TYPE_SEQUENCE (tfd->tree).positive_int_const) {
			dataf (data, DELIM_COMMA);
			IDL_emit_IDL_literal (
				IDL_TYPE_SEQUENCE (tfd->tree).positive_int_const, data);
		}
		dataf (data, ">");
		return FALSE;

	case IDLN_TYPE_STRUCT:
		if (!data->su_def)
			doindent ();
		save_flag (su_def, TRUE);
		data->inline_props = TRUE;
		IDL_emit_IDL_properties (IDL_TYPE_STRUCT (tfd->tree).ident, data);
		dataf (data, "struct" DELIM_SPACE);
		IDL_emit_IDL_ident (IDL_TYPE_STRUCT (p).ident, tfd, data);
		dataf (data, DELIM_SPACE);
		IDL_emit_IDL_curly_brace_open (tfd, data);
		IDL_tree_walk (IDL_TYPE_STRUCT (p).member_list, tfd,
			       (IDL_tree_func) IDL_emit_node_pre_func,
			       (IDL_tree_func) IDL_emit_node_post_func,
			       data);
		restore_flag (su_def);
		if (data->su_def)
			IDL_emit_IDL_curly_brace_close (tfd, data);
		else
			IDL_emit_IDL_curly_brace_close_sc (tfd, data);
		return FALSE;

	case IDLN_TYPE_UNION:
		if (!data->su_def)
			doindent ();
		save_flag (su_def, TRUE);
		data->inline_props = TRUE;
		IDL_emit_IDL_properties (IDL_TYPE_UNION (tfd->tree).ident, data);
		dataf (data, "union" DELIM_SPACE);
		IDL_emit_IDL_ident (IDL_TYPE_UNION (p).ident, tfd, data);
		dataf (data, DELIM_SPACE);
		dataf (data, "switch" DELIM_SPACE "(");
		save_flag (idents, TRUE);
		IDL_tree_walk (IDL_TYPE_UNION (p).switch_type_spec, tfd,
			       (IDL_tree_func) IDL_emit_node_pre_func,
			       (IDL_tree_func) IDL_emit_node_post_func,
			       data);
		restore_flag (idents);
		dataf (data, ")" DELIM_SPACE "{"); nl ();
		IDL_tree_walk (IDL_TYPE_UNION (p).switch_body, tfd,
			       (IDL_tree_func) IDL_emit_node_pre_func,
			       (IDL_tree_func) IDL_emit_node_post_func,
			       data);
		restore_flag (su_def);
		if (data->su_def)
			idataf (data, "}");
		else {
			idataf (data, "};");
			nl ();
		}
		return FALSE;

	default:
		break;
	}

	return TRUE;
}

static gboolean IDL_emit_IDL_module_all (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	if (tfd->step == 0) {
		idataf (data, "module" DELIM_SPACE);
		IDL_emit_IDL_ident (IDL_MODULE (tfd->tree).ident, tfd, data);
		dataf (data, DELIM_SPACE);
		IDL_emit_IDL_curly_brace_open (tfd, data);
		save_flag (idents, FALSE);
	} else {
		restore_flag (idents);
		IDL_emit_IDL_curly_brace_close_sc (tfd, data);
	}

	return TRUE;
}

static gboolean IDL_emit_IDL_interface_all (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	if (tfd->step == 0) {
		data->inline_props = FALSE;
		IDL_emit_IDL_properties (IDL_INTERFACE (tfd->tree).ident, data);
		idataf (data, "interface" DELIM_SPACE);
		IDL_emit_IDL_ident (IDL_INTERFACE (tfd->tree).ident, tfd, data);
		dataf (data, DELIM_SPACE);
		if (IDL_INTERFACE (tfd->tree).inheritance_spec) {
			dataf (data, ":" DELIM_SPACE);
			IDL_output_delim (IDL_INTERFACE (tfd->tree).inheritance_spec, tfd, data,
					  (IDL_tree_func) IDL_emit_IDL_ident_force_pre, NULL,
					  IDLN_IDENT, IDLN_NONE, TRUE, DELIM_COMMA);
			dataf (data, DELIM_SPACE);
		}
		IDL_emit_IDL_curly_brace_open (tfd, data);
		save_flag (idents, FALSE);
	} else {
		restore_flag (idents);
		IDL_emit_IDL_curly_brace_close_sc (tfd, data);
	}

	return TRUE;
}

static gboolean IDL_emit_IDL_forward_dcl_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	idataf (data, "interface" DELIM_SPACE);
	IDL_emit_IDL_ident (IDL_FORWARD_DCL (tfd->tree).ident, tfd, data);

	return TRUE;
}

static gboolean IDL_emit_IDL_attr_dcl_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	IDL_emit_IDL_indent (tfd, data);
	data->inline_props = TRUE;
	IDL_emit_IDL_properties (IDL_LIST (IDL_ATTR_DCL (tfd->tree).simple_declarations).data,
				 data);
	if (IDL_ATTR_DCL (tfd->tree).f_readonly) dataf (data, "readonly" DELIM_SPACE);
	dataf (data, "attribute" DELIM_SPACE);
	save_flag (idents, TRUE);
	IDL_tree_walk (IDL_ATTR_DCL (tfd->tree).param_type_spec, tfd,
		       (IDL_tree_func) IDL_emit_node_pre_func,
		       (IDL_tree_func) IDL_emit_node_post_func,
		       data);
	restore_flag (idents);
	dataf (data, DELIM_SPACE);
	IDL_output_delim (IDL_ATTR_DCL (tfd->tree).simple_declarations, tfd, data,
			  (IDL_tree_func) IDL_emit_IDL_ident_force_pre, NULL,
			  IDLN_IDENT, IDLN_NONE, TRUE, DELIM_COMMA);
	IDL_emit_IDL_sc (tfd, data);

	return FALSE;
}

static gboolean IDL_emit_IDL_op_dcl_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	IDL_emit_IDL_indent (tfd, data);
	data->inline_props = TRUE;
	IDL_emit_IDL_properties (IDL_OP_DCL (tfd->tree).ident, data);
	if (IDL_OP_DCL (tfd->tree).f_noscript) dataf (data, "noscript" DELIM_SPACE);
	if (IDL_OP_DCL (tfd->tree).f_oneway) dataf (data, "oneway" DELIM_SPACE);
	if (IDL_OP_DCL (tfd->tree).op_type_spec) {
		save_flag (idents, TRUE);
		IDL_tree_walk (IDL_OP_DCL (tfd->tree).op_type_spec, tfd,
			       (IDL_tree_func) IDL_emit_node_pre_func,
			       (IDL_tree_func) IDL_emit_node_post_func,
			       data);
		restore_flag (idents);
	} else
		dataf (data, "void");
	dataf (data, DELIM_SPACE "%s" DELIM_SPACE "(",
	       IDL_IDENT (IDL_OP_DCL (tfd->tree).ident).str);
	if (IDL_OP_DCL (tfd->tree).parameter_dcls)
		IDL_output_delim (IDL_OP_DCL (tfd->tree).parameter_dcls, tfd, data,
				  (IDL_tree_func) IDL_emit_node_pre_func,
				  (IDL_tree_func) IDL_emit_node_post_func,
				  IDLN_PARAM_DCL, IDLN_NONE, FALSE, DELIM_COMMA);
	if (IDL_OP_DCL (tfd->tree).f_varargs)
		dataf (data, DELIM_COMMA "...");
	dataf (data, ")");
	if (IDL_OP_DCL (tfd->tree).raises_expr) {
		nl (); indent ();
		idataf (data, DELIM_SPACE "raises" DELIM_SPACE "(");
		IDL_output_delim (IDL_OP_DCL (tfd->tree).raises_expr, tfd, data,
				  (IDL_tree_func) IDL_emit_IDL_ident_force_pre, NULL,
				  IDLN_IDENT, IDLN_NONE, TRUE, DELIM_COMMA);
		dataf (data, ")");
		unindent ();
	}
	if (IDL_OP_DCL (tfd->tree).context_expr) {
		nl (); indent ();
		idataf (data, DELIM_SPACE "context" DELIM_SPACE "(");
		IDL_output_delim (IDL_OP_DCL (tfd->tree).context_expr, tfd, data,
				  (IDL_tree_func) IDL_emit_IDL_literal_force_pre, NULL,
				  IDLN_STRING, IDLN_NONE, TRUE, DELIM_COMMA);
		dataf (data, ")");
		unindent ();
	}
	IDL_emit_IDL_sc (tfd, data);

	return FALSE;
}

static gboolean IDL_emit_IDL_param_dcl_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	data->inline_props = TRUE;
	IDL_emit_IDL_properties (IDL_PARAM_DCL (tfd->tree).simple_declarator, data);
	switch (IDL_PARAM_DCL (tfd->tree).attr) {
	case IDL_PARAM_IN: dataf (data, "in" DELIM_SPACE); break;
	case IDL_PARAM_OUT: dataf (data, "out" DELIM_SPACE); break;
	case IDL_PARAM_INOUT: dataf (data, "inout" DELIM_SPACE); break;
	}
	save_flag (idents, TRUE);
	IDL_tree_walk (IDL_PARAM_DCL (tfd->tree).param_type_spec, tfd,
		       (IDL_tree_func) IDL_emit_node_pre_func,
		       (IDL_tree_func) IDL_emit_node_post_func,
		       data);
	restore_flag (idents);
	dataf (data, DELIM_SPACE);
	IDL_emit_IDL_ident (IDL_PARAM_DCL (tfd->tree).simple_declarator, tfd, data);

	return FALSE;
}

static gboolean IDL_emit_IDL_type_dcl_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	IDL_tree_func_data down_tfd;
	IDL_tree q;

	IDL_emit_IDL_indent (tfd, data);
	data->inline_props = TRUE;
	IDL_emit_IDL_properties (IDL_LIST (IDL_TYPE_DCL (tfd->tree).dcls).data, data);
	dataf (data, "typedef" DELIM_SPACE);
	save_flag (idents, TRUE);
	save_flag (su_def, TRUE);
	IDL_tree_walk (IDL_TYPE_DCL (tfd->tree).type_spec, tfd,
		       (IDL_tree_func) IDL_emit_node_pre_func,
		       (IDL_tree_func) IDL_emit_node_post_func,
		       data);
	dataf (data, DELIM_SPACE);
	down_tfd = *tfd;
	down_tfd.up = tfd;
	for (q = IDL_TYPE_DCL (tfd->tree).dcls; q; q = IDL_LIST (q).next) {
		down_tfd.tree = q;
		IDL_tree_walk (IDL_LIST (q).data, &down_tfd,
			       (IDL_tree_func) IDL_emit_node_pre_func,
			       (IDL_tree_func) IDL_emit_node_post_func,
			       data);
		if (IDL_LIST (q).next)
			dataf (data, DELIM_COMMA);
	}
	restore_flag (idents);
	restore_flag (su_def);
	IDL_emit_IDL_sc (tfd, data);

	return FALSE;
}

static gboolean IDL_emit_IDL_const_dcl_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	idataf (data, "const" DELIM_SPACE);
	save_flag (idents, TRUE);
	IDL_tree_walk (IDL_CONST_DCL (tfd->tree).const_type, tfd,
		       (IDL_tree_func) IDL_emit_node_pre_func,
		       (IDL_tree_func) IDL_emit_node_post_func,
		       data);
	restore_flag (idents);
	dataf (data, DELIM_SPACE);
	IDL_emit_IDL_ident (IDL_CONST_DCL (tfd->tree).ident, tfd, data);
	dataf (data, DELIM_SPACE "=" DELIM_SPACE);
	save_flag (literals, TRUE);
	IDL_tree_walk (IDL_CONST_DCL (tfd->tree).const_exp, tfd,
		       (IDL_tree_func) IDL_emit_node_pre_func,
		       (IDL_tree_func) IDL_emit_node_post_func,
		       data);
	restore_flag (literals);
	IDL_emit_IDL_sc (tfd, data);

	return FALSE;
}

static gboolean IDL_emit_IDL_except_dcl_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	idataf (data, "exception" DELIM_SPACE);
	IDL_emit_IDL_ident (IDL_EXCEPT_DCL (tfd->tree).ident, tfd, data);
	dataf (data, DELIM_SPACE);
	IDL_emit_IDL_curly_brace_open (tfd, data);

	return TRUE;
}

static gboolean IDL_emit_IDL_native_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	IDL_emit_IDL_indent (tfd, data);
	data->inline_props = TRUE;
	IDL_emit_IDL_properties (IDL_NATIVE (tfd->tree).ident, data);
	dataf (data, "native" DELIM_SPACE);
	IDL_emit_IDL_ident (IDL_NATIVE (tfd->tree).ident, tfd, data);
	if (IDL_NATIVE (tfd->tree).user_type)
		dataf (data, DELIM_SPACE "(%s)", IDL_NATIVE (tfd->tree).user_type);
	IDL_emit_IDL_sc (tfd, data);

	return TRUE;
}

static gboolean IDL_emit_IDL_case_stmt_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	IDL_tree_func_data down_tfd;
	IDL_tree q;

	save_flag (idents, TRUE);
	save_flag (literals, TRUE);
	down_tfd = *tfd;
	down_tfd.up = tfd;
	for (q = IDL_CASE_STMT (tfd->tree).labels; q; q = IDL_LIST (q).next) {
		if (IDL_LIST (q).data) {
			down_tfd.tree = q;
			idataf (data, "case" DELIM_SPACE);
			IDL_tree_walk (IDL_LIST (q).data, &down_tfd,
				       (IDL_tree_func) IDL_emit_node_pre_func,
				       (IDL_tree_func) IDL_emit_node_post_func,
				       data);
			dataf (data, ":");
		} else
			idataf (data, "default:");
		nl ();
	}
	restore_flag (literals);
	restore_flag (idents);
	indent ();

	return FALSE;
}

static gboolean IDL_emit_IDL_case_stmt_post (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	IDL_tree_walk (IDL_CASE_STMT (tfd->tree).element_spec, tfd,
		       (IDL_tree_func) IDL_emit_node_pre_func,
		       (IDL_tree_func) IDL_emit_node_post_func,
		       data);
	unindent ();

	return FALSE;
}

static gboolean IDL_emit_IDL_member_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	IDL_emit_IDL_indent (tfd, data);
	save_flag (idents, TRUE);
	IDL_tree_walk (IDL_MEMBER (tfd->tree).type_spec, tfd,
		       (IDL_tree_func) IDL_emit_node_pre_func,
		       (IDL_tree_func) IDL_emit_node_post_func,
		       data);
	restore_flag (idents);

	return FALSE;
}

static gboolean IDL_emit_IDL_member_post (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	dataf (data, DELIM_SPACE);
	IDL_output_delim (IDL_MEMBER (tfd->tree).dcls, tfd, data,
			  (IDL_tree_func) IDL_emit_IDL_type_pre, NULL,
			  IDLN_IDENT, IDLN_TYPE_ARRAY, FALSE, DELIM_COMMA);
	IDL_emit_IDL_sc (tfd, data);

	return TRUE;
}

static gboolean IDL_emit_IDL_codefrag_pre (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	if (data->flags & IDLF_OUTPUT_CODEFRAGS) {
		GSList *slist;

		dataf (data, "%%{ %s", IDL_CODEFRAG (tfd->tree).desc); nl ();
		for (slist = IDL_CODEFRAG (tfd->tree).lines; slist; slist = slist->next) {
			dataf (data, "%s", (char *) slist->data); nl ();
		}
		dataf (data, "%%}"); nl ();
	}

	return TRUE;
}

struct IDL_emit_node {
	IDL_tree_func pre;
	IDL_tree_func post;
};

static const struct IDL_emit_node * const IDL_get_IDL_emission_table (void)
{
	static gboolean initialized = FALSE;
	static struct IDL_emit_node table[IDLN_LAST];
	struct IDL_emit_node *s;

	if (!initialized) {

		s = &table[IDLN_MODULE];
		s->pre = s->post = (IDL_tree_func) IDL_emit_IDL_module_all;

		s = &table[IDLN_INTERFACE];
		s->pre = s->post = (IDL_tree_func) IDL_emit_IDL_interface_all;

		s = &table[IDLN_FORWARD_DCL];
		s->pre = (IDL_tree_func) IDL_emit_IDL_forward_dcl_pre;
		s->post = (IDL_tree_func) IDL_emit_IDL_sc;

		s = &table[IDLN_ATTR_DCL];
		s->pre = (IDL_tree_func) IDL_emit_IDL_attr_dcl_pre;

		s = &table[IDLN_OP_DCL];
		s->pre = (IDL_tree_func) IDL_emit_IDL_op_dcl_pre;

		s = &table[IDLN_PARAM_DCL];
		s->pre = (IDL_tree_func) IDL_emit_IDL_param_dcl_pre;

		s = &table[IDLN_TYPE_DCL];
		s->pre = (IDL_tree_func) IDL_emit_IDL_type_dcl_pre;

		s = &table[IDLN_CONST_DCL];
		s->pre = (IDL_tree_func) IDL_emit_IDL_const_dcl_pre;

		s = &table[IDLN_EXCEPT_DCL];
		s->pre = (IDL_tree_func) IDL_emit_IDL_except_dcl_pre;
		s->post = (IDL_tree_func) IDL_emit_IDL_curly_brace_close_sc;

		s = &table[IDLN_MEMBER];
		s->pre = (IDL_tree_func) IDL_emit_IDL_member_pre;
		s->post = (IDL_tree_func) IDL_emit_IDL_member_post;

		s = &table[IDLN_NATIVE];
		s->pre = (IDL_tree_func) IDL_emit_IDL_native_pre;

		s = &table[IDLN_CASE_STMT];
		s->pre = (IDL_tree_func) IDL_emit_IDL_case_stmt_pre;
		s->post = (IDL_tree_func) IDL_emit_IDL_case_stmt_post;

		s = &table[IDLN_IDENT];
		s->pre = (IDL_tree_func) IDL_emit_IDL_ident_pre;

		s = &table[IDLN_CODEFRAG];
		s->pre = (IDL_tree_func) IDL_emit_IDL_codefrag_pre;

		table[IDLN_TYPE_FLOAT].pre =
			table[IDLN_TYPE_CHAR].pre =
			table[IDLN_TYPE_WIDE_CHAR].pre =
			table[IDLN_TYPE_BOOLEAN].pre =
			table[IDLN_TYPE_OCTET].pre =
			table[IDLN_TYPE_ANY].pre =
			table[IDLN_TYPE_OBJECT].pre =
			table[IDLN_TYPE_TYPECODE].pre =
			table[IDLN_TYPE_FIXED].pre =
			table[IDLN_TYPE_INTEGER].pre =
			table[IDLN_TYPE_STRING].pre =
			table[IDLN_TYPE_WIDE_STRING].pre =
			table[IDLN_TYPE_ENUM].pre =
			table[IDLN_TYPE_ARRAY].pre =
			table[IDLN_TYPE_SEQUENCE].pre =
			table[IDLN_TYPE_STRUCT].pre =
			table[IDLN_TYPE_UNION].pre = (IDL_tree_func) IDL_emit_IDL_type_pre;

		table[IDLN_FLOAT].pre =
			table[IDLN_INTEGER].pre =
			table[IDLN_FIXED].pre =
			table[IDLN_CHAR].pre =
			table[IDLN_WIDE_CHAR].pre =
			table[IDLN_BOOLEAN].pre =
			table[IDLN_STRING].pre =
			table[IDLN_WIDE_STRING].pre = (IDL_tree_func) IDL_emit_IDL_literal_pre;

		initialized = TRUE;
	}

	return table;
}

static gboolean IDL_emit_node_pre_func (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	const struct IDL_emit_node * const s =
		&IDL_get_IDL_emission_table () [IDL_NODE_TYPE (tfd->tree)];

	if (s->pre)
		return (*s->pre) (tfd, data);

	return TRUE;
}

static gboolean IDL_emit_node_post_func (IDL_tree_func_data *tfd, IDL_output_data *data)
{
	const struct IDL_emit_node * const s =
		&IDL_get_IDL_emission_table () [IDL_NODE_TYPE (tfd->tree)];

	if (s->post)
		return (*s->post) (tfd, data);

	return TRUE;
}

void IDL_tree_to_IDL (IDL_tree p, IDL_ns ns, FILE *output, unsigned long output_flags)
{
	IDL_output_data data;

	g_return_if_fail (output != NULL);

	data.ns = ns;
	data.mode = OUTPUT_FILE;
	data.u.o = output;
	data.flags = output_flags;
	data.ilev = 0;
	data.idents = TRUE;
	data.literals = TRUE;
	data.inline_props = TRUE;
	data.su_def = FALSE;

	if (ns == NULL)
		data.flags |= IDLF_OUTPUT_NO_QUALIFY_IDENTS;

	IDL_tree_walk (p, NULL,
		       (IDL_tree_func) IDL_emit_node_pre_func,
		       (IDL_tree_func) IDL_emit_node_post_func,
		       &data);
}

GString *IDL_tree_to_IDL_string (IDL_tree p, IDL_ns ns, unsigned long output_flags)
{
	IDL_output_data data;

	data.ns = ns;
	data.mode = OUTPUT_STRING;
	data.u.s = g_string_new (NULL);
	data.flags = output_flags;
	data.ilev = 0;
	data.idents = TRUE;
	data.literals = TRUE;
	data.inline_props = TRUE;
	data.su_def = FALSE;

	if (ns == NULL)
		data.flags |= IDLF_OUTPUT_NO_QUALIFY_IDENTS;

	IDL_tree_walk (p, NULL,
		       (IDL_tree_func) IDL_emit_node_pre_func,
		       (IDL_tree_func) IDL_emit_node_post_func,
		       &data);

	return data.u.s;
}

/*
 * Local variables:
 * mode: C
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 */
