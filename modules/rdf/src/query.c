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

/* 
   This file implements query support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#ifdef XP_PC
#define strcasecomp strcmp
#endif

#include "query.h"
#include "rdf-int.h"

/* prototypes */
PRBool variableTermp(TermStruc term);
PRBool resourceTermp(TermStruc term);
PRBool constantTermp(TermStruc term);
RDF_Error addVariableToList(RDF_Variable variable, RDF_VariableList *list);
void destroyVariableList(RDF_VariableList list, PRBool destroyVariables);
void destroyLiteralList(RDF_LiteralList list, PRBool destroyLiterals);
PRBool comparisonp(RDF_Resource r);
PRBool specialResourcePredicate(RDF_Resource r);
PRBool findLiteral(RDF_Literal target, RDF_LiteralList list);
RDF_Literal getNthLiteral(RDF_Query q, uint16 index);
PRBool constantp(TermStruc term, RDF_VariableList knownVars);
RDF_Literal oneUnknownAs(RDF_Query q, RDF_LiteralList orderedLiterals, RDF_VariableList knownVars);
RDF_Literal noUnknownAs(RDF_Query q, RDF_LiteralList orderedLiterals, RDF_VariableList knownVars);
PRBool orderConjuncts(RDF_Query q);
PRBool processNextLiteral(RDF_Query q, RDF_Literal literal);
void addLiteral(RDF_Query q, RDF_Literal literal);

/* Variable API */
PR_PUBLIC_API(PRBool) RDF_IsResultVariable(RDF_Variable var) {
	return var->isResult;
}

PR_PUBLIC_API(void) RDF_SetResultVariable(RDF_Variable var, PRBool isResult) {
	var->isResult = isResult;
}

PR_PUBLIC_API(void*) RDF_GetVariableValue(RDF_Variable var) {
	if (var->query->queryRunning)
		return var->value;
	else return NULL;
}

PR_PUBLIC_API(void) RDF_SetVariableValue(RDF_Variable var, void* value, RDF_ValueType type) {
	if (!var->query->queryRunning) var->query->conjunctsOrdered = false;
	var->value = value;
	var->type = type;
}

PRBool variableTermp(TermStruc term) {
	return term.type == RDF_VARIABLE_TERM_TYPE;
}

PRBool resourceTermp(TermStruc term) {
	return term.type == RDF_RESOURCE_TERM_TYPE;
}

PRBool constantTermp(TermStruc term) {
	return term.type == RDF_CONSTANT_TERM_TYPE;
}

/* Adds the variable to the variable list if it isn't already on the list. */
RDF_Error addVariableToList(RDF_Variable variable, RDF_VariableList *list) {
	PRBool found = false;
	RDF_VariableList current = *list;
	while (current != NULL) {
		if (current->element == variable) {
			found = true;
			break;
		}
		current = current->next;
	}
	if (!found) {
		RDF_VariableList newlist = (RDF_VariableList)getMem(sizeof(RDF_VariableListStruc));
		if (newlist == NULL) return RDF_NO_MEMORY;
		newlist->element = variable;
		newlist->next = *list;
		*list = newlist;
	}
	return noRDFErr;
}

void destroyVariableList(RDF_VariableList list, PRBool destroyVariables) {
	if (list != NULL) {
		destroyVariableList(list->next, destroyVariables);
		if (destroyVariables) freeMem(list->element);	/* free the variable */
		freeMem(list);									/* free the list structure */
	}
}

void destroyLiteralList(RDF_LiteralList list, PRBool destroyLiterals) {
	if (list != NULL) {
		destroyLiteralList(list->next, destroyLiterals);
		if (destroyLiterals) {
			RDF_Literal literal = list->element;
			if (literal->c != NULL) RDF_DisposeCursor(literal->c);
			if (literal->valueType == RDF_STRING_TYPE) {
				int i;
				for (i = 0; i < literal->valueCount; i++) {
					TermStruc term = *(literal->v + i);
					if (constantTermp(term)) freeMem(term.value);
				}
			}
			freeMem(literal->v);
			freeMem(list->element);	/* free the literal */
		}
		freeMem(list);									/* free the list structure */
	}
}

