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

#ifndef	_RDF_BMK2MCF_H_
#define	_RDF_BMK2MCF_H_


#include "rdf.h"
#include "rdf-int.h"
#include "vocab.h"
#include "stdio.h"
#include "ctype.h"


/* bmk2mcf.c data structures and defines */

#define OPEN_TITLE_STRING  "<TITLE>"
#define CLOSE_TITLE_STRING "</TITLE>"
#define OPEN_H1_STRING     "<H1>"
#define CLOSE_H1_STRING    "</H1>"
#define OPEN_H3_STRING     "<H3"
#define CLOSE_H3_STRING    "</H3>"
#define OPEN_DL_STRING     "<DL>"
#define CLOSE_DL_STRING    "</DL>"
#define DT_STRING	   "<DT>"
#define PAR_STRING	   "<P>"
#define DD_STRING	   "<DD>"

#define IN_TITLE		1
#define IN_H3			5
#define IN_ITEM_TITLE		7
#define IN_ITEM_DESCRIPTION	9



/* bmk2mcf.c function prototypes */

XP_BEGIN_PROTOS

RDF_Resource		createSeparator(void);
RDF_Resource		createContainer (char* id);
char *			resourceDescription (RDF rdf, RDF_Resource r);
char *			resourceLastVisitDate (RDF rdf, RDF_Resource r);
char *			resourceLastModifiedDate (RDF rdf, RDF_Resource r);
void			parseNextBkBlob (RDFFile f, char* blob, int32 size);
void			parseNextBkToken (RDFFile f, char* token);
void			addDescription (RDFFile f, RDF_Resource r, char* token);
void			bkStateTransition (RDFFile f, char* token);
void			newFolderBkItem(RDFFile f, char* token);
void			newLeafBkItem (RDFFile f, char* token);
char *			numericDate(char *url);
void			HT_WriteOutAsBookmarks1 (RDF rdf, PRFileDesc *fp, RDF_Resource u, RDF_Resource top, int indent);
void			flushBookmarks();

XP_END_PROTOS

#endif
