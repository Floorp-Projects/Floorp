/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * Generate XPCOM headers from XPIDL.
 */

#include "xpidl.h"

/* is this node from an aggregate type (interface)? */
#define UP_IS_AGGREGATE(node)						      \
    (IDL_NODE_UP(node) &&						      \
     IDL_NODE_TYPE(IDL_NODE_UP(node)) == IDLN_INTERFACE)

/* is this type output in the form "<foo> *"? */
#define STARRED_TYPE(node) (IDL_NODE_TYPE(node) == IDLN_TYPE_STRING ||	      \
			    IDL_NODE_TYPE(node) == IDLN_TYPE_WIDE_STRING ||   \
			    (IDL_NODE_TYPE(node) == IDLN_IDENT &&	      \
			     UP_IS_AGGREGATE(node)) )

static gboolean
ident(TreeState *state)
{
    printf("%s", IDL_IDENT(state->tree).str);
    return TRUE;
}
static gboolean
add_interface_ref_maybe(IDL_tree p, gpointer user_data)
{
    GHashTable *hash = (GHashTable *)user_data;
    if (IDL_NODE_TYPE(p) == IDLN_IDENT &&
	!g_hash_table_lookup(hash, IDL_IDENT(p).str))
	g_hash_table_insert(hash, IDL_IDENT(p).str, "FOUND");
}

static gboolean
find_interface_refs(IDL_tree p, gpointer user_data)
{
    IDL_tree node;
    switch(IDL_NODE_TYPE(p)) {
      case IDLN_ATTR_DCL:
	node = IDL_ATTR_DCL(p).param_type_spec;
	break;
      case IDLN_OP_DCL:
	/*
	IDL_tree_walk_in_order(IDL_OP_DCL(p).parameter_dcls, generate_includes,
			       user_data);
	*/
	node = IDL_OP_DCL(p).op_type_spec;
	break;
      case IDLN_PARAM_DCL:
	node = IDL_PARAM_DCL(p).param_type_spec;
	break;
      case IDLN_INTERFACE:
	node = IDL_INTERFACE(p).inheritance_spec;
	if (node)
	    xpidl_list_foreach(node, add_interface_ref_maybe, user_data);
	node = NULL;
	break;
      default:
	node = NULL;
    }
    if (node && IDL_NODE_TYPE(node) == IDLN_IDENT)
	add_interface_ref_maybe(node, user_data);
    return TRUE;
}

static void
write_header(gpointer key, gpointer value, gpointer user_data)
{
    char *ident = (char *)key;
    TreeState *state = (TreeState *)user_data;
    fprintf(state->file, "#include \"%s.h\" /* interface %s */\n",
	    ident, ident);
}

static gboolean
pass_1(TreeState *state)
{
    GHashTable *hash = g_hash_table_new(g_str_hash, g_str_equal);
    if (!hash)
	return FALSE;
    fputs("#include \"nscore.h\"\n", state->file);
    IDL_tree_walk_in_order(state->tree, find_interface_refs, hash);
    g_hash_table_foreach(hash, write_header, state);
    g_hash_table_destroy(hash);
    return TRUE;
}

static gboolean
interface(TreeState *state)
{
    IDL_tree iface = state->tree, iter;

    char *className =
	IDL_ns_ident_to_qstring(IDL_IDENT_TO_NS(IDL_INTERFACE(iface).ident),
				"_", 0);
    fprintf(state->file, "\n/* starting interface %s */\n",
	    className);

    fputs("class ", state->file);
    state->tree = IDL_INTERFACE(iface).ident;
    if (!ident(state))
	return FALSE;

    if ((iter = IDL_INTERFACE(iface).inheritance_spec)) {
	fputs(" : ", state->file);
	for (; iter; iter = IDL_LIST(iter).next) {
	    state->tree = IDL_LIST(iter).data;
	    fputs("public ", state->file);
	    if (!ident(state))
		return FALSE;
	    if (IDL_LIST(iter).next)
		fputs(", ", state->file);
	}
    }
    fputs(" {\n", state->file);
    state->tree = IDL_INTERFACE(iface).body;

