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

#ifndef	_RDF_SCOOK_H_
#define	_RDF_SCOOK_H_


#include "rdf-int.h"



/* scook.c data structures */

typedef struct _SCookDBStruct {
  PLHashTable* lhash;
  PLHashTable* rhash;
  char* reader;
  RDFFile rf;
  RDFT   db;
} SCookDBStruct;

typedef SCookDBStruct* SCookDB;



/* scook.c function prototypes */

XP_BEGIN_PROTOS

char *		makeSCookPathname(char* name);
PRBool		SCookAssert1 (RDFT mcf,   RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
PRBool		SCookAssert3 (RDFT mcf,   RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
PRBool		SCookAssert2 (RDFFile  file, RDFT mcf,  RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
Assertion	SCookAssert (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
PRBool		SCookUnassert (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
Assertion	SCookRemove (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
PRBool		SCookHasAssertion (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
void *		SCookGetSlotValue (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv);
RDF_Cursor	SCookGetSlotValues (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
void *		SCookNextValue (RDFT mcf, RDF_Cursor c);
RDF_Error	SCookDisposeCursor (RDFT mcf, RDF_Cursor c);
Assertion	getArg1 (RDFT r, RDF_Resource u);
Assertion	getArg2 (RDFT r, RDF_Resource u);
void		setArg1 (RDFT r, RDF_Resource u, Assertion as);
void		setArg2 (RDFT r, RDF_Resource u, Assertion as);
void		gcSCookFile (RDFT rdf, RDFFile f);
void		disposeAllSCookFiles (RDFT rdf, RDFFile f);
void		SCookDisposeDB (RDFT rdf);
void		possiblyAccessSCookFile (RDFT mcf, RDF_Resource u, RDF_Resource s, PRBool inversep);
RDFT		MakeSCookDB (char* url);

XP_END_PROTOS

#endif
