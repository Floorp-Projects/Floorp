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
#include "utils.h"
#include "plstr.h"

	/* globals */



RDFT
MakeRemoteStore (char* url)
{
  if (startsWith("rdf:remoteStore", url)) {
    if (gRemoteStore == 0) {
      gRemoteStore = NewRemoteStore(url);
      return gRemoteStore;
    } else return gRemoteStore;
  } else return NULL;
}



RDFT
MakeFileDB (char* url)
{
  if (strchr(url, ':')) {
    RDFT ntr = NewRemoteStore(url);
    ntr->possiblyAccessFile = RDFFilePossiblyAccessFile ;
    if (strcmp(gNavCntrUrl, url) == 0) 
      readRDFFile(url, RDF_GetResource(NULL, url, 1), 0, ntr); 
    return ntr;
  } else return NULL;
}



PRBool
asEqual(RDFT r, Assertion as, RDF_Resource u, RDF_Resource s, void* v, 
	       RDF_ValueType type)
{
  XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )v)));
  return ((as->db == r) && (as->u == u) && (as->s == s) && (as->type == type) && 
	  ((as->value == v) || 
	   ((type == RDF_STRING_TYPE) && ((strcmp(v, as->value) == 0) || (((char *)v)[0] =='\0')))));
}



Assertion
makeNewAssertion (RDFT r, RDF_Resource u, RDF_Resource s, void* v, 
			    RDF_ValueType type, PRBool tv)
{
  Assertion newAs = (Assertion) getMem(sizeof(struct RDF_AssertionStruct));
  XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )v)));
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
  XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )v)));
  if (as != NULL) {
    addToAssertionList(fi, as);
    return 1;
  } else return 0;
}



PRBool
remoteUnassert3 (RDFFile fi, RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
		     RDF_ValueType type)
{
  Assertion as = remoteStoreRemove(mcf, u, s, v, type);
  XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )v)));
  if (as != NULL) {
    removeFromAssertionList(fi, as);
    return 1;
  } else return 0;
}



void
remoteStoreflushChildren(RDFT mcf, RDF_Resource parent)
{
	RDF_Cursor		c;
	RDF_Resource		child;
#if 0
	RDF_Cursor		cc;
	RDF_Resource		s;
	char			*value;
#endif

	if (parent == NULL)	return;
	if ((c = remoteStoreGetSlotValues (mcf, parent, gCoreVocab->RDF_parent,
		RDF_RESOURCE_TYPE,  true, true)) != NULL)
	{
		while((child = remoteStoreNextValue (mcf, c)) != NULL)
		{
			remoteStoreflushChildren(mcf, child);

			/* XXX should we remove all arcs coming off of this node? */
#if 0
			if ((cc = remoteStoreArcLabelsOut(mcf, child)) != NULL)
			{
				if ((s = remoteStoreNextValue (mcf, cc)) != NULL)
				{
					if (s == gCoreVocab->RDF_name)
					{
						value = remoteStoreGetSlotValue (mcf, child, s,
							RDF_STRING_TYPE, PR_FALSE, PR_TRUE);
						if (value != NULL)
						{
							remoteStoreRemove (mcf, child, s,
								value, RDF_STRING_TYPE);
						}
					}
				}
				remoteStoreDisposeCursor(mcf, cc);
			}
#endif
			remoteStoreRemove (mcf, child, gCoreVocab->RDF_parent,
				parent, RDF_RESOURCE_TYPE);
		}
		remoteStoreDisposeCursor (mcf, c);
	}
}



Assertion
remoteStoreAdd (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
			  RDF_ValueType type, PRBool tv)
{
  Assertion nextAs, prevAs, newAs; 
  XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )v)));
  nextAs = prevAs = u->rarg1;

  if (s == gNavCenter->RDF_Command)
  {
  	if ((type == RDF_RESOURCE_TYPE) && (tv) && (v == gNavCenter->RDF_Command_Refresh))
  	{
  		/* flush any children of 'u' */
  		remoteStoreflushChildren(mcf, u);
	}
	/* don't store RDF Commands in the remote store */
  	return(NULL);
  }
  
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
  XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )v)));
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
      return false;	/* true; */
    }
  }
  return false;
}



static void
possiblyAccessFile (RDFT mcf, RDF_Resource u, RDF_Resource s, PRBool inversep)
{
    if (mcf->possiblyAccessFile) 
    (*(mcf->possiblyAccessFile))(mcf, u, s, inversep); 
}



void
RDFFilePossiblyAccessFile (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep)
{
  if ((resourceType(u) == RDF_RT) && 
      (startsWith(rdf->url, resourceID(u))) &&
	 
      (s == gCoreVocab->RDF_parent) && (containerp(u))) {
    readRDFFile( resourceID(u), u, false, rdf);
    /*    if(newFile) newFile->lastReadTime = PR_Now(); */
  }
}