PRBool comparisonp(RDF_Resource r) {
	return (r == gCoreVocab->RDF_substring || 
			r == gCoreVocab->RDF_notSubstring ||
			r == gCoreVocab->RDF_lessThan || 
			r == gCoreVocab->RDF_greaterThan ||
			r == gCoreVocab->RDF_lessThanOrEqual ||
			r == gCoreVocab->RDF_greaterThanOrEqual ||
			r == gCoreVocab->RDF_equals ||
			r == gCoreVocab->RDF_notEquals ||
			r == gCoreVocab->RDF_stringEquals ||
			r == gCoreVocab->RDF_notStringEquals);
}

PRBool specialResourcePredicate(RDF_Resource r) {
	return (r == gCoreVocab->RDF_notInstanceOf || r == gCoreVocab->RDF_notParent);
}

/* Creates and initializes a query. Returns NULL if not enough memory. */
PR_PUBLIC_API(RDF_Query) RDF_CreateQuery(RDF rdf) {
	RDF_Query q = (RDF_Query)getMem(sizeof(RDF_QueryStruc));
	if (q == NULL) return q;
	q->variables = NULL;
	q->literals = NULL;
	q->numLiterals = 0;
	q->index = 0;
	q->conjunctsOrdered = false;
	q->queryRunning = false;
	q->rdf = rdf;
	return q;
}

/* Destroys a query. */
PR_PUBLIC_API(void) RDF_DestroyQuery(RDF_Query q) {
	destroyLiteralList(q->literals, true);		/* free the literals */
	q->literals = NULL;
	destroyVariableList(q->variables, true);	/* free the variables */
	q->variables = NULL;
	freeMem(q);									/* free the query */
	q = NULL;
}

/* Returns the variable with the given name in the query.
   If not found:
   	if the query is running, return NULL
   	otherwise create a new variable with the given name.
   	if there isn't enough memory, return NULL.
*/
PR_PUBLIC_API(RDF_Variable) RDF_GetVariable(RDF_Query q, char* name) {
	RDF_Variable newvar;
	RDF_VariableList newlist;
	RDF_VariableList current = q->variables;
	while (current != NULL) {
		if (stringEquals(current->element->id, name)) return current->element;
		if (current->next == NULL) break;
		else current = current->next;
	}
	if (q->queryRunning) return NULL;
	newvar = (RDF_Variable)getMem(sizeof(RDF_VariableStruc));
	if (newvar == NULL) return NULL;
	newvar->id = copyString(name);
	newvar->query = q;
	/* create a list containing the variable and append it to the front of the variable list */
	newlist = (RDF_VariableList)getMem(sizeof(RDF_VariableListStruc));
	if (newlist == NULL) {
		freeMem(newvar);
		return NULL;
	}
	newlist->element = newvar;
	newlist->next = q->variables;
	q->variables = newlist;
	return newvar;
}

/* Returns the number of variables in the query. */
PR_PUBLIC_API(uint16) RDF_GetVariableListCount(RDF_Query q) {
	RDF_VariableList current = q->variables;
	uint16 count = 0;
	while (current != NULL) {
		current = current->next;
		count++;
	}
	return count;
}

/* Returns the variables belonging to the query at the given index. */
PR_PUBLIC_API(RDF_Variable) RDF_GetNthVariable(RDF_Query q, uint16 index) {
	RDF_VariableList current = q->variables;
	uint16 count = 0;
	while (current != NULL) {
		if (count == index)
			return current->element;
		else {
			current = current->next;
			count++;
		}
	}
	return NULL;
}

/* Returns true if target is found in literal */
PRBool findLiteral(RDF_Literal target, RDF_LiteralList list) {
	while (list != NULL) {
		if (list->element == target) return true;
		else list = list->next;
	}
	return false;
}

/* Returns the literal belonging to the query at the given index. */
RDF_Literal getNthLiteral(RDF_Query q, uint16 index) {
	RDF_LiteralList current = q->literals;
	uint16 count = 0;
	while (current != NULL) {
		if (count == index)
			return current->element;
		else {
			current = current->next;
			count++;
		}
	}
	return NULL;
}

/* A term is constant if it is not a variable, if it is a variable with
   a non-null value, or if it is a known variable.
*/
PRBool constantp(TermStruc term, RDF_VariableList knownVars) {
	if (term.type != RDF_VARIABLE_TERM_TYPE) return true;
	else {
		RDF_Variable var = (RDF_Variable)term.value;
		if (var->value != NULL) return true;
		else {
			while (knownVars != NULL) {
				if (knownVars->element == var) return true;
				knownVars = knownVars->next;
			}
			return false;
		}
	}
}

