/***************************************************************************

    parser.y (IDL yacc parser and tree generation)

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

    $Id: parser.y,v 1.1 1999/05/06 15:05:38 beard%netscape.com Exp $

***************************************************************************/
%{
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "rename.h"
#include "util.h"

#define do_binop(rv,op,a,b)		do {		\
	if (IDL_binop_chktypes (op, a, b))		\
		YYABORT;				\
	if (!(__IDL_flags & IDLF_NO_EVAL_CONST)) {	\
		rv = IDL_binop_eval (op, a, b);		\
		IDL_tree_free (a);			\
		IDL_tree_free (b);			\
		if (!rv) YYABORT;			\
	} else {					\
		rv = IDL_binop_new (op, a, b);		\
	}						\
} while (0)

#define do_unaryop(rv,op,a)		do {		\
	if (IDL_unaryop_chktypes (op, a))		\
		YYABORT;				\
	if (!(__IDL_flags & IDLF_NO_EVAL_CONST)) {	\
		rv = IDL_unaryop_eval (op, a);		\
		IDL_tree_free (a);			\
		if (!rv) YYABORT;			\
	} else {					\
		rv = IDL_unaryop_new (op, a);		\
	}						\
} while (0)

#define assign_props(tree,props)	do {		\
	if (__IDL_flags & IDLF_PROPERTIES)		\
		IDL_NODE_PROPERTIES (tree) = (props);	\
	else						\
		__IDL_free_properties (props);		\
} while (0)

extern int		yylex				(void);
static IDL_declspec_t	IDL_parse_declspec		(const char *strspec);
static int		IDL_binop_chktypes		(enum IDL_binop op,
							 IDL_tree a,
							 IDL_tree b);
static int		IDL_unaryop_chktypes		(enum IDL_unaryop op,
							 IDL_tree a);
static IDL_tree		IDL_binop_eval			(enum IDL_binop op,
							 IDL_tree a,
							 IDL_tree b);
static IDL_tree		IDL_unaryop_eval		(enum IDL_unaryop op,
							 IDL_tree a);
static IDL_tree		list_start			(IDL_tree a,
							 gboolean filter_null);
static IDL_tree		list_chain			(IDL_tree a,
							 IDL_tree b,
							 gboolean filter_null);
static IDL_tree		zlist_chain			(IDL_tree a,
							 IDL_tree b,
							 gboolean filter_null);
static int		do_token_error			(IDL_tree p,
							 const char *message,
							 gboolean prev);
static void		illegal_context_type_error	(IDL_tree p,
							 const char *what);
static void		illegal_type_error		(IDL_tree p,
							 const char *message);
%}

%union {
	IDL_tree tree;
	struct {
		IDL_tree tree;
		gpointer data;
	} treedata;
	GHashTable *hash_table;
	char *str;
	gboolean boolean;
	IDL_declspec_t declspec;
	IDL_longlong_t integer;
	double floatp;
	enum IDL_unaryop unaryop;
	enum IDL_param_attr paramattr;
}

/* Terminals */
%token			TOK_ANY
%token			TOK_ATTRIBUTE
%token			TOK_BOOLEAN
%token			TOK_CASE
%token			TOK_CHAR
%token			TOK_CONST
%token			TOK_CONTEXT
%token			TOK_DEFAULT
%token			TOK_DOUBLE
%token			TOK_ENUM
%token			TOK_EXCEPTION
%token			TOK_FALSE
%token			TOK_FIXED
%token			TOK_FLOAT
%token			TOK_IN 
%token			TOK_INOUT
%token			TOK_INTERFACE
%token			TOK_LONG
%token			TOK_MODULE
%token			TOK_NATIVE
%token			TOK_NOSCRIPT
%token			TOK_OBJECT
%token			TOK_OCTET
%token			TOK_ONEWAY
%token			TOK_OP_SCOPE
%token			TOK_OP_SHL
%token			TOK_OP_SHR
%token			TOK_OUT
%token			TOK_RAISES
%token			TOK_READONLY 
%token			TOK_SEQUENCE
%token			TOK_SHORT
%token			TOK_STRING
%token			TOK_STRUCT
%token			TOK_SWITCH
%token			TOK_TRUE
%token			TOK_TYPECODE
%token			TOK_TYPEDEF
%token			TOK_UNION
%token			TOK_UNSIGNED
%token			TOK_VARARGS
%token			TOK_VOID
%token			TOK_WCHAR
%token			TOK_WSTRING
%token <floatp>		TOK_FLOATP
%token <integer>	TOK_INTEGER
%token <str>		TOK_DECLSPEC TOK_PROP_KEY
%token <str>		TOK_PROP_VALUE TOK_NATIVE_TYPE
%token <str>		TOK_IDENT TOK_SQSTRING TOK_DQSTRING TOK_FIXEDP
%token <tree>		TOK_CODEFRAG

/* Non-Terminals */
%type <tree>		add_expr
%type <tree>		and_expr
%type <tree>		any_type
%type <tree>		array_declarator
%type <tree>		attr_dcl
%type <tree>		attr_dcl_def
%type <tree>		base_type_spec
%type <tree>		boolean_lit
%type <tree>		boolean_type
%type <tree>		case_label
%type <tree>		case_label_list
%type <tree>		case_stmt
%type <tree>		case_stmt_list
%type <tree>		char_lit
%type <tree>		char_type
%type <tree>		codefrag
%type <tree>		complex_declarator
%type <tree>		const_dcl
%type <tree>		const_dcl_def
%type <tree>		const_exp
%type <tree>		const_type
%type <tree>		constr_type_spec
%type <tree>		context_expr
%type <tree>		cur_ns_new_or_prev_ident
%type <tree>		declarator
%type <tree>		declarator_list
%type <tree>		definition
%type <tree>		definition_list
%type <tree>		element_spec
%type <tree>		enum_type
%type <tree>		enumerator_list
%type <tree>		except_dcl
%type <tree>		except_dcl_def
%type <tree>		export
%type <tree>		export_list
%type <tree>		fixed_array_size
%type <tree>		fixed_array_size_list
%type <tree>		fixed_pt_const_type
%type <tree>		fixed_pt_lit
%type <tree>		fixed_pt_type
%type <tree>		floating_pt_lit
%type <tree>		floating_pt_type
%type <tree>		ident
%type <tree>		illegal_ident
%type <tree>		integer_lit
%type <tree>		integer_type
%type <tree>		interface
%type <tree>		interface_body
%type <tree>		interface_catch_ident
%type <tree>		is_context_expr
%type <tree>		is_raises_expr
%type <tree>		literal
%type <tree>		member
%type <tree>		member_list
%type <tree>		member_zlist
%type <tree>		module
%type <tree>		mult_expr
%type <tree>		new_ident
%type <tree>		new_or_prev_scope
%type <tree>		new_scope
%type <tree>		ns_global_ident
%type <tree>		ns_new_ident
%type <tree>		ns_prev_ident
%type <tree>		ns_scoped_name
%type <tree>		object_type
%type <tree>		octet_type
%type <tree>		op_dcl
%type <tree>		op_dcl_def
%type <tree>		op_param_type_spec
%type <tree>		op_param_type_spec_illegal
%type <tree>		op_type_spec
%type <tree>		or_expr
%type <tree>		param_dcl
%type <tree>		param_dcl_list
%type <tree>		param_type_spec
%type <tree>		pop_scope
%type <tree>		positive_int_const
%type <tree>		primary_expr
%type <tree>		raises_expr
%type <tree>		scoped_name
%type <tree>		scoped_name_list
%type <tree>		sequence_type
%type <tree>		shift_expr
%type <tree>		simple_declarator
%type <tree>		simple_declarator_list
%type <tree>		simple_type_spec
%type <tree>		specification
%type <tree>		string_lit
%type <tree>		string_lit_list
%type <tree>		string_type
%type <tree>		struct_type
%type <tree>		switch_body
%type <tree>		switch_type_spec
%type <tree>		template_type_spec
%type <tree>		type_dcl
%type <tree>		type_dcl_def
%type <tree>		type_declarator
%type <tree>		type_spec
%type <tree>		unary_expr
%type <tree>		union_type
%type <tree>		useless_semicolon
%type <tree>		wide_char_type
%type <tree>		wide_string_type
%type <tree>		xor_expr
%type <tree>		z_definition_list
%type <tree>		z_inheritance
%type <tree>		z_new_ident_catch
%type <tree>		z_new_scope_catch
%type <tree>		typecode_type

%type <treedata>	parameter_dcls
%type <declspec>	z_declspec module_declspec
%type <hash_table>	z_props prop_hash
%type <boolean>		is_readonly is_oneway is_noscript
%type <boolean>		is_varargs is_cvarargs
%type <integer>		signed_int unsigned_int
%type <paramattr>	param_attribute
%type <str>		sqstring dqstring dqstring_cat
%type <unaryop>		unary_op

