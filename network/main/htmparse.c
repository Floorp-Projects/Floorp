/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
/*** htmparse.c ***************************************************/
/*   description:	html parser                                   */


 /********************************************************************

  $Revision: 1.2 $
  $Date: 1998/05/19 00:53:23 $

 *********************************************************************/

#include "xp.h"
#include "xp_str.h"
#include "htmparse.h"
#include "prtypes.h"
#include "pa_tags.h"
#include "pa_parse.h" /* for pa_tokenize_tag */
#include "prmem.h"
#if 0
#include "prio.h"  /* for test only */
#include <stdio.h> /* for test only */
#endif

typedef char ParseState;

/* states of the parser */
#define PS_START 0 /* starting state */
#define PS_BETWEEN_TAGS 1 /* characters not enclosed by < > */
#define PS_TAG_NAME 2
#define PS_EMPTY_TAG 3
#define PS_CLOSE_BRACKET 4
#define PS_ATTRIBUTE 5
#define PS_EQUALS 6
#define PS_VALUE 7
#define PS_START_COMMENT 8
#define PS_END_COMMENT 9

typedef struct _CRAWL_TagStruc {
	char *name;
	intn token;
	char **attributeNames;
	char **attributeValues; /* max length of html attribute is 1024 chars */
	uint16 sizeNames;
	uint16 numNames;
	uint16 sizeValues;
	uint16 numValues;
	PRBool emptyTagp;
	PRBool endTagp;
} CRAWL_TagStruc;

/* maintains state of parser */
typedef struct _CRAWL_ParseObjStruc {
	ParseState state;
	CRAWL_Tag tag;
	char *data;
	uint16 dataLen;
	uint16 dataSize;
	char *str;
	uint16 strLen;
	uint16 strSize;
	char prev1;
	char prev2;
	char inQuote; /* current quote character. when not in quote, value is '\0' */
	PRBool inComment; /* we don't support comment nesting anymore */
	PRBool inScript; /* inside <SCRIPT> and </SCRIPT> */
	PRBool skipWhitespace;
	PRBool isRDF;
} CRAWL_ParseObjStruc;

/* prototypes */
static CRAWL_Tag crawl_makeTag();
static void crawl_recycleTag(CRAWL_Tag tag);
static void crawl_destroyTag(CRAWL_Tag tag);
static void crawl_recycleParseObj(CRAWL_ParseObj obj);

int crawl_appendString(char **str, uint16 *len, uint16 *size, char c);
int crawl_appendStringList(char ***list_p, uint16 *len, uint16 *size, char *str);

/* accessors */
PR_IMPLEMENT(CRAWL_Tag) CRAWL_GetTagParsed(CRAWL_ParseObj obj) {
	if (obj->data != NULL) return NULL;
	else return obj->tag;
}

PR_IMPLEMENT(char*) CRAWL_GetDataParsed(CRAWL_ParseObj obj) {
	if (obj->data != NULL) return obj->data;
	else return NULL;
}

PR_IMPLEMENT(char*) CRAWL_GetTagName(CRAWL_Tag tag) {
	return tag->name;
}

PR_IMPLEMENT(intn) CRAWL_GetTagToken(CRAWL_Tag tag) {
	return tag->token;
}

PR_IMPLEMENT(PRBool) CRAWL_IsEmptyTag(CRAWL_Tag tag) {
	return tag->emptyTagp;
}

PR_IMPLEMENT(PRBool) CRAWL_IsEndTag(CRAWL_Tag tag) {
	return tag->endTagp;
}

PR_IMPLEMENT(uint16) CRAWL_GetNumberOfAttributes(CRAWL_Tag tag) {
	return tag->numNames;
}

PR_IMPLEMENT(char*) CRAWL_GetNthAttributeName(CRAWL_Tag tag, uint16 n) {
	return *(tag->attributeNames + n);
}

PR_IMPLEMENT(char*) CRAWL_GetNthAttributeValue(CRAWL_Tag tag, uint16 n) {
	return *(tag->attributeValues + n);
}