    if (!process_node(state))
	return FALSE;

    fprintf(state->file, "\n};\n");

    return TRUE;
}

static gboolean
list(TreeState *state)
{
    IDL_tree iter;
    for (iter = state->tree; iter; iter = IDL_LIST(iter).next) {
	state->tree = IDL_LIST(iter).data;
	if (!process_node(state))
	    return FALSE;
    }
    return TRUE;
}

static gboolean
xpcom_type(TreeState *state)
{
    if (!state->tree) {
	fputs("void", state->file);
	return TRUE;
    }

    switch(IDL_NODE_TYPE(state->tree)) {
      case IDLN_TYPE_INTEGER: {
	  gboolean sign = IDL_TYPE_INTEGER(state->tree).f_signed;
	  switch (IDL_TYPE_INTEGER(state->tree).f_type) {
	    case IDL_INTEGER_TYPE_SHORT:
	      fputs(sign ? "PRInt16" : "PRUint16", state->file);
	      break;
	    case IDL_INTEGER_TYPE_LONG:
	      fputs(sign ? "PRInt32" : "PRUint32", state->file);
	      break;
	    case IDL_INTEGER_TYPE_LONGLONG:
	      fputs(sign ? "PRInt64" : "PRUint64", state->file);
	      break;
	    default:
	      g_error("Unknown integer type %d\n",
		      IDL_TYPE_INTEGER(state->tree).f_type);
	      return FALSE;
	  }
	  break;
      }
      case IDLN_TYPE_CHAR:
	fputs("char", state->file);
	break;
      case IDLN_TYPE_WIDE_CHAR:
	fputs("PRUint16", state->file);	/* wchar_t? */
	break;
      case IDLN_TYPE_WIDE_STRING:
	fputs("PRUnichar *", state->file);
	break;
      case IDLN_TYPE_STRING:
	fputs("char *", state->file);
	break;
      case IDLN_TYPE_BOOLEAN:
	fputs("PRBool", state->file);
	break;
      case IDLN_IDENT:
	fputs(IDL_IDENT(state->tree).str, state->file);
	if (UP_IS_AGGREGATE(state->tree))
	    fputs(" *", state->file);
	break;
      default:
	fprintf(state->file, "unknown_type_%d", IDL_NODE_TYPE(state->tree));
	break;
    }
    return TRUE;
}

static gboolean
type_integer(TreeState *state)
{
    IDL_tree p = state->tree;

    if (!IDL_TYPE_INTEGER(p).f_signed)
	fputs("unsigned ", state->file);

    switch(IDL_TYPE_INTEGER(p).f_type) {
      case IDL_INTEGER_TYPE_SHORT:
	printf("short");
	break;
      case IDL_INTEGER_TYPE_LONG:
	printf("long");
	break;
      case IDL_INTEGER_TYPE_LONGLONG:
	printf("long long");
	break;
    }
    return TRUE;
}

static gboolean
type(TreeState *state)
{
    if (!state->tree) {
	fputs("void", state->file);
	return TRUE;
    }

    switch(IDL_NODE_TYPE(state->tree)) {
      case IDLN_TYPE_INTEGER:
	return type_integer(state);
      case IDLN_TYPE_STRING:
	fputs("string", state->file);
	return TRUE;
      case IDLN_TYPE_WIDE_STRING:
	fputs("wstring", state->file);
	return TRUE;
      case IDLN_TYPE_CHAR:
	fputs("char", state->file);
	return TRUE;
      case IDLN_TYPE_WIDE_CHAR:
	fputs("wchar", state->file);
	return TRUE;
      case IDLN_TYPE_BOOLEAN:
	fputs("boolean", state->file);
	return TRUE;
      case IDLN_IDENT:
	fputs(IDL_IDENT(state->tree).str, state->file);
	break;
      default:
	fprintf(state->file, "unknown_type_%d", IDL_NODE_TYPE(state->tree));
	return TRUE;
    }
}

