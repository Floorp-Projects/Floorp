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

#ifndef	_RDF_UTILS_H_
#define	_RDF_UTILS_H_

#include "rdf.h"
#include "rdf-int.h"
#include "htrdf.h"
#include "prprf.h"
#include "xp.h"
#include "glue.h"
#include <stdarg.h>
#include <ctype.h>




/* utils.c data structures and defines */

#define CONTAINER_FLAG 0x01
#define LOCKED_FLAG    0x02



/* utils.c function prototypes */

XP_BEGIN_PROTOS

RDF_Resource	getMCFFrtop (char* furl);
void		addToResourceList (RDFFile f, RDF_Resource u);
void		addToAssertionList (RDFFile f, Assertion as);
void		ht_fprintf(PRFileDesc *file, const char *fmt, ...);
void		ht_rjcprintf(PRFileDesc *file, const char *fmt, const char *data);
char *		makeDBURL(char* name);
PLHashNumber	idenHash (const void *key);
int		idenEqual (const void *v1, const void *v2);
PRBool		inverseTV (PRBool tv);
char *		append2Strings (const char* str1, const char* str2);
void		stringAppendBase (char* dest, const char* addition);
void		stringAppend (char* dest, const char* addition);
int16		charSearch (const char c, const char* data);
PRBool		endsWith (const char* pattern, const char* uuid);
PRBool		startsWith (const char* pattern, const char* uuid);
PRBool		substring (const char* pattern, const char* data);
int16		revCharSearch (const char c, const char* data);
PRBool		urlEquals (const char* url1, const char* url2);
PRBool		isSeparator (RDF_Resource r);
void		setContainerp (RDF_Resource r, PRBool val);
PRBool		containerp (RDF_Resource r);
uint8		resourceType (RDF_Resource r);
void		setResourceType (RDF_Resource r, uint8 val);
char *		resourceID(RDF_Resource r);
char *		makeResourceName (RDF_Resource node);

XP_END_PROTOS

#endif