/* Returns a literal with a constant term which has not been ordered yet. */
RDF_Literal oneUnknownAs(RDF_Query q, RDF_LiteralList orderedLiterals, RDF_VariableList knownVars) {
	RDF_LiteralList list = q->literals;
	RDF_Literal answer = NULL;
	while (list != NULL) {
		RDF_Literal current = list->element;
		if (!findLiteral(current, orderedLiterals)) { 
			if (((constantp(current->u, knownVars)) || (constantp(*current->v, knownVars))) && 
				current->valueCount == 1 && /* no disjunctions */
				!comparisonp(current->s) && /* comparisons must have constant terms */
				!specialResourcePredicate(current->s) /* notTypeOf and notParent must have constant terms */
				/* && current->tv == true */) {
				if (answer == NULL) {
					answer = current;
				} else {
					/* fix me - deal with branching factor */
					answer = current;
				}
			}
		} 
		list = list->next;
	}
	return answer;
}

/* Returns a literal with both terms constant which has not been ordered yet. */
RDF_Literal noUnknownAs(RDF_Query q, RDF_LiteralList orderedLiterals, RDF_VariableList knownVars) {
	RDF_LiteralList list = q->literals;
	while (list != NULL) {
		RDF_Literal current = list->element;
		if (!findLiteral(current, orderedLiterals) &&
			constantp(current->u, knownVars)) {
			int i;
			PRBool foundNonConstant = false;
			for (i = 0; i < current->valueCount; i++) { /* all values constant? */
				TermStruc term = *(current->v + i);
				if (!constantp(term, knownVars)) foundNonConstant = true;
			}
			if (foundNonConstant) list = list->next;
			else return current;
		}
		else list = list->next;
	}
	return NULL;
}

/* Returns true if the conjuncts were ordered succesfully.
*/
PRBool orderConjuncts(RDF_Query q) {
	uint16 numOrdered = 0;
	RDF_Error err = noRDFErr;
	RDF_LiteralList orderedLiterals = NULL;
	RDF_LiteralList orderedLiteralsTail = orderedLiterals;
	RDF_VariableList knownVars = NULL;

	if (q->conjunctsOrdered) return true;
	
	while (numOrdered < q->numLiterals) {
		RDF_Literal lit1 = noUnknownAs(q, orderedLiterals, knownVars);
		if (lit1 != NULL) {
			/* add literal to ordered list */
			RDF_LiteralList newlist = (RDF_LiteralList)getMem(sizeof(RDF_LiteralListStruc));
			if (newlist == NULL) break;
			newlist->element = lit1;
			newlist->next = NULL;
			if (orderedLiterals == NULL)
				orderedLiterals = orderedLiteralsTail = newlist;
			else {
				orderedLiteralsTail->next = newlist;
				orderedLiteralsTail = orderedLiteralsTail->next;
			}
			numOrdered++;
		} else {
			RDF_Literal lit2 = oneUnknownAs(q, orderedLiterals, knownVars);
			if (lit2 != NULL) {
				RDF_LiteralList newlist = (RDF_LiteralList)getMem(sizeof(RDF_LiteralListStruc));
				if (newlist == NULL) break;
				/* add unit and value as known variables */
				lit2->variable = (RDF_Variable) (constantp(lit2->u, knownVars) ? lit2->v->value : lit2->u.value);
				if (variableTermp(lit2->u)) {
					err = addVariableToList((RDF_Variable)lit2->u.value, &knownVars);
					if (err != noRDFErr) break;
				}
				if (variableTermp(*lit2->v)) {
					err = addVariableToList((RDF_Variable)lit2->v->value, &knownVars);
					if (err != noRDFErr) break;
				}
				/* add literal to ordered list */
				newlist->element = lit2;
				newlist->next = NULL;
				if (orderedLiterals == NULL)
					orderedLiterals = orderedLiteralsTail = newlist;
				else {
					orderedLiteralsTail->next = newlist;
					orderedLiteralsTail = orderedLiteralsTail->next;
				}
				numOrdered++;
			} else break;
		} 
	}
	if (numOrdered == q->numLiterals) {	/* ordered all succesfully so replace literals */
		RDF_LiteralList old = q->literals;
		q->literals = orderedLiterals;
		destroyLiteralList(old, false);
		q->conjunctsOrdered = true;
	} else {							/* unsuccessful so destroy ordered list structure */
		destroyLiteralList(orderedLiterals, false);
		q->conjunctsOrdered = false;
	}
	destroyVariableList(knownVars, false); /* free the knownVars list structure */
	return q->conjunctsOrdered;
}

