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

#ifndef	_RDF_MCFF2MCF_H_
#define	_RDF_MCFF2MCF_H_


#include "rdf-int.h"



/* mcff2mcf.c data structures and defines */




/* mcff2mcf.c function prototypes */

XP_BEGIN_PROTOS

RDF_Resource		getMCFFrtop (char* furl);
RDFFile			makeRDFFile (char* url, RDF_Resource top, PRBool localp);
void			initRDFFile (RDFFile ans);
void			finishRDFParse (RDFFile f);
void			abortRDFParse (RDFFile f);
void			addToResourceList (RDFFile f, RDF_Resource u);
void			addToAssertionList (RDFFile f, Assertion as);
void			parseNextRDFBlob (RDFFile f, char* blob, int32 size);
int			parseNextMCFBlob(NET_StreamClass *stream, char* blob, int32 size);
void			parseNextMCFLine (RDFFile f, char* line);
void			finishMCFParse (RDFFile f);
void			resourceTransition (RDFFile f);
void			assignHeaderSlot (RDFFile f, char* slot, char* value);
RDF_Error		getFirstToken (char* line, char* nextToken, int16* l);
void			addSlotValue (RDFFile f, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
void			assignSlot (RDF_Resource u, char* slot, char* value, RDFFile f);
RDF_Error		parseSlotValue (RDFFile f, RDF_Resource s, char* value, void** parsed_value, RDF_ValueType* data_type);
void			derelativizeURL (char* tok, char* url, RDFFile f);
RDF_Resource		resolveReference (char *tok, RDFFile f);
RDF_Resource		resolveGenlPosReference(char* tok,  RDFFile f);
char *			getRelURL (RDF_Resource u, RDF_Resource top);
PRBool			bookmarkSlotp (RDF_Resource s);

XP_END_PROTOS

#endif
