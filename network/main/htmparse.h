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
/*** htmparse.h ***************************************************/
/*   description:	html parser									  */ 


 /********************************************************************

   The client calls CRAWL_ParserPut, and gets called back with
   a function whose prototype is described by CRAWL_ParseFunc. The
   parser calls this function whenever a begin or end tag, or run of
   character data has been scanned, and does not build a tree.

  $Revision: 1.1 $
  $Date: 1998/04/30 20:53:21 $

 *********************************************************************/


#ifndef htmparse_h___
#define htmparse_h___
#include "prtypes.h"

/* return values from CRAWL_ParserPut */
#define CRAWL_PARSE_NO_ERROR 0			/* no error */
#define CRAWL_PARSE_ERROR 1			/* syntax error */
#define CRAWL_PARSE_TERMINATE 2		/* don't continue to parse */
#define CRAWL_PARSE_OUT_OF_MEMORY 3	/* out of memory */

/* return values for CRAWL_ParseFunc callback function */
#define PARSE_GET_NEXT_TOKEN 0
#define PARSE_STOP 1
#define PARSE_OUT_OF_MEMORY 2

typedef struct _CRAWL_TagStruc *CRAWL_Tag;

/* 
	The client of this API creates a reference to this with CRAWL_MakeParseObj and provides it as
	a parameter to CRAWL_ParserPut. It is passed as a parameter to the CRAWL_ParseFunc function and
	the parsed data may be extracted from it through the API. It should be considered opaque data.
*/
typedef struct _CRAWL_ParseObjStruc *CRAWL_ParseObj;

/*
	Typedef for a parse callback. The memory in CRAWL_ParseObj is reused across successive calls.
*/
typedef int
(PR_CALLBACK *CRAWL_ParseFunc)(CRAWL_ParseObj obj, PRBool isTag, void *data);

/****************************************************************************************/
/* public API																			*/
/****************************************************************************************/

NSPR_BEGIN_EXTERN_C

 /* 
	Returns the tag parsed 
 */
PR_EXTERN(CRAWL_Tag) 
CRAWL_GetTagParsed(CRAWL_ParseObj obj);

/* 
	Returns the character data between tags 
*/
PR_EXTERN(char*) 
CRAWL_GetDataParsed(CRAWL_ParseObj obj);

/* 
	Returns the tag name 
*/
PR_EXTERN(char*) 
CRAWL_GetTagName(CRAWL_Tag tag);

/* 
	Returns the libparse tag code as defined in pa_tags.h 
*/
PR_EXTERN(intn) 
CRAWL_GetTagToken(CRAWL_Tag tag);

/* 
	a tag of the form <tagname /> Empty tags are recognized only if the page has been designated as
	containing RDF (the API doesn't support this yet)
*/
PR_EXTERN(PRBool) 
CRAWL_IsEmptyTag(CRAWL_Tag tag);

/* 
	A tag of the form </tagname>. 
*/
PR_EXTERN(PRBool) 
CRAWL_IsEndTag(CRAWL_Tag tag);

/*
	Returns the tag attribute given a name
*/
PR_EXTERN(char*) 
CRAWL_GetAttributeValue(CRAWL_Tag tag, char *attributeName);

/*
	Returns the number of attributes for the tag.
*/
PR_EXTERN(uint16) 
CRAWL_GetNumberOfAttributes(CRAWL_Tag tag);

/*
	Returns the nth attribute of the tag.
*/
PR_EXTERN(char*) 
CRAWL_GetNthAttributeName(CRAWL_Tag tag, uint16 n);

/*
	Returns the nth attribute value of the tag.
*/
PR_EXTERN(char*) 
CRAWL_GetNthAttributeValue(CRAWL_Tag tag, uint16 n);

/*
	Creates a new CRAWL_ParseObj suitable for passing to CRAWL_ParserPut.
	Returns NULL if out of memory.
*/
PR_EXTERN(CRAWL_ParseObj) 
CRAWL_MakeParseObj();

/*
	Destroys the CRAWL_ParseObj and all associated memory
*/
PR_EXTERN(void) 
CRAWL_DestroyParseObj(CRAWL_ParseObj obj);

/* 
	Parse characters in buffer and call func when an element (tag or data) has been
	scanned. Returns an error code. The same CRAWL_ParseObj must be provided for successive
	puts in the same buffer. It is up to the caller to create and destroy the CRAWL_ParseObj.
*/
PR_EXTERN(int) 
CRAWL_ParserPut(CRAWL_ParseObj obj, char *str, uint32 len, CRAWL_ParseFunc func, void *data);

NSPR_END_EXTERN_C

#endif /* htmparse_h___ */
