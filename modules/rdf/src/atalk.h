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

#ifndef	_RDF_ATALK_H_
#define	_RDF_ATALK_H_


#include <Appletalk.h>
#include "rdf.h"
#include "rdf-int.h"
#include "mcf.h"
#include "vocab.h"
#include "utils.h"


/* atalk.c data structures and defines */


/* atalk.c function prototypes */

XP_BEGIN_PROTOS

void		getZones();
void		processZones(char *zones, uint16 numZones);
void		checkServerLookup (MPPParamBlock *nbp);
void		getServers(RDF_Resource parent);
Assertion	atalkarg1 (RDF_Resource u);
Assertion	setatalkarg1 (RDF_Resource u, Assertion as);
Assertion	atalkarg2 (RDF_Resource u);
Assertion	setatalkarg2 (RDF_Resource u, Assertion as);
PRBool		AtalkHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
void *		AtalkGetSlotValue(RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,	PRBool inversep, PRBool tv);
PRBool		AtalkAdd (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
void *		AtalkNextValue (RDFT rdf, RDF_Cursor c);
RDF_Cursor	AtalkGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
RDF_Error	AtalkDisposeCursor (RDFT rdf, RDF_Cursor c);
RDF_Error	AtalkDestroy (RDFT r);
RDFT		MakeAtalkStore (char* url);

XP_END_PROTOS

#endif