PRBool
remoteStoreHasAssertion (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv)
{
  Assertion nextAs;
  XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )v)));
  
  if ((s == gNavCenter->RDF_Command) && (type == RDF_RESOURCE_TYPE) && (tv) && (v == gNavCenter->RDF_Command_Refresh))
  {
  	return true;
  }
  
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

  if ((s == gWebData->RDF_URL) && (tv) && (!inversep) && (type == RDF_STRING_TYPE))
  {	
	
    XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )resourceID(u))));
    return copyString(resourceID(u));         
  }	

  nextAs = (inversep ? u->rarg2 : u->rarg1);
  while (nextAs != null) {
    if ((nextAs->db == mcf) && (nextAs->s == s) && (nextAs->tv == tv) && (nextAs->type == type)) {
      void* ans = (inversep ? nextAs->u : nextAs->value);
      if (type == RDF_STRING_TYPE) {
        XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )ans)));
	return copyString((char*)ans); 
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



RDF_Cursor
remoteStoreArcLabelsIn (RDFT mcf, RDF_Resource u)
{
  if (u->rarg2) {
    RDF_Cursor c = (RDF_Cursor)getMem(sizeof(struct RDF_CursorStruct));
    c->u = u;
    c->queryType = RDF_ARC_LABELS_IN_QUERY;
    c->pdata = u->rarg2;
    return c;
  } else return NULL;
}



RDF_Cursor
remoteStoreArcLabelsOut (RDFT mcf, RDF_Resource u)
{
  if (u->rarg1) {
    RDF_Cursor c = (RDF_Cursor)getMem(sizeof(struct RDF_CursorStruct));
    c->u = u;
    c->queryType = RDF_ARC_LABELS_OUT_QUERY;
    c->pdata = u->rarg1;
    return c;
  } else return NULL;
}



void *
arcLabelsOutNextValue (RDFT mcf, RDF_Cursor c)
{
  while (c->pdata != null) {
    Assertion as = (Assertion) c->pdata;
    if ((as->db == mcf) && (as->u == c->u)) {
      c->value = as->s;
      c->pdata = as->next;
      return c->value;
    }
    c->pdata = as->next;
  }  
  return null;
}



void *
arcLabelsInNextValue (RDFT mcf, RDF_Cursor c)
{
  while (c->pdata != null) {
    Assertion as = (Assertion) c->pdata;
    if ((as->db == mcf) && (as->value == c->u)) {
      c->value = as->s;
      c->pdata = as->invNext;
      return c->value;
    }
    c->pdata = as->invNext;
  }  
  return null;
}



void *
remoteStoreNextValue (RDFT mcf, RDF_Cursor c)
{
  if (c->queryType == RDF_ARC_LABELS_OUT_QUERY) {
    return arcLabelsOutNextValue(mcf, c);
  } else if (c->queryType == RDF_ARC_LABELS_IN_QUERY) {
    return arcLabelsInNextValue(mcf, c);
  } else {
    while (c->pdata != null) {
      Assertion as = (Assertion) c->pdata;
      if ((as->db == mcf) && (as->s == c->s) && (as->tv == c->tv) && (c->type == as->type)) {
        c->value = (c->inversep ? as->u : as->value);
        c->pdata = (c->inversep ? as->invNext : as->next);
        return c->value;
      }
    c->pdata = (c->inversep ? as->invNext : as->next);
  }
  return null;
  }
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
gcRDFFileInt (RDFFile f)
{
  int32 n = 0;
  while (n < f->assertionCount) {
    Assertion as = *(f->assertionList + n);
    remoteStoreRemove(f->db, as->u, as->s, as->value, as->type);
    freeAssertion(as);
    *(f->assertionList + n) = NULL;
    n++;
  }
  n = 0;
  while (n < f->resourceCount) {
    *(f->resourceList + n) = NULL;
    n++;
  }
}



RDF_Error
DeleteRemStore (RDFT db)
{
  RDFFile f = (RDFFile) db->pdata;
  RDFFile next;
  while (f) {
    next = f->next;
    gcRDFFileInt(f);
    freeMem(f->assertionList);
    freeMem(f->resourceList);	
    f = next;
  }
  freeMem(db);
  return 0;
}



RDF_Error 
remStoreUpdate (RDFT db, RDF_Resource u)
{
  RDFFile f = db->pdata;
  if (f != NULL) {
    int32 n = 0;
    PRBool proceedp = 0;
    while (n < f->resourceCount) {
      if (*(f->resourceList + n++) == u) {
        proceedp = 1;
        break;
      }
    }

    if (proceedp) {
      RDF_Resource top = f->top;
      char* url = db->url;
      PRBool localp = f->localp;    
      gcRDFFileInt(f);
      freeMem(f->assertionList);
      freeMem(f->resourceList);	
      f->assertionList = NULL;
      f->resourceList = NULL;
      initRDFFile(f);
      f->refreshingp = 1;
      beginReadingRDFFile(f);
      return 0;
    } else return -1;
  } else return -1;
}
    


void
gcRDFFile (RDFFile f)
{
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
  gcRDFFileInt(f);
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
  RDFFile f = makeNewRDFFile(url, top, localp, db);
  if (!f) return NULL;
  beginReadingRDFFile(f);
  return f;
}



RDFFile 
makeNewRDFFile (char* url, RDF_Resource top, PRBool localp, RDFT db)
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
    newFile->unassert = remoteUnassert3;
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
  /*if ((resourceType(u) == RDF_RT) && (startsWith("rdf:ht", rdf->url)) &&      
	     (s == gCoreVocab->RDF_parent) && 
        (containerp(u))) {
    RDFFile newFile = readRDFFile( resourceID(u), u, false, rdf);
    if(newFile) newFile->lastReadTime = PR_Now();
  } */
}