%%

specification:		/* empty */			{ yyerror ("Empty file"); YYABORT; }
|			definition_list			{ __IDL_root = $1; }
	;

z_definition_list:	/* empty */			{ $$ = NULL; }
|			definition_list
	;

definition_list:	definition			{ $$ = list_start ($1, TRUE); }
|			definition_list definition	{ $$ = list_chain ($1, $2, TRUE); }
	;

check_semicolon:	';'
|			/* empty */			{
	if (do_token_error ($<tree>0, "Missing semicolon after", TRUE))
		YYABORT;
}
	;

useless_semicolon:	';'				{
	yyerror ("Dangling semicolon has no effect");
	$$ = NULL;
}
	;

check_comma:		','
|			/* empty */			{
	if (do_token_error ($<tree>0, "Missing comma after", TRUE))
		YYABORT;
}
	;

illegal_ident:		scoped_name			{
	if (IDL_NODE_UP ($1))
	    do_token_error (IDL_NODE_UP ($1), "Illegal context for", FALSE);
	else
		yyerror ("Illegal context for identifier");
	YYABORT;
}
	;

definition:		type_dcl check_semicolon
|			const_dcl check_semicolon
|			except_dcl check_semicolon
|			interface check_semicolon
|			module check_semicolon
|			codefrag
|			illegal_ident
|			useless_semicolon
	;

module_declspec:	z_declspec TOK_MODULE
	;

module:			module_declspec
			new_or_prev_scope		{
	if (IDL_NODE_UP ($2) != NULL &&
	    IDL_NODE_TYPE (IDL_NODE_UP ($2)) != IDLN_MODULE) {
		yyerror ("Module definition conflicts");
		do_token_error (IDL_NODE_UP ($2), "with", FALSE);
		YYABORT;
	}
}			'{'
				z_definition_list
			'}' pop_scope			{
	IDL_tree module;

	if ($5 == NULL) {
		yyerrorv ("Empty module declaration `%s' is not legal IDL",
			  IDL_IDENT ($2).str);
		module = NULL;
	}

	if (__IDL_flags & IDLF_COMBINE_REOPENED_MODULES) {
		if (IDL_NODE_UP ($2) == NULL)
			module = IDL_module_new ($2, $5);
		else {
			module = IDL_NODE_UP ($2);
			IDL_MODULE (module).definition_list =
				IDL_list_concat (IDL_MODULE (module).definition_list, $5);
			module = NULL;
		}
	} else
		module = IDL_module_new ($2, $5);

	$$ = module;
	if ($$) {
		IDL_NODE_DECLSPEC ($$) = $1;	
		if (__IDL_inhibits > 0)
			IDL_NODE_DECLSPEC ($$) |= IDLF_DECLSPEC_EXIST |
				IDLF_DECLSPEC_INHIBIT;
	}
}
	;

interface_catch_ident:	new_or_prev_scope
|			TOK_OBJECT			{
	yyerror ("Interfaces cannot be named `Object'");
	YYABORT;
}
|			TOK_TYPECODE			{
	yyerror ("Interfaces cannot be named `TypeCode'");
	YYABORT;
}
	;

interface:		z_declspec
			z_props
			TOK_INTERFACE
			interface_catch_ident		{
	assert ($4 != NULL);
	assert (IDL_NODE_TYPE ($4) == IDLN_IDENT);
	assert (IDL_IDENT_TO_NS ($4) != NULL);
	assert (IDL_NODE_TYPE (IDL_IDENT_TO_NS ($4)) == IDLN_GENTREE);

	if (IDL_NODE_UP ($4) != NULL &&
	    IDL_NODE_TYPE (IDL_NODE_UP ($4)) != IDLN_INTERFACE &&
	    IDL_NODE_TYPE (IDL_NODE_UP ($4)) != IDLN_FORWARD_DCL) {
		yyerrorl ("Interface definition conflicts",
			  __IDL_prev_token_line - __IDL_cur_token_line);
		do_token_error (IDL_NODE_UP ($4), "with", FALSE);
		YYABORT;
	} else if (IDL_NODE_UP ($4) != NULL &&
		   IDL_NODE_TYPE (IDL_NODE_UP ($4)) != IDLN_FORWARD_DCL) {
		yyerrorv ("Cannot redeclare interface `%s'", IDL_IDENT ($4).str);
		IDL_tree_error ($4, "Previous declaration of interface `%s'", IDL_IDENT ($4).str);
		YYABORT;
	} else if (IDL_NODE_UP ($4) != NULL &&
		   IDL_NODE_TYPE (IDL_NODE_UP ($4)) == IDLN_FORWARD_DCL)
		__IDL_assign_this_location ($4, __IDL_cur_filename, __IDL_cur_line);
}
			pop_scope
			z_inheritance			{
	IDL_GENTREE (IDL_IDENT_TO_NS ($4))._import = $7;
	IDL_ns_push_scope (__IDL_root_ns, IDL_IDENT_TO_NS ($4));
	if (IDL_ns_check_for_ambiguous_inheritance ($4, $7))
		__IDL_is_okay = FALSE;
}			'{'
				interface_body
			'}' pop_scope			{
 	$$ = IDL_interface_new ($4, $7, $10);
	IDL_NODE_DECLSPEC ($$) = $1;
	if (__IDL_inhibits > 0)
		IDL_NODE_DECLSPEC ($$) |= IDLF_DECLSPEC_EXIST | IDLF_DECLSPEC_INHIBIT;
	assign_props (IDL_INTERFACE ($$).ident, $2);
}
|			z_declspec
			z_props
			TOK_INTERFACE
			interface_catch_ident
			pop_scope			{
	if ($2) yywarningv (IDL_WARNING1,
			    "Ignoring properties for forward declaration `%s'",
			    IDL_IDENT ($4).str);
	$$ = IDL_forward_dcl_new ($4);
	IDL_NODE_DECLSPEC ($$) = $1;
	if (__IDL_inhibits > 0)
		IDL_NODE_DECLSPEC ($$) |= IDLF_DECLSPEC_EXIST | IDLF_DECLSPEC_INHIBIT;
}
	;

z_inheritance:		/* empty */			{ $$ = NULL; }
|			':' scoped_name_list		{
	GHashTable *table = g_hash_table_new (g_direct_hash, g_direct_equal);
	gboolean die = FALSE;
	IDL_tree p = $2;

	assert (IDL_NODE_TYPE (p) == IDLN_LIST);
	for (; p != NULL && !die; p = IDL_LIST (p).next) {
		assert (IDL_LIST (p).data != NULL);
		assert (IDL_NODE_TYPE (IDL_LIST (p).data) == IDLN_IDENT);

		if (g_hash_table_lookup_extended (table, IDL_LIST (p).data, NULL, NULL)) {
			char *s = IDL_ns_ident_to_qstring (IDL_LIST (p).data, "::", 0);
			yyerrorv ("Cannot inherit from interface `%s' more than once", s);
			g_free (s);
			die = TRUE;
			break;
		} else
			g_hash_table_insert (table, IDL_LIST (p).data, NULL);

		if (IDL_NODE_TYPE (IDL_NODE_UP (IDL_LIST (p).data)) == IDLN_FORWARD_DCL) {
			char *s = IDL_ns_ident_to_qstring (IDL_LIST (p).data, "::", 0);
			yyerrorv ("Incomplete definition of interface `%s'", s);
			IDL_tree_error (IDL_LIST (p).data,
					"Previous forward declaration of `%s'", s);
			g_free (s);
			die = TRUE;
		}
		else if (IDL_NODE_TYPE (IDL_NODE_UP (IDL_LIST (p).data)) != IDLN_INTERFACE) {
			char *s = IDL_ns_ident_to_qstring (IDL_LIST (p).data, "::", 0);
			yyerrorv ("`%s' is not an interface", s);
			IDL_tree_error (IDL_LIST (p).data,
					"Previous declaration of `%s'", s);
			g_free (s);
			die = TRUE;
		}
	}

	g_hash_table_destroy (table);

	if (die)
		YYABORT;

	$$ = $2;
}
	;

scoped_name_list:	scoped_name			{ $$ = list_start ($1, TRUE); }
|			scoped_name_list
			check_comma scoped_name		{ $$ = list_chain ($1, $3, TRUE); }
	;

interface_body:		export_list
	;

export_list:		/* empty */			{ $$ = NULL; }
|			export_list export		{ $$ = zlist_chain ($1, $2, TRUE); }
	;

export:			type_dcl check_semicolon
|			except_dcl check_semicolon
|			op_dcl check_semicolon
|			attr_dcl check_semicolon
|			const_dcl check_semicolon
|			codefrag
|			useless_semicolon
	;