PR_IMPLEMENT(char*) CRAWL_GetAttributeValue(CRAWL_Tag tag, char *attributeName) {
	int count = 0;
	while (count < tag->numNames) {
		if (PL_strcasecmp(attributeName, *(tag->attributeNames + count)) == 0)
			return *(tag->attributeValues + count);
		count++;
	}
	return NULL;
}

static CRAWL_Tag crawl_makeTag() {
	CRAWL_Tag tag = PR_NEWZAP(CRAWL_TagStruc);
	if (tag == NULL) return NULL;
	tag->sizeNames = tag->sizeValues = 4;
	tag->attributeNames = (char**)PR_MALLOC(sizeof(char*) * tag->sizeNames);
	if (tag->attributeNames == NULL) return NULL;
	tag->attributeValues = (char**)PR_MALLOC(sizeof(char*) * tag->sizeValues);
	if (tag->attributeValues == NULL) return NULL;
	return tag;
}

static void crawl_recycleTag(CRAWL_Tag tag) {
	int count;
	if (tag->name != NULL) PR_Free(tag->name);
	tag->name = NULL;
	for (count = 0; count < tag->numNames; count++) {
		PR_Free(*(tag->attributeNames + count));
	}
	tag->numNames = 0;
	for (count = 0; count < tag->numValues; count++) {
		PR_Free(*(tag->attributeValues + count));
	}
	tag->numValues = 0;
	tag->emptyTagp = PR_FALSE;
	tag->endTagp = PR_FALSE;
}

static void crawl_destroyTag(CRAWL_Tag tag) {
	crawl_recycleTag(tag);
	if (tag->attributeNames != NULL) PR_Free(tag->attributeNames);
	if (tag->attributeValues != NULL) PR_Free(tag->attributeValues);
	PR_Free(tag);
}

static void crawl_recycleParseObj(CRAWL_ParseObj obj) {
	crawl_recycleTag(obj->tag);
	if (obj->data != NULL) PR_Free(obj->data);
	obj->data = NULL;
	obj->dataLen = obj->dataSize = 0;
}

PR_IMPLEMENT(CRAWL_ParseObj) CRAWL_MakeParseObj() {
	CRAWL_ParseObj obj = PR_NEWZAP(CRAWL_ParseObjStruc);
	if (obj == NULL) return NULL;
	obj->tag = crawl_makeTag();
	if (obj->tag == NULL) {
		PR_Free(obj);
		return NULL;
	}
	return obj;
}

PR_IMPLEMENT(void) CRAWL_DestroyParseObj(CRAWL_ParseObj obj) {
	crawl_destroyTag(obj->tag);
	if (obj->data != NULL) PR_Free(obj->data);
	obj->data = NULL;
	obj->dataLen = obj->dataSize = 0;
	if (obj->str != NULL) PR_Free(obj->str);
	obj->str = NULL;
	obj->strLen = obj->strSize = 0;
	PR_Free(obj);
}

#define STRING_EXPANSION_INCREMENT 16
/* returns 0 if no error, -1 if no memory */
int crawl_appendString(char **str, uint16 *len, uint16 *size, char c) {
	if (*len == *size) {
		char *newName = (char*)PR_MALLOC(*size + STRING_EXPANSION_INCREMENT);
		char *old = *str;
		if (newName == NULL) return -1;
		memcpy(newName, *str, *size);
		*str = newName;
		if (old != NULL) PR_Free(old);
		*size += STRING_EXPANSION_INCREMENT;
	}
	*(*str + *len) = c;
	++(*len);
	return 0;
}

#define STRINGLIST_EXPANSION_INCREMENT 8

/* returns 0 if no error, -1 if no memory */
int crawl_appendStringList(char ***list_p, uint16 *len, uint16 *size, char *str) {
	char **list = *list_p;
	if (*len == *size) {
		char **newList = (char**)PR_MALLOC(sizeof(char*) * (*size + STRINGLIST_EXPANSION_INCREMENT));
		char **old = list;
		if (newList == NULL) return -1;
		memcpy(newList, list, (sizeof(char*) * (*size)));
		list = newList;
		if (old != NULL) PR_Free(old);
		*size += STRINGLIST_EXPANSION_INCREMENT;
	}
	*(list + *len) = str;
	++(*len);
	*list_p = list;
	return 0;
}

