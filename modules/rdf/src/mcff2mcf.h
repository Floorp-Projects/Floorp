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

#ifndef	_RDF_MCFF2MCF_H_
#define	_RDF_MCFF2MCF_H_


#include "rdf-int.h"



/* mcff2mcf.c data structures and defines */




/* mcff2mcf.c function prototypes */

XP_BEGIN_PROTOS

RDF_Resource		getMCFFrtop (char* furl);
RDFFile			makeRDFFile (char* url, RDF_Resource top, PRBool localp);
void			initRDFFile (RDFFile ans);
void			finishRDFParse (RDFFile f);
void			abortRDFParse (RDFFile f);
void			addToResourceList (RDFFile f, RDF_Resource u);
void			addToAssertionList (RDFFile f, Assertion as);
void			parseNextRDFBlob (RDFFile f, char* blob, int32 size);
int			parseNextMCFBlob(NET_StreamClass *stream, char* blob, int32 size);
void			parseNextMCFLine (RDFFile f, char* line);
void			finishMCFParse (RDFFile f);
void			resourceTransition (RDFFile f);
void			assignHeaderSlot (RDFFile f, char* slot, char* value);
RDF_Error		getFirstToken (char* line, char* nextToken, int16* l);

void			assignSlot (RDF_Resource u, char* slot, char* value, RDFFile f);
RDF_Error		parseSlotValue (RDFFile f, RDF_Resource s, char* value, void** parsed_value, RDF_ValueType* data_type);
void			derelativizeURL (char* tok, char* url, RDFFile f);
RDF_Resource		resolveReference (char *tok, RDFFile f);
RDF_Resource		resolveGenlPosReference(char* tok,  RDFFile f);
char *			getRelURL (RDF_Resource u, RDF_Resource top);
PRBool			bookmarkSlotp (RDF_Resource s);

XP_END_PROTOS

#endif
