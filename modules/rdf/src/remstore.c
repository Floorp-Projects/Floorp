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

/* 
   This file implements remote store support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "remstore.h"
#include "hist2rdf.h"
#include "fs2rdf.h"
#include "pm2rdf.h"
#include "rdf-int.h"
#include "bmk2mcf.h"


	/* globals */
PLHashTable	*RDFFileDBHash = NULL;




RDFT
MakeRemoteStore (char* url)
{
  if (startsWith("rdf:remoteStore", url)) {
    if (gRemoteStore == 0) {
      RDFT ntr = (RDFT)getMem(sizeof(struct RDF_TranslatorStruct));
      ntr->assert = NULL;
      ntr->unassert = NULL;
      ntr->getSlotValue = remoteStoreGetSlotValue;
      ntr->getSlotValues = remoteStoreGetSlotValues;
      ntr->hasAssertion = remoteStoreHasAssertion;
      ntr->nextValue = remoteStoreNextValue;
      ntr->disposeCursor = remoteStoreDisposeCursor;
      ntr->possiblyAccessFile = RDFFilePossiblyAccessFile ;
      gRemoteStore = ntr;
      ntr->url = copyString(url);
      return ntr;
    } else return gRemoteStore;
  } else return NULL;
}



RDFT
existingRDFFileDB (char* url)
{
	if (RDFFileDBHash == 0) 
		RDFFileDBHash = PL_NewHashTable(100, PL_HashString, PL_CompareStrings, PL_CompareValues, NULL, NULL);
	return  PL_HashTableLookup(RDFFileDBHash, url);
}



RDFT
MakeFileDB (char* url)
{
  if (endsWith(".rdf", url) || endsWith(".mcf", url)) {
    RDFT ntr = existingRDFFileDB(url);
    if (ntr) return ntr;    
    ntr = (RDFT)getMem(sizeof(struct RDF_TranslatorStruct));
    ntr->assert = NULL;
    ntr->unassert = NULL;
    ntr->getSlotValue = remoteStoreGetSlotValue;
    ntr->getSlotValues = remoteStoreGetSlotValues;
    ntr->hasAssertion = remoteStoreHasAssertion;
    ntr->nextValue = remoteStoreNextValue;
    ntr->disposeCursor = remoteStoreDisposeCursor;
    ntr->possiblyAccessFile = RDFFilePossiblyAccessFile ;
    ntr->url = copyString(url);
    PL_HashTableAdd(RDFFileDBHash, url, ntr);
    readRDFFile(url, NULL, 1, ntr);
    return ntr;
  } else return NULL;
}



PRBool
asEqual(RDFT r, Assertion as, RDF_Resource u, RDF_Resource s, void* v, 
	       RDF_ValueType type)
{
  return ((as->db == r) && (as->u == u) && (as->s == s) && (as->type == type) && 
	  ((as->value == v) || 
	   ((type == RDF_STRING_TYPE) && (strcmp(v, as->value) == 0))));
}



Assertion
makeNewAssertion (RDFT r, RDF_Resource u, RDF_Resource s, void* v, 
			    RDF_ValueType type, PRBool tv)
{
  Assertion newAs = (Assertion) getMem(sizeof(struct RDF_AssertionStruct));
  newAs->u = u;
  newAs->s = s;
  newAs->value = v;
  newAs->type = type;
  newAs->tv = tv;
  newAs->db = r;
  if (strcmp(r->url, "rdf:history")) {
    int n = 0;
  }
  return newAs;
}



void
freeAssertion (Assertion as)
{
  if (as->type == RDF_STRING_TYPE) {
    freeMem(as->value);
  } 
  freeMem(as);
}
 


PRBool
remoteAssert3 (RDFFile fi, RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
		     RDF_ValueType type, PRBool tv)
{
  Assertion as = remoteStoreAdd(mcf, u, s, v, type, tv);
  if (as != NULL) {
    void addToAssertionList (RDFFile f, Assertion as) ;
    addToAssertionList(fi, as);
    return 1;
  } else return 0;
}



