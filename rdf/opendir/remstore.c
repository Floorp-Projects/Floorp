/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "rdf-int.h"
#include "gs.h"

int
asEqual(RDFT r, Assertion as, RDF_Resource u, RDF_Resource s, void* v, 
	       RDF_ValueType type)
{
  return (((r == NULL) || (as->db == r)) && 
	  (as->u == u) && 
	  (as->s == s) && 
	  (as->type == type) && 
	  ((as->value == v) || 
	   ((type == RDF_STRING_TYPE) && (strcmp((char*)v, (char*)as->value) == 0))));
}



Assertion
makeNewAssertion (RDFT r, RDF_Resource u, RDF_Resource s, void* v, 
			    RDF_ValueType type, int tv)
{
  Assertion newAs = (Assertion) fgetMem(sizeof(RDF_AssertionStruct));
  newAs->u = u;
  newAs->s = s;
  newAs->value = v;
  newAs->type = type;
  newAs->db = r;
  return newAs;
}


void
addToAssertionList (RDFT f, Assertion as)
{
  if (f->assertionListCount >= f->assertionListSize) {
    f->assertionList = (Assertion*) realloc(f->assertionList, 
			       (sizeof(Assertion*) *
				    (f->assertionListSize = 
				     f->assertionListSize + GROW_LIST_INCR)));
  }
  *(f->assertionList + f->assertionListCount++) = as;
}

void
freeAssertion (Assertion as)
{
  if (as->type == RDF_STRING_TYPE) {
    freeMem(as->value);
  } 
  freeMem(as);
}


Assertion
remoteStoreAdd (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
			  RDF_ValueType type, int tv)
{
  Assertion  newAs = makeNewAssertion(mcf, u, s, v, type, tv);
  newAs->next = u->rarg1;
  u->rarg1 = newAs;

  if (type == RDF_RESOURCE_TYPE) {
    RDF_Resource iu = (RDF_Resource)v;
    newAs->invNext  = iu->rarg2;
    iu->rarg2       = newAs;
  }
  /*  if (type == RDF_STRING_TYPE)   RDFGS_AddSearchIndex(mcf, (char*) v, s, u); */
  
  return newAs;
}


Assertion
remoteStoreRemove (RDFT mcf, RDF_Resource u, RDF_Resource s,
			     void* v, RDF_ValueType type)
{
  Assertion nextAs, prevAs, ans;
  int found = false;
  nextAs = prevAs = u->rarg1;
  while (nextAs != null) {
    if (asEqual(mcf, nextAs, u, s, v, type)) {
      if (prevAs == nextAs) {
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
  return ans;
}

int
remoteStoreHasAssertion (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, int tv)
{
  Assertion nextAs;
  
  nextAs = u->rarg1;
  while (nextAs != null) {
    if (asEqual(mcf, nextAs, u, s, v, type)) return true;
    nextAs = nextAs->next;
  }
  return false;
}



void *
getSlotValue (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, int inversep,  int tv)
{
  Assertion nextAs;

  nextAs = (inversep ? u->rarg2 : u->rarg1);
  while (nextAs != null) {
    if (((nextAs->db == mcf) || (!mcf)) && (nextAs->s == s) 
        && (nextAs->type == type)) {
      return (inversep ? nextAs->u : nextAs->value);
    }
    nextAs = (inversep ? nextAs->invNext : nextAs->next);
  }
  return null;
}

RDF_Cursor
getSlotValues (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  int inversep, int tv)
{
  Assertion as = (inversep ? u->rarg2 : u->rarg1);
  RDF_Cursor c;
  if (!as) return NULL;
  c = (RDF_Cursor)getMem(sizeof(RDF_CursorStruct));
  c->u = u;
  c->s = s;
  c->type = type;
  c->inversep = inversep;
  c->count = 0;
  c->db = mcf;
  c->pdata = as;
  c->queryType = GET_SLOT_VALUES; 
  return c;
}

void *
nextValue (RDF_Cursor c) {
  if (!c) return null;
  while (c->pdata != null) {
    Assertion as = (Assertion) c->pdata;
    if (((as->db == c->db) || (!c->db)) && (as->s == c->s) &&  (c->type == as->type)) {
      c->value = (c->inversep ? as->u : as->value);
      c->pdata = (c->inversep ? as->invNext : as->next);
      return c->value;
    }
    c->pdata = (c->inversep ? as->invNext : as->next);
  }
  return null;
}

void
disposeCursor (RDF_Cursor c)
{
  if (c) freeMem(c);
}


void
unloadRDFT (RDFT f)
{
  int n = 0;
  while (n < f->assertionListCount) {
    Assertion as = *(f->assertionList + n);
    remoteStoreRemove(f, as->u, as->s, as->value, as->type);
    freeAssertion(as);
    *(f->assertionList + n) = NULL;
    n++;
  }
  f->assertionListCount = 0;
  f->assertionListSize = 0;
  freeMem(f->assertionList);
  f->assertionList = NULL;
}

