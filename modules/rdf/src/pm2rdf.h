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

#ifndef	_RDF_PM2RDF_H_
#define	_RDF_PM2RDF_H_

#include "rdf.h"
#include "rdf-int.h"
#include "utils.h"



/* pm2rdf.c data structures and defines */

#define pmUnitp(u) ((resourceType(u) == PM_RT) || (resourceType(u) == IM_RT))

#ifdef	XP_WIN
#define	POPMAIL_URL_OFFSET	9
#define	IMAPMAIL_URL_OFFSET	6
#else
#define	POPMAIL_URL_OFFSET	8
#define	IMAPMAIL_URL_OFFSET	5
#endif



/* pm2rdf.c function prototypes */

XP_BEGIN_PROTOS

char *		popmailboxesurl(void);
char *		imapmailboxesurl(void);
void		buildMailList(RDF_Resource ms);
PRDir *		OpenMailDir(char *name);
RDFT		MakeMailStore (char* url);
RDF_Error	PMInit (RDFT ntr);
PRBool		pmHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
void *		pmGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv);
RDF_Cursor	pmGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
void *		pmNextValue (RDFT rdf, RDF_Cursor c);
RDF_Error	pmDisposeCursor (RDFT rdf, RDF_Cursor c);
RDF_Resource	CreatePMUnit (char* nname, RDF_BT rdfType, PRBool isDirectoryFlag);

XP_END_PROTOS

#endif
