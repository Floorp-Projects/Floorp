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

/* This file contains the RDF APIs for Brooklyn. These are just
   wrappers around functions implemented in other files */

#include "rdf-int.h"

void  RDF_Initialize () {
  rdf_init();
}

void RDF_ReadFile (char* fileName) {
  readRDFFile(fileName);
}

int RDF_Consume (char* fileName, char* data, int len) {
  return rdf_DigestNewStuff (fileName, data, len) ;
}

void RDF_Unload (RDFT f) {
  unloadRDFT (f);
}

RDFT RDF_GetRDFT (char* url, int createp) {
  return getRDFT (url, createp) ;
}

RDF_Resource RDF_GetResource (char* url, int createp) {
  return getResource (url, createp) ;
}

int RDF_Assert(RDFT db, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type) {
 return (NULL != remoteStoreAdd (db, u, s, v, type, 1)); 
}

int RDF_Unassert (RDFT db, RDF_Resource u, RDF_Resource s,  void* v, RDF_ValueType type) {
  return (NULL != remoteStoreRemove (db, u, s, v, type));
}

int RDF_HasAssertion (RDFT db, RDF_Resource u, RDF_Resource s, 
		      void* v, RDF_ValueType type) {
  return remoteStoreHasAssertion (db,  u,  s,  v,  type, 1);
}
  

void* RDF_OnePropValue (RDFT db, RDF_Resource u, RDF_Resource s, RDF_ValueType type) {
  return getSlotValue (db,  u,  s,  type, 0, 1);
}

RDF_Cursor RDF_GetTargets (RDFT db, RDF_Resource u, RDF_Resource s, RDF_ValueType type) {
  return getSlotValues (db,  u,  s,  type, 0, 1);
}

RDF_Cursor RDF_GetSourcess (RDFT db, RDF_Resource u, RDF_Resource s) {
  return getSlotValues (db,  u,  s, RDF_RESOURCE_TYPE, 1, 1);
} 

void* RDF_NextValue (RDF_Cursor c) {
  return nextValue (c) ;
}

void RDF_DisposeCursor (RDF_Cursor c) {
  disposeCursor(c);
}

char** RDF_processPathQuery(char* query) {
  //  return processRDFQuery (query) ;
}