type_dcl:		z_declspec type_dcl_def		{
	$$ = $2;
	IDL_NODE_DECLSPEC ($$) = $1;
	if (__IDL_inhibits > 0)
 		IDL_NODE_DECLSPEC ($$) |= IDLF_DECLSPEC_EXIST | IDLF_DECLSPEC_INHIBIT;
}
	;

type_dcl_def:		z_props TOK_TYPEDEF
			type_declarator			{
	IDL_tree_node node;
	IDL_tree p, dcl;

	$$ = $3;
	node.properties = $1;
	for (p = IDL_TYPE_DCL ($3).dcls; p; p = IDL_LIST (p).next) {
		dcl = IDL_LIST (p).data;
		IDL_tree_properties_copy (&node, dcl);
	}
	__IDL_free_properties (node.properties);
}
|			struct_type
|			union_type
|			enum_type
|			z_props TOK_NATIVE
			simple_declarator		{
	$$ = IDL_native_new ($3);
	assign_props (IDL_NATIVE ($$).ident, $1);
}
|			z_props TOK_NATIVE simple_declarator
			'('				{
	/* Enable native type scanning */
	if (__IDL_flags & IDLF_XPIDL)
		__IDL_flagsi |= IDLFP_NATIVE;
	else {
		yyerror ("Native syntax not enabled");
		YYABORT;
	}
}			TOK_NATIVE_TYPE			{
	$$ = IDL_native_new ($3);
	IDL_NATIVE ($$).user_type = $6;
	assign_props (IDL_NATIVE ($$).ident, $1);
}
	;

type_declarator:	type_spec declarator_list	{ $$ = IDL_type_dcl_new ($1, $2); }
	;

type_spec:		simple_type_spec
|			constr_type_spec
	;

simple_type_spec:	base_type_spec
|			template_type_spec
|			scoped_name
	;

constr_type_spec:	struct_type
|			union_type
|			enum_type
	;

z_new_ident_catch:	/* empty */			{
	yyerrorv ("Missing identifier in %s definition", $<str>0);
	YYABORT;
}
|			new_ident
	;

z_new_scope_catch:	/* empty */			{
	yyerrorv ("Missing identifier in %s definition", $<str>0);
	YYABORT;
}
|			new_scope
	;

struct_type:		z_props TOK_STRUCT
			{ $<str>$ = "struct"; }
			z_new_scope_catch '{'		{
	g_hash_table_insert (__IDL_structunion_ht, $4, $4);
	$$ = IDL_type_struct_new ($4, NULL);
}				member_list
			'}' pop_scope			{
	g_hash_table_remove (__IDL_structunion_ht, $4);
	$$ = $<tree>6;
	__IDL_assign_up_node ($$, $7);
	IDL_TYPE_STRUCT ($$).member_list = $7;
	assign_props (IDL_TYPE_STRUCT ($$).ident, $1);
}
	;

union_type:		z_props TOK_UNION
			{ $<str>$ = "union"; }
			z_new_scope_catch TOK_SWITCH '('
				switch_type_spec
			')' '{'				{
	g_hash_table_insert (__IDL_structunion_ht, $4, $4);
	$$ = IDL_type_union_new ($4, $7, NULL);
}				switch_body
			'}' pop_scope			{
	g_hash_table_remove (__IDL_structunion_ht, $4);
	$$ = $<tree>10;
	__IDL_assign_up_node ($$, $11);
	IDL_TYPE_UNION ($$).switch_body = $11;
	assign_props (IDL_TYPE_UNION ($$).ident, $1);
}
	;

switch_type_spec:	integer_type
|			char_type
|			boolean_type
|			enum_type
|			scoped_name
	;

switch_body:		case_stmt_list
	;

case_stmt_list:		case_stmt			{ $$ = list_start ($1, TRUE); }
|			case_stmt_list case_stmt	{ $$ = list_chain ($1, $2, TRUE); }
	;

case_stmt:		case_label_list
			element_spec check_semicolon	{ $$ = IDL_case_stmt_new ($1, $2); }
	;

element_spec:		type_spec declarator		{
	char *s;

	$$ = IDL_member_new ($1, list_start ($2, TRUE));
	if (IDL_NODE_TYPE ($1) == IDLN_IDENT &&
	    g_hash_table_lookup (__IDL_structunion_ht, $1)) {
		s = IDL_ns_ident_to_qstring ($2, "::", 0);
		yyerrorv ("Member `%s'", s);
		do_token_error (IDL_NODE_UP ($1), "recurses", TRUE);
		g_free (s);
	}
}
	;

case_label_list:	case_label			{ $$ = list_start ($1, FALSE); }
|			case_label_list case_label	{ $$ = list_chain ($1, $2, FALSE); }
	;

case_label:		TOK_CASE const_exp ':'		{ $$ = $2; }
|			TOK_DEFAULT ':'			{ $$ = NULL; }
	;

const_dcl:		z_declspec const_dcl_def	{
	$$ = $2;
	IDL_NODE_DECLSPEC ($$) = $1;
	if (__IDL_inhibits > 0)
 		IDL_NODE_DECLSPEC ($$) |= IDLF_DECLSPEC_EXIST | IDLF_DECLSPEC_INHIBIT;
}
	;

const_dcl_def:		TOK_CONST const_type new_ident
			'=' const_exp			{
	$$ = IDL_const_dcl_new ($2, $3, $5);
	/* Should probably do some type checking here... */
}
	;

except_dcl:		z_declspec except_dcl_def	{
	$$ = $2;
	IDL_NODE_DECLSPEC ($$) = $1;
	if (__IDL_inhibits > 0)
 		IDL_NODE_DECLSPEC ($$) |= IDLF_DECLSPEC_EXIST | IDLF_DECLSPEC_INHIBIT;
}
	;

except_dcl_def:		TOK_EXCEPTION new_scope '{'
				member_zlist
			'}' pop_scope			{ $$ = IDL_except_dcl_new ($2, $4); }
	;

member_zlist:		/* empty */			{ $$ = NULL; }
|			member_zlist member		{ $$ = zlist_chain ($1, $2, TRUE); }
	;

is_readonly:		/* empty */			{ $$ = FALSE; }
|			TOK_READONLY			{ $$ = TRUE; }
	;

attr_dcl:		z_declspec attr_dcl_def		{
	$$ = $2;
	IDL_NODE_DECLSPEC ($$) = $1;
	if (__IDL_inhibits > 0)
 		IDL_NODE_DECLSPEC ($$) |= IDLF_DECLSPEC_EXIST | IDLF_DECLSPEC_INHIBIT;
}
	;

attr_dcl_def:		z_props
			is_readonly
			TOK_ATTRIBUTE
			{ $<str>$ = "attribute"; }
			param_type_spec
			simple_declarator_list		{
	IDL_tree_node node;
	IDL_tree p, dcl;

	$$ = IDL_attr_dcl_new ($2, $5, $6);
	node.properties = $1;
	for (p = $6; p; p = IDL_LIST (p).next) {
		dcl = IDL_LIST (p).data;
		IDL_tree_properties_copy (&node, dcl);
	}
	__IDL_free_properties (node.properties);
	if (__IDL_inhibits > 0)
 		IDL_NODE_DECLSPEC ($$) |= IDLF_DECLSPEC_EXIST | IDLF_DECLSPEC_INHIBIT;
}
	;

param_type_spec:	op_param_type_spec
|			TOK_VOID			{
	yyerrorv ("Illegal type `void' for %s", $<str>0);
	$$ = NULL;
}
	;

op_param_type_spec_illegal:
			sequence_type
|			constr_type_spec
	;

op_param_type_spec:	base_type_spec
|			string_type
|			wide_string_type
|			fixed_pt_type
|			scoped_name
|			op_param_type_spec_illegal	{
	illegal_context_type_error ($1, $<str>0);
	$$ = $1;
}
	;

is_noscript:		/* empty */			{ $$ = FALSE; }
|			TOK_NOSCRIPT			{ $$ = TRUE; }
	;

is_oneway:		/* empty */			{ $$ = FALSE; }
|			TOK_ONEWAY			{ $$ = TRUE; }
	;

op_dcl:		z_declspec op_dcl_def			{
	$$ = $2;
	IDL_NODE_DECLSPEC ($$) = $1;
	if (__IDL_inhibits > 0)
 		IDL_NODE_DECLSPEC ($$) |= IDLF_DECLSPEC_EXIST | IDLF_DECLSPEC_INHIBIT;
}
	;

op_dcl_def:		z_props
			is_noscript
			is_oneway
			op_type_spec
			new_scope parameter_dcls pop_scope
			is_raises_expr
			is_context_expr			{
	$$ = IDL_op_dcl_new ($3, $4, $5, $6.tree, $8, $9);
	IDL_OP_DCL ($$).f_noscript = $2;
	IDL_OP_DCL ($$).f_varargs = GPOINTER_TO_INT ($6.data);
	assign_props (IDL_OP_DCL ($$).ident, $1);
}
	;

