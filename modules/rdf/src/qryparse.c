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

/* qryparse.c

A parser for queries expressed in RDF syntax.

There is a query tag (RDF:Query) which encloses one or more literal tags. 
It has an id (which is currently ignored).
Literal tags (RDF:Literal) are like assertions, except they may contain variables.
Variables are specified as hrefs, the first character of which is $. 

The format of a literal whose range is a resource type is: 

<RDF:Literal href=domain> 
    <property href=range> 
</RDF:Literal> 

a literal whose range is a string or int is expressed as: 

<RDF:Literal href=domain> 
    <property>string-or-int-value</property> 
</RDF:Literal> 

Note: in order for the query engine to correctly retrieve property values 
which are strings or ints, you must add assertions about the
property's range (otherwise it is assumed to be a resource). 
The range property may be the resource named "String", "Int", or any
other resource. 

For example: 
<RDF:Resource id="motto"> 
 <domain href="State.mcf"/> 
 <range href="String"/> 
</RDF:Resource> 

Here is an example of a query: 

<RDF:Query id="query1"> 
 <result href="$result"/>
 <RDF:Literal href="$var1"> 
  <typeof href="Country.mcf"/> 
 </RDF:Literal> 
 <RDF:Literal href="$var1"> 
  <state href="$var2"/> 
 </RDF:Literal> 
 <RDF:Literal href="$var2"> 
  <capitalCity href="$result"/> 
 </RDF:Literal> 
</RDF:Query> 
  
In the Prolog-like syntax this looks like: 
typeof($var1, Country) & state($var1, $var2) & capitalCity($var2, $result) 
*/

#include "query.h"
#include "rdf-int.h"

#define QUERY_TAG "RDF:Query"
#define LITERAL_TAG "RDF:Literal"
#define RESULT_TAG "RDF:result"
#define SEQ_TAG "RDF:seq"
#define SEQ_END_TAG "</RDF:seq>"
#define LI_TAG "RDF:li"
#define RDF_OBJECT 10 /* status */
#define RDF_PROPERTY 11 /* status */
#define RDF_SEQ 12
#define RDF_LI 13
#define RDF_PARSE_ERROR 5 /* this should go in rdf.h */

/* analogous to RDFFile */
typedef struct _QueryParseStruct {
	uint16 status; /* whether we're parsing an object or property */
	PRBool tv; /* truth value of current literal */
	uint16 depth;
	RDF rdf;
	RDF_Query query;
	RDFElement elf;
	TermStruc stack[16];
	TermStruc* value;
	uint8 valueCount;
	uint8 valueSize;
} QueryParseStruct;

extern void parseRDFElement(RDFElement elf, char* token);
extern char* getElfProp(char* prop, RDFElement elf);
extern PRBool variableTermp(TermStruc term);
extern PRBool resourceTermp(TermStruc term);
extern PRBool constantTermp(TermStruc term);

/* prototypes */
PRBool variablep(char* elf);
RDF_ValueType rangeType(RDF rdf, RDF_Resource prop);
RDF_Error parseNextQueryToken (QueryParseStruct *q, char* token);
RDF_Query parseQuery(RDF rdf, char* blob, int32 size);
RDF_Error parsePropertyValue(QueryParseStruct *q, char* token);
RDF_Error parseEndTag(QueryParseStruct *q, char* token);
RDF_Error parseTag (QueryParseStruct *q, char* token);
RDF_Error addValueToList(QueryParseStruct *q, void* value, RDF_TermType type);
TermStruc* copyTermList(TermStruc* list, uint8 count);

PRBool variablep(char* elf) {
	return elf[0] == '$';
}

/* Returns the ValueType of the range of the property specified */
RDF_ValueType rangeType(RDF rdf, RDF_Resource prop) {
	RDF_Resource rangeType;
	if (prop == gCoreVocab->RDF_substring) return RDF_STRING_TYPE;
	else if (prop == gCoreVocab->RDF_notSubstring) return RDF_STRING_TYPE;
	else if (prop == gCoreVocab->RDF_stringEquals) return RDF_STRING_TYPE;
	else if (prop == gCoreVocab->RDF_notStringEquals) return RDF_STRING_TYPE;
	else if (prop == gCoreVocab->RDF_lessThan) return RDF_INT_TYPE;
	else if (prop == gCoreVocab->RDF_greaterThan) return RDF_INT_TYPE;
	else if (prop == gCoreVocab->RDF_lessThanOrEqual) return RDF_INT_TYPE;
	else if (prop == gCoreVocab->RDF_greaterThanOrEqual) return RDF_INT_TYPE;
	else if (prop == gCoreVocab->RDF_equals) return RDF_INT_TYPE;
	else if (prop == gCoreVocab->RDF_notEquals) return RDF_INT_TYPE;
	/* fix me - add RDF_stringEquals */
	rangeType = RDF_GetSlotValue(rdf, prop, gCoreVocab->RDF_range, RDF_RESOURCE_TYPE, false, true);
	if (rangeType == NULL) return RDF_RESOURCE_TYPE; /* don't know so assume resource */
	else if (rangeType == gCoreVocab->RDF_StringType) return RDF_STRING_TYPE;
	else if (rangeType == gCoreVocab->RDF_IntType) return RDF_INT_TYPE;
	else return RDF_RESOURCE_TYPE; /* not string or int so must be a resource */
}

