/***************************************************************************

    ns.c (libIDL namespace functions)

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

    $Id: ns.c,v 1.1 1999/05/06 15:05:06 beard%netscape.com Exp $

***************************************************************************/
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "rename.h"
#include "util.h"

static int is_inheritance_conflict (IDL_tree p);

IDL_ns IDL_ns_new (void)
{
	IDL_ns ns;

	ns = g_new0 (struct _IDL_ns, 1);
	if (ns == NULL) {
		yyerror ("IDL_ns_new: memory exhausted");
		return NULL;
	}

	IDL_NS (ns).global = IDL_gentree_new (IDL_ident_hash,
					      IDL_ident_equal,
					      IDL_ident_new (""));
	IDL_NS (ns).file =  IDL_NS (ns).current = IDL_NS (ns).global;
	IDL_NS (ns).inhibits = g_hash_table_new (g_direct_hash, g_direct_equal);
	IDL_NS (ns).filename_hash = g_hash_table_new (g_str_hash, g_str_equal);

	return ns;
}

static void filename_hash_free (char *filename, IDL_fileinfo *fi)
{
	g_free (filename);
	g_free (fi);
}

void IDL_ns_free (IDL_ns ns)
{
	assert (ns != NULL);

	g_hash_table_foreach (IDL_NS (ns).inhibits, (GHFunc)IDL_tree_free, NULL);
	g_hash_table_destroy (IDL_NS (ns).inhibits);
	g_hash_table_foreach (IDL_NS (ns).filename_hash, (GHFunc) filename_hash_free, NULL);
	g_hash_table_destroy (IDL_NS (ns).filename_hash);
	IDL_tree_free (IDL_NS (ns).global);

	g_free (ns);
}

#define IDL_NS_ASSERTS		do {						\
	assert (ns != NULL);							\
	if (__IDL_is_parsing) {							\
		assert (IDL_NS (ns).global != NULL);				\
		assert (IDL_NS (ns).file != NULL);				\
		assert (IDL_NS (ns).current != NULL);				\
		assert (IDL_NODE_TYPE (IDL_NS (ns).global) == IDLN_GENTREE);	\
		assert (IDL_NODE_TYPE (IDL_NS (ns).file) == IDLN_GENTREE);	\
		assert (IDL_NODE_TYPE (IDL_NS (ns).current) == IDLN_GENTREE);	\
	}									\
} while (0)

int IDL_ns_prefix (IDL_ns ns, const char *s)
{
	char *r;
	int l;

	IDL_NS_ASSERTS;

	if (s == NULL)
		return FALSE;

	if (*s == '"')
		r = g_strdup (s + 1);
	else
		r = g_strdup (s);

	l = strlen (r);
	if (l && r[l - 1] == '"')
		r[l - 1] = 0;

	if (IDL_GENTREE (IDL_NS (ns).current)._cur_prefix)
		g_free (IDL_GENTREE (IDL_NS (ns).current)._cur_prefix);

	IDL_GENTREE (IDL_NS (ns).current)._cur_prefix = r;

	return TRUE;
}

IDL_tree IDL_ns_resolve_this_scope_ident (IDL_ns ns, IDL_tree scope, IDL_tree ident)
{
	IDL_tree p, q;

	IDL_NS_ASSERTS;

	p = scope;

	while (p != NULL) {
		q = IDL_ns_lookup_this_scope (ns, p, ident, NULL);
		if (q != NULL)
			return q;
		p = IDL_NODE_UP (p);
	}

	return p;
}

IDL_tree IDL_ns_resolve_ident (IDL_ns ns, IDL_tree ident)
{
	return IDL_ns_resolve_this_scope_ident (ns, IDL_NS (ns).current, ident);
}