op_type_spec:		{ $<str>$ = "operation return value"; }
			op_param_type_spec		{ $$ = $2; }
|			TOK_VOID			{ $$ = NULL; }
	;

is_varargs:		/* empty */			{ $$ = FALSE; }
|			TOK_VARARGS			{ $$ = TRUE; }
	;

is_cvarargs:		/* empty */			{ $$ = FALSE; }
|			',' TOK_VARARGS			{ $$ = TRUE; }
	;

parameter_dcls:		'('
			param_dcl_list
			is_cvarargs
			')'				{
	$$.tree = $2;
	$$.data = GINT_TO_POINTER ($3);
}
|			'(' is_varargs ')'		{
	$$.tree = NULL;
	$$.data = GINT_TO_POINTER ($2);
}
	;

param_dcl_list:		param_dcl			{ $$ = list_start ($1, TRUE); }
|			param_dcl_list
			check_comma param_dcl		{ $$ = list_chain ($1, $3, TRUE); }
	;

param_dcl:		z_props
			param_attribute
			{ $<str>$ = "parameter"; }
			param_type_spec
			simple_declarator		{
	$$ = IDL_param_dcl_new ($2, $4, $5);
	assign_props (IDL_PARAM_DCL ($$).simple_declarator, $1);
}
	;

param_attribute:	TOK_IN				{ $$ = IDL_PARAM_IN; }
|			TOK_OUT				{ $$ = IDL_PARAM_OUT; }
|			TOK_INOUT			{ $$ = IDL_PARAM_INOUT; }
|			param_type_spec			{
	yyerrorv ("Missing direction attribute (in, out, inout) before parameter");
	IDL_tree_free ($1);
}
	;

is_raises_expr:		/* empty */			{ $$ = NULL; }
|			raises_expr
	;

is_context_expr:	/* empty */			{ $$ = NULL; }
|			context_expr
	;

raises_expr:		TOK_RAISES '('
				scoped_name_list
			')'				{ $$ = $3; }
	;

context_expr:		TOK_CONTEXT '('
				string_lit_list
			')'				{ $$ = $3; }
	;

const_type:		integer_type
|			char_type
|			wide_char_type
|			boolean_type
|			floating_pt_type
|			string_type
|			wide_string_type
|			fixed_pt_const_type
|			scoped_name
	;

const_exp:		or_expr
	;

or_expr:		xor_expr
|			or_expr '|' xor_expr		{ do_binop ($$, IDL_BINOP_OR, $1, $3); }
	;

xor_expr:		and_expr
|			xor_expr '^' and_expr		{ do_binop ($$, IDL_BINOP_XOR, $1, $3); }
	;

and_expr:		shift_expr
|			and_expr '&' shift_expr		{ do_binop ($$, IDL_BINOP_AND, $1, $3); }
	;

shift_expr:		add_expr
|			shift_expr TOK_OP_SHR add_expr	{ do_binop ($$, IDL_BINOP_SHR, $1, $3); }
|			shift_expr TOK_OP_SHL add_expr	{ do_binop ($$, IDL_BINOP_SHL, $1, $3); }
	;

add_expr:		mult_expr
|			add_expr '+' mult_expr		{ do_binop ($$, IDL_BINOP_ADD, $1, $3); }
|			add_expr '-' mult_expr		{ do_binop ($$, IDL_BINOP_SUB, $1, $3); }
	;

mult_expr:		unary_expr
|			mult_expr '*' unary_expr	{ do_binop ($$, IDL_BINOP_MULT, $1, $3); }
|			mult_expr '/' unary_expr	{ do_binop ($$, IDL_BINOP_DIV, $1, $3); }
|			mult_expr '%' unary_expr	{ do_binop ($$, IDL_BINOP_MOD, $1, $3); }
	;

unary_expr:		unary_op primary_expr		{ do_unaryop ($$, $1, $2); }
|			primary_expr
	;

unary_op:		'-'				{ $$ = IDL_UNARYOP_MINUS; }
|			'+'				{ $$ = IDL_UNARYOP_PLUS; }
|			'~'				{ $$ = IDL_UNARYOP_COMPLEMENT; }
	;

primary_expr:		scoped_name			{
	IDL_tree p, literal;
	
	assert (IDL_NODE_TYPE ($1) == IDLN_IDENT);

	p = IDL_NODE_UP ($1);
	
	if ((literal = IDL_resolve_const_exp ($1, IDLN_ANY))) {
		++IDL_NODE_REFS (literal);
		$$ = literal;
		IDL_tree_free ($1);
	} else
		$$ = $1;
}
|			literal
|			'(' const_exp ')'		{ $$ = $2; }
	;

literal:		integer_lit
|			string_lit
|			char_lit
|			fixed_pt_lit
|			floating_pt_lit
|			boolean_lit
	;

enum_type:		z_props TOK_ENUM
			{ $<str>$ = "enum"; }
			z_new_ident_catch '{'
				enumerator_list
			'}'				{
	$$ = IDL_type_enum_new ($4, $6);
	assign_props (IDL_TYPE_ENUM ($$).ident, $1);
}
	;

scoped_name:		ns_scoped_name			{
	assert ($1 != NULL);
	assert (IDL_NODE_TYPE ($1) == IDLN_GENTREE);
	assert (IDL_NODE_TYPE (IDL_GENTREE ($1).data) == IDLN_IDENT);
	$$ = IDL_GENTREE ($1).data;
}
	;

ns_scoped_name:		ns_prev_ident
|			TOK_OP_SCOPE ns_global_ident	{ $$ = $2; }
|			ns_scoped_name TOK_OP_SCOPE
			ident				{
	IDL_tree p;

	assert (IDL_NODE_TYPE ($1) == IDLN_GENTREE);
	assert (IDL_NODE_TYPE ($3) == IDLN_IDENT);

#ifdef YYDEBUG
	if (yydebug)
		printf ("looking in %s\n", IDL_IDENT (IDL_GENTREE ($1).data).str);
#endif

	if ((p = IDL_ns_lookup_this_scope (__IDL_root_ns, $1, $3, NULL)) == NULL) {
#ifdef YYDEBUG
		if (yydebug)
			printf ("'%s'\n", IDL_IDENT ($3).str);
#endif
		yyerrorv ("`%s' undeclared identifier", IDL_IDENT ($3).str);
		IDL_tree_free ($3);
		YYABORT;
	}
	IDL_tree_free ($3);
	++IDL_NODE_REFS (IDL_GENTREE (p).data);
	$$ = p;
}
	;

enumerator_list:	new_ident			{ $$ = list_start ($1, TRUE); }
|			enumerator_list
			check_comma new_ident		{ $$ = list_chain ($1, $3, TRUE); }
	;

member_list:		member				{ $$ = list_start ($1, TRUE); }
|			member_list member		{ $$ = list_chain ($1, $2, TRUE); }
	;

member:			type_spec declarator_list
			check_semicolon			{
	char *s;

	$$ = IDL_member_new ($1, $2);
	if (IDL_NODE_TYPE ($1) == IDLN_IDENT &&
	    g_hash_table_lookup (__IDL_structunion_ht, $1)) {
		s = IDL_ns_ident_to_qstring (IDL_LIST ($2).data, "::", 0);
		yyerrorv ("Member `%s'", s);
		do_token_error (IDL_NODE_UP ($1), "recurses", TRUE);
		g_free (s);
	}
}
	;

base_type_spec:		floating_pt_type
|			integer_type
|			char_type
|			wide_char_type
|			boolean_type
|			octet_type
|			any_type
|			object_type
|			typecode_type
	;

template_type_spec:	sequence_type
|			string_type
|			wide_string_type
|			fixed_pt_type
	;

sequence_type:		TOK_SEQUENCE '<'
				simple_type_spec ',' positive_int_const
			'>'				{ $$ = IDL_type_sequence_new ($3, $5); }
|			TOK_SEQUENCE '<'
				simple_type_spec
			'>'				{ $$ = IDL_type_sequence_new ($3, NULL); }
	;

floating_pt_type:	TOK_FLOAT			{ $$ = IDL_type_float_new (IDL_FLOAT_TYPE_FLOAT); }
|			TOK_DOUBLE			{ $$ = IDL_type_float_new (IDL_FLOAT_TYPE_DOUBLE); }
|			TOK_LONG TOK_DOUBLE		{ $$ = IDL_type_float_new (IDL_FLOAT_TYPE_LONGDOUBLE); }
	;

fixed_pt_type:		TOK_FIXED '<'
				positive_int_const ',' integer_lit
			'>'				{ $$ = IDL_type_fixed_new ($3, $5); }
	;

