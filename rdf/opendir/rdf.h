/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* This is the header file for the functions exported from RDF */



typedef struct _RDF_ResourceStruct* RDF_Resource;
typedef struct _RDF_FileStruct* RDFT;
typedef struct _RDF_CursorStruct* RDF_Cursor;

typedef enum { 
  RDF_RESOURCE_TYPE,
  RDF_INT_TYPE,
  RDF_STRING_TYPE
} RDF_ValueType;


void  RDF_Initialize () ;
int RDF_Consume (char* fileName, char* data, int len) ;
void RDF_ReadFile (char* fileName) ;
void RDF_Unload (RDFT f);
RDFT RDF_GetRDFT (char* url, int createp);

RDF_Resource RDF_GetResource (char* url, int createp);

int RDF_Assert(RDFT db, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
int RDF_Unassert (RDFT db, RDF_Resource u, RDF_Resource s,  void* v, RDF_ValueType type);

int RDF_HasAssertion (RDFT db, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
void* RDF_OnePropValue (RDFT db, RDF_Resource u, RDF_Resource s, RDF_ValueType type);
RDF_Cursor RDF_GetTargets (RDFT db, RDF_Resource u, RDF_Resource s, RDF_ValueType type);
RDF_Cursor RDF_GetSourcess (RDFT db, RDF_Resource u, RDF_Resource s);
void* RDF_NextValue (RDF_Cursor c) ;
void RDF_DisposeCursor (RDF_Cursor c);

char** RDF_processPathQuery(char* query);