IDL_tree IDL_ns_lookup_this_scope (IDL_ns ns, IDL_tree scope, IDL_tree ident, gboolean *conflict)
{
	IDL_tree p, q;

	IDL_NS_ASSERTS;

	if (conflict)
		*conflict = TRUE;

	if (scope == NULL)
		return NULL;

	assert (IDL_NODE_TYPE (scope) == IDLN_GENTREE);

	/* Search this namespace */
	if (g_hash_table_lookup_extended (
		IDL_GENTREE (scope).children, ident, NULL, (gpointer)&p)) {
		assert (IDL_GENTREE (p).data != NULL);
		assert (IDL_NODE_TYPE (IDL_GENTREE (p).data) == IDLN_IDENT);
		return p;
	}

	/* If there are inherited namespaces, look in those before giving up */
	q = IDL_GENTREE (scope)._import;
	if (!q)
		return NULL;

	assert (IDL_NODE_TYPE (q) == IDLN_LIST);
	for (; q != NULL; q = IDL_LIST (q).next) {
		IDL_tree r;

		assert (IDL_LIST (q).data != NULL);
		assert (IDL_NODE_TYPE (IDL_LIST (q).data) == IDLN_IDENT);
		assert (IDL_IDENT_TO_NS (IDL_LIST (q).data) != NULL);
		assert (IDL_NODE_TYPE (IDL_IDENT_TO_NS (IDL_LIST (q).data)) == IDLN_GENTREE);

		/* Search imported namespace scope q */
		if (g_hash_table_lookup_extended (
			IDL_GENTREE (IDL_IDENT_TO_NS (IDL_LIST (q).data)).children,
			ident, NULL, (gpointer)&p)) {
			assert (IDL_GENTREE (p).data != NULL);
			assert (IDL_NODE_TYPE (IDL_GENTREE (p).data) == IDLN_IDENT);

			/* This needs more work, it won't do full ambiguity detection */
			if (conflict && !is_inheritance_conflict (p))
				*conflict = FALSE;

			return p;
		}

		/* Search up one level */
		if (IDL_NODE_TYPE (IDL_NODE_UP (IDL_LIST (q).data)) == IDLN_INTERFACE &&
		    (r = IDL_ns_lookup_this_scope (
			    ns, IDL_IDENT_TO_NS (IDL_LIST (q).data), ident, conflict)))
			return r;
	}

	return NULL;
}

IDL_tree IDL_ns_lookup_cur_scope (IDL_ns ns, IDL_tree ident, gboolean *conflict)
{
	return IDL_ns_lookup_this_scope (ns, IDL_NS (ns).current, ident, conflict);
}

IDL_tree IDL_ns_place_new (IDL_ns ns, IDL_tree ident)
{
	IDL_tree p, up_save;
	gboolean does_conflict;

	IDL_NS_ASSERTS;

	p = IDL_ns_lookup_cur_scope (ns, ident, &does_conflict);
	if (p != NULL && does_conflict)
		return NULL;

	/* The namespace tree is separate from the primary parse tree,
	   so keep the primary tree node's parent the same */
	up_save = IDL_NODE_UP (ident);
	p = IDL_gentree_chain_child (IDL_NS (ns).current, ident);
	IDL_NODE_UP (ident) = up_save;

	if (p == NULL)
		return NULL;

	assert (IDL_NODE_TYPE (p) == IDLN_GENTREE);

	IDL_IDENT_TO_NS (ident) = p;

	assert (IDL_NODE_UP (IDL_IDENT_TO_NS (ident)) == IDL_NS (ns).current);

	/* Generate default repository ID */
	IDL_IDENT_REPO_ID (ident) =
		IDL_ns_ident_make_repo_id (__IDL_root_ns, p, NULL, NULL, NULL);

	return p;
}

void IDL_ns_push_scope (IDL_ns ns, IDL_tree ns_ident)
{
	IDL_NS_ASSERTS;

	assert (IDL_NODE_TYPE (ns_ident) == IDLN_GENTREE);
	assert (IDL_NODE_TYPE (IDL_GENTREE (ns_ident).data) == IDLN_IDENT);
	assert (IDL_NS (ns).current == IDL_NODE_UP (ns_ident));

	IDL_NS (ns).current = ns_ident;
}

