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

#ifndef	_RDF_FS2RDF_H_
#define	_RDF_FS2RDF_H_


#include "rdf-int.h"
#include "xp_mem.h"
#include "client.h"
#include "prio.h"
#include "prlong.h"

#ifdef	XP_MAC
#include <Appletalk.h>
#include <Devices.h>
#include <Files.h>
#endif



/* fs2rdf.c data structures and defines */

extern	int	RDF_VOLUME_DESC_STR, RDF_DIRECTORY_DESC_STR, RDF_FILE_DESC_STR;

#define fsUnitp(u) (resourceType(u) == LFS_RT)

#define XP_DIRECTORY_SEPARATOR '/'


#ifdef	XP_WIN
#define	FS_URL_OFFSET		8
#else
#define	FS_URL_OFFSET		7
#endif



/* fs2rdf.c function prototypes */

XP_BEGIN_PROTOS

void		GuessIEBookmarks(void);
char *		getVolume(int16 volNum, PRBool afpVols);
PRDir *		OpenDir(char *name);
RDFT		MakeLFSStore (char* url);
PRBool		fsRemoveFile(char *filePathname, PRBool justCheckWriteAccess);
PRBool		fsRemoveDir(char *filePathname, PRBool justCheckWriteAccess);
PRBool		fsUnassert (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
PRBool		fsHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
void *		fsGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv);
PRBool		fileDirectoryp(RDF_Resource u);
RDF_Cursor	fsGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
void *		fsNextValue (RDFT rdf, RDF_Cursor c);
RDF_Error	fsDisposeCursor (RDFT rdf, RDF_Cursor c);
RDF_Resource	CreateFSUnit (char* nname, PRBool isDirectoryFlag);

XP_END_PROTOS

#endif
