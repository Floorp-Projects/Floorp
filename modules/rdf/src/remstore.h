/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef	_RDF_REMSTORE_H_
#define	_RDF_REMSTORE_H_


#include "rdf-int.h"

#include "prtime.h"



/* remstore.c data structures and defines */

struct RDFTOutStruct {
	char		*buffer;
	int32		bufferSize;
	int32		bufferPos;
	char		*temp;
	RDFT		store;
};
typedef struct RDFTOutStruct	*RDFTOut;



/* remstore.c function prototypes */

XP_BEGIN_PROTOS

RDFT		MakeRemoteStore (char* url);
RDFT		existingRDFFileDB (char* url);
RDFT		MakeFileDB (char* url);
void		freeAssertion (Assertion as);
PRBool		remoteAssert3 (RDFFile fi, RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
PRBool		remoteUnassert3 (RDFFile fi, RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
void		remoteStoreflushChildren(RDFT mcf, RDF_Resource parent);
Assertion	remoteStoreAdd (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
Assertion	remoteStoreRemove (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
PRBool		fileReadablep (char* id);
PRBool		remoteStoreHasAssertionInt (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
PRBool		remoteStoreHasAssertion (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
void *		remoteStoreGetSlotValue (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv);
RDF_Cursor	remoteStoreGetSlotValuesInt (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
RDF_Cursor	remoteStoreGetSlotValues (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
RDF_Cursor	remoteStoreArcLabelsIn (RDFT mcf, RDF_Resource u);
RDF_Cursor	remoteStoreArcLabelsOut (RDFT mcf, RDF_Resource u);
void *		arcLabelsOutNextValue (RDFT mcf, RDF_Cursor c);
void *		arcLabelsInNextValue (RDFT mcf, RDF_Cursor c);
void *		remoteStoreNextValue (RDFT mcf, RDF_Cursor c);
RDF_Error	remoteStoreDisposeCursor (RDFT mcf, RDF_Cursor c);
RDF_Error	DeleteRemStore (RDFT db);
RDF_Error 	remStoreUpdate (RDFT db, RDF_Resource u);
void		gcRDFFile (RDFFile f);
void		RDFFilePossiblyAccessFile (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep);
void		possiblyRefreshRDFFiles ();
void		SCookPossiblyAccessFile (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep);
RDFT		MakeSCookDB (char* url);
void		addToRDFTOut (RDFTOut out);
PRIntn		RDFSerializerEnumerator (PLHashEntry *he, PRIntn i, void *arg);
RDFFile makeNewRDFFile (char* url, RDF_Resource top, PRBool localp, RDFT db) ;
static PRBool	fileReadp (RDFT rdf, char* url, PRBool mark);
static void	possiblyAccessFile (RDFT mcf, RDF_Resource u, RDF_Resource s, PRBool inversep);
static RDFFile	leastRecentlyUsedRDFFile (RDF mcf);
static PRBool	freeSomeRDFSpace (RDF mcf);
RDFFile  reReadRDFFile (char* url, RDF_Resource top, PRBool localp, RDFT db);

XP_END_PROTOS

#endif