RDFT
NewRemoteStore (char* url)
{
	RDFT		ntr;

	if ((ntr = (RDFT)getMem(sizeof(struct RDF_TranslatorStruct))) != NULL)
	{
		ntr->getSlotValue = remoteStoreGetSlotValue;
		ntr->getSlotValues = remoteStoreGetSlotValues;
		ntr->hasAssertion = remoteStoreHasAssertion;
		ntr->nextValue = remoteStoreNextValue;
		ntr->disposeCursor = remoteStoreDisposeCursor;
		ntr->url = copyString(url);
		ntr->destroy =  DeleteRemStore;
		ntr->arcLabelsIn = remoteStoreArcLabelsIn;
		ntr->arcLabelsOut = remoteStoreArcLabelsOut;
		ntr->update = remStoreUpdate;
	}
	return(ntr);
}



RDFT
MakeSCookDB (char* url)
{
  if (startsWith("rdf:scook:", url) || (startsWith("rdf:ht", url))) {
    RDFT ntr = NewRemoteStore(url);
    ntr->possiblyAccessFile = SCookPossiblyAccessFile;
    return ntr;
  } else return NULL;
}



void
addToRDFTOut (RDFTOut out)
{
  int32 len = strlen(out->temp);
  if (len + out->bufferPos < out->bufferSize) {
    PL_strcat(out->buffer, out->temp);
    out->bufferPos = out->bufferPos + len;
    memset(out->temp, '\0', 1000);
  } else {
    PR_Realloc(out->buffer, out->bufferSize + 20000);
    out->bufferSize = out->bufferSize + 20000;
    addToRDFTOut (out);
  }
}



PRIntn
RDFSerializerEnumerator (PLHashEntry *he, PRIntn i, void *arg)
{
  RDF_Resource u = (RDF_Resource)he->value;
  RDFTOut out    = (RDFTOut) arg;
  Assertion as = u->rarg1;
  PRBool somethingOutp = 0;
  while (as) {
    if (as->db == out->store) {
      if (!somethingOutp) {
        somethingOutp = 1;
        sprintf(out->temp, "<RDF:Description href=\"%s\">\n", resourceID(as->u));
        addToRDFTOut(out);
      }
      if (as->type == RDF_RESOURCE_TYPE) {
        sprintf(out->temp, "       <%s href=\"%s\"/>\n", resourceID(as->s), 
                resourceID((RDF_Resource)as->value));    
      } else if (as->type == RDF_INT_TYPE) {
        sprintf(out->temp, "       <%s dt=\"int\">%i</%s>\n", resourceID(as->s), 
                (int)as->value, resourceID(as->s));     
      } else {  
        sprintf(out->temp, "       <%s>%s</%s>\n", resourceID(as->s), 
                (char*)as->value, resourceID(as->s));     
      }
      addToRDFTOut(out);
    }
    as = as->next;
  }
  if (somethingOutp) {
    sprintf(out->temp, "</RDF:Description>\n\n");
    addToRDFTOut(out);
  }
  return  HT_ENUMERATE_NEXT;    
}


char* 
RDF_SerializeRDFStore (RDFT store) {
  RDFTOut out = getMem(sizeof(struct RDFTOutStruct));
  char* ans = out->buffer = getMem(20000);
  out->bufferSize = 20000;
  out->temp = getMem(1000);
  out->store = store;
  sprintf(out->temp, "<RDF:RDF>\n\n");
  addToRDFTOut(out);
  PL_HashTableEnumerateEntries(resourceHash, RDFSerializerEnumerator, out);
  sprintf(out->temp, "</RDF:RDF>\n\n");
  addToRDFTOut(out);
  freeMem(out->temp);
  freeMem(out);
  return ans;
}