PRBool processNextLiteral(RDF_Query q, RDF_Literal literal) {
	if (literal->c != NULL) {
		RDF_SetVariableValue(literal->variable, RDF_NextValue(literal->c), literal->valueType);
		return (RDF_GetVariableValue(literal->variable) != NULL);
	} else {
		RDF_Resource u; /* unit is a resource except for the comparison case so don't set it yet. */
		RDF_Resource s = literal->s;
		void* v;
		if (literal->variable == NULL) {
			PRBool result = false;
			PRBool ans = false;
			int i;
			if (literal->bt) return false;
			/* find a value that satisfies predicate */
			for (i = 0; i < literal->valueCount; i++) {
				TermStruc valueTerm = *(literal->v + i);
				if (comparisonp(s)) {
					/* unit is a variable in all comparisons.
					   value may be a variable or a constant.
					*/
					switch (literal->valueType) {
						char *str, *pattern;
						int i, j;
					case RDF_STRING_TYPE:
						if (literal->u.type != RDF_VARIABLE_TERM_TYPE) return false; /* error */
						if (((RDF_Variable)literal->u.value)->type != RDF_STRING_TYPE) return false;
						str = (char*)RDF_GetVariableValue((RDF_Variable)literal->u.value);
						switch (valueTerm.type) {
						case RDF_VARIABLE_TERM_TYPE:
							pattern = (char*)RDF_GetVariableValue((RDF_Variable)valueTerm.value);
							break;
						case RDF_CONSTANT_TERM_TYPE:
							pattern = (char*)valueTerm.value;
							break;
						default:
							/* should blow up */
							return false;
							break;
						}
						if (s == gCoreVocab->RDF_stringEquals) {
							ans = stringEquals(pattern, str);
						} else if (s == gCoreVocab->RDF_substring) {
							ans = substring(pattern, str);
						} else if (s == gCoreVocab->RDF_notStringEquals) {
							ans = !stringEquals(pattern, str);
						} else if (s == gCoreVocab->RDF_notSubstring) {
							ans = !substring(pattern, str);
						} else return false; /* error */
						break;
					case RDF_INT_TYPE:
						if (literal->u.type != RDF_VARIABLE_TERM_TYPE) return false; /* error */
						if (((RDF_Variable)literal->u.value)->type != RDF_INT_TYPE) return false;
						i = (int)RDF_GetVariableValue((RDF_Variable)literal->u.value);
						switch (valueTerm.type) {
						case RDF_VARIABLE_TERM_TYPE:
							j = (int)RDF_GetVariableValue((RDF_Variable)valueTerm.value);
							break;
						case RDF_CONSTANT_TERM_TYPE:
							j = (int)valueTerm.value;
							break;
						default:
							/* should blow up */
							return false;
							break;
						}
						if (s == gCoreVocab->RDF_equals) {
							ans = (i == j);
						} else if (s == gCoreVocab->RDF_notEquals) {
							ans = (i != j);
						} else if (s == gCoreVocab->RDF_lessThan) {
							ans = (i < j);
						} else if (s == gCoreVocab->RDF_greaterThan) {
							ans = (i > j);
						} else if (s == gCoreVocab->RDF_lessThanOrEqual) {
							ans = (i <= j);
						} else if (s == gCoreVocab->RDF_greaterThanOrEqual) {
							ans = (i >= j);
						} else return false;
						break;
					default:
						return false;
						break;
					}
				}
				else {
					u = (RDF_Resource)((variableTermp(literal->u)) ? ((RDF_Variable)literal->u.value)->value : literal->u.value);
					/* special predicates */
					if (s == gCoreVocab->RDF_notInstanceOf) {
						ans = !RDF_HasAssertion(q->rdf, u, gCoreVocab->RDF_instanceOf, valueTerm.value, literal->valueType, true);
					} else if (s == gCoreVocab->RDF_notParent) {
						ans = !RDF_HasAssertion(q->rdf, u, gCoreVocab->RDF_parent, valueTerm.value, literal->valueType, true);
					} else ans = RDF_HasAssertion(q->rdf, u, s, valueTerm.value, literal->valueType, true);
				}
				literal->bt = true;
				/* result = ((literal->tv == true) ? ans : !ans); */
				result = ans;
				if (result) return result; /* otherwise try next value */
			}
			return result;
		} 
		u = (RDF_Resource)((variableTermp(literal->u)) ? ((RDF_Variable)literal->u.value)->value : literal->u.value);
		v = (variableTermp(*literal->v)) ? ((RDF_Variable)literal->v->value)->value : literal->v->value;
		if ((u == NULL) && variableTermp(literal->u) && resourceTermp(*literal->v)) {
			literal->c = RDF_GetSources(q->rdf, (RDF_Resource)v, s, literal->valueType,  true);
			if (literal->c == NULL) return false;
			RDF_SetVariableValue(literal->variable, RDF_NextValue(literal->c), literal->valueType);
			return (RDF_GetVariableValue(literal->variable) != NULL);
		} else if ((v == NULL) && variableTermp(*literal->v) && (u != NULL)) {
			literal->c = RDF_GetTargets(q->rdf, u, s, literal->valueType,  true); /* note arg order differs from java implementation */
			if (literal->c == NULL) return false;
			RDF_SetVariableValue(literal->variable, RDF_NextValue(literal->c), literal->valueType);
			return (RDF_GetVariableValue(literal->variable) != NULL);
		} else return false;			
	}
}

