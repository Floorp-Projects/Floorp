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


#ifndef rdf_h___
#define rdf_h___


#include "prtypes.h"


typedef int16 RDF_Error;
typedef uint16 RDF_ValueType;


#define RDF_ILLEGAL_ASSERT ((RDF_Error)0x0001)
#define RDF_ILLEGAL_KILL   ((RDF_Error)0x0002)
#define RDF_NO_MEMORY      ((RDF_Error)0x0003)


#define RDF_RESOURCE_TYPE ((RDF_ValueType)0x0001)
#define RDF_INT_TYPE      ((RDF_ValueType)0x0002)
#define RDF_STRING_TYPE   ((RDF_ValueType)0x0003)
#define RDF_NATIVE_TYPE   ((RDF_ValueType)0x0004)

NSPR_BEGIN_EXTERN_C

typedef struct RDF_ResourceStruct* RDF_Resource;
typedef struct RDF_CursorStruct* RDF_Cursor;
typedef struct RDF_DBStruct* RDF;
typedef struct RDF_TranslatorStruct *RDFT;
typedef uint32 RDF_EventType;

#define RDF_ASSERT_NOTIFY	((RDF_EventType)0x00000001)
#define RDF_DELETE_NOTIFY	((RDF_EventType)0x00000002)
#define RDF_KILL_NOTIFY		((RDF_EventType)0x00000004)
#define RDF_CREATE_NOTIFY	((RDF_EventType)0x00000008)
#define RDF_RESOURCE_GC_NOTIFY	((RDF_EventType)0x00000010)
#define RDF_INSERT_NOTIFY	((RDF_EventType)0x00000020)

struct RDF_NotificationStruct;

typedef struct  RDF_AssertEventStruct {
  RDF_Resource u;
  RDF_Resource s;
  void*        v;
  RDF_ValueType type;
  PRBool       tv;
  char*        dataSource;
} *RDF_AssertEvent;


typedef struct RDF_UnassertEventStruct {
  RDF_Resource u;
  RDF_Resource s;
  void*        v;
  RDF_ValueType type;
  char*        dataSource;
} *RDF_UnassertEvent;

typedef struct RDF_KillEventStruct {
  RDF_Resource u;
} *RDF_KillEvent;


typedef struct RDF_EventStruct {
  RDF_EventType eventType;
  union ev {
    struct RDF_AssertEventStruct assert;
    struct RDF_UnassertEventStruct unassert;
    struct RDF_KillEventStruct    kill;
  } event;
} *RDF_Event;




typedef struct _RDF_InitParamsStruct {
	char	*profileURL;
	char	*bookmarksURL;
	char	*globalHistoryURL;
} RDF_InitParamsStruct;

typedef struct _RDF_InitParamsStruct* RDF_InitParams;

typedef struct RDF_NotificationStruct* RDF_Notification; 

typedef void (*RDF_NotificationProc)(RDF_Event theEvent, void* pdata);


/* core rdf apis */

PR_PUBLIC_API(RDF) RDF_GetDB(const char** dbs);
PR_PUBLIC_API(RDF_Error) RDF_ReleaseDB(RDF rdf);
PR_PUBLIC_API(RDFT) RDF_AddDataSource(RDF rdf, char* dataSource);
PR_PUBLIC_API(RDF_Error) RDF_ReleaseDataSource(RDF rdf, RDFT dataSource);
PR_PUBLIC_API(RDF_Resource) RDF_GetResource(RDF db, char* id, PRBool createp);
PR_PUBLIC_API(RDF_Error) RDF_ReleaseResource(RDF db, RDF_Resource resource);
PR_PUBLIC_API(RDF_Error) RDF_DeleteAllArcs(RDF rdfDB, RDF_Resource source);
PR_PUBLIC_API(RDF_Error) RDF_Update(RDF rdfDB, RDF_Resource u);

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
PR_PUBLIC_API(RDF_Error) RDF_Init(RDF_InitParams params);
PR_PUBLIC_API(RDF_Error) RDF_Shutdown(void);


/** utilities : move out of here!!! **/
/* well known resources */


#include "vocab.h"

/* the stuff in vocab.h will supercede whats below. I am leaving this here
   only for the very near future */


PR_PUBLIC_API(char*) RDF_GetResourceName(RDF rdfDB, RDF_Resource node);

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

#endif /* rdf_h___ */
