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

#ifndef	_RDF_NLCSTORE_H_
#define	_RDF_NLCSTORE_H_


#include "rdf-int.h"
#include "rdf.h"
#include "xp.h"
#include "mcom_ndbm.h"
#include "xpassert.h"
#include "xpgetstr.h"


#if !defined(IS_LITTLE_ENDIAN) && !defined(IS_BIG_ENDIAN)
#error Must have a byte order
#endif

#ifdef IS_LITTLE_ENDIAN
#define COPY_INT16(_a,_b)  XP_MEMCPY(_a, _b, sizeof(uint16));
#else
#define	COPY_INT16(_a,_b)				\
	do {						\
	((char *)(_a))[0] = ((char *)(_b))[1];		\
	((char *)(_a))[1] = ((char *)(_b))[0];		\
	} while(0)
#endif
#if !defined(XP_MAC) && !defined(COPY_INT16)
	#define COPY_INT16(_a,_b)  XP_MEMCPY(_a, _b, sizeof(int16));
#endif



/* nlcstore.c data structures and defines */

extern	int		RDF_PERSONAL_TOOLBAR_NAME;

typedef struct _DBMAsStruct {
  uint8 size[3];
  char tag;
  char *data;
} DBMAsStruct;

typedef DBMAsStruct* DBMAs;

typedef struct _DBMRDFStruct {
  DB *propDB;
  DB *invPropDB;
  DB *nameDB;
  DB *childrenDB;
} *DBMRDF;


#define dataOfDBMAs(dbmas) ((char*)((char*)dbmas + 4))
#define dbmasSize(dbmas) ((size_t)(((1 << 16) * dbmas->size[0]) + ((1 << 8) * dbmas->size[1]) + dbmas->size[2]))
#define nthdbmas(data, n) ((DBMAs)((char*)data + n))
#define valueTypeOfAs(nas) ((RDF_ValueType) ((*(((unsigned char *)nas)+3)) & 0x0F))
#define tvOfAs(nas) ((PRBool)((nas->tag & 0x10) != 0))
#define valueEqual(type, v1, v2) (((type == RDF_RESOURCE_TYPE) && stringEquals((char*)v1, resourceID((RDF_Resource)v2))) || \
	  ((type == RDF_INT_TYPE) && (compareUnalignedUINT32Ptrs(v1,v2))) || \
	  ((type == RDF_STRING_TYPE) && stringEquals((char*)v1, (char*)v2)))



/* nlcstore.c function prototypes */

XP_BEGIN_PROTOS

PRBool		compareUnalignedUINT32Ptrs(void *v1, void *v2);
char *		makeRDFDBURL(char* directory, char* name);
void		readInBookmarksOnInit(RDFFile f);
void		DBM_OpenDBMStore (DBMRDF store, char* directory);
RDF_Error	DBM_CloseRDFDBMStore (RDFT r);
char *		makeUSKey (RDF_Resource u, RDF_Resource s, PRBool inversep, size_t *size);
DB *		getUSDB (RDFT r, RDF_Resource u, RDF_Resource s, PRBool inversep);
void		freeKey (char* keyData, RDF_Resource u, RDF_Resource s, PRBool inversep);
DBMAs *		DBM_GetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep, size_t *size);
void		DBM_PutSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep, void* value, size_t size);
PRBool		nlocalStoreHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
void *		nlocalStoreGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep, PRBool tv);
RDF_Cursor	nlocalStoreGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep, PRBool tv);
void *		nlocalStoreNextValue (RDFT rdf, RDF_Cursor c);
RDF_Error	nlocalStoreDisposeCursor (RDFT rdf, RDF_Cursor c);
DBMAs		makeAsBlock (void* v, RDF_ValueType type, PRBool tv,  size_t *size);
PRBool		nlocalStoreAssert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
PRBool		nlocalStoreAssert1 (RDFFile f, RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
PRBool		nlocalStoreUnassert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
void		addSlotsHere (RDFT rdf, RDF_Resource u, RDF_Resource s);
void		deleteSlotsHere (RDFT rdf, RDF_Resource u, RDF_Resource s);
void		addSlotsIn (RDFT rdf, RDF_Resource u, RDF_Resource s);
void		deleteSlotsIn (RDFT rdf, RDF_Resource u, RDF_Resource s);
void		nlclStoreKill (RDFT rdf, RDF_Resource u);
PRBool		nlocalStoreAddChildAt(RDFT rdf, RDF_Resource parent, RDF_Resource ref, RDF_Resource new, PRBool beforep);
RDFT		MakeLocalStore (char* url);

XP_END_PROTOS

#endif
