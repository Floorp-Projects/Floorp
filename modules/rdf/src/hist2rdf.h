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

#ifndef	_RDF_HIST2RDF_H_
#define	_RDF_HIST2RDF_H_


#include "rdf-int.h"
#include "mcom_ndbm.h"
#include "time.h"
#include "xp_mcom.h"
#include "xp_mem.h"
#include "xp_str.h"
#include "xpgetstr.h"
#include "glhist.h"

#include <stdio.h>

#ifndef	NSPR20
#include "prosdep.h"	/* for IS_LITTLE_ENDIAN / IS_BIG_ENDIAN */
#else
#include "prtypes.h"
#endif



#if !defined(IS_LITTLE_ENDIAN) && !defined(IS_BIG_ENDIAN)
#error Must have a byte order
#endif

#ifdef IS_LITTLE_ENDIAN
#define COPY_INT32(_a,_b)  XP_MEMCPY(_a, _b, sizeof(int32));
#else
#define	COPY_INT32(_a,_b)				\
	do {						\
	((char *)(_a))[0] = ((char *)(_b))[3];		\
	((char *)(_a))[1] = ((char *)(_b))[2];		\
	((char *)(_a))[2] = ((char *)(_b))[1];		\
	((char *)(_a))[3] = ((char *)(_b))[0];		\
	} while(0)
#endif

#if !defined(XP_MAC) && !defined(COPY_INT32)
	#define COPY_INT32(_a,_b)  XP_MEMCPY(_a, _b, sizeof(int32));
#endif



/* hist2rdf.c data structures and defines */

#define remoteAddParent(child, parent)   histAddParent(child, parent);
#define remoteAddName(u, name)  remoteStoreAdd(gRemoteStore, u, gCoreVocab->RDF_name, copyString(name), RDF_STRING_TYPE, 1);

#define SECS_IN_HOUR            (60L * 60L)
#define SECS_IN_DAY             (SECS_IN_HOUR * 24L)



/* hist2rdf.c function prototypes */

XP_BEGIN_PROTOS

void		collateHistory (RDF r, RDF_Resource u, PRBool byDateFlag);
void		collateOneHist (RDF r, RDF_Resource u, char* url, char* title, time_t lastAccessDate, time_t firstAccessDate, uint32 numAccesses, PRBool byDateFlag);
RDF_Resource	hostUnitOfURL (RDF r, RDF_Resource top, RDF_Resource nu, char* title);
void		hourRange(char *buffer, struct tm *theTm);
RDF_Resource	hostUnitOfDate (RDF r, RDF_Resource u, time_t lastAccessDate);
void		deleteCurrentSitemaps (char *address);
void		addRelatedLinks (char* address);
PRBool		displayHistoryItem (char* url);
RDF_Resource	HistCreate (char* url, PRBool createp);
Assertion	histAddParent (RDF_Resource child, RDF_Resource parent);
PRBool		historyUnassert (RDFT hst,  RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
RDF_Cursor	historyStoreGetSlotValuesInt (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep, PRBool tv);
PRBool		historyStoreHasAssertion (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
RDFT		MakeHistoryStore (char* url);
void		dumpHist ();

XP_END_PROTOS

#endif