/* returns index to last character of buffer parsed */
PR_IMPLEMENT(int) CRAWL_ParserPut(CRAWL_ParseObj obj, char *str, uint32 len, CRAWL_ParseFunc func, void *data) {
	uint32 n = 0; /* where we are in the buffer */
	uint32 lastn = 0; /* position the last time in the loop */
	char c;

	while (n < len) {
		if (lastn < n) { /* we advanced a character */
			obj->prev1 = obj->prev2;
			obj->prev2 = c;
		}
		lastn = n;
		c = *(str + n);
		if (obj->inComment) {
			/* if we're in a comment, ignore everything until we detect end of comment */
			if ((obj->prev1 == '-') && (obj->prev2 == '-') && (c == '>')) obj->inComment = PR_FALSE;
			n++;
		} else if (obj->skipWhitespace) {
			if ((c == ' ') || (c == '\n') || (c == '\r')) {
				n++;
			} else obj->skipWhitespace = PR_FALSE;
		} else {
			PRBool endOfString = PR_FALSE;
			switch (obj->state) {
			case PS_START:
			/* PS_START - expecting open bracket or character data */
				if (c == '<') {
					obj->state = PS_TAG_NAME;
					n++;
				} else {
					obj->state = PS_BETWEEN_TAGS;
				}
				break;
			case PS_BETWEEN_TAGS:
			/* PS_BETWEEN_TAGS - expecting open bracket (terminating character data) or more character data */
				if (obj->inQuote == c) {
					obj->inQuote = '\0'; /* close quote */
				} else if ((c == '"') || (obj->inScript && (c == '\''))) { /* start a quote, only double quotes significant in between tags */
					obj->inQuote = c;
				}
				/* open bracket not in quoted section indicates end of data */
				if ((obj->inQuote == '\0') && (c == '<')) {
					obj->state = PS_START;
					if (crawl_appendString(&obj->data, &obj->dataLen, &obj->dataSize, '\0') != 0) /* null terminate string */
						return CRAWL_PARSE_OUT_OF_MEMORY;
					if (func(obj, PR_FALSE, data) == PARSE_STOP) return CRAWL_PARSE_TERMINATE;
					crawl_recycleParseObj(obj);
				} else {
					if (crawl_appendString(&obj->data, &obj->dataLen, &obj->dataSize, c) != 0)
						return CRAWL_PARSE_OUT_OF_MEMORY;
					n++;
				}
				break;
			case PS_TAG_NAME:
			/* PS_TAG_NAME - terminated by space, \r, \n, >, / */
				if ((c == '"') || (c == '\'')) return CRAWL_PARSE_ERROR; /* error - these are not allowed in tagname */
				else if (c == ' ') {
					/* Note: Both mozilla and XML don't allow any spaces between < and tagname.
					   Need to check for zero-length tagname.
					*/
					if (obj->str == NULL) return CRAWL_PARSE_ERROR; /* obj->str is the buffer we're working on */
					endOfString = PR_TRUE;
					obj->state = PS_ATTRIBUTE;
					obj->skipWhitespace = PR_TRUE;
					n++;
				} else if (c == '/') {
					if (obj->tag->name == NULL) obj->tag->endTagp = PR_TRUE; /* indicates end tag if no tag name read yet */
					else if (obj->isRDF) { /* otherwise its an empty tag (RDF only) */
						endOfString = PR_TRUE;
						obj->tag->emptyTagp = PR_TRUE;
						obj->state = PS_CLOSE_BRACKET;
					} else return CRAWL_PARSE_ERROR;
					n++;
				} else if (c == '>') {
					endOfString = PR_TRUE;
					obj->state = PS_CLOSE_BRACKET;
				} else if ((c != '\r') && (c != '\n')) {
					if (crawl_appendString(&obj->str, &obj->strLen, &obj->strSize, c) != 0)
						return CRAWL_PARSE_OUT_OF_MEMORY;
					n++;
				} else {
					endOfString = PR_TRUE;
					obj->state = PS_ATTRIBUTE; /* note - mozilla allows newline after tag name */
					obj->skipWhitespace = PR_TRUE;
					n++;
				}
				if (endOfString) {
					if (crawl_appendString(&obj->str, &obj->strLen, &obj->strSize, '\0') != 0) /* null terminate string */
						return CRAWL_PARSE_OUT_OF_MEMORY;
					if (strcmp(obj->str, "!--") == 0) {  /* html comment */
						obj->inComment = PR_TRUE;
						obj->state = PS_START;
					} else {
						obj->tag->name = obj->str;
						obj->tag->token = pa_tokenize_tag(obj->str);
					}
					obj->str = NULL;
					obj->strLen = obj->strSize = 0;
					endOfString = PR_FALSE;
				}
				break;
			case PS_CLOSE_BRACKET:
			/* PS_CLOSE_BRACKET - expecting a close bracket, anything else is an error */
				if (c == '>') {
					if (!obj->isRDF && (obj->tag->token == P_SCRIPT)) {
						/* we're inside a script tag (not RDF) */
						if (obj->tag->endTagp) obj->inScript = PR_FALSE;
						else obj->inScript = PR_TRUE;
					}
					if (func(obj, PR_TRUE, data) == PARSE_STOP) return CRAWL_PARSE_TERMINATE;
					crawl_recycleParseObj(obj);
					obj->state = PS_START;
					n++;
				} else return CRAWL_PARSE_ERROR; /* error */
				break;
			case PS_ATTRIBUTE:
			/* PS_ATTRIBUTE - expecting an attribute name, or / (RDF only) or > indicating no more attributes */
				/* accept attributes without values, such as <tag attr1 attr2=val2>
				   or <tag attr2=val2 attr1>
				*/
				if (obj->inQuote == c) {
					obj->inQuote = '\0'; /* close quote */
				} else if (((c == '"') || (c == '\'')) && (obj->inQuote == '\0')) {
					/* start a quote if none is already in effect */
					obj->inQuote = c;
				}
				if (obj->inQuote == '\0') {
					if ((((c == '/') && obj->isRDF) || (c == '>')) && (obj->str == NULL)) {
						obj->state = PS_CLOSE_BRACKET;
					} else if ((c == ' ') || (c == '=') || (c == '\n') || (c == '\r') || ((c == '/') && obj->isRDF) || (c == '>')) {
						if (crawl_appendString(&obj->str, &obj->strLen, &obj->strSize, '\0') != 0) /* null terminate string */
							return CRAWL_PARSE_OUT_OF_MEMORY;
						if (crawl_appendStringList(&obj->tag->attributeNames, &obj->tag->numNames, &obj->tag->sizeNames, obj->str) != 0)
							return CRAWL_PARSE_OUT_OF_MEMORY;
						obj->str = NULL;
						obj->strLen = obj->strSize = 0;
						obj->state = PS_EQUALS; /* if non-null attribute name */
					} else {
						if (crawl_appendString(&obj->str, &obj->strLen, &obj->strSize, c) != 0)
							return CRAWL_PARSE_OUT_OF_MEMORY;
						n++;
					}
				} else {
					if (crawl_appendString(&obj->str, &obj->strLen, &obj->strSize, c) != 0)
						return CRAWL_PARSE_OUT_OF_MEMORY;
					n++;
				}
				break;
			case PS_EQUALS:
				if ((c == ' ') || (c == '\n') || (c == '\r')) {
					obj->skipWhitespace = PR_TRUE;
					n++;
				} else if (c == '=') {
					obj->skipWhitespace = PR_TRUE;
					obj->state = PS_VALUE;
					n++;
				} else { /* no value for the attribute - error in RDF? */
					if (crawl_appendString(&obj->str, &obj->strLen, &obj->strSize, '\0') != 0) /* null terminate string */
						return CRAWL_PARSE_OUT_OF_MEMORY;
					if (crawl_appendStringList(&obj->tag->attributeValues, &obj->tag->numValues, &obj->tag->sizeValues, obj->str) != 0)
						return CRAWL_PARSE_OUT_OF_MEMORY;
					obj->str = NULL;
					obj->strLen = obj->strSize = 0;
					obj->state = PS_ATTRIBUTE;
				}
				break;
			case PS_VALUE:
			/* expecting a value, or space, / (RDF only), or > indicating end of value. */
				{
					PRBool include = PR_TRUE; /* whether the current character should be included in value */
					if (obj->inQuote == c) {
						obj->inQuote = '\0'; /* close quote */
						include = PR_FALSE;
					} else if (((c == '"') || (c == '\'')) && (obj->inQuote == '\0')) {
						/* start a quote if none is already in effect */
						obj->inQuote = c;
						include = PR_FALSE;
					}
					if (obj->inQuote == '\0') {
						if ((c == '/') && obj->isRDF) {
							endOfString = PR_TRUE;
							obj->state = PS_CLOSE_BRACKET;
							n++;
						} else if (c == '>') {
							endOfString = PR_TRUE;
							obj->state = PS_CLOSE_BRACKET;
						} else if ((c == ' ') || (c == '\r') || (c == '\n')) {
							endOfString = PR_TRUE;
							obj->skipWhitespace = PR_TRUE;
							obj->state = PS_ATTRIBUTE; /* if non-null value name */
							n++;
						} else if (include) {
							if (crawl_appendString(&obj->str, &obj->strLen, &obj->strSize, c) != 0)
								return CRAWL_PARSE_OUT_OF_MEMORY;
							n++;
						} else n++;
					} else if (include) {
						if (crawl_appendString(&obj->str, &obj->strLen, &obj->strSize, c) != 0)
							return CRAWL_PARSE_OUT_OF_MEMORY;
						n++;
					} else n++;
					if (endOfString) {
						if (crawl_appendString(&obj->str, &obj->strLen, &obj->strSize, '\0') != 0) /* null terminate string */
							return CRAWL_PARSE_OUT_OF_MEMORY;
						if (crawl_appendStringList(&obj->tag->attributeValues, &obj->tag->numValues, &obj->tag->sizeValues, obj->str) != 0)
							return CRAWL_PARSE_OUT_OF_MEMORY;
						obj->str = NULL;
						obj->strLen = obj->strSize = 0;
						endOfString = PR_FALSE;
					}
					break;
				}
			default:
				break;
			}
		}
	}
	return CRAWL_PARSE_NO_ERROR;
}