PR_PUBLIC_API(PRBool) RDF_RunQuery (RDF_Query q) {
	q->queryRunning = true;
	orderConjuncts(q);
	if (q->conjunctsOrdered)
		return true;
	else return false;
}

PR_PUBLIC_API(PRBool) RDF_GetNextElement(RDF_Query q) {
	if (!q->queryRunning) RDF_RunQuery(q);
	if (q->index == q->numLiterals) q->index--;
	while (q->index < q->numLiterals) {
		RDF_Literal lit = getNthLiteral(q, q->index); /* we know it's it range so don't have to check for NULL */
		if (!processNextLiteral(q, lit)) {
			if (q->index == 0) return false;
			lit->c = NULL;
			if (lit->variable != NULL) RDF_SetVariableValue(lit->variable, (void*)NULL, RDF_RESOURCE_TYPE);
			lit->bt = false;
			q->index--;
		} else {
			q->index++;
		}
	}
	return true;
}

PR_PUBLIC_API(void) RDF_EndQuery (RDF_Query q) {
	q->queryRunning = false;
}

PR_PUBLIC_API(RDF_VariableList) RDF_GetVariableList(RDF_Query q) {
	return q->variables;
}

/* Note: should put a test for overflow of numLiterals */

/* Adds a literal to the query. */
void addLiteral(RDF_Query q, RDF_Literal literal) {
	RDF_LiteralList newlist = (RDF_LiteralList)getMem(sizeof(RDF_LiteralListStruc));
	if (newlist == NULL) return;
	newlist->element = literal;
	newlist->next = q->literals;
	q->literals = newlist;
	q->numLiterals++;
}

PR_PUBLIC_API(RDF_Error) RDF_AddConjunctVRV(RDF_Query q, RDF_Variable arg1, RDF_Resource s, RDF_Variable arg2, RDF_ValueType type) {
	RDF_Literal literal = (RDF_Literal)getMem(sizeof(LiteralStruc));
	if (literal == NULL) return RDF_NO_MEMORY;
	literal->u.value = (void*)arg1;
	literal->u.type = RDF_VARIABLE_TERM_TYPE;
	((RDF_Variable)literal->u.value)->type = RDF_RESOURCE_TYPE;
	literal->s = s;
	literal->valueCount = 1;
	literal->v = (TermStruc*)getMem(sizeof(TermStruc));
	literal->v->value = (void*)arg2;
	literal->v->type = RDF_VARIABLE_TERM_TYPE;
	((RDF_Variable)literal->v->value)->type = literal->valueType = type;
	literal->tv = true;
	addLiteral(q, literal);	
	q->conjunctsOrdered = false;
	return noRDFErr;
}