fixed_pt_const_type:	TOK_FIXED			{ $$ = IDL_type_fixed_new (NULL, NULL); }
	;

integer_type:		signed_int			{ $$ = IDL_type_integer_new (TRUE, $1); }
|			unsigned_int			{ $$ = IDL_type_integer_new (FALSE, $1); }
	;

signed_int:		signed_short_int		{ $$ = IDL_INTEGER_TYPE_SHORT; }
|			signed_long_int			{ $$ = IDL_INTEGER_TYPE_LONG; }
|			signed_longlong_int		{ $$ = IDL_INTEGER_TYPE_LONGLONG; }
	;

signed_short_int:	TOK_SHORT
	;

signed_long_int:	TOK_LONG
	;

signed_longlong_int:	TOK_LONG TOK_LONG
	;

unsigned_int:		unsigned_short_int		{ $$ = IDL_INTEGER_TYPE_SHORT; }
|			unsigned_long_int		{ $$ = IDL_INTEGER_TYPE_LONG; }
|			unsigned_longlong_int		{ $$ = IDL_INTEGER_TYPE_LONGLONG; }
	;

unsigned_short_int:	TOK_UNSIGNED TOK_SHORT
	;

unsigned_long_int:	TOK_UNSIGNED TOK_LONG
	;

unsigned_longlong_int:	TOK_UNSIGNED TOK_LONG TOK_LONG
	;

char_type:		TOK_CHAR			{ $$ = IDL_type_char_new (); }
	;

wide_char_type:		TOK_WCHAR			{ $$ = IDL_type_wide_char_new (); }
	;

boolean_type:		TOK_BOOLEAN			{ $$ = IDL_type_boolean_new (); }
	;

octet_type:		TOK_OCTET			{ $$ = IDL_type_octet_new (); }
	;

any_type:		TOK_ANY				{ $$ = IDL_type_any_new (); }
	;

object_type:		TOK_OBJECT			{ $$ = IDL_type_object_new (); }
	;

typecode_type:		TOK_TYPECODE			{ $$ = IDL_type_typecode_new (); }
	;

string_type:		TOK_STRING '<'
				positive_int_const
				'>'			{ $$ = IDL_type_string_new ($3); }
|			TOK_STRING			{ $$ = IDL_type_string_new (NULL); }
	;

wide_string_type:	TOK_WSTRING '<'
				positive_int_const
			'>'				{ $$ = IDL_type_wide_string_new ($3); }
|			TOK_WSTRING			{ $$ = IDL_type_wide_string_new (NULL); }
	;

declarator_list:	declarator			{ $$ = list_start ($1, TRUE); }
|			declarator_list 
			check_comma declarator		{ $$ = list_chain ($1, $3, TRUE); }
	;

declarator:		simple_declarator
|			complex_declarator
	;

simple_declarator:	new_ident
	;

complex_declarator:	array_declarator
	;

simple_declarator_list:	simple_declarator		{ $$ = list_start ($1, TRUE); }
|			simple_declarator_list
			check_comma simple_declarator	{ $$ = list_chain ($1, $3, TRUE); }
	;

array_declarator:	new_ident
			fixed_array_size_list		{
	IDL_tree p;
	int i;

	$$ = IDL_type_array_new ($1, $2);
	for (i = 1, p = $2; p; ++i, p = IDL_LIST (p).next)
		if (!IDL_LIST (p).data) {
			char *s = IDL_ns_ident_to_qstring ($1, "::", 0);
			yyerrorv ("Missing value in dimension %d of array `%s'", i, s);
			g_free (s);
		}
}
	;

fixed_array_size_list:	fixed_array_size		{ $$ = list_start ($1, FALSE); }
|			fixed_array_size_list
			fixed_array_size		{ $$ = list_chain ($1, $2, FALSE); }
	;

fixed_array_size:	'[' positive_int_const ']'	{ $$ = $2; }
|			'[' ']'				{ $$ = NULL; }
	;

prop_hash:		TOK_PROP_KEY
			TOK_PROP_VALUE			{
	$$ = g_hash_table_new (IDL_strcase_hash, IDL_strcase_equal);
	g_hash_table_insert ($$, $1, $2);
}
|			prop_hash ','
			TOK_PROP_KEY
			TOK_PROP_VALUE			{
	$$ = $1;
	g_hash_table_insert ($$, $3, $4);
}
|			TOK_PROP_KEY			{
	$$ = g_hash_table_new (IDL_strcase_hash, IDL_strcase_equal);
	g_hash_table_insert ($$, $1, g_strdup (""));
}
|			prop_hash ','
			TOK_PROP_KEY			{
	$$ = $1;
	g_hash_table_insert ($$, $3, g_strdup (""));
}
	;

ident:			TOK_IDENT			{ $$ = IDL_ident_new ($1); }
	;

new_ident:		ns_new_ident			{
	assert ($1 != NULL);
	assert (IDL_NODE_TYPE ($1) == IDLN_GENTREE);
	assert (IDL_NODE_TYPE (IDL_GENTREE ($1).data) == IDLN_IDENT);
	$$ = IDL_GENTREE ($1).data;
}
	;

new_scope:		ns_new_ident			{
	IDL_ns_push_scope (__IDL_root_ns, $1);
#ifdef YYDEBUG
	if (yydebug)
		printf ("entering new/prev scope of %s\n", 
		       IDL_IDENT (IDL_GENTREE (IDL_NS (__IDL_root_ns).current).data).str);
#endif
	assert (IDL_NODE_TYPE (IDL_GENTREE ($1).data) == IDLN_IDENT);
	$$ = IDL_GENTREE ($1).data;
}
	;

new_or_prev_scope:	cur_ns_new_or_prev_ident	{
	IDL_ns_push_scope (__IDL_root_ns, $1);
	assert (IDL_NS (__IDL_root_ns).current != NULL);
	assert (IDL_NODE_TYPE (IDL_NS (__IDL_root_ns).current) == IDLN_GENTREE);
	assert (IDL_GENTREE (IDL_NS (__IDL_root_ns).current).data != NULL);
	assert (IDL_NODE_TYPE (IDL_GENTREE (IDL_NS (__IDL_root_ns).current).data) == IDLN_IDENT);
#ifdef YYDEBUG
	if (yydebug)
		printf ("entering new/prev scope of %s\n", 
		       IDL_IDENT (IDL_GENTREE (IDL_NS (__IDL_root_ns).current).data).str);
#endif
	assert (IDL_NODE_TYPE (IDL_GENTREE ($1).data) == IDLN_IDENT);
	$$ = IDL_GENTREE ($1).data;
}
	;

pop_scope:		/* empty */			{
#ifdef YYDEBUG
	if (yydebug)
		printf ("scope to parent of %s\n", 
		       IDL_IDENT (IDL_GENTREE (IDL_NS (__IDL_root_ns).current).data).str);
#endif
	IDL_ns_pop_scope (__IDL_root_ns);
}
	;

ns_new_ident:		ident				{
	IDL_tree p;

	if ((p = IDL_ns_place_new (__IDL_root_ns, $1)) == NULL) {
		IDL_tree q;
		int i;

		p = IDL_ns_lookup_cur_scope (__IDL_root_ns, $1, NULL);

		for (i = 0, q = IDL_GENTREE (p).data;
		     q && (IDL_NODE_TYPE (q) == IDLN_IDENT ||
			   IDL_NODE_TYPE (q) == IDLN_LIST) && i < 4;
		     ++i)
			if (IDL_NODE_UP (q))
				q = IDL_NODE_UP (q);

		if (q) {
			IDL_tree_error ($1, "`%s' conflicts", IDL_IDENT ($1).str);
			do_token_error (q, "with", FALSE);
		} else
			yyerrorv ("`%s' duplicate identifier", IDL_IDENT ($1).str);

		IDL_tree_free ($1);
		YYABORT;
	}
	assert (IDL_IDENT ($1)._ns_ref == p);
	++IDL_NODE_REFS (IDL_GENTREE (p).data);
	if (__IDL_new_ident_comments != NULL) {
		assert (IDL_IDENT ($1).comments == NULL);
		IDL_IDENT ($1).comments = __IDL_new_ident_comments;
		__IDL_new_ident_comments = NULL;
	}
	$$ = p;
}
	;

ns_prev_ident:		ident				{
	IDL_tree p;

	if ((p = IDL_ns_resolve_ident (__IDL_root_ns, $1)) == NULL) {
		yyerrorv ("`%s' undeclared identifier", IDL_IDENT ($1).str);
		IDL_tree_free ($1);
		YYABORT;
	}
	IDL_tree_free ($1);
	assert (IDL_GENTREE (p).data != NULL);
	assert (IDL_IDENT (IDL_GENTREE (p).data)._ns_ref == p);
	++IDL_NODE_REFS (IDL_GENTREE (p).data);
	$$ = p;
}
	;

