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

#ifdef	XP_MAC

#include <Appletalk.h>
#include <Devices.h>
#include <Gestalt.h>
#include "rdf.h"
#include "rdf-int.h"
#include "mcf.h"
#include "vocab.h"
#include "utils.h"
#include "prefapi.h"


/* atalk.c data structures and defines */

extern	int	RDF_APPLETALK_TOP_NAME, RDF_AFP_CLIENT_37_STR, RDF_AFP_AUTH_FAILED_STR;
extern	int	RDF_AFP_PW_EXPIRED_STR, RDF_AFP_ALREADY_MOUNTED_STR, RDF_AFP_MAX_SERVERS_STR;
extern	int	RDF_AFP_NOT_RESPONDING_STR, RDF_AFP_SAME_NODE_STR, RDF_AFP_ERROR_NUM_STR;
extern	int	RDF_VOLUME_DESC_STR, RDF_DIRECTORY_DESC_STR, RDF_FILE_DESC_STR;

#define	ATALK_NOHIERARCHY_PREF	"browser.navcenter.appletalk.zone.nohierarchy"

#define	ATALK_CMD_PREFIX		"Command:at:"

#define	kAppleShareVerGestalt		'afps'
#define	kAppleShareVer_3_7		0x00000006
#define	AFPX_PROT_VERSION		0
#define	BASE_AFPX_OFFSET		30
#define	BASE_AFP_OFFSET			24

typedef	struct	_ourNBPUserDataStruct
{
	RDFT			rdf;
	char			*parentID;
} ourNBPUserDataStruct;
typedef	ourNBPUserDataStruct *ourNBPUserDataPtr;


/* atalk.c function prototypes */

XP_BEGIN_PROTOS

#ifdef	XP_MAC
PRBool		isAFPVolume(short ioVRefNum);
#endif

void		getZones(RDFT rdf);
void		processZones(RDFT rdf, char *zones, uint16 numZones, XP_Bool noHierarchyFlag);
void		checkServerLookup (MPPParamBlock *nbp);
void		getServers(RDFT rdf, RDF_Resource parent);
void		AtalkPossible(RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep);
RDF_Error	AtalkDestroy (RDFT r);
PRBool		AtalkHasAssertion (RDFT mcf, RDF_Resource u, RDF_Resource s, void *v, RDF_ValueType type, PRBool tv);
PRBool		AtalkAssert (RDFT rdf, RDF_Resource u, RDF_Resource s, void *v, RDF_ValueType type, PRBool tv);
char *		convertAFPtoUnescapedFile(char *id);
void *		AtalkGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv);
RDF_Cursor	AtalkGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
void *		AtalkNextValue (RDFT rdf, RDF_Cursor c);
RDF_Resource	CreateAFPFSUnit (char *nname, PRBool isDirectoryFlag);
RDFT		MakeAtalkStore (char* url);

XP_END_PROTOS

#endif

#endif