void IDL_ns_pop_scope (IDL_ns ns)
{
	IDL_NS_ASSERTS;

	if (IDL_NODE_UP (IDL_NS (ns).current) != NULL)
		IDL_NS (ns).current = IDL_NODE_UP (IDL_NS (ns).current);
}

IDL_tree IDL_ns_qualified_ident_new (IDL_tree nsid)
{
	IDL_tree l = NULL, item;

	while (nsid != NULL) {
		if (IDL_GENTREE (nsid).data == NULL) {
			nsid = IDL_NODE_UP (nsid);
			continue;
		}
		assert (IDL_GENTREE (nsid).data != NULL);
		assert (IDL_NODE_TYPE (IDL_GENTREE (nsid).data) == IDLN_IDENT);
		item = IDL_list_new (IDL_ident_new (
			g_strdup (IDL_IDENT (IDL_GENTREE (nsid).data).str)));
		l = IDL_list_concat (item, l);
		nsid = IDL_NODE_UP (nsid);
	}

	return l;
}

gchar *IDL_ns_ident_to_qstring (IDL_tree ns_ident, const char *join, int levels)
{
	IDL_tree l, q;
	int len, joinlen;
	char *s;
	int count = 0, start_level;

	if (levels < 0 || levels > 64)
		return NULL;

	if (ns_ident == NULL)
		return NULL;

	if (IDL_NODE_TYPE (ns_ident) == IDLN_IDENT)
		ns_ident = IDL_IDENT_TO_NS (ns_ident);

	assert (IDL_NODE_TYPE (ns_ident) == IDLN_GENTREE);

	l = IDL_ns_qualified_ident_new (ns_ident);

	if (l == NULL)
		return NULL;

	if (join == NULL)
		join = "";

	joinlen = strlen (join);
	for (len = 0, q = l; q != NULL; q = IDL_LIST (q).next) {
		IDL_tree i = IDL_LIST (q).data;
		assert (IDL_NODE_TYPE (q) == IDLN_LIST);
		assert (IDL_NODE_TYPE (i) == IDLN_IDENT);
		if (IDL_IDENT (i).str != NULL)
			len += strlen (IDL_IDENT (i).str) + joinlen;
		++count;
	}

	if (levels == 0)
		start_level = 0;
	else
		start_level = count - levels;

	assert (start_level >= 0 && start_level < count);

	s = g_malloc (len + 1);
	if (s == NULL) {
		IDL_tree_free (l);
		return NULL;
	}
	s[0] = '\0';
	for (q = l; q != NULL; q = IDL_LIST (q).next) {
		IDL_tree i = IDL_LIST (q).data;
		if (start_level > 0) {
			--start_level;
			continue;
		}
		if (s[0] != '\0')
			strcat (s, join);
		strcat (s, IDL_IDENT (i).str);
	}

	IDL_tree_free (l);

	return s;
}

int IDL_ns_scope_levels_from_here (IDL_ns ns, IDL_tree ident, IDL_tree parent)
{
	IDL_tree p, scope_here, scope_ident;
	int levels;

	g_return_val_if_fail (ns != NULL, 1);
	g_return_val_if_fail (ident != NULL, 1);

	while (parent && !IDL_NODE_IS_SCOPED (parent))
		parent = IDL_NODE_UP (parent);

	if (parent == NULL)
		return 1;

	if ((scope_here = IDL_tree_get_scope (parent)) == NULL ||
	    (scope_ident = IDL_tree_get_scope (ident)) == NULL)
		return 1;

	assert (IDL_NODE_TYPE (scope_here) == IDLN_GENTREE);
	assert (IDL_NODE_TYPE (scope_ident) == IDLN_GENTREE);

	for (levels = 1; scope_ident;
	     ++levels, scope_ident = IDL_NODE_UP (scope_ident)) {
		p = IDL_ns_resolve_this_scope_ident (
			ns, scope_here, IDL_GENTREE (scope_ident).data);
		if (p == scope_ident)
			return levels;
	}

	return 1;
}