PR_PUBLIC_API(RDF_Error) RDF_AddConjunctVRO(RDF_Query q, RDF_Variable arg1, RDF_Resource s, void* arg2, RDF_ValueType type) {
	RDF_Literal literal = (RDF_Literal)getMem(sizeof(LiteralStruc));
	if (literal == NULL) return RDF_NO_MEMORY;
	literal->u.value = (void*)arg1;
	literal->u.type = RDF_VARIABLE_TERM_TYPE;
	((RDF_Variable)literal->u.value)->type = type;
	literal->s = s;
	literal->valueCount = 1;
	literal->v = (TermStruc*)getMem(sizeof(TermStruc));
	literal->v->value = (type == RDF_STRING_TYPE) ? (void*)copyString((char*)arg2) : (void*)arg2;
	literal->v->type = RDF_CONSTANT_TERM_TYPE;
	literal->valueType = type;
	literal->tv = true;
	addLiteral(q, literal);	
	q->conjunctsOrdered = false;		
	return noRDFErr;
}

PR_PUBLIC_API(RDF_Error) RDF_AddConjunctRRO(RDF_Query q, RDF_Resource arg1, RDF_Resource s, void* arg2, RDF_ValueType type) {
	RDF_Literal literal = (RDF_Literal)getMem(sizeof(LiteralStruc));
	if (literal == NULL) return RDF_NO_MEMORY;
	literal->u.value = (void*)arg1;
	literal->u.type = RDF_RESOURCE_TERM_TYPE;
	literal->s = s;
	literal->valueCount = 1;
	literal->v = (TermStruc*)getMem(sizeof(TermStruc));
	literal->v->value = (type == RDF_STRING_TYPE) ? (void*)copyString((char*)arg2) : (void*)arg2;
	literal->v->type = RDF_CONSTANT_TERM_TYPE;
	literal->valueType = type;
	literal->tv = true;
	addLiteral(q, literal);	
	q->conjunctsOrdered = false;		
	return noRDFErr;
}

PR_PUBLIC_API(RDF_Error) RDF_AddConjunctVRR(RDF_Query q, RDF_Variable arg1, RDF_Resource s, RDF_Resource arg2) {
	RDF_Literal literal = (RDF_Literal)getMem(sizeof(LiteralStruc));
	if (literal == NULL) return RDF_NO_MEMORY;
	literal->u.value = (void*)arg1;
	literal->u.type = RDF_VARIABLE_TERM_TYPE;
	((RDF_Variable)literal->u.value)->type = RDF_RESOURCE_TYPE;
	literal->s = s;
	literal->valueCount = 1;
	literal->v = (TermStruc*)getMem(sizeof(TermStruc));
	literal->v->value = (void*)arg2;
	literal->v->type = RDF_RESOURCE_TERM_TYPE;
	literal->valueType = RDF_RESOURCE_TYPE;
	literal->tv = true;
	addLiteral(q, literal);	
	q->conjunctsOrdered = false;
	return noRDFErr;
}

PR_PUBLIC_API(RDF_Error) RDF_AddConjunctRRV(RDF_Query q, RDF_Resource arg1, RDF_Resource s, RDF_Variable arg2, RDF_ValueType type) {
	RDF_Literal literal = (RDF_Literal)getMem(sizeof(LiteralStruc));
	if (literal == NULL) return RDF_NO_MEMORY;
	literal->u.value = arg1;
	literal->u.type = RDF_RESOURCE_TERM_TYPE;
	literal->s = s;
	literal->valueCount = 1;
	literal->v = (TermStruc*)getMem(sizeof(TermStruc));
	literal->v->value = (void*)arg2;
	literal->v->type = RDF_VARIABLE_TERM_TYPE;
	((RDF_Variable)literal->v->value)->type = literal->valueType = type;
	literal->tv = true;
	addLiteral(q, literal);	
	q->conjunctsOrdered = false;
	return noRDFErr;
}

PR_PUBLIC_API(RDF_Error) RDF_AddConjunctVRL(RDF_Query q, RDF_Variable arg1, RDF_Resource s, RDF_Term arg2, RDF_ValueType type, uint8 count) {
	RDF_Literal literal = (RDF_Literal)getMem(sizeof(LiteralStruc));
	if (literal == NULL) return RDF_NO_MEMORY;
	literal->u.value = (void*)arg1;
	literal->u.type = RDF_VARIABLE_TERM_TYPE;
	((RDF_Variable)literal->u.value)->type = type;
	literal->s = s;
	literal->valueCount = count;
	literal->v = (void*)arg2;
	/* value and type of each term already assigned */
	literal->valueType = type;
	literal->tv = true;
	addLiteral(q, literal);	
	q->conjunctsOrdered = false;		
	return noRDFErr;
}
