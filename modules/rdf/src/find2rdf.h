/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
