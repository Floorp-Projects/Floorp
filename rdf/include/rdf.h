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


#ifndef rdf_h___
#define rdf_h___

#include "nspr.h"
#include "nsError.h"

typedef int RDF_Error;

#define RDF_ERROR_ILLEGAL_ASSERT 1 /* NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_RDF,1) */
#define RDF_ERROR_ILLEGAL_KILL   2 /* NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_RDF,2) */
#define RDF_ERROR_UNABLE_TO_CREATE 3 /*NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_RDF,3) */

#define RDF_ERROR_NO_MEMORY NS_ERROR_OUT_OF_MEMORY /* XXX remove this */

NSPR_BEGIN_EXTERN_C

typedef struct RDF_ResourceStruct* RDF_Resource;
typedef struct RDF_CursorStruct* RDF_Cursor;
typedef struct RDF_DBStruct* RDF;
typedef struct RDF_TranslatorStruct *RDFT;

typedef char* RDF_String;

typedef enum { 
  RDF_ANY_TYPE,
  RDF_RESOURCE_TYPE,
  RDF_INT_TYPE,
  RDF_STRING_TYPE,
#ifdef RDF_BLOB
  RDF_BLOB_TYPE
#endif
} RDF_ValueType;


#ifdef RDF_BLOB
typedef struct RDF_BlobStruct {
  PRUint32 size;
  void* data;
} *RDF_Blob;
#endif

typedef struct RDF_NodeStruct {
  RDF_ValueType type;
  union {
    RDF_Resource r;
    RDF_String s;
#ifdef RDF_BLOB
    RDF_Blob b;
#endif
  } value;
} *RDF_Node;

typedef PRUint32 RDF_EventType;
#define RDF_ASSERT_NOTIFY	((RDF_EventType)0x00000001)
#define RDF_DELETE_NOTIFY	((RDF_EventType)0x00000002)
#define RDF_KILL_NOTIFY		((RDF_EventType)0x00000004)
#define RDF_CREATE_NOTIFY	((RDF_EventType)0x00000008)
#define RDF_RESOURCE_GC_NOTIFY	((RDF_EventType)0x00000010)
#define RDF_INSERT_NOTIFY	((RDF_EventType)0x00000020)

typedef PRUint32 RDF_EventMask;
#define RDF_ANY_NOTIFY      ((RDF_EventMask)0xFFFFFFFF)

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

#include "vocab.h"
#include "rdfc.h"

NSPR_END_EXTERN_C

#endif /* rdf_h___ */