/* Returns query parsed from blob, NULL if there was a parsing error.
   This is adapted from parseNextRDFXMLBlob, the main differences being that
   a file structure is not maintained. blob must contain the entire query.
*/
RDF_Query parseQuery(RDF rdf, char* blob, int32 size) {
	RDF_Error err = noRDFErr;
	QueryParseStruct q;
	char line[LINE_SIZE];
	char holdOver[LINE_SIZE];
	int32 n, last, m;
	PRBool somethingseenp = false;
	n = last = 0;
	q.depth = 0;
	q.elf = (RDFElement)getMem(sizeof(RDFElementStruct));
	q.rdf = rdf;
	q.query = RDF_CreateQuery(rdf);
	q.tv = true;
	q.value = NULL;
	q.valueCount = 0;
	q.valueSize = 0;

	memset(holdOver, '\0', LINE_SIZE);
	while (n < size) {
		char c = blob[n];
		m = 0;
		somethingseenp = false;
		memset(line, '\0', LINE_SIZE);
		if (holdOver[0] != '\0') {
		  memcpy(line, holdOver, strlen(holdOver));
		  m = strlen(holdOver);
		  somethingseenp = true;
		  memset(holdOver, '\0', LINE_SIZE);
		}
		while ((m < 300) && (c != '<') && (c != '>')) {
			line[m] = c;
			m++;
			somethingseenp = (somethingseenp || ((c != ' ') && (c != '\r') && (c != '\n')));
			n++;
			if (n < size) c = blob[n];
			else break;
		}
		if (c == '>') line[m] = c;
		n++;
		if (m > 0) {
			if ((c == '<') || (c == '>')) {
				last = n;
				if (c == '<') holdOver[0] = '<'; 
				if (somethingseenp == true) {
					err = parseNextQueryToken(&q, line);
					if (err != noRDFErr) {
						if (q.query != NULL) RDF_DestroyQuery(q.query);
						q.query = NULL;
						break; /* while (n < size) */
					}
				}
			} else if (size > last) {
				memcpy(holdOver, line, m);
			}
		} else if (c == '<') holdOver[0] = '<';
	}
	if (q.elf != NULL) freeMem(q.elf);
	return q.query;
}

RDF_Error addValueToList(QueryParseStruct *q, void* value, RDF_TermType type) {
	RDF_Error err = noRDFErr;
	int increment = 5;
	if (q->valueSize == q->valueCount) {
		TermStruc* old = q->value;
		TermStruc* newTermList = (TermStruc*)getMem((q->valueSize + increment) * sizeof(TermStruc));
		if (newTermList == NULL) return RDF_NO_MEMORY;
		memcpy((char*)newTermList, (char*)q->value, (q->valueSize)* sizeof(TermStruc));
		q->value = newTermList;
		q->valueSize = q->valueSize + increment;
		freeMem(old);
	}
	(q->value + q->valueCount)->value = value;
	(q->value + q->valueCount)->type = type;
	q->valueCount++;
	return err;
}

TermStruc* copyTermList(TermStruc* list, uint8 count) {
	TermStruc* newList = (TermStruc*)getMem(count * sizeof(TermStruc));
	if (newList == NULL) return NULL;
	memcpy((char*)newList, (char*)list, count * sizeof(TermStruc));
	return newList;
}

