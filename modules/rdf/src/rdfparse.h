/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef	_RDF_RDFPARSE_H_
#define	_RDF_RDFPARSE_H_


#include "rdf-int.h"




/* rdfparse.c data structures and defines */

#define wsc(c) ((c == ' ') || (c == '\r') || (c == '\n') || (c == '\t'))


#define EXPECTING_OBJECT 21
#define EXPECTING_PROPERTY 22



/* rdfparse.c function prototypes */

XP_BEGIN_PROTOS

char		decodeEntityRef (char* string, int32* stringIndexPtr, int32 len);
char *		copyStringIgnoreWhiteSpace(char* string);
char *		getHref(char** attlist);
int		parseNextRDFXMLBlob (NET_StreamClass *stream, char* blob, int32 size);
void		parseRDFProcessingInstruction (RDFFile f, char* token);
PR_PUBLIC_API(char *)		getAttributeValue (char** attlist, char* elName);
PRBool		tagEquals (RDFFile f, char* tag1, char* tag2);
void		addElementProps (char** attlist, char* elementName, RDFFile f, RDF_Resource obj);
PRBool		knownObjectElement (char* eln);
char *		possiblyMakeAbsolute (RDFFile f, char* url);
PRBool		containerTagp (RDFFile f, char* elementName);
RDF_Resource	ResourceFromElementName (RDFFile f, char* elementName);
void		parseNextRDFToken (RDFFile f, char* token);
void		tokenizeElement (char* attr, char** attlist, char** elementName);
void		outputRDFTreeInt (RDF rdf, PRFileDesc *fp, RDF_Resource node, uint32 depth, PRBool localOnly);
void		outputRDFTree (RDF rdf, PRFileDesc *fp, RDF_Resource node);

XP_END_PROTOS

#endif
