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

#ifndef	_RDF_COLUMNS_H_
#define	_RDF_COLUMNS_H_


#include "xp.h"
#include "xpassert.h"
#include "rdf-int.h"
#include "htrdf.h"



/* columns.c data structures */



/* columns.c function prototypes */

XP_BEGIN_PROTOS

RDF_Cursor	ColumnsGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
void *		ColumnsGetSlotValue(RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep, PRBool tv);
void *		ColumnsNextValue (RDFT rdf, RDF_Cursor c);
RDF_Error	ColumnsDisposeCursor (RDFT rdf, RDF_Cursor c);
RDFT		MakeColumnStore (char* url);

XP_END_PROTOS

#endif