RDF_Error parsePropertyValue(QueryParseStruct *q, char* token) {
	RDF_Error err = noRDFErr;
	if ((q->depth == 3) && (q->status == RDF_PROPERTY)) {
		/* parse the property value */
		RDF_Resource slot = (RDF_Resource)q->stack[q->depth-1].value;
		RDF_ValueType type = rangeType(q->rdf, slot);
		TermStruc unitTerm = q->stack[q->depth-2];
		switch (type) { /* switch on value type of property */
			int i;
		case RDF_RESOURCE_TYPE:
			err = RDF_PARSE_ERROR;
			break;
		case RDF_STRING_TYPE:
			if (variablep(token)) {
				RDF_Variable rangeVar = RDF_GetVariable(q->query, token);
				if (variableTermp(unitTerm))
					err = RDF_AddConjunctVRV(q->query, (RDF_Variable)unitTerm.value, slot, rangeVar, type);
				else err = RDF_AddConjunctRRV(q->query, (RDF_Resource)unitTerm.value, slot, rangeVar, type);
			} else if (variableTermp(unitTerm)) {
					err = RDF_AddConjunctVRO(q->query, (RDF_Variable)unitTerm.value, slot, (void*)token, type);
			} else err = RDF_AddConjunctRRO(q->query, (RDF_Resource)unitTerm.value, slot, (void*)token, type);
			break;
		case RDF_INT_TYPE:
			if (variablep(token)) {
				RDF_Variable rangeVar = RDF_GetVariable(q->query, token);
				if (variableTermp(unitTerm))
					err = RDF_AddConjunctVRV(q->query, (RDF_Variable)unitTerm.value, slot, rangeVar, type);
				else err = RDF_AddConjunctRRV(q->query, (RDF_Resource)unitTerm.value, slot, rangeVar, type);
			} else if (sscanf(token, "%d", &i) == 1) { /* fix me */
				if (variableTermp(unitTerm)) {
					err = RDF_AddConjunctVRO(q->query, (RDF_Variable)unitTerm.value, slot, (void*)i, type);
				} else err = RDF_AddConjunctRRO(q->query, (RDF_Resource)unitTerm.value, slot, (void*)i, type);
			} else err = RDF_PARSE_ERROR;
			break;
		default:
			err = RDF_PARSE_ERROR; /* should never get here */
			break;
		}
	} else if (q->status == RDF_LI) {
		RDF_Resource slot = (RDF_Resource)q->stack[q->depth-3].value;
		RDF_ValueType type = rangeType(q->rdf, slot);
		switch (type) {
			int i;
		case RDF_RESOURCE_TYPE:
			err = RDF_PARSE_ERROR;
			break;
		case RDF_STRING_TYPE:
			if (variablep(token))
				err = addValueToList(q, RDF_GetVariable(q->query, token), RDF_VARIABLE_TERM_TYPE);
			else err = addValueToList(q, copyString(token), RDF_CONSTANT_TERM_TYPE);
			break;
		case RDF_INT_TYPE:
			if (variablep(token)) {
				err = addValueToList(q, RDF_GetVariable(q->query, token), RDF_VARIABLE_TERM_TYPE);
			} else if (sscanf(token, "%d", &i) == 1) { /* fix me */
				err = addValueToList(q, (void*)i, RDF_CONSTANT_TERM_TYPE);
			} else err = RDF_PARSE_ERROR;
			break;
		default:
			err = RDF_PARSE_ERROR; /* should never get here */
			break;
		}
	}
	return err;
}

