/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
