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

/* 
   This file implements Super Cookie support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "rdf-int.h"
#include "scook.h"
#include "glue.h"
#include "remstore.h"


	/* externs */
extern char* profileDirURL;



char *
makeSCookPathname(char* name)
{
  char		*ans ;
  size_t		s;

  if ((ans = (char*) getMem(strlen(profileDirURL) + strlen(name) + 8)) != NULL) {
    s = strlen(profileDirURL);
    memcpy(ans, profileDirURL, s);
    if (ans[s-1] != '/') {
      ans[s++] = '/';
    }
    memcpy(&ans[s], "SCook/", 5);
    s = s + 5;

#ifdef	XP_WIN
    if (ans[9] == '|') ans[9] = ':';
#endif

    CallPRMkDirUsingFileURL(ans, 00700);  
    memcpy(&ans[s], name, strlen(name));
  }
  return(ans);
}



PRBool
SCookAssert1 (RDFT mcf,   RDF_Resource u, RDF_Resource s, void* v, 
		     RDF_ValueType type, PRBool tv)
{
	return 0;
}



PRBool
SCookAssert3 (RDFT mcf,   RDF_Resource u, RDF_Resource s, void* v, 
		     RDF_ValueType type, PRBool tv)
{
	return (SCookAssert(mcf, u, s, v, type, tv) != NULL);
}



PRBool
SCookAssert2 (RDFFile  file, RDFT mcf,  RDF_Resource u, RDF_Resource s, void* v, 
		     RDF_ValueType type, PRBool tv)
{
  Assertion as = SCookAssert(mcf , u, s, v, type, tv);
  if (as != NULL) {
    void addToAssertionList (RDFFile f, Assertion as) ;
    addToAssertionList(file, as);
    return 1;
  } else return 0;
}



Assertion
SCookAssert (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
			  RDF_ValueType type, PRBool tv)
{
  Assertion nextAs, prevAs, newAs; 
  nextAs = prevAs = getArg1(mcf, u);
  while (nextAs != null) {
    if (asEqual(mcf, nextAs, u, s, v, type)) return null;
    prevAs = nextAs;
    nextAs = nextAs->next;
  }
  newAs = makeNewAssertion(mcf, u, s, v, type, tv);
  if (prevAs == null) {
    setArg1(mcf, u, newAs);
  } else {
    prevAs->next = newAs;
  }
  if (type == RDF_RESOURCE_TYPE) {
    nextAs = prevAs = getArg2(mcf, (RDF_Resource)v);
    while (nextAs != null) {
      prevAs = nextAs;
      nextAs = nextAs->invNext;
    }
    if (prevAs == null) {
      setArg2(mcf,  ((RDF_Resource)v), newAs);
    } else {
      prevAs->invNext = newAs;
    }
  }
  /* XXX have to mark the entire subtree XXX */  
  sendNotifications2(mcf, RDF_ASSERT_NOTIFY, u, s, v, type, tv);
  return newAs;
}



PRBool
SCookUnassert (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
		       RDF_ValueType type)
{
  Assertion as = SCookRemove(mcf, u, s, v, type);
  freeMem(as);
  return (as != NULL);
}