cur_ns_new_or_prev_ident:
			ident				{
	IDL_tree p;

	if ((p = IDL_ns_lookup_cur_scope (__IDL_root_ns, $1, NULL)) == NULL) {
		p = IDL_ns_place_new (__IDL_root_ns, $1);
		assert (p != NULL);
		assert (IDL_IDENT ($1)._ns_ref == p);
		if (__IDL_new_ident_comments != NULL) {
			assert (IDL_IDENT ($1).comments == NULL);
			IDL_IDENT ($1).comments = __IDL_new_ident_comments;
			__IDL_new_ident_comments = NULL;
		}
	} else {
		IDL_tree_free ($1);
		assert (IDL_GENTREE (p).data != NULL);
		assert (IDL_IDENT (IDL_GENTREE (p).data)._ns_ref == p);
	}
	++IDL_NODE_REFS (IDL_GENTREE (p).data);
	$$ = p;
}
	;

ns_global_ident:	ident				{
	IDL_tree p;

	if ((p = IDL_ns_lookup_this_scope (
		__IDL_root_ns,IDL_NS (__IDL_root_ns).file, $1, NULL)) == NULL) {
		yyerrorv ("`%s' undeclared identifier", IDL_IDENT ($1).str);
		IDL_tree_free ($1);
		YYABORT;
	}
	IDL_tree_free ($1);
	assert (IDL_GENTREE (p).data != NULL);
	assert (IDL_IDENT (IDL_GENTREE (p).data)._ns_ref == p);
	++IDL_NODE_REFS (IDL_GENTREE (p).data);
	$$ = p;
}
	;

string_lit_list:	string_lit			{ $$ = list_start ($1, TRUE); }
|			string_lit_list
			check_comma string_lit		{ $$ = list_chain ($1, $3, TRUE); }
	;

positive_int_const:	const_exp			{
	IDL_tree literal, ident = NULL;
	IDL_longlong_t value = 0;

	if ((literal = IDL_resolve_const_exp ($1, IDLN_INTEGER))) {
		assert (IDL_NODE_TYPE (literal) == IDLN_INTEGER);
		value = IDL_INTEGER (literal).value;
		IDL_tree_free ($1);
	}

	if (literal && IDL_NODE_UP (literal) &&
	    IDL_NODE_TYPE (IDL_NODE_UP (literal)) == IDLN_CONST_DCL)
		ident = IDL_CONST_DCL (IDL_NODE_UP (literal)).ident;
	
	if (literal == NULL) {
		if (!(__IDL_flags & IDLF_NO_EVAL_CONST))
			yyerror ("Could not resolve constant expression");
		$$ = $1;
	} else if (value == 0) {
		yyerror ("Zero array size is illegal");
		if (ident)
			IDL_tree_error (ident, "From constant declared here");
		$$ = NULL;
	} else if (value < 0) {
		yywarningv (IDL_WARNING1, "Cannot use negative value %"
			    IDL_LL "d, using %" IDL_LL "d",
			   value, -value);
		if (ident)
			IDL_tree_warning (ident,
					  IDL_WARNING1, "From constant declared here");
		$$ = IDL_integer_new (-value);
	}
	else
		$$ = IDL_integer_new (value);
}
	;

z_declspec:		/* empty */			{ $$ = 0; }
|			TOK_DECLSPEC			{
	$$ = IDL_parse_declspec ($1);
	g_free ($1);
}
	;

z_props:		/* empty */			{ $$ = NULL; }
|			'['				{
	/* Enable property scanning */
	if (__IDL_flags & IDLF_PROPERTIES)
		__IDL_flagsi |= IDLFP_PROPERTIES;
	else {
		yyerror ("Property syntax not enabled");
		YYABORT;
	}
}			prop_hash
			']'				{ $$ = $3; }
	;

integer_lit:		TOK_INTEGER			{ $$ = IDL_integer_new ($1); }
	;

string_lit:		dqstring_cat			{ $$ = IDL_string_new ($1); }
	;

char_lit:		sqstring			{ $$ = IDL_char_new ($1); }
	;

fixed_pt_lit:		TOK_FIXEDP			{ $$ = IDL_fixed_new ($1); }
	;

floating_pt_lit:	TOK_FLOATP			{ $$ = IDL_float_new ($1); }
	;

boolean_lit:		TOK_TRUE			{ $$ = IDL_boolean_new (TRUE); }
|			TOK_FALSE			{ $$ = IDL_boolean_new (FALSE); }
	;

codefrag:		z_declspec TOK_CODEFRAG		{
	$$ = $2;
	IDL_NODE_DECLSPEC ($$) = $1;	
	if (__IDL_inhibits > 0)
		IDL_NODE_DECLSPEC ($$) |= IDLF_DECLSPEC_EXIST | IDLF_DECLSPEC_INHIBIT;
}
	;

dqstring_cat:		dqstring
|			dqstring_cat dqstring		{
	char *catstr = g_malloc (strlen ($1) + strlen ($2) + 1);
	strcpy (catstr, $1); g_free ($1);
	strcat (catstr, $2); g_free ($2);
	$$ = catstr;
}
	;

dqstring:		TOK_DQSTRING			{
	char *s = IDL_do_escapes ($1);
	g_free ($1);
	$$ = s;
}
	;

sqstring:		TOK_SQSTRING			{
	char *s = IDL_do_escapes ($1);
	g_free ($1);
	$$ = s;
}
	;

%%

static const char *IDL_ns_get_cur_prefix (IDL_ns ns)
{
	IDL_tree p;

	p = IDL_NS (ns).current;

	assert (p != NULL);

	while (p && !IDL_GENTREE (p)._cur_prefix)
		p = IDL_NODE_UP (p);

	return p ? IDL_GENTREE (p)._cur_prefix : NULL;
}

gchar *IDL_ns_ident_make_repo_id (IDL_ns ns, IDL_tree p,
				  const char *p_prefix, int *major, int *minor)
{
	GString *s = g_string_new (NULL);
	const char *prefix;
	char *q;

	assert (p != NULL);
	
	if (IDL_NODE_TYPE (p) == IDLN_IDENT)
		p = IDL_IDENT_TO_NS (p);

	assert (p != NULL);

	prefix = p_prefix ? p_prefix : IDL_ns_get_cur_prefix (ns);

	q = IDL_ns_ident_to_qstring (p, "/", 0);
	g_string_sprintf (s, "IDL:%s%s%s:%d.%d",
			  prefix ? prefix : "",
			  prefix && *prefix ? "/" : "",
			  q,
			  major ? *major : 1,
			  minor ? *minor : 0);
	g_free (q);

	q = s->str;
	g_string_free (s, FALSE);

	return q;
}

static const char *get_name_token (const char *s, char **tok)
{
	const char *begin = s;
	int state = 0;

	if (!s)
		return NULL;

	while (isspace (*s)) ++s;
	
	while (1) switch (state) {
	case 0:		/* Unknown */
		if (*s == ':')
			state = 1;
		else if (isalnum (*s) || *s == '_') {
			begin = s;
			state = 2;
		} else
			return NULL;
		break;
	case 1:		/* Scope */
		if (strncmp (s, "::", 2) == 0) {
			char *r = g_malloc (3);
			strcpy (r, "::");
			*tok = r;
			return s + 2;
		} else	/* Invalid */
			return NULL;
		break;
	case 2:
		if (isalnum (*s) || *s == '_')
			++s;
		else {
			char *r = g_malloc (s - begin + 1);
			strncpy (r, begin, s - begin + 1);
			r[s - begin] = 0;
			*tok = r;
			return s;
		}
		break;
	}
}

static IDL_tree IDL_ns_pragma_parse_name (IDL_ns ns, const char *s)
{
	IDL_tree p = IDL_NS (ns).current;
	int start = 1;
	char *tok;

	while (p && *s && (s = get_name_token (s, &tok))) {
		if (tok == NULL)
			return NULL;
		if (strcmp (tok, "::") == 0) {
			if (start) {
				/* Globally scoped */
				p = IDL_NS (ns).file;
			}
			g_free (tok);
		} else {
			IDL_tree ident = IDL_ident_new (tok);
			p = IDL_ns_lookup_this_scope (__IDL_root_ns, p, ident, NULL);
			IDL_tree_free (ident);
		}
		start = 0;
	}
	
	return p;
}