/* If insertion was made, return true, else there was a collision */
static gboolean heap_insert_ident (IDL_tree interface_ident, GTree *heap, IDL_tree any)
{
	IDL_tree p;

	assert (any != NULL);
	assert (heap != NULL);

	if ((p = g_tree_lookup (heap, any))) {
		char *newi;
		char *i1, *i2;
		char *what1 = "identifier", *what2 = what1;
		char *who1, *who2;
		IDL_tree q;

		assert (IDL_NODE_TYPE (p) == IDLN_IDENT);

		newi = IDL_ns_ident_to_qstring (IDL_IDENT_TO_NS (interface_ident), "::", 0);
		i1 = IDL_ns_ident_to_qstring (IDL_IDENT_TO_NS (p), "::", 0);
		i2 = IDL_ns_ident_to_qstring (IDL_IDENT_TO_NS (any), "::", 0);

		q = p;
		while (q && (IDL_NODE_TYPE (q) == IDLN_IDENT || IDL_NODE_TYPE (q) == IDLN_LIST))
			q = IDL_NODE_UP (q);
		assert (q != NULL);
		IDL_tree_get_node_info (q, &what1, &who1);

		q = any;
		while (q && (IDL_NODE_TYPE (q) == IDLN_IDENT || IDL_NODE_TYPE (q) == IDLN_LIST))
			q = IDL_NODE_UP (q);
		assert (q != NULL);
		IDL_tree_get_node_info (q, &what2, &who2);

		yyerrorv ("Ambiguous inheritance in interface `%s' from %s `%s' and %s `%s'",
			  newi, what1, i1, what2, i2);
		IDL_tree_error (p, "%s `%s' conflicts with", what1, i1);
		IDL_tree_error (any, "%s `%s'", what2, i2);

		g_free (newi); g_free (i1); g_free (i2);

		return FALSE;
	}

	g_tree_insert (heap, any, any);

	return TRUE;
}

static int is_visited_interface (GHashTable *visited_interfaces, IDL_tree scope)
{
	assert (scope != NULL);
	assert (IDL_NODE_TYPE (scope) == IDLN_GENTREE);
	/* If already visited, do not visit again */
	return g_hash_table_lookup_extended (visited_interfaces, scope, NULL, NULL);
}

static void mark_visited_interface (GHashTable *visited_interfaces, IDL_tree scope)
{
	assert (scope != NULL);
	assert (IDL_NODE_TYPE (scope) == IDLN_GENTREE);
	g_hash_table_insert (visited_interfaces, scope, scope);
}

static int is_inheritance_conflict (IDL_tree p)
{
	if (IDL_GENTREE (p).data == NULL)
		return FALSE;

	assert (IDL_NODE_TYPE (IDL_GENTREE (p).data) == IDLN_IDENT);

	if (IDL_NODE_UP (IDL_GENTREE (p).data) == NULL)
		return FALSE;

	if (!(IDL_NODE_TYPE (IDL_NODE_UP (IDL_GENTREE (p).data)) == IDLN_OP_DCL ||
	      (IDL_NODE_UP (IDL_GENTREE (p).data) &&
	       IDL_NODE_TYPE (IDL_NODE_UP (IDL_NODE_UP (IDL_GENTREE (p).data))) == IDLN_ATTR_DCL)))
		return FALSE;

	return TRUE;
}

typedef struct {
	IDL_tree interface_ident;
	GTree *ident_heap;
	int insert_conflict;
} InsertHeapData;

