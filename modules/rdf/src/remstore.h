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

#ifndef	_RDF_REMSTORE_H_
#define	_RDF_REMSTORE_H_


#include "rdf-int.h"

#ifdef NSPR20
#include "prtime.h"
#else
#include "prtime.h"
#endif



/* remstore.c data structures and defines */




/* remstore.c function prototypes */

XP_BEGIN_PROTOS

RDFT		MakeRemoteStore (char* url);
RDFT		MakeFileDB (char* url);
void		freeAssertion (Assertion as);
PRBool		remoteAssert3 (RDFFile fi, RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
Assertion	remoteStoreAdd (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
Assertion	remoteStoreRemove (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
PRBool		fileReadablep (char* id);
PRBool		remoteStoreHasAssertionInt (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
PRBool		remoteStoreHasAssertion (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
void *		remoteStoreGetSlotValue (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv);
RDF_Cursor	remoteStoreGetSlotValuesInt (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
RDF_Cursor	remoteStoreGetSlotValues (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
void *		remoteStoreNextValue (RDFT mcf, RDF_Cursor c);
RDF_Error	remoteStoreDisposeCursor (RDFT mcf, RDF_Cursor c);
void		gcRDFFile (RDFFile f);
void RDFFilePossiblyAccessFile (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep) ;
void		possiblyRefreshRDFFiles ();

static PRBool	fileReadp (RDFT rdf, char* url, PRBool mark);
static void	possiblyAccessFile (RDFT mcf, RDF_Resource u, RDF_Resource s, PRBool inversep);
static RDFFile	leastRecentlyUsedRDFFile (RDF mcf);
static PRBool	freeSomeRDFSpace (RDF mcf);

XP_END_PROTOS

#endif
