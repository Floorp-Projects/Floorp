/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef	_RDF_FIND_H_
#define	_RDF_FIND_H_

#include "xp.h"
#include "xpassert.h"
#include "xpgetstr.h"
#include "rdf-int.h"
#include "utils.h"


/* find.c data structures and defines */

typedef	struct	_findTokenStruct	{
	char			*token;
	char			*value;
	} findTokenStruct;


/* find.c function prototypes */

XP_BEGIN_PROTOS

void			parseResourceIntoFindTokens(RDF_Resource u, findTokenStruct *tokens);
RDF_Cursor		parseFindURL(RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
RDF_Cursor		FindGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
void *			findNextURL(RDF_Cursor c);
void *			FindNextValue(RDFT rdf, RDF_Cursor c);
void *			FindGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep, PRBool tv);
void			FindPossible(RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep);
RDF_Error		FindDisposeCursor(RDFT mcf, RDF_Cursor c);
void			findPossiblyAddName(RDFT rdf, RDF_Resource u);
PRBool			FindAssert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
RDFT			MakeFindStore (char* url);

XP_END_PROTOS

#endif