void IDL_ns_ID (IDL_ns ns, const char *s)
{
	char name[1024], id[1024];
	IDL_tree p, ident;
	int n;

	n = sscanf (s, "%1023s \"%1023s\"", name, id);
	if (n < 2 && __IDL_is_parsing) {
		yywarning (IDL_WARNING1, "Malformed pragma ID");
		return;
	}
	if (id[strlen (id) - 1] == '"')
		id[strlen (id) - 1] = 0;

	p = IDL_ns_pragma_parse_name (__IDL_root_ns, name);
	if (!p && __IDL_is_parsing) {
		yywarningv (IDL_WARNING1, "Unknown identifier `%s' in pragma ID", name);
		return;
	}

	/* We have resolved the identifier, so assign the repo id */
	assert (IDL_NODE_TYPE (p) == IDLN_GENTREE);
	assert (IDL_GENTREE (p).data != NULL);
	assert (IDL_NODE_TYPE (IDL_GENTREE (p).data) == IDLN_IDENT);
	ident = IDL_GENTREE (p).data;

	if (IDL_IDENT_REPO_ID (ident) != NULL)
		g_free (IDL_IDENT_REPO_ID (ident));

	IDL_IDENT_REPO_ID (ident) = g_strdup (id);
}

void IDL_ns_version (IDL_ns ns, const char *s)
{
	char name[1024];
	int n, major, minor;
	IDL_tree p, ident;

	n = sscanf (s, "%1023s %u %u", name, &major, &minor);
	if (n < 3 && __IDL_is_parsing) {
		yywarning (IDL_WARNING1, "Malformed pragma version");
		return;
	}

	p = IDL_ns_pragma_parse_name (__IDL_root_ns, name);
	if (!p && __IDL_is_parsing) {
		yywarningv (IDL_WARNING1, "Unknown identifier `%s' in pragma version", name);
		return;
	}

	/* We have resolved the identifier, so assign the repo id */
	assert (IDL_NODE_TYPE (p) == IDLN_GENTREE);
	assert (IDL_GENTREE (p).data != NULL);
	assert (IDL_NODE_TYPE (IDL_GENTREE (p).data) == IDLN_IDENT);
	ident = IDL_GENTREE (p).data;

	if (IDL_IDENT_REPO_ID (ident) != NULL) {
		char *v = strrchr (IDL_IDENT_REPO_ID (ident), ':');
		if (v) {
			GString *s;

			*v = 0;
			s = g_string_new (NULL);
			g_string_sprintf (s, "%s:%d.%d",
					  IDL_IDENT_REPO_ID (ident), major, minor);
			g_free (IDL_IDENT_REPO_ID (ident));
			IDL_IDENT_REPO_ID (ident) = s->str;
			g_string_free (s, FALSE);
		} else if (__IDL_is_parsing)
			yywarningv (IDL_WARNING1, "Cannot find RepositoryID OMG IDL version in ID `%s'",
				    IDL_IDENT_REPO_ID (ident));
	} else
		IDL_IDENT_REPO_ID (ident) =
			IDL_ns_ident_make_repo_id (
				__IDL_root_ns, p, NULL, &major, &minor);
}

int IDL_inhibit_get (void)
{
	g_return_val_if_fail (__IDL_is_parsing, -1);

	return __IDL_inhibits;
}

void IDL_inhibit_push (void)
{
	g_return_if_fail (__IDL_is_parsing);

	++__IDL_inhibits;
}

void IDL_inhibit_pop (void)
{
	g_return_if_fail (__IDL_is_parsing);

	if (--__IDL_inhibits < 0)
		__IDL_inhibits = 0;
}

static void IDL_inhibit (IDL_ns ns, const char *s)
{
	if (g_strcasecmp ("push", s) == 0)
		IDL_inhibit_push ();
	else if (g_strcasecmp ("pop", s) == 0)
		IDL_inhibit_pop ();
}

void __IDL_do_pragma (const char *s)
{
	int n;
	char directive[256];

	g_return_if_fail (__IDL_is_parsing);
	g_return_if_fail (s != NULL);

	if (sscanf (s, "%255s%n", directive, &n) < 1)
		return;
	s += n;
	while (isspace (*s)) ++s;

	if (strcmp (directive, "prefix") == 0)
		IDL_ns_prefix (__IDL_root_ns, s);
	else if (strcmp (directive, "ID") == 0)
		IDL_ns_ID (__IDL_root_ns, s);
	else if (strcmp (directive, "version") == 0)
		IDL_ns_version (__IDL_root_ns, s);
	else if (strcmp (directive, "inhibit") == 0)
		IDL_inhibit (__IDL_root_ns, s);
}

static IDL_declspec_t IDL_parse_declspec (const char *strspec)
{
	IDL_declspec_t flags = IDLF_DECLSPEC_EXIST;

	if (strspec == NULL)
		return flags;

	if (strcmp (strspec, "inhibit") == 0)
		flags |= IDLF_DECLSPEC_INHIBIT;
	else if (__IDL_is_parsing)
		yywarningv (IDL_WARNING1, "Ignoring unknown declspec `%s'", strspec);

	return flags;
}

void IDL_file_set (const char *filename, int line)
{
	IDL_fileinfo *fi;
	char *orig;

	g_return_if_fail (__IDL_is_parsing);

	if (filename) {
		__IDL_cur_filename = g_strdup (filename);

		if (
#ifdef HAVE_CPP_PIPE_STDIN
			!strlen (__IDL_cur_filename)
#else
			__IDL_tmp_filename &&
			!strcmp (__IDL_cur_filename, __IDL_tmp_filename)
#endif
			) {
			g_free (__IDL_cur_filename);
			__IDL_cur_filename = g_strdup (__IDL_real_filename);
		}

		if (g_hash_table_lookup_extended (__IDL_filename_hash, __IDL_cur_filename,
						  (gpointer) &orig, (gpointer) &fi)) {
			g_free (__IDL_cur_filename);
			__IDL_cur_filename = orig;
			__IDL_cur_fileinfo = fi;
		} else {
			fi = g_new0 (IDL_fileinfo, 1);
			__IDL_cur_fileinfo = fi;
			g_hash_table_insert (__IDL_filename_hash, __IDL_cur_filename, fi);
		}
	}

	if (__IDL_cur_line > 0)
		__IDL_cur_line = line;
}

void IDL_file_get (const char **filename, int *line)
{
	g_return_if_fail (__IDL_is_parsing);

	if (filename)
		*filename = __IDL_cur_filename;

	if (line)
		*line = __IDL_cur_line;
}

static int do_token_error (IDL_tree p, const char *message, gboolean prev)
{
	int dienow;
	char *what = NULL, *who = NULL;

	assert (p != NULL);

	dienow = IDL_tree_get_node_info (p, &what, &who);

	assert (what != NULL);
	
	if (who && *who)
		IDL_tree_error (p, "%s %s `%s'", message, what, who);
	else
		IDL_tree_error (p, "%s %s", message, what);
	
	return dienow;
}

static void illegal_context_type_error (IDL_tree p, const char *what)
{
	GString *s = g_string_new (NULL);

	g_string_sprintf (s, "Illegal type `%%s' for %s", what);
	illegal_type_error (p, s->str);
	g_string_free (s, TRUE);
}

static void illegal_type_error (IDL_tree p, const char *message)
{
	GString *s;

	s = IDL_tree_to_IDL_string (p, NULL, IDLF_OUTPUT_NO_NEWLINES);
	yyerrorv (message, s->str);
	g_string_free (s, TRUE);
}

static IDL_tree list_start (IDL_tree a, gboolean filter_null)
{
	IDL_tree p;

	if (!a && filter_null)
		return NULL;

	p = IDL_list_new (a);

	return p;
}

static IDL_tree list_chain (IDL_tree a, IDL_tree b, gboolean filter_null)
{
	IDL_tree p;

	if (filter_null) {
		if (!b)
			return a;
		if (!a)
			return list_start (b, filter_null);
	}

	p = IDL_list_new (b);
	a = IDL_list_concat (a, p);

	return a;
}

static IDL_tree zlist_chain (IDL_tree a, IDL_tree b, gboolean filter_null)
{
	if (a == NULL)
		return list_start (b, filter_null);
	else
		return list_chain (a, b, filter_null);
}

static int IDL_binop_chktypes (enum IDL_binop op, IDL_tree a, IDL_tree b)
{
	if (IDL_NODE_TYPE (a) != IDLN_BINOP &&
	    IDL_NODE_TYPE (b) != IDLN_BINOP &&
	    IDL_NODE_TYPE (a) != IDLN_UNARYOP &&
	    IDL_NODE_TYPE (b) != IDLN_UNARYOP &&
	    IDL_NODE_TYPE (a) != IDL_NODE_TYPE (b)) {
		yyerror ("Invalid mix of types in constant expression");
		return -1;
	}

	switch (op) {
	case IDL_BINOP_MULT:
	case IDL_BINOP_DIV:
	case IDL_BINOP_ADD:
	case IDL_BINOP_SUB:
		break;

	case IDL_BINOP_MOD:
	case IDL_BINOP_SHR:
	case IDL_BINOP_SHL:
	case IDL_BINOP_AND:
	case IDL_BINOP_OR:
	case IDL_BINOP_XOR:
		if ((IDL_NODE_TYPE (a) != IDLN_INTEGER ||
		     IDL_NODE_TYPE (b) != IDLN_INTEGER) &&
		    !(IDL_NODE_TYPE (a) == IDLN_BINOP ||
		      IDL_NODE_TYPE (b) == IDLN_BINOP ||
		      IDL_NODE_TYPE (a) == IDLN_UNARYOP ||
		      IDL_NODE_TYPE (b) == IDLN_UNARYOP)) {
			yyerror ("Invalid operation on non-integer value");
			return -1;
		}
		break;
	}

	return 0;
}