#if 0
void printParseObj(CRAWL_ParseObj obj, PRBool isTag, void *data) {
	if (isTag) {
		CRAWL_Tag tag = CRAWL_GetTagParsed(obj);
		if (CRAWL_IsEndTag(tag)) {
			printf("</%s>\n", CRAWL_GetTagName(tag));
		} else {
			uint16 i;
			printf("<%s", CRAWL_GetTagName(tag));
			for (i = 0; i < CRAWL_GetNumberOfAttributes(tag); i++) {
				printf(" %s=\"%s\"", CRAWL_GetNthAttributeName(tag, i), CRAWL_GetNthAttributeValue(tag, i));
			}
			if (CRAWL_IsEmptyTag(tag)) printf("/>\n");
			else printf(">\n");
		}
	} else printf(">>>>>%s<<<<<\n", CRAWL_GetDataParsed(obj));
}

void parseLocalFile (char *url) {
	    PRFileDesc *fp;
	    int32 len;
		char *path;
		static char buf[512]; /* xxx alloc */
		CRAWL_ParseObj parse;

		/* XXX need to unescape URL */
		path=&url[8];
		fp = PR_Open(path,  PR_RDONLY, 0644);  /* WR_ONLY|PR_TRUNCATE */
		if(fp == NULL)
		{
			/* abortRDFParse(file); */
			return;
		}
		parse = CRAWL_MakeParseObj();
		while((len=PR_Read(fp, buf, 512))>0) {
			int result;
		    result = CRAWL_ParserPut(parse, buf, len, printParseObj, NULL);
			if (result == len) printf("************NO ERRORS************\n");
			else printf("************PARSING ERROR************\n");
		}
		PR_Close(fp);
		CRAWL_DestroyParseObj(parse);
		/* finishRDFParse(file); */
		return;
}
#endif