Assertion
remoteStoreAdd (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
			  RDF_ValueType type, PRBool tv)
{
  Assertion nextAs, prevAs, newAs; 
  nextAs = prevAs = u->rarg1;
  
  while (nextAs != null) {
    if (asEqual(mcf, nextAs, u, s, v, type)) return null;
    prevAs = nextAs;
    nextAs = nextAs->next;
  }
  newAs = makeNewAssertion(mcf, u, s, v, type, tv);
  if (prevAs == null) {
    u->rarg1 = newAs;
  } else {
    prevAs->next = newAs;
  }
  if (type == RDF_RESOURCE_TYPE) {
    nextAs = prevAs = ((RDF_Resource)v)->rarg2;
    while (nextAs != null) {
      prevAs = nextAs;
      nextAs = nextAs->invNext;
    }
    if (prevAs == null) {
      ((RDF_Resource)v)->rarg2 = newAs;
    } else {
      prevAs->invNext = newAs;
    }
  }
  sendNotifications2(mcf, RDF_ASSERT_NOTIFY, u, s, v, type, tv);
  return newAs;
}



Assertion
remoteStoreRemove (RDFT mcf, RDF_Resource u, RDF_Resource s,
			     void* v, RDF_ValueType type)
{
  Assertion nextAs, prevAs, ans;
  PRBool found = false;
  nextAs = prevAs = u->rarg1;
  while (nextAs != null) {
    if (asEqual(mcf, nextAs, u, s, v, type)) {
      if (prevAs == null) {
	u->rarg1 = nextAs->next;
      } else {
	prevAs->next = nextAs->next;
      }
      found = true;
      ans = nextAs;
      break;
    }
    prevAs = nextAs;
    nextAs = nextAs->next; 
  }
  if (found == false) return null;
  if (type == RDF_RESOURCE_TYPE) {
    nextAs = prevAs = ((RDF_Resource)v)->rarg2;
    while (nextAs != null) {
      if (nextAs == ans) {
	if (prevAs == nextAs) {
	  ((RDF_Resource)v)->rarg2 = nextAs->invNext;
	} else {
	  prevAs->invNext = nextAs->invNext;
	}
      }
      prevAs = nextAs;
      nextAs = nextAs->invNext;
    }
  }
  sendNotifications2(mcf, RDF_DELETE_NOTIFY, u, s, v, type, ans->tv);
  return ans;
}



static PRBool
fileReadp (RDFT rdf, char* url, PRBool mark)
{
  RDFFile f;
  RDFFile rdfFiles = (RDFFile) rdf->pdata;
  uint n = 0;
  for (f = rdfFiles; (f != NULL) ; f = f->next) {
    if (urlEquals(url, f->url)) {
      if (mark == true)	f->lastReadTime = PR_Now();
      return true;
    }
  }
  return false;
}



static void
possiblyAccessFile (RDFT mcf, RDF_Resource u, RDF_Resource s, PRBool inversep)
{
  if (mcf->possiblyAccessFile) (*(mcf->possiblyAccessFile))(mcf, u, s, inversep);
}

void RDFFilePossiblyAccessFile (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep) {
  if ((resourceType(u) == RDF_RT) && (strstr(rdf->url, ".rdf") || strstr(rdf->url, ".mcf")) &&
	  (strstr(resourceID(u), ".rdf") || strstr(resourceID(u), ".mcf")) &&
      (s == gCoreVocab->RDF_parent) && (containerp(u))) {
    RDFFile newFile = readRDFFile( resourceID(u), u, false, rdf);
    if(newFile) newFile->lastReadTime = PR_Now();
  }
}




