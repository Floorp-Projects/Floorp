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

#ifndef	_RDF_FS2RDF_H_
#define	_RDF_FS2RDF_H_


#include "rdf-int.h"
#include "xp_mem.h"
#include "client.h"
#include "prio.h"
#include "prlong.h"
#include "nlcstore.h"
#include "remstore.h"

#ifdef	XP_MAC
#include <Appletalk.h>
#include <Devices.h>
#include <Files.h>
#include <FinderRegistry.h>
#include <Folders.h>
#include <Processes.h>

#include "FullPath.h"
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

#ifdef	XP_MAC
OSErr		nativeMacPathname(char *fileURL, FSSpec *fss);
OSErr		getPSNbyTypeSig(ProcessSerialNumber *thePSN, OSType pType, OSType pSignature);
#endif

void		importForProfile (char *dir, const char *uname);
void		GuessIEBookmarks(void);
char *		getVolume(int16 volNum, PRBool afpVols);
PRDir *		OpenDir(char *name);
RDFT		MakeLFSStore (char* url);
PRBool		fsAssert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
PRBool		fsRemoveFile(RDFT rdf, char *filePathname, PRBool justCheckWriteAccess);
PRBool		fsRemoveDir(RDFT rdf, char *filePathname, PRBool justCheckWriteAccess);
PRBool		fsUnassert (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
PRBool		fsHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
void *		fsGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv);
PRBool		fileDirectoryp(RDF_Resource u);
RDF_Cursor	fsGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
void *		fsNextValue (RDFT rdf, RDF_Cursor c);
PRBool		isFileVisible(char *fileURL);
RDF_Error	fsDisposeCursor (RDFT rdf, RDF_Cursor c);
RDF_Resource	CreateFSUnit (char* nname, PRBool isDirectoryFlag);

XP_END_PROTOS

#endif
