/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef query_h___
#define query_h___

/* the RDF Query API. */

#include "rdf.h"

typedef uint8 RDF_TermType; /* type of a term */

/* values of RDF_TermType */
#define RDF_VARIABLE_TERM_TYPE 1
#define RDF_RESOURCE_TERM_TYPE 2
#define RDF_CONSTANT_TERM_TYPE 3

typedef struct _RDF_VariableStruc {
	void* value;
	RDF_ValueType type;
	char* id;
	PRBool isResult;
	struct _RDF_QueryStruc* query;
} RDF_VariableStruc;

typedef RDF_VariableStruc* RDF_Variable;

typedef struct _RDF_VariableListStruc {
	RDF_Variable element;
	struct _RDF_VariableListStruc* next;
} RDF_VariableListStruc;

typedef RDF_VariableListStruc* RDF_VariableList;

PR_PUBLIC_API(PRBool) RDF_IsResultVariable(RDF_Variable var);
PR_PUBLIC_API(void) RDF_SetResultVariable(RDF_Variable var, PRBool isResult);
PR_PUBLIC_API(void*) RDF_GetVariableValue(RDF_Variable var);
PR_PUBLIC_API(void) RDF_SetVariableValue(RDF_Variable var, void* value, RDF_ValueType type);

typedef struct _Term {
	void* value;
	RDF_TermType type;
} TermStruc;

typedef struct TermStruc* RDF_Term;

typedef struct _LiteralStruc {
	TermStruc u;
	RDF_Resource s;
	TermStruc* v;
	uint8 valueCount; /* disjunction */
	RDF_Cursor c;
	PRBool tv;
	PRBool bt;
	RDF_Variable variable;
	RDF_ValueType valueType;
} LiteralStruc;

typedef LiteralStruc* RDF_Literal;

typedef struct _RDF_LiteralListStruc {
	RDF_Literal element;
	struct _RDF_LiteralListStruc* next;
} RDF_LiteralListStruc;

typedef RDF_LiteralListStruc* RDF_LiteralList;

typedef struct _RDF_QueryStruc {
  struct _RDF_VariableListStruc* variables;
  struct _RDF_LiteralListStruc* literals;
  uint16 numLiterals;
  uint16 index;
  PRBool conjunctsOrdered;
  PRBool queryRunning;
  RDF rdf;
} RDF_QueryStruc;

typedef RDF_QueryStruc* RDF_Query;

/* Note: the API assumes that the query parameter is non-NULL. */
PR_PUBLIC_API(RDF_Query) RDF_CreateQuery(RDF rdf);
PR_PUBLIC_API(void) RDF_DestroyQuery(RDF_Query q);
PR_PUBLIC_API(RDF_Variable) RDF_GetVariable(RDF_Query q, char* name);
PR_PUBLIC_API(PRBool) RDF_RunQuery (RDF_Query q);
PR_PUBLIC_API(PRBool) RDF_GetNextElement(RDF_Query q);
PR_PUBLIC_API(void) RDF_EndQuery (RDF_Query q);
PR_PUBLIC_API(RDF_VariableList) RDF_GetVariableList(RDF_Query q);
PR_PUBLIC_API(uint16) RDF_GetVariableListCount(RDF_Query q);
PR_PUBLIC_API(RDF_Variable) RDF_GetNthVariable(RDF_Query q, uint16 index);
PR_PUBLIC_API(RDF_Error) RDF_AddConjunctVRV(RDF_Query q, RDF_Variable arg1, RDF_Resource s, RDF_Variable arg2, RDF_ValueType type);
PR_PUBLIC_API(RDF_Error) RDF_AddConjunctVRO(RDF_Query q, RDF_Variable arg1, RDF_Resource s, void* arg2, RDF_ValueType type);
PR_PUBLIC_API(RDF_Error) RDF_AddConjunctRRO(RDF_Query q, RDF_Resource arg1, RDF_Resource s, void* arg2, RDF_ValueType type);
PR_PUBLIC_API(RDF_Error) RDF_AddConjunctVRR(RDF_Query q, RDF_Variable arg1, RDF_Resource s, RDF_Resource arg2);
PR_PUBLIC_API(RDF_Error) RDF_AddConjunctRRV(RDF_Query q, RDF_Resource arg1, RDF_Resource s, RDF_Variable arg2, RDF_ValueType type);
PR_PUBLIC_API(RDF_Error) RDF_AddConjunctVRL(RDF_Query q, RDF_Variable arg1, RDF_Resource s, RDF_Term arg2, RDF_ValueType type, uint8 count);

#endif /* query_h___ */