static int IDL_unaryop_chktypes (enum IDL_unaryop op, IDL_tree a)
{
	switch (op) {
	case IDL_UNARYOP_PLUS:
	case IDL_UNARYOP_MINUS:
		break;

	case IDL_UNARYOP_COMPLEMENT:
		if (IDL_NODE_TYPE (a) != IDLN_INTEGER &&
		    !(IDL_NODE_TYPE (a) == IDLN_BINOP ||
		      IDL_NODE_TYPE (a) == IDLN_UNARYOP)) {
			yyerror ("Operand to complement must be integer");
			return -1;
		}
		break;
	}

	return 0;
}

static IDL_tree IDL_binop_eval_integer (enum IDL_binop op, IDL_tree a, IDL_tree b)
{
	IDL_tree p = NULL;

	assert (IDL_NODE_TYPE (a) == IDLN_INTEGER);

	switch (op) {
	case IDL_BINOP_MULT:
		p = IDL_integer_new (IDL_INTEGER (a).value * IDL_INTEGER (b).value);
		break;

	case IDL_BINOP_DIV:
		if (IDL_INTEGER (b).value == 0) {
			yyerror ("Divide by zero in constant expression");
			return NULL;
		}
		p = IDL_integer_new (IDL_INTEGER (a).value / IDL_INTEGER (b).value);
		break;

	case IDL_BINOP_ADD:
		p = IDL_integer_new (IDL_INTEGER (a).value + IDL_INTEGER (b).value);
		break;

	case IDL_BINOP_SUB:
		p = IDL_integer_new (IDL_INTEGER (a).value - IDL_INTEGER (b).value);
		break;

	case IDL_BINOP_MOD:
		if (IDL_INTEGER (b).value == 0) {
			yyerror ("Modulo by zero in constant expression");
			return NULL;
		}
		p = IDL_integer_new (IDL_INTEGER (a).value % IDL_INTEGER (b).value);
		break;

	case IDL_BINOP_SHR:
		p = IDL_integer_new (IDL_INTEGER (a).value >> IDL_INTEGER (b).value);
		break;

	case IDL_BINOP_SHL:
		p = IDL_integer_new (IDL_INTEGER (a).value << IDL_INTEGER (b).value);
		break;

	case IDL_BINOP_AND:
		p = IDL_integer_new (IDL_INTEGER (a).value & IDL_INTEGER (b).value);
		break;

	case IDL_BINOP_OR:
		p = IDL_integer_new (IDL_INTEGER (a).value | IDL_INTEGER (b).value);
		break;

	case IDL_BINOP_XOR:
		p = IDL_integer_new (IDL_INTEGER (a).value ^ IDL_INTEGER (b).value);
		break;
	}

	return p;
}

static IDL_tree IDL_binop_eval_float (enum IDL_binop op, IDL_tree a, IDL_tree b)
{
	IDL_tree p = NULL;

	assert (IDL_NODE_TYPE (a) == IDLN_FLOAT);

	switch (op) {
	case IDL_BINOP_MULT:
		p = IDL_float_new (IDL_FLOAT (a).value * IDL_FLOAT (b).value);
		break;

	case IDL_BINOP_DIV:
		if (IDL_FLOAT (b).value == 0.0) {
			yyerror ("Divide by zero in constant expression");
			return NULL;
		}
		p = IDL_float_new (IDL_FLOAT (a).value / IDL_FLOAT (b).value);
		break;

	case IDL_BINOP_ADD:
		p = IDL_float_new (IDL_FLOAT (a).value + IDL_FLOAT (b).value);
		break;

	case IDL_BINOP_SUB:
		p = IDL_float_new (IDL_FLOAT (a).value - IDL_FLOAT (b).value);
		break;

	default:
		break;
	}

	return p;
}

static IDL_tree IDL_binop_eval (enum IDL_binop op, IDL_tree a, IDL_tree b)
{
	assert (IDL_NODE_TYPE (a) == IDL_NODE_TYPE (b));

	switch (IDL_NODE_TYPE (a)) {
	case IDLN_INTEGER: return IDL_binop_eval_integer (op, a, b);
	case IDLN_FLOAT: return IDL_binop_eval_float (op, a, b);
	default: return NULL;
	}
}

static IDL_tree IDL_unaryop_eval_integer (enum IDL_unaryop op, IDL_tree a)
{
	IDL_tree p = NULL;

	assert (IDL_NODE_TYPE (a) == IDLN_INTEGER);

	switch (op) {
	case IDL_UNARYOP_PLUS:
		p = IDL_integer_new (IDL_INTEGER (a).value);
		break;

	case IDL_UNARYOP_MINUS:
		p = IDL_integer_new (-IDL_INTEGER (a).value);
		break;

	case IDL_UNARYOP_COMPLEMENT:
		p = IDL_integer_new (~IDL_INTEGER (a).value);
		break;
	}
       
	return p;
}

static IDL_tree IDL_unaryop_eval_fixed (enum IDL_unaryop op, IDL_tree a)
{
	IDL_tree p = NULL;

	assert (IDL_NODE_TYPE (a) == IDLN_FIXED);

	switch (op) {
	case IDL_UNARYOP_PLUS:
		p = IDL_fixed_new (IDL_FIXED (a).value);
		break;

	default:
		break;
	}
       
	return p;
}

static IDL_tree IDL_unaryop_eval_float (enum IDL_unaryop op, IDL_tree a)
{
	IDL_tree p = NULL;

	assert (IDL_NODE_TYPE (a) == IDLN_FLOAT);

	switch (op) {
	case IDL_UNARYOP_PLUS:
		p = IDL_float_new (IDL_FLOAT (a).value);
		break;

	case IDL_UNARYOP_MINUS:
		p = IDL_float_new (-IDL_FLOAT (a).value);
		break;

	default:
		break;
	}
       
	return p;
}

static IDL_tree IDL_unaryop_eval (enum IDL_unaryop op, IDL_tree a)
{
	switch (IDL_NODE_TYPE (a)) {
	case IDLN_INTEGER: return IDL_unaryop_eval_integer (op, a);
	case IDLN_FIXED: return IDL_unaryop_eval_fixed (op, a);
	case IDLN_FLOAT: return IDL_unaryop_eval_float (op, a);
	default: return NULL;
	}
}

IDL_tree IDL_resolve_const_exp (IDL_tree p, IDL_tree_type type)
{
	gboolean resolved_value = FALSE, die = FALSE;
	gboolean wrong_type = FALSE;

	while (!resolved_value && !die) {
		if (IDL_NODE_TYPE (p) == IDLN_IDENT) {
			IDL_tree q = IDL_NODE_UP (p);
			
			assert (q != NULL);
			if (IDL_NODE_UP (q) &&
			    IDL_NODE_TYPE (IDL_NODE_UP (q)) == IDLN_TYPE_ENUM) {
				p = q;
				die = TRUE;
				break;
			} else if (IDL_NODE_TYPE (q) != IDLN_CONST_DCL) {
				p = q;
				wrong_type = TRUE;
				die = TRUE;
			} else
 				p = IDL_CONST_DCL (q).const_exp;
		}
		
		if (p == NULL ||
		    IDL_NODE_TYPE (p) == IDLN_BINOP ||
		    IDL_NODE_TYPE (p) == IDLN_UNARYOP) {
			die = TRUE;
			continue;
		}
		
		resolved_value = IDL_NODE_IS_LITERAL (p);
	}

	if (resolved_value &&
	    type != IDLN_ANY &&
	    IDL_NODE_TYPE (p) != type)
		wrong_type = TRUE;
	
	if (wrong_type) {
		yyerror ("Invalid type for constant");
		IDL_tree_error (p, "Previous resolved type declaration");
		return NULL;
	}

	return resolved_value ? p : NULL;
}

void IDL_queue_new_ident_comment (const char *str)
{
	g_return_if_fail (str != NULL);

	__IDL_new_ident_comments = g_slist_append (__IDL_new_ident_comments, g_strdup (str));
}

/*
 * Local variables:
 * mode: C
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 */
