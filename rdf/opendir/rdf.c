/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  
RDF_Resource RDF_OnePropSource (RDFT db, RDF_Resource u, RDF_Resource s) {
  return (RDF_Resource) getSlotValue(db, u, s, RDF_RESOURCE_TYPE, 1, 1);
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
  return 0;
}