static gboolean
param_dcls(TreeState *state)
{
    IDL_tree iter;
    fputs("(", state->file);
    for (iter = state->tree; iter; iter = IDL_LIST(iter).next) {
	struct _IDL_PARAM_DCL decl = IDL_PARAM_DCL(IDL_LIST(iter).data);
	switch(decl.attr) {
	  case IDL_PARAM_IN:
	    fputs("in ", state->file);
	    break;
	  case IDL_PARAM_OUT:
	    fputs("out ", state->file);
	    break;
	  case IDL_PARAM_INOUT:
	    fputs("inout ", state->file);
	    break;
	  default:;
	}
	state->tree = (IDL_tree)decl.param_type_spec;
	if (!type(state))
	    return FALSE;
	fputs(" ", state->file);
	state->tree = (IDL_tree)decl.simple_declarator;
	if (!process_node(state))
	    return FALSE;
	if (IDL_LIST(iter).next)
	    fputs(", ", state->file);
    }
    fputs(")", state->file);
    return TRUE;
}

/*
 * An attribute declaration looks like:
 *
 * [ IDL_ATTR_DCL]
 *   - param_type_spec [IDL_TYPE_* or NULL for void]
 *   - simple_declarations [IDL_LIST]
 *     - data [IDL_IDENT]
 *     - next [IDL_LIST or NULL if no more idents]
 *       - data [IDL_IDENT]
 */

#define ATTR_IDENT(tree) (IDL_IDENT(IDL_LIST(IDL_ATTR_DCL(tree).simple_declarations).data))
#define ATTR_TYPE_DECL(tree) (IDL_ATTR_DCL(tree).param_type_spec)
#define ATTR_TYPE(tree) (IDL_NODE_TYPE(ATTR_TYPE_DECL(tree)))

static gboolean
attr_accessor(TreeState *state, gboolean getter)
{
    char *attrname = ATTR_IDENT(state->tree).str;
    if (getter && (ATTR_TYPE(state->tree) == IDLN_TYPE_BOOLEAN)) {
	fprintf(state->file, "  NS_IMETHOD Is%c%s(PRBool *aIs%c%s);\n",
		toupper(attrname[0]), attrname + 1,
		toupper(attrname[0]), attrname + 1);
    } else {
	IDL_tree orig = state->tree;
	fprintf(state->file, "  NS_IMETHOD %cet%c%s(",
		getter ? 'G' : 'S',
		toupper(attrname[0]), attrname + 1);
	state->tree = ATTR_TYPE_DECL(state->tree);
	if (!xpcom_type(state))
	    return FALSE;
	state->tree = orig;
	fprintf(state->file, "%s%sa%c%s);\n",
		(STARRED_TYPE(orig) ? "" : " "),
		getter ? "*" : "",
		toupper(attrname[0]), attrname + 1);
    }
    return TRUE;
}

static gboolean
attr_dcl(TreeState *state)
{
    gboolean ro = IDL_ATTR_DCL(state->tree).f_readonly;
    IDL_tree orig = state->tree;
    fprintf(state->file, "\n  /* %sattribute ",
	    ro ? "readonly " : "");
    state->tree = IDL_ATTR_DCL(state->tree).param_type_spec;
    if (state->tree && !type(state))
	return FALSE;
    fputs(" ", state->file);
    state->tree = IDL_ATTR_DCL(orig).simple_declarations;
    if (state->tree && !process_node(state))
	return FALSE;
    fputs("; */\n", state->file);

    state->tree = orig;
    return attr_accessor(state, TRUE) && (ro || attr_accessor(state, FALSE));
}

