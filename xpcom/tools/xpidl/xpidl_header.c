#include "xpidl.h"

static gboolean
ident(TreeState *state)
{
    printf("%s", IDL_IDENT(state->tree).str);
    return TRUE;
}

static gboolean
interface(TreeState *state)
{
    printf("starting interface %s\n", IDL_INTERFACE(state->tree).ident);
    state->tree = IDL_INTERFACE(state->tree).body;
    return process_node(state);
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
type_integer(TreeState *state)
{
    IDL_tree p = state->tree;

    if (!IDL_TYPE_INTEGER(p).f_signed)
	fputs("unsigned ", stdout);

    switch(IDL_TYPE_INTEGER(p).f_type) {
      case IDL_INTEGER_TYPE_SHORT:
	printf("short");
	break;
      case IDL_INTEGER_TYPE_LONG:
	printf("long");
	break;
      case IDL_INTEGER_TYPE_LONGLONG:
	printf("longlong");
	break;
    }
    return TRUE;
}

static gboolean
type(TreeState *state)
{
    if (!state->tree) {
	fputs("void", stdout);
	return TRUE;
    }

    switch(IDL_NODE_TYPE(state->tree)) {
      case IDLN_TYPE_INTEGER:
	return type_integer(state);
      case IDLN_TYPE_STRING:
	fputs("string", stdout);
	return TRUE;
      default:
	printf("unknown_type_%d", IDL_NODE_TYPE(state->tree));
	return TRUE;
    }
}

static gboolean
param_dcls(TreeState *state)
{
    IDL_tree iter;
    fputs("(", stdout);
    for (iter = state->tree; iter; iter = IDL_LIST(iter).next) {
	struct _IDL_PARAM_DCL decl = IDL_PARAM_DCL(IDL_LIST(iter).data);
	switch(decl.attr) {
	  case IDL_PARAM_IN:
	    fputs("in ", stdout);
	    break;
	  case IDL_PARAM_OUT:
	    fputs("out ", stdout);
	    break;
	  case IDL_PARAM_INOUT:
	    fputs("inout ", stdout);
	    break;
	  default:;
	}
	state->tree = (IDL_tree)decl.param_type_spec;
	if (!type(state))
	    return FALSE;
	fputs(" ", stdout);
	state->tree = (IDL_tree)decl.simple_declarator;
	if (!process_node(state))
	    return FALSE;
	if (IDL_LIST(iter).next)
	    fputs(", ", stdout);
    }
    fputs(")", stdout);
    return TRUE;
}

static gboolean
attr_dcl(TreeState *state)
{
    IDL_tree orig = state->tree;
    printf("%sattribute ", IDL_ATTR_DCL(state->tree).f_readonly ?
	   "readonly " : "");
    state->tree = IDL_ATTR_DCL(state->tree).param_type_spec;
    if (state->tree && !process_node(state))
	return FALSE;
    fputs(" ", stdout);
    state->tree = IDL_ATTR_DCL(orig).simple_declarations;
    if (state->tree && !process_node(state))
	return FALSE;
    puts(";");
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
    state->tree = op.op_type_spec;
    if (!type(state))
	return FALSE;
    fputs(" ", stdout);
    state->tree = op.ident;
    if (state->tree && !process_node(state))
	return FALSE;
    state->tree = op.parameter_dcls;
    if (!param_dcls(state))
	return FALSE;
    fputs(";\n", stdout);
    return TRUE;
}

nodeHandler headerDispatch[] = {
  NULL,            /* IDLN_NONE */
  NULL,            /* IDLN_ANY */
  list,            /* IDLN_LIST */
  NULL,            /* IDLN_GENTREE */
  NULL,            /* IDLN_INTEGER */
  NULL,            /* IDLN_STRING */
  NULL,            /* IDLN_WIDE_STRING */
  NULL,            /* IDLN_CHAR */
  NULL,            /* IDLN_WIDE_CHAR */
  NULL,            /* IDLN_FIXED */
  NULL,            /* IDLN_FLOAT */
  NULL,            /* IDLN_BOOLEAN */
  ident,           /* IDLN_IDENT */
  NULL,            /* IDLN_TYPE_DCL */
  NULL,            /* IDLN_CONST_DCL */
  NULL,            /* IDLN_EXCEPT_DCL */
  attr_dcl,        /* IDLN_ATTR_DCL */
  op_dcl,          /* IDLN_OP_DCL */
  param_dcls,      /* IDLN_PARAM_DCL */
  NULL,            /* IDLN_FORWARD_DCL */
  type_integer,    /* IDLN_TYPE_INTEGER */
  type,            /* IDLN_TYPE_FLOAT */
  type,            /* IDLN_TYPE_FIXED */
  type,            /* IDLN_TYPE_CHAR */
  type,            /* IDLN_TYPE_WIDE_CHAR */
  type,            /* IDLN_TYPE_STRING */
  type,            /* IDLN_TYPE_WIDE_STRING */
  type,            /* IDLN_TYPE_BOOLEAN */
  type,            /* IDLN_TYPE_OCTET */
  type,            /* IDLN_TYPE_ANY */
  type,            /* IDLN_TYPE_OBJECT */
  type,            /* IDLN_TYPE_ENUM */
  type,            /* IDLN_TYPE_SEQUENCE */
  type,            /* IDLN_TYPE_ARRAY */
  type,            /* IDLN_TYPE_STRUCT */
  type,            /* IDLN_TYPE_UNION */
  NULL,            /* IDLN_MEMBER */
  NULL,            /* IDLN_NATIVE */
  NULL,            /* IDLN_CASE_STMT */
  interface,       /* IDLN_INTERFACE */
  NULL,            /* IDLN_MODULE */
  NULL,            /* IDLN_BINOP */
  NULL,            /* IDLN_UNARYOP */
  NULL             /* IDLN_TYPE_TYPECODE */
};
