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
    char *className =
	IDL_ns_ident_to_qstring(IDL_IDENT_TO_NS(IDL_INTERFACE(state->tree).ident), "_", 0);
    fprintf(state->file, "/* starting interface %s */\n",
	    className);

    fprintf(state->file, "class %s {\n", className);

    state->tree = IDL_INTERFACE(state->tree).body;

    free(className);
    if (!process_node(state))
	return FALSE;

    fprintf(state->file, "\n}\n");

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
      case IDLN_TYPE_STRING:
	fputs("nsString *", state->file);
	break;
      case IDLN_TYPE_BOOLEAN:
	fputs("PRBool", state->file);
	break;
      case IDLN_IDENT:
	fprintf(state->file, "%s *", IDL_IDENT(state->tree).str);
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
	printf("longlong");
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
      case IDLN_TYPE_BOOLEAN:
	fputs("boolean", state->file);
	return TRUE;
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
	fprintf(state->file, "  NS_IMETHOD Is%c%s(PRBool &aIs%c%s);\n",
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
	fprintf(state->file, " %sa%c%s);\n",
		getter ? "&" : "",
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
    if (state->tree && !process_node(state))
	return FALSE;
    fputs(" ", state->file);
    state->tree = IDL_ATTR_DCL(orig).simple_declarations;
    if (state->tree && !process_node(state))
	return FALSE;
    fputs("; */\n", state->file);

    state->tree = orig;
    return attr_accessor(state, TRUE) && (ro || attr_accessor(state, FALSE));
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
    if (!xpcom_type(state))
	return FALSE;
    fprintf(state->file, " %s",
	    IDL_PARAM_DCL(param).attr == IDL_PARAM_IN ? "" : "&");
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
    fputs(");\n", state->file);
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
