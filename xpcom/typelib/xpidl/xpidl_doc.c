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