static gboolean
do_enum(TreeState *state)
{
    IDL_tree enumb = state->tree, iter;
    
    fprintf(state->file, "\nenum %s {\n",
	    IDL_IDENT(IDL_TYPE_ENUM(enumb).ident).str);

    for (iter = IDL_TYPE_ENUM(enumb).enumerator_list;
	 iter; iter = IDL_LIST(iter).next)
	fprintf(state->file, "  %s%s\n", IDL_IDENT(IDL_LIST(iter).data).str,
		IDL_LIST(iter).next ? ",": "");

    fputs("};\n", state->file);
    return TRUE;
}

/*
 * param generation:
 * in string foo	-->	nsString * foo
 * out string foo       -->     nsString * &foo;
 * inout string foo     -->     nsString * &foo;
 */

static gboolean
xpcom_param(TreeState *state)
{
    IDL_tree param = state->tree;
    state->tree = IDL_PARAM_DCL(param).param_type_spec;

    /* in params that are pointers should be const */
    if (STARRED_TYPE(state->tree) &&
	IDL_PARAM_DCL(param).attr == IDL_PARAM_IN)
	fputs("const ", state->file);

    if (!xpcom_type(state))
	return FALSE;
    fprintf(state->file, "%s%s",
	    STARRED_TYPE(state->tree)  ? "" : " ",
	    IDL_PARAM_DCL(param).attr == IDL_PARAM_IN ? "" : "*");
    fprintf(state->file, "%s",
	    IDL_IDENT(IDL_PARAM_DCL(param).simple_declarator).str);
    return TRUE;
}

/*
 * A method is an `operation', therefore a method decl is an `op dcl'.
 * I blame Elliot.
 */
static gboolean
op_dcl(TreeState *state)
{
    struct _IDL_OP_DCL op = IDL_OP_DCL(state->tree);
    IDL_tree iter;
    state->tree = op.op_type_spec;
    fputs("\n  /* ", state->file);
    if (!type(state))
	return FALSE;
    fputs(" ", state->file);
    state->tree = op.ident;
    if (state->tree && !process_node(state))
	return FALSE;
    state->tree = op.parameter_dcls;
    if (!param_dcls(state))
	return FALSE;
    fputs("; */\n", state->file);

    fprintf(state->file, "  NS_IMETHOD %s(", IDL_IDENT(op.ident).str);
    for (iter = op.parameter_dcls; iter; iter = IDL_LIST(iter).next) {
	state->tree = IDL_LIST(iter).data;
	if (!xpcom_param(state))
	    return FALSE;
	if (IDL_LIST(iter).next)
	    fputs(", ", state->file);
    }
    fputs(") = 0;\n", state->file);
    return TRUE;
}

nodeHandler *headerDispatch()
{
  static nodeHandler table[IDLN_LAST];
  static gboolean initialized = FALSE;

  if (!initialized) {
    table[IDLN_NONE] = pass_1;
    table[IDLN_LIST] = list;
    table[IDLN_IDENT] = ident;
    table[IDLN_ATTR_DCL] = attr_dcl;
    table[IDLN_OP_DCL] = op_dcl;
    table[IDLN_PARAM_DCL] = param_dcls;
    table[IDLN_TYPE_INTEGER] = type_integer;
    table[IDLN_TYPE_FLOAT] = type;
    table[IDLN_TYPE_FIXED] = type;
    table[IDLN_TYPE_CHAR] = type;
    table[IDLN_TYPE_WIDE_CHAR] = type;
    table[IDLN_TYPE_STRING] = type;
    table[IDLN_TYPE_WIDE_STRING] = type;
    table[IDLN_TYPE_BOOLEAN] = type;
    table[IDLN_TYPE_OCTET] = type;
    table[IDLN_TYPE_ANY] = type;
    table[IDLN_TYPE_OBJECT] = type;
    table[IDLN_TYPE_ENUM] = do_enum;
    table[IDLN_TYPE_SEQUENCE] = type;
    table[IDLN_TYPE_ARRAY] = type;
    table[IDLN_TYPE_STRUCT] = type;
    table[IDLN_TYPE_UNION] = type;
    table[IDLN_INTERFACE] = interface;
    initialized = TRUE;
  }
  
  return table;  
}
