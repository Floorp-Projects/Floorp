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

#ifndef	_RDF_ES2MCF_H_
#define	_RDF_ES2MCF_H_

#include "rdf-int.h"
#include "net.h"



/* es2mcf.c data structures and defines */

typedef struct  {
        char            *parent;
        char            *child;
        int             method;
} _esFEData;



/* es2mcf.c function prototypes */

XP_BEGIN_PROTOS

RDFT		MakeESFTPStore (char* url);
_esFEData *     esMakeFEData(RDF_Resource parent, RDF_Resource child, int method);
void            esFreeFEData(_esFEData *feData);
void ESFTPPossiblyAccessFile (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep) ;
RDF_Error	ESInit (RDFT ntr);
PRBool		ESFTPRT (RDF_Resource u);
PRBool		ESAssert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
PRBool		ESUnassert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
PRBool		ESDBAdd (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
PRBool		ESDBRemove (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
PRBool		ESHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
void *		ESGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv);
RDF_Cursor	ESGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
void *		ESNextValue (RDFT mcf, RDF_Cursor c);
RDF_Error	ESDisposeCursor (RDFT mcf, RDF_Cursor c);
void		es_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx);
char *		nativeFilename(char *filename);
void		ESAddChild (RDFT rdf, RDF_Resource parent, RDF_Resource child);
void		ESRemoveChild (RDFT rdf, RDF_Resource parent, RDF_Resource child);
void		possiblyAccessES(RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep);
void		parseNextESFTPLine (RDFFile f, char* line);
int		parseNextESFTPBlob(NET_StreamClass *stream, char* blob, int32 size);

XP_END_PROTOS

#endif

