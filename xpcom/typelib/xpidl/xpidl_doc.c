#include "xpidl.h"

/*
 * Generates documentation from javadoc-style comments in XPIDL files.
 */

nodeHandler docDispatch[] = {
  NULL,            /* IDLN_NONE */
  NULL,            /* IDLN_ANY */
  NULL,            /* IDLN_LIST */
  NULL,            /* IDLN_GENTREE */
  NULL,            /* IDLN_INTEGER */
  NULL,            /* IDLN_STRING */
  NULL,            /* IDLN_WIDE_STRING */
  NULL,            /* IDLN_CHAR */
  NULL,            /* IDLN_WIDE_CHAR */
  NULL,            /* IDLN_FIXED */
  NULL,            /* IDLN_FLOAT */
  NULL,            /* IDLN_BOOLEAN */
  NULL,            /* IDLN_IDENT */
  NULL,            /* IDLN_TYPE_DCL */
  NULL,            /* IDLN_CONST_DCL */
  NULL,            /* IDLN_EXCEPT_DCL */
  NULL,            /* IDLN_ATTR_DCL */
  NULL,            /* IDLN_OP_DCL */
  NULL,            /* IDLN_PARAM_DCL */
  NULL,            /* IDLN_FORWARD_DCL */
  NULL,            /* IDLN_TYPE_INTEGER */
  NULL,            /* IDLN_TYPE_FLOAT */
  NULL,            /* IDLN_TYPE_FIXED */
  NULL,            /* IDLN_TYPE_CHAR */
  NULL,            /* IDLN_TYPE_WIDE_CHAR */
  NULL,            /* IDLN_TYPE_STRING */
  NULL,            /* IDLN_TYPE_WIDE_STRING */
  NULL,            /* IDLN_TYPE_BOOLEAN */
  NULL,            /* IDLN_TYPE_OCTET */
  NULL,            /* IDLN_TYPE_ANY */
  NULL,            /* IDLN_TYPE_OBJECT */
  NULL,            /* IDLN_TYPE_ENUM */
  NULL,            /* IDLN_TYPE_SEQUENCE */
  NULL,            /* IDLN_TYPE_ARRAY */
  NULL,            /* IDLN_TYPE_STRUCT */
  NULL,            /* IDLN_TYPE_UNION */
  NULL,            /* IDLN_MEMBER */
  NULL,            /* IDLN_NATIVE */
  NULL,            /* IDLN_CASE_STMT */
  NULL,            /* IDLN_INTERFACE */
  NULL,            /* IDLN_MODULE */
  NULL,            /* IDLN_BINOP */
  NULL,            /* IDLN_UNARYOP */
  NULL             /* IDLN_TYPE_TYPECODE */
};