RDF_Error parseTag (QueryParseStruct *q, char* token) {
	RDF_Error err = noRDFErr;
	RDFElement elf = q->elf;
	memset((char*)elf, '\0', sizeof(RDFElementStruct));
	parseRDFElement(elf, token);

	/* the block can start with Query, Literal or a property name */

	if (startsWith(QUERY_TAG, elf->tagName)) {
		char* url = getElfProp("id", elf);
		/* don't have anything to do with id right now */
		q->stack[q->depth++].value = (void*)NULL;
		q->status = RDF_OBJECT;
	} else if (startsWith(LITERAL_TAG, elf->tagName)) {
		char* domain = getElfProp("href", elf);
		if (variablep(domain)) {
			q->stack[q->depth].value = RDF_GetVariable(q->query, domain);
			q->stack[q->depth].type = RDF_VARIABLE_TERM_TYPE;
			q->depth++;
		} else {
			q->stack[q->depth].value = resourceFromID(domain, true);
			q->stack[q->depth].type = RDF_RESOURCE_TERM_TYPE;
			q->depth++;
		}
		q->status = RDF_OBJECT;
		/*
		if (stringEquals(LITERAL_NEGATION_TAG, elf->tagName))
			q->tv = false;
		else q->tv = true;
		*/
	} else if (stringEquals(elf->tagName, RESULT_TAG) && (q->depth == 1)) {
		/* set a result variable */
		char* range = getElfProp("href", elf);
		RDF_Variable resultVar = RDF_GetVariable(q->query, range);
		RDF_SetResultVariable(resultVar, true);
		q->status = RDF_OBJECT;
	} else if (stringEquals(elf->tagName, SEQ_TAG) && (q->depth == 3)) {
		/* ignore stack value */
		q->depth++;
		q->status = RDF_SEQ;
		q->valueSize = 10;
		q->valueCount = 0;
		q->value = (TermStruc*)getMem(q->valueSize * sizeof(TermStruc));
		if (q->value == NULL) err = RDF_PARSE_ERROR;
	} else if (stringEquals(elf->tagName, LI_TAG) && (q->depth == 4)) {
		/* ignore stack value */
		if (elf->emptyTagp) { /* <RDF:li href="$var"/> */
			char* range = getElfProp("href", elf);
			RDF_Resource slot = (RDF_Resource)q->stack[q->depth-2].value;
			RDF_ValueType type = rangeType(q->rdf, slot);
			if (type == RDF_RESOURCE_TYPE) {
				if (variablep(range)) {
					err = addValueToList(q, RDF_GetVariable(q->query, range), RDF_VARIABLE_TERM_TYPE);
				} else err = addValueToList(q, resourceFromID(range, true), RDF_RESOURCE_TERM_TYPE);
			} else err = RDF_PARSE_ERROR;
		} else { /* <RDF:li> */
			q->depth++;
			q->status = RDF_LI;
		}
	} else if (q->depth != 1) { /* property */
		char* pname = elf->tagName;
		RDF_Resource slot = resourceFromID(pname, true);
		if (elf->emptyTagp) {
			char* range = getElfProp("href", elf);
			RDF_ValueType type = rangeType(q->rdf, slot);
			TermStruc unitTerm = q->stack[q->depth-1];
			switch (type) { /* switch on value type of property */
			case RDF_RESOURCE_TYPE:
				if (variablep(range)) {
					RDF_Variable rangeVar = RDF_GetVariable(q->query, range);
					if (variableTermp(unitTerm)) {
						err = RDF_AddConjunctVRV(q->query, (RDF_Variable)unitTerm.value, slot, rangeVar, type);
					} else {
						err = RDF_AddConjunctRRV(q->query, (RDF_Resource)unitTerm.value, slot, rangeVar, type);
					}
				} else {
					RDF_Resource rangeRsrc = resourceFromID(range, true);
					if (variableTermp(unitTerm)) {
						/* RDF_AddConjunctVRR */
						err = RDF_AddConjunctVRR(q->query, (RDF_Variable)unitTerm.value, slot, rangeRsrc);
					} else err = RDF_PARSE_ERROR;
				}
				break;
			default:
				err = RDF_PARSE_ERROR; /* strings and ints cannot be inside href */
				break;
			}
			q->status = RDF_OBJECT;
		} else {
			/* this isn't really a term, its just a property but we access it in the same way as a term */
			q->stack[q->depth].value = slot;
			q->stack[q->depth].type = RDF_RESOURCE_TERM_TYPE;
			q->depth++;
			q->status = RDF_PROPERTY;
		}
	}
	return err;
}

RDF_Error parseEndTag(QueryParseStruct *q, char* token) {
	RDF_Error err = noRDFErr;
	if (stringEquals(SEQ_END_TAG, token)) {
		RDF_Resource slot = (RDF_Resource)q->stack[q->depth-2].value;
		RDF_ValueType type = rangeType(q->rdf, slot);
		TermStruc unitTerm = q->stack[q->depth-3];
		/* copy the value list, add the conjunct, and destroy the list - the engine destroys the copy. */
		TermStruc* copy = copyTermList(q->value, q->valueCount);
		if (copy == NULL) return RDF_NO_MEMORY;
		err = RDF_AddConjunctVRL(q->query, (RDF_Variable)unitTerm.value, slot, (RDF_Term)copy, type, q->valueCount);
		q->valueCount = q->valueSize = 0;
		freeMem(q->value);
	}
	if (q->depth == 0) return RDF_PARSE_ERROR;
	q->depth--;
	if (q->status == RDF_OBJECT)
		q->status = RDF_PROPERTY;
	else if (q->status == RDF_LI)
		q->status = RDF_SEQ;
	else if (q->status == RDF_SEQ)
		q->status = RDF_OBJECT; /* also terminates the property */
	return err;
}

/* this is adapted from parseNextRDFToken, the difference being in the actions. */
RDF_Error parseNextQueryToken (QueryParseStruct *q, char* token) {
	RDF_Error err = noRDFErr;
	if (token[0] == '\n' || token[0] == '\r') return err;
	if (token[0] != '<') {
		err = parsePropertyValue(q, token);
	} else if (token[1] == '/') {
		err = parseEndTag(q, token);
	} else { 
		err = parseTag(q, token);
	}
	return err;
}