PRBool
remoteStoreHasAssertion (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv)
{
  Assertion nextAs;
  nextAs = u->rarg1;
  while (nextAs != null) {
    if (asEqual(mcf, nextAs, u, s, v, type) && (nextAs->tv == tv)) return true;
    nextAs = nextAs->next;
  }
  possiblyAccessFile(mcf, u, s, 0);
  return false;
}



void *
remoteStoreGetSlotValue (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv)
{
  Assertion nextAs;
  nextAs = (inversep ? u->rarg2 : u->rarg1);
  while (nextAs != null) {
    if ((nextAs->db == mcf) && (nextAs->s == s) && (nextAs->tv == tv) && (nextAs->type == type)) {
      void* ans = (inversep ? nextAs->u : nextAs->value);
      if (type == RDF_STRING_TYPE) {
#ifdef DEBUG_RDF_GetSlotValue_Memory_Needs_Freedom
	return copyString((char*)ans); 
#else
	return ans;
#endif
      } else return ans;
    }
    nextAs = (inversep ? nextAs->invNext : nextAs->next);
  }
  if (s == gCoreVocab->RDF_parent) possiblyAccessFile(mcf, u, s, inversep);
  return null;
}



RDF_Cursor
remoteStoreGetSlotValuesInt (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv)
{
  Assertion as = (inversep ? u->rarg2 : u->rarg1);
  RDF_Cursor c;
  if (as == null) {
    possiblyAccessFile(mcf, u, s, inversep);
    as = (inversep ? u->rarg2 : u->rarg1);
    if (as == NULL) return null;
  }  	
  c = (RDF_Cursor)getMem(sizeof(struct RDF_CursorStruct));
  c->u = u;
  c->s = s;
  c->type = type;
  c->inversep = inversep;
  c->tv = tv;
  c->count = 0;
  c->pdata = as;
  return c;
}



RDF_Cursor
remoteStoreGetSlotValues (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv)
{
 
  return remoteStoreGetSlotValuesInt(mcf, u, s, type, inversep, tv);
}



void *
remoteStoreNextValue (RDFT mcf, RDF_Cursor c)
{
  while (c->pdata != null) {
    Assertion as = (Assertion) c->pdata;
    if ((as->db == mcf) && (as->s == c->s) && (as->tv == c->tv) && (c->type == as->type)) {
      if (c->s == gCoreVocab->RDF_slotsHere) {
        c->value = as->s;
      } else { 
        c->value = (c->inversep ? as->u : as->value);
      }
      c->pdata = (c->inversep ? as->invNext : as->next);
      return c->value;
    }
    c->pdata = (c->inversep ? as->invNext : as->next);
  }
  
  return null;
}



RDF_Error
remoteStoreDisposeCursor (RDFT mcf, RDF_Cursor c)
{
  freeMem(c);
  return noRDFErr;
}



static RDFFile
leastRecentlyUsedRDFFile (RDF mcf)
{
  RDFFile lru = mcf->files;
  RDFFile f;
#ifndef HAVE_LONG_LONG
  int64 result;
#endif /* !HAVE_LONG_LONG */
  for (f = mcf->files ; (f != NULL) ; f = f->next) {
    if (!f->locked) {
#ifndef HAVE_LONG_LONG
      LL_SUB(result, lru->lastReadTime, f->lastReadTime);
      if ((!LL_IS_ZERO(result) && LL_GE_ZERO(result)) && (f->localp == false))
#else
	if (((lru->lastReadTime - f->lastReadTime) > 0) && (f->localp == false))
#endif /* !HAVE_LONG_LONG */
	  lru = f;
    }
  }
  if (!lru->locked) {
    return lru;
  } else return NULL;
}



