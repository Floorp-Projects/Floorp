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

#ifndef rdfc_h__
#define rdfc_h__

#include "rdf.h"
#include "nspr.h"

/* core rdf apis */

NSPR_BEGIN_EXTERN_C

typedef struct _RDF_InitParamsStruct {
	char	*profileURL;
	char	*bookmarksURL;
	char	*globalHistoryURL;
} RDF_InitParamsStruct;

typedef struct _RDF_InitParamsStruct* RDF_InitParams;

struct RDF_NotificationStruct;
typedef struct RDF_NotificationStruct* RDF_Notification; 

typedef void (*RDF_NotificationProc)(RDF_Event theEvent, void* pdata);

PR_PUBLIC_API(RDF) RDF_GetDB(const char** dbs);
PR_PUBLIC_API(RDF_Error) RDF_ReleaseDB(RDF rdf);
PR_PUBLIC_API(RDFT) RDF_AddDataSource(RDF rdf, char* dataSource);
PR_PUBLIC_API(RDF_Error) RDF_ReleaseDataSource(RDF rdf, RDFT dataSource);
PR_PUBLIC_API(RDF_Resource) RDF_GetResource(RDF db, char* id, PRBool createp);
PR_PUBLIC_API(RDF_Error) RDF_ReleaseResource(RDF db, RDF_Resource resource);
PR_PUBLIC_API(RDF_Error) RDF_DeleteAllArcs(RDF rdfDB, RDF_Resource source);
PR_PUBLIC_API(RDF_Error) RDF_Update(RDF rdfDB, RDF_Resource u);
PR_PUBLIC_API(char*) RDF_ResourceID(RDF_Resource u);

PR_PUBLIC_API(RDF_Notification) RDF_AddNotifiable (RDF rdfDB, RDF_NotificationProc callBack, RDF_Event ev, void* pdata);
PR_PUBLIC_API(RDF_Error) RDF_DeleteNotifiable (RDF_Notification ns);


PR_PUBLIC_API(PRBool) RDF_Assert(RDF rdfDB, RDF_Resource source, RDF_Resource arcLabel, 
				 void* target, RDF_ValueType targetType);
PR_PUBLIC_API(PRBool) RDF_AssertFalse(RDF rdfDB, RDF_Resource source, RDF_Resource arcLabel, 
				      void* target, RDF_ValueType targetType);
PR_PUBLIC_API(PRBool) RDF_Unassert(RDF rdfDB, RDF_Resource source, RDF_Resource arcLabel, 
				   void* target, RDF_ValueType targetType);

 
PR_PUBLIC_API(PRBool) RDF_CanAssert(RDF rdfDB, RDF_Resource u, RDF_Resource arcLabel, void* v, RDF_ValueType targetType);
PR_PUBLIC_API(PRBool) RDF_CanAssertFalse(RDF rdfDB, RDF_Resource u, RDF_Resource arcLabel, void* v, RDF_ValueType targetType);
PR_PUBLIC_API(PRBool) RDF_CanUnassert(RDF rdfDB, RDF_Resource u, RDF_Resource arcLabel, void* v, RDF_ValueType targetType);
 

PR_PUBLIC_API(PRBool) RDF_HasAssertion (RDF rdfDB, RDF_Resource source, RDF_Resource arcLabel, 
					void* target, RDF_ValueType targetType, PRBool tv);
PR_PUBLIC_API(void*) RDF_GetSlotValue (RDF rdfDB, RDF_Resource u, RDF_Resource s, RDF_ValueType targetType, 
				       PRBool inversep, PRBool tv);
PR_PUBLIC_API(RDF_Cursor) RDF_GetTargets (RDF rdfDB, RDF_Resource source, RDF_Resource arcLabel, 
					  RDF_ValueType targetType,  PRBool tv);	
PR_PUBLIC_API(RDF_Cursor) RDF_GetSources (RDF rdfDB, RDF_Resource target, RDF_Resource arcLabel, 
					  RDF_ValueType sourceType,  PRBool tv);	
PR_PUBLIC_API(RDF_Cursor) RDF_ArcLabelsOut (RDF rdfDB, RDF_Resource u);
PR_PUBLIC_API(RDF_Cursor) RDF_ArcLabelsIn (RDF rdfDB, RDF_Resource u);
PR_PUBLIC_API(void*) RDF_NextValue(RDF_Cursor c);
PR_PUBLIC_API(char*) RDF_ValueDataSource(RDF_Cursor c);
PR_PUBLIC_API(RDF_ValueType) RDF_CursorValueType(RDF_Cursor c);
PR_PUBLIC_API(RDF_Error) RDF_DisposeCursor (RDF_Cursor c);



/*** Guha needs to get his act together and figure out how to do this.
PR_PUBLIC_API(RDF_Error) RDF_Undo(RDF rdf);
***/

/* These two should be removed soon. They are here because Nav Center
depends on them. */

/* PR_PUBLIC_API(RDF_Error) RDF_Init(char *profileDirURL); */
/* PR_PUBLIC_API(RDF_Error) RDF_Init(RDF_InitParams params); */
PR_PUBLIC_API(RDF_Error) RDF_Init(void);
PR_PUBLIC_API(RDF_Error) RDF_Shutdown(void);

/* the stuff in vocab.h will supercede whats below. I am leaving this here
   only for the very near future */


/** utilities : move out of here!!! **/
/* well known resources */


PR_PUBLIC_API(char*) RDF_GetResourceName(RDF rdfDB, RDF_Resource node);
PR_PUBLIC_API(void) SetBookmarkURL(const char* url);
PR_PUBLIC_API(RDF_Resource) RDFUtil_GetFirstInstance (RDF_Resource type, char* defaultURL);
PR_PUBLIC_API(void) RDFUtil_SetFirstInstance (RDF_Resource type, RDF_Resource item);

typedef void (*printProc)(void* data, char* str);
PR_PUBLIC_API(void) outputMCFTree  (RDF db, printProc printer, void* data, RDF_Resource node);
PR_PUBLIC_API(RDF_Resource) RDFUtil_GetBreadcrumb();
PR_PUBLIC_API(RDF_Resource) RDFUtil_GetQuickFileFolder();
PR_PUBLIC_API(void) RDFUtil_SetQuickFileFolder(RDF_Resource container);
PR_PUBLIC_API(RDF_Resource) RDFUtil_GetPTFolder();
PR_PUBLIC_API(void) RDFUtil_SetPTFolder(RDF_Resource container);
PR_PUBLIC_API(RDF_Cursor)  RDF_Find (RDF_Resource s, RDF_Resource match, void* v, RDF_ValueType type);
PR_PUBLIC_API(RDF_Resource) RDFUtil_GetNewBookmarkFolder();
PR_PUBLIC_API(void) RDFUtil_SetNewBookmarkFolder(RDF_Resource container);
PR_PUBLIC_API(RDF_Resource) RDFUtil_GetDefaultSelectedView();
PR_PUBLIC_API(void) RDFUtil_SetDefaultSelectedView(RDF_Resource container);

/** end utilities **/

/* this stuff is stuck in here for netlib */

NSPR_END_EXTERN_C

#endif /* rdfc_h__ */
