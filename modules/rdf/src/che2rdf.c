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

#include "rdf-int.h"

/* We need to define a new class of urls for cache objects.
   e.g., cache:<something here>

  The url for the rdf datastore corresponding to the cache
  is rdf:cache.

  The Cache Schema.
   The cache consists of a hierarchy of objects. The standard
   hierarchy relation (RDF_parent). In addition, each object
   may have the following properties.
    lastAccess
    lastModified

  container cache objects start with cache:container ...
    
   */


RDFT gCacheStore = NULL;

RDFT MakeCacheStore (char* url) {
  if (startsWith("rdf:cache", url)) {
    if (gCacheStore != NULL) {
      return gCacheStore;
    } else {
      RDF_Translator ntr = (RDF_Translator)getMem(sizeof(RDF_TranslatorStruct));
      ntr->assert = NULL;
      ntr->unassert = NULL;
      ntr->getSlotValue = cacheGetSlotValue;
      ntr->getSlotValues = cacheGetSlotValues;
      ntr->hasAssertion = cacheHasAssertion;
      ntr->nextValue = cacheNextValue;
      ntr->disposeCursor = cacheDisposeCursor;
      gCacheStore = ntr;
      return ntr;
    }
  } else return NULL;
}


PRBool
cacheHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
		   RDF_ValueType type, PRBool tv) {
  if ((resourceType(u) == CACHE_RT) && tv) {
    /*e.g., u->id =  cache:http://www.netscape.com/
            s     = gWebData->RDF_size
            v     = 1000
            type  = RDF_INT_TYPE
      return true if the cache object corresponding to u has a size of 1000
     e.g.,  u->id = cache:http://www.netscape.com/
            s     = gCoreVocab->RDF_parent
            type  = RDF_RESOURCE_TYPE
            v->   = "cache:container:MemoryCache"
	    */
        
  } else {
    return 0;
  }
}

	
RDF_Cursor cacheGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, 
				     RDF_ValueType type,  PRBool inversep, PRBool tv) {
  if ((resourceType(u) == CACHE_RT) && tv && (s == gCoreVocab->RDF_parent) && inversep) {
    RDF_Cursor c;   
    c = (RDF_Cursor) getMem(sizeof(RDF_CursorStruc));
    c->u = u;
    c->count = 0;
    c->pdata = NULL;
    c->type = type;
    return c;
  } else return NULL;
}


void* cacheNextValue (RDFT rdf, RDF_Cursor c) {
  "return the next value, update count, return NUll when there are no more.
   the children are nodes. to create a new node, call RDF_Create(url, 1);
   If something is a container, after getting the object, do  setContainerp(r, 1);"
    }

RDF_Error cacheDisposeCursor (RDFT rdf, RDF_Cursor c) {
"dispose it. c could be NULL"
  }


void* cacheGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, 
			 PRBool inversep,  PRBool tv) {
  if ("willing to answer this") {
    return the value;
  } else {
    return NULL;
  }
}