static void insert_heap_cb (IDL_tree ident, IDL_tree p, InsertHeapData *data)
{
	if (!is_inheritance_conflict (p))
		return;

	if (!heap_insert_ident (
		data->interface_ident, data->ident_heap, IDL_GENTREE (p).data))
		data->insert_conflict = 1;
}

/* Return true if adds went okay */
static int IDL_ns_load_idents_to_tables (IDL_tree interface_ident, IDL_tree ident_scope,
					 GTree *ident_heap, GHashTable *visited_interfaces)
{
	IDL_tree q, scope;
	InsertHeapData data;

	assert (ident_scope != NULL);
	assert (IDL_NODE_TYPE (ident_scope) == IDLN_IDENT);

	scope = IDL_IDENT_TO_NS (ident_scope);

	if (!scope)
		return TRUE;

	assert (IDL_NODE_TYPE (scope) == IDLN_GENTREE);
	assert (IDL_GENTREE (scope).data != NULL);
	assert (IDL_NODE_TYPE (IDL_GENTREE (scope).data) == IDLN_IDENT);
	assert (IDL_NODE_UP (IDL_GENTREE (scope).data) != NULL);
	assert (IDL_NODE_TYPE (IDL_NODE_UP (IDL_GENTREE (scope).data)) == IDLN_INTERFACE);

	if (is_visited_interface (visited_interfaces, scope))
		return TRUE;

	/* Search this namespace */
	data.interface_ident = interface_ident;
	data.ident_heap = ident_heap;
	data.insert_conflict = 0;
	g_hash_table_foreach (IDL_GENTREE (scope).children, (GHFunc)insert_heap_cb, &data);

	/* If there are inherited namespaces, look in those before giving up */
	q = IDL_GENTREE (scope)._import;
	if (!q)
		data.insert_conflict = 0;
	else
		assert (IDL_NODE_TYPE (q) == IDLN_LIST);

	/* Add inherited namespace identifiers into heap */
	for (; q != NULL; q = IDL_LIST (q).next) {
		int r;

		assert (IDL_LIST (q).data != NULL);
		assert (IDL_NODE_TYPE (IDL_LIST (q).data) == IDLN_IDENT);
		assert (IDL_IDENT_TO_NS (IDL_LIST (q).data) != NULL);
		assert (IDL_NODE_TYPE (IDL_IDENT_TO_NS (IDL_LIST (q).data)) == IDLN_GENTREE);
		assert (IDL_NODE_TYPE (IDL_NODE_UP (IDL_LIST (q).data)) == IDLN_INTERFACE);

		if (!(r = IDL_ns_load_idents_to_tables (interface_ident, IDL_LIST (q).data,
							ident_heap, visited_interfaces)))
			data.insert_conflict = 1;
	}

	mark_visited_interface (visited_interfaces, scope);

	return data.insert_conflict == 0;
}

int IDL_ns_check_for_ambiguous_inheritance (IDL_tree interface_ident, IDL_tree p)
{
	/* We use a sorted heap to check for namespace collisions,
	   since we must do case-insensitive collision checks.
	   visited_interfaces is a hash of visited interface nodes, so
	   we only visit common ancestors once. */
	GTree *ident_heap;
	GHashTable *visited_interfaces;
	int is_ambiguous = 0;

	if (!p)
		return 0;

	ident_heap = g_tree_new (IDL_ident_cmp);
	visited_interfaces = g_hash_table_new (g_direct_hash, g_direct_equal);

	assert (IDL_NODE_TYPE (p) == IDLN_LIST);
	for (; p;  p = IDL_LIST (p).next) {
		if (!IDL_ns_load_idents_to_tables (interface_ident, IDL_LIST (p).data,
						   ident_heap, visited_interfaces))
			is_ambiguous = 1;
	}

	g_tree_destroy (ident_heap);
	g_hash_table_destroy (visited_interfaces);

	return is_ambiguous;
}

/*
 * Local variables:
 * mode: C
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 */