void
gcRDFFile (RDFFile f)
{
  int16 n = 0;
  RDFFile f1 = (RDFFile) f->db->pdata;

  if (f->locked) return;
  
  if (f == f1) {
    f->db->pdata = f->next;
  } else {
    RDFFile prev = f1;
    while (f1 != NULL) {
      if (f1 == f) {
	prev->next = f->next;
	break;
      }
      prev = f1;
      f1 = f1->next;
    }
  }
  
  while (n < f->assertionCount) {
    Assertion as = *(f->assertionList + n);
    remoteStoreRemove(f->db, as->u, as->s, as->value, as->type);
    freeAssertion(as);
    *(f->assertionList + n) = NULL;
    n++;
  }
  n = 0;
  while (n < f->resourceCount) {
    RDF_Resource u = *(f->resourceList + n);
    possiblyGCResource(u);
    n++;
  }
  freeMem(f->assertionList);
  freeMem(f->resourceList);	
}



static PRBool
freeSomeRDFSpace (RDF mcf)
{
  RDFFile lru = leastRecentlyUsedRDFFile (mcf);
  if (lru== NULL) {
    return false;
  } else {
    gcRDFFile(lru);
    freeMem(lru);
    return true;
  }
}



RDFFile
readRDFFile (char* url, RDF_Resource top, PRBool localp, RDFT db)
{
  if ((!strstr(url, ":/")) ||
      (fileReadp(db, url, true))) {
    return NULL;
  } else {
    RDFFile newFile = makeRDFFile(url, top, localp);  
    if (db->pdata) {  
      newFile->next = (RDFFile) db->pdata;
      db->pdata = newFile;
    } else {
      db->pdata = (RDFFile) newFile;
  }
    newFile->assert = remoteAssert3;
    if (top) {
      if (resourceType(top) == RDF_RT) {
        if (strstr(url, ".mcf")) {
          newFile->fileType = RDF_MCF;
        } else {
          newFile->fileType = RDF_XML;
      }
      } else {
        newFile->fileType = resourceType(top);
      }
    }
    newFile->db = db;
    beginReadingRDFFile(newFile);
    return newFile;
  }
}



void
possiblyRefreshRDFFiles ()
{
  RDFFile f = (RDFFile)gRemoteStore->pdata;
  PRTime tm = PR_Now();  
  while (f != NULL) {
    if (f->expiryTime != NULL) {
      PRTime *expiry = f->expiryTime;
#ifdef HAVE_LONG_LONG
      if ((tm  - *expiry) > 0) 
#else
      int64 result;
      LL_SUB(result, tm, *expiry);
      if ((!LL_IS_ZERO(result) && LL_GE_ZERO(result)))
#endif
	{
      gcRDFFile (f);
      initRDFFile(f);
      beginReadingRDFFile(f);      
    } 
    }
	f = f->next;
  }

}



void
SCookPossiblyAccessFile (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep)
{
	if ((resourceType(u) == RDF_RT) && (strcmp(rdf->url, "rdf:ht") ==0) &&
	  (strstr(resourceID(u), ".rdf") || strstr(resourceID(u), ".mcf")) &&
	     (s == gCoreVocab->RDF_parent) && 
        (containerp(u))) {
    RDFFile newFile = readRDFFile( resourceID(u), u, false, rdf);
    if(newFile) newFile->lastReadTime = PR_Now();
  }
}



RDFT
MakeSCookDB (char* url)
{
  if (startsWith("rdf:scook:", url) || (startsWith("rdf:ht", url))) {
    RDFT ntr = (RDFT)getMem(sizeof(struct RDF_TranslatorStruct));
    ntr->assert = NULL;
    ntr->unassert = NULL;
    ntr->getSlotValue = remoteStoreGetSlotValue;
    ntr->getSlotValues = remoteStoreGetSlotValues;
    ntr->hasAssertion = remoteStoreHasAssertion;
    ntr->nextValue = remoteStoreNextValue;
    ntr->disposeCursor = remoteStoreDisposeCursor;
    ntr->possiblyAccessFile = RDFFilePossiblyAccessFile ;
    ntr->url = copyString(url);
    return ntr;
  } else return NULL;
}