Assertion
SCookRemove (RDFT mcf, RDF_Resource u, RDF_Resource s, 
			     void* v, RDF_ValueType type)
{
  Assertion nextAs, prevAs, ans;
  PRBool found = false;
  nextAs = prevAs = getArg1(mcf, u);
  while (nextAs != null) {
    if (asEqual(mcf, nextAs, u, s, v, type)) {
      if (prevAs == nextAs) {
	setArg1(mcf, u, nextAs->next);
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
    nextAs = prevAs = getArg2(mcf, (RDF_Resource)v);
    while (nextAs != null) {
      if (nextAs == ans) {
	if (prevAs == nextAs) {
	  setArg2(mcf, ((RDF_Resource)v), nextAs->invNext);
	} else {
	  prevAs->invNext = nextAs->invNext;
	}
      }
      prevAs = nextAs;
      nextAs = nextAs->invNext;
    }
  }
  /* Need to make sure that if something is removed from the bookmark tree,
     the type is updated */
  sendNotifications2(mcf, RDF_DELETE_NOTIFY, u, s, v, type, ans->tv);
  return ans;
}



PRBool
SCookHasAssertion (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv)
{
  Assertion nextAs;
  nextAs = getArg1(mcf, u);
  while (nextAs != null) {
    if (asEqual(mcf, nextAs, u, s, v, type) && (nextAs->tv == tv)) return true;
    nextAs = nextAs->next;
  }
  possiblyAccessSCookFile(mcf, u, s, 0);
  return false;
}



void *
SCookGetSlotValue (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv)
{
  Assertion nextAs;
  nextAs = (inversep ? getArg2(mcf, u) : getArg1(mcf, u));
  while (nextAs != null) {
    if ((nextAs->s == s) && (nextAs->tv == tv) && (nextAs->type == type)) {
      return (inversep ? nextAs->u : nextAs->value);
    }
    nextAs = (inversep ? nextAs->invNext : nextAs->next);
  }
   possiblyAccessSCookFile(mcf, u, s, inversep);
  return null;
}



RDF_Cursor
SCookGetSlotValues (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv)
{
Assertion as = (inversep ? getArg2(mcf, u) : getArg1(mcf, u));
  RDF_Cursor c;
  if (as == null) {
    possiblyAccessSCookFile(mcf, u, s, inversep);
    as = (inversep ? getArg2(mcf, u) : getArg1(mcf, u));
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



void *
SCookNextValue (RDFT mcf, RDF_Cursor c)
{
  while (c->pdata != null) {
    Assertion as = (Assertion) c->pdata;
    if ((as->s == c->s) && (as->tv == c->tv) && (c->type == as->type)) {
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
SCookDisposeCursor (RDFT mcf, RDF_Cursor c)
{
  freeMem(c);
  return noRDFErr;
}



Assertion
getArg1 (RDFT r, RDF_Resource u)
{
  return PL_HashTableLookup(((SCookDB)r->pdata)->lhash, u);
}



Assertion
getArg2 (RDFT r, RDF_Resource u)
{
  return PL_HashTableLookup(((SCookDB)r->pdata)->rhash, u);
}



void
setArg1 (RDFT r, RDF_Resource u, Assertion as)
{
  if (as == NULL) {
    PL_HashTableRemove(((SCookDB)r->pdata)->lhash, u);
  } else {
    PL_HashTableAdd(((SCookDB)r->pdata)->lhash, u, as);
  }
}



void
setArg2 (RDFT r, RDF_Resource u, Assertion as)

{
 if (as == NULL) {
    PL_HashTableRemove(((SCookDB)r->pdata)->rhash, u);
  } else {
    PL_HashTableAdd(((SCookDB)r->pdata)->rhash, u, as);
  }
}



void
gcSCookFile (RDFT rdf, RDFFile f)
{
  int16 n = 0;
  RDFFile f1;
  SCookDB sk = (SCookDB)rdf->pdata;
  f1 = sk->rf;
  
  if (f->locked) return;
  
  if (f == f1) {
    sk->rf = f->next;
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
    SCookUnassert(rdf, as->u, as->s, as->value, as->type);
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



void
disposeAllSCookFiles (RDFT rdf, RDFFile f)
{
  if (f != NULL) {
    disposeAllSCookFiles(rdf, f->next);
    gcSCookFile(rdf, f);
  }
}



void
SCookDisposeDB (RDFT rdf)
{
  SCookDB db = (SCookDB)rdf->pdata;
  disposeAllSCookFiles(rdf, db->rf);
  PL_HashTableDestroy(db->rhash);
  PL_HashTableDestroy(db->lhash);
  freeMem(db);
}



static PRBool
SCookFileReadp (RDFT rdf, RDF_Resource u)
{
  RDFFile f;
  SCookDB db = (SCookDB)rdf->pdata;
  uint n = 0;
  for (f = db->rf; (f != NULL) ; f = f->next) {
	  n++;
    if (urlEquals( resourceID(u), f->url)) {
      return true;
    }
  }
  return false;
}



void
possiblyAccessSCookFile (RDFT mcf, RDF_Resource u, RDF_Resource s, PRBool inversep)
{
  if ((s == gCoreVocab->RDF_parent) && (strstr(resourceID(u), ":/")) &&
      (((SCookDB)mcf->pdata)->rf != NULL) && (containerp(u)) && 
      (resourceType(u) == RDF_RT) && (!SCookFileReadp(mcf, u))) {
    RDFFile newFile = makeRDFFile( resourceID(u), u, 0);
    SCookDB db = (SCookDB)mcf->pdata;
    newFile->next = db->rf;
    newFile->fileType = RDF_XML;
    newFile->db   = mcf;
    db->rf = newFile;
    newFile->db = mcf;
    newFile->assert = SCookAssert2;
    beginReadingRDFFile(newFile);
  }
}

void  SCookPossiblyAccessFile1 (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep) {
	if ((resourceType(u) == RDF_RT) && (strcmp(rdf->url, "rdf:ht") ==0) &&
	  (strstr(resourceID(u), ".rdf") || strstr(resourceID(u), ".mcf")) &&
	     (s == gCoreVocab->RDF_parent) && 
        (containerp(u))) {
    RDFFile newFile = readRDFFile( resourceID(u), u, false, rdf);
    if(newFile) newFile->lastReadTime = PR_Now();
  }
}


RDFT
MakeSCookDB1 (char* url)
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
