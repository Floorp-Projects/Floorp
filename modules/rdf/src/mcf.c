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
   This file (which should probably be called rdf.c) implements the
   core rdf query mechanism.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "mcf.h"
#include "columns.h"
#include "find2rdf.h"
#include "fs2rdf.h"
#include "hist2rdf.h"
#include "nlcstore.h"
#include "remstore.h"
#include "es2mcf.h"
#include "pm2rdf.h"
#include "scook.h"
#include "atalk.h"
#include "ht.h"
#include "utils.h"

#ifdef DEBUG_guha
#define SMART_MAIL 1
#endif


	/* globals */
RDFL		gAllDBs = 0;



RDFT
getTranslator (char* url)
{
  RDFT ans = PL_HashTableLookup(dataSourceHash, url);
  if (ans) return ans;
#ifdef MOZILLA_CLIENT
  if (startsWith(url, "rdf:localStore")) {
    ans = MakeLocalStore(url);
  } else
#endif
  if (startsWith(url, "rdf:remoteStore")) {
    ans =  MakeRemoteStore(url);
  } 
#ifdef MOZILLA_CLIENT  
  else if (startsWith(url, "rdf:history")) {
    ans = MakeHistoryStore(url);
  } else if (startsWith(url, "rdf:esftp")) {
    ans = MakeESFTPStore(url);
    /*  } else if (startsWith(url, "rdf:mail")) {
    ans = MakeMailStore(url); */
  } else if (startsWith(url, "rdf:lfs")) {
    ans = MakeLFSStore(url);

#ifdef	XP_MAC
  } else if (startsWith(url, "rdf:appletalk")) {
    ans = MakeAtalkStore(url);
#endif

  } else if (strstr(url, ".rdf") || strstr(url, ".RDF") || 
		     strstr(url, ".mcf") || strstr(url, ".MCF")) {
    ans = MakeFileDB(url);
  } else if (startsWith("rdf:columns", url)) {
    ans = MakeColumnStore (url);
  } else if (startsWith("rdf:ht", url) || startsWith("rdf:scook", url)) {
    ans = MakeSCookDB(url);
  } else if (startsWith("rdf:CookieStore", url)) {
    ans = MakeCookieStore(url);
  } else if (startsWith("rdf:find", url)) {
    ans = MakeFindStore(url);
  } 
#endif

    else if (startsWith("http://", url)) {
	  ans = MakeFileDB(url); 
    } else if (startsWith("mailbox://", url)){
#ifdef SMART_MAIL 
      ans = MakePopDB(url);
#else
      ans = NULL;
#endif
    } else if (startsWith("mailaccount://", url)) {
#ifdef SMART_MAIL
      ans = MakeMailAccountDB(url);
#else
      ans = NULL;
#endif
    } else {
	  ans = NULL;
  }
#ifdef MOZILLA_CLIENT
#ifdef DEBUG
  {
    char* traceLine = getMem(500);
    sprintf(traceLine, "\nCreated %s \n", url);
    FE_Trace(traceLine);
    freeMem(traceLine);
  }
#endif
#endif

  if (ans) PL_HashTableAdd(dataSourceHash, ans->url, ans);
  return ans;
}



PR_PUBLIC_API(RDF)
RDF_GetDB (const char** dataSources)
{
  int32 n = 0;
  int32 m = 0;
  char* next ;
  RDF r = (RDF) getMem(sizeof(struct RDF_DBStruct)) ;
  RDFL nrl = (RDFL)getMem(sizeof(struct RDF_ListStruct));
  while (*(dataSources + n++)) {} 
  r->translators = (RDFT*)getMem((n) * sizeof(RDFT));
  n = 0;
  while ((next = (char*) *(dataSources + n)) != NULL) {
    RDFL rl = (RDFL)getMem(sizeof(struct RDF_ListStruct));
    r->translators[m] = getTranslator(next);
    if (r->translators[m] != NULL) { 
		rl->rdf = r;
		rl->next = r->translators[m]->rdf;
		r->translators[m]->rdf = rl;
		m++;
	} else {
      freeMem(rl);
      /*      r->numTranslators = m;
      r->translatorArraySize = n;
      RDF_ReleaseDB(r);
      return NULL; */
	}
    n++;
  }
  r->numTranslators = m;
  r->translatorArraySize = n;
  nrl->rdf = r;
  nrl->next = gAllDBs;
  gAllDBs = nrl;
  return r;
}



PR_PUBLIC_API(RDFT)
RDF_AddDataSource(RDF rdf, char* dataSource)
{
  RDFT newDB;
  int32 n = 0;
  RDFT next;
  while (((next = rdf->translators[n++]) != NULL) && (n < rdf->numTranslators)) {
    if (strcmp(next->url, dataSource) == 0) return next;
  }
#ifdef MOZILLA_CLIENT
#ifdef DEBUG
  {
    char* traceLine = getMem(500);
    sprintf(traceLine, "\nAdding %s \n", dataSource);
    FE_Trace(traceLine);
    freeMem(traceLine);
  }
#endif
#endif
  if (rdf->numTranslators >= rdf->translatorArraySize) {
    RDFT* tmp = (RDFT*)getMem((rdf->numTranslators+5)*(sizeof(RDFT)));
    memcpy(tmp, rdf->translators, (rdf->numTranslators * sizeof(RDFT)));
    rdf->translatorArraySize = rdf->numTranslators + 5;
    freeMem(rdf->translators);
    rdf->translators = tmp;
  }
  newDB = getTranslator(dataSource);
  if (!newDB) {
    return NULL;  
  } else {
    RDFL rl = (RDFL)getMem(sizeof(struct RDF_ListStruct));
    RDFT tr = rdf->translators[rdf->numTranslators];
    rl->rdf = rdf;
    rl->next = newDB->rdf;
    newDB->rdf = rl;
    rdf->translators[rdf->numTranslators] = newDB;
    rdf->numTranslators++;
    return newDB;
  }	
}



PR_PUBLIC_API(RDF_Error)
RDF_ReleaseDataSource(RDF rdf, RDFT dataSource)
{
  RDFT* temp = (RDFT*)getMem((rdf->numTranslators-1)*(sizeof(RDFT)));
  int32 m = 0;
  int32 n= 0;
  RDFT next;
  while ((next = rdf->translators[n++]) != NULL) {
    if (next != dataSource) {
      *(temp + m++) = (RDFT) next;
    }
  }
  memset(rdf->translators, '\0', sizeof(RDFT) * rdf->numTranslators);
  memcpy(rdf->translators, temp, sizeof(RDFT) * (rdf->numTranslators -1));
  rdf->numTranslators--;
  dataSource->rdf = deleteFromRDFList(dataSource->rdf, rdf);
  if ((dataSource->rdf == NULL) && (dataSource->destroy != NULL)) {
      PL_HashTableRemove(dataSourceHash,  dataSource->url); 
      (*dataSource->destroy)(dataSource);
  }
  return 0;
}



RDFL
deleteFromRDFList (RDFL xrl, RDF db)
{
  RDFL rl = xrl;
  RDFL prev = rl;
  while (rl) {
      if (rl->rdf == db) {
        if (prev == rl) {
          RDFL ans = rl->next;
          freeMem(rl);
          return ans;
        } else {
          prev->next = rl->next;
          freeMem(rl);
          return xrl;
        }
      }
      prev = rl;
      rl   = rl->next;
  }
  return xrl;
}



void
disposeAllDBs ()
{
  while (gAllDBs) {
    RDF_ReleaseDB(gAllDBs->rdf);
  }
}



PR_PUBLIC_API(RDF_Error) 
RDF_ReleaseDB(RDF rdf)
{
  if (rdf != NULL) {
    uint32 n = 0;
    uint32 size = rdf->numTranslators;
    while (n < size) { 
	  RDFT rdft = (*((RDFT*)rdf->translators + n));
	  RDFL rlx ;
	  if (rdft) {
		  rlx =  rdft->rdf; 
		  rdft->rdf =  deleteFromRDFList(rlx, rdf); 
		  if (rdft->rdf == NULL) callExitRoutine(n, rdf);
	  }
      n++;
    }
    gAllDBs = deleteFromRDFList(gAllDBs, rdf);
  }
  
  freeMem(rdf->translators);
  freeMem(rdf);
  return noRDFErr;
}



RDF_Resource
addDep (RDF db, RDF_Resource u)
{
 
  if (db) {
    RDFL rl = u->rdf;
    while (rl) {
      if (rl->rdf == db) return u;
      rl = rl->next;
    }
    rl = (RDFL) getMem(sizeof(struct RDF_ListStruct));
    rl->rdf = db;
    rl->next = u->rdf;
    u->rdf = rl;
    return u;
  }
  return u;
}



PRBool
rdfassert(RDF rdf, RDF_Resource u, RDF_Resource  s, void* value, 
	      RDF_ValueType type,  PRBool tv)
{
   int32 size =  rdf->numTranslators;
   int32 n = size -1;

   XP_ASSERT( (type != RDF_STRING_TYPE ) || IsUTF8String((const char*)value));
   
   /* XXX - what happens here if there is no local store? */     
   if ((size > 1) && (callHasAssertions(0, rdf, u, s, value, type, (!tv)))) {
     setLockedp(u, 1);
     callUnassert(0, rdf, u, s, value, type); 
     setLockedp(u, 0);
     if (RDF_HasAssertion(rdf, u, s, value, type, tv)) return 1;
   }
   while (n > -1) {
     if (callAssert(n, rdf, u, s, value, type, tv)) return 1; 
     n--; 
   }  
   return 0;
}


  

PR_PUBLIC_API(PRBool)
RDF_Assert (RDF rdf, RDF_Resource u, RDF_Resource  s, void* value, RDF_ValueType type)
{
	return rdfassert(rdf, u, s, value, type, 1);
}



PR_PUBLIC_API(PRBool)
RDF_AssertFalse (RDF rdf, RDF_Resource u, RDF_Resource  s, void* value, RDF_ValueType type)
{
	return rdfassert(rdf, u, s, value, type, 0);
}



PR_PUBLIC_API(PRBool)
RDF_CanAssert(RDF r, RDF_Resource u, RDF_Resource s, 
		    void* v, RDF_ValueType type)
{
	XP_ASSERT( (type != RDF_STRING_TYPE ) || IsUTF8String((const char*)v));
	return true;
}



PR_PUBLIC_API(PRBool)
RDF_CanAssertFalse(RDF r, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type)
{
	XP_ASSERT( (type != RDF_STRING_TYPE ) || IsUTF8String((const char*)v));
	return true;
}



PR_PUBLIC_API(PRBool)
RDF_CanUnassert(RDF r, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type)
{
	XP_ASSERT( (type != RDF_STRING_TYPE ) || IsUTF8String((const char*)v));
	return true;
}



PR_PUBLIC_API(PRBool)
RDF_Unassert (RDF rdf, RDF_Resource u, RDF_Resource s, void* value, RDF_ValueType type)
{
  PRBool allok = 1;
  int32 size =  rdf->numTranslators;
  int32 n = 0;
  
   XP_ASSERT( (type != RDF_STRING_TYPE ) || IsUTF8String((const char*)value));
  setLockedp(u, 1);
  while (allok && (n < size)) {
    if (callHasAssertions(n, rdf, u, s, value, type, 1)) {
      allok = callUnassert(n, rdf, u, s, value, type);
    }
    n++;
  }
  if (!allok) allok = callAssert(0, rdf, u, s, value, type, 0); 
  setLockedp(u, 0);
  return allok;
}



PRBool
containerIDp(char* id)
{
  return (endsWith(".rdf", id) || (endsWith(".mco", id)) || (endsWith(".mcf", id)) || 
           strstr(id, ".rdf#"));
}



char *
makeNewID ()
{
	PRTime		tm = PR_Now();
	char		*id;

#ifndef HAVE_LONG_LONG
	double		doubletm;
#endif /* !HAVE_LONG_LONG */


	id = (char *)getMem(ID_BUF_SIZE);

#ifdef HAVE_LONG_LONG
	PR_snprintf(id, ID_BUF_SIZE, "%1.0f", (double)tm);
#else
	LL_L2D(doubletm, tm);
	PR_snprintf(id, ID_BUF_SIZE, "%1.0f", doubletm);
#endif /* HAVE_LONG_LONG */

	/* prevent collisions by checking hash. */
	while (PL_HashTableLookup(resourceHash, id) != NULL)
	{
#ifdef HAVE_LONG_LONG
		tm = tm + (PR_MSEC_PER_SEC * 60); /* fix me - not sure what the increment should be */
		PR_snprintf(id, ID_BUF_SIZE, "%1.0f", (double)tm);
#else
		int64 incr;
		LL_I2L(incr, (PR_MSEC_PER_SEC * 60));
		LL_ADD(tm, tm, incr);
		LL_L2D(doubletm, tm);
		PR_snprintf(id, ID_BUF_SIZE, "%1.0f", doubletm);
#endif /* HAVE_LONG_LONG */
	}
	return (id);
}



PRBool
iscontainerp (RDF_Resource u)
{
  char* id = resourceID(u);
  if (containerIDp(id)) {
    return 1;
  } 
#ifdef MOZILLA_CLIENT  
  else if (startsWith("file:", id)) {
    return (endsWith("/", id) || fileDirectoryp(u));
  } else if (startsWith("ftp:", id) && (endsWith("/", id))) {
    return 1;
  } else if (startsWith("cache:container", id)) {
    return 1;
  } else if (startsWith("find:", id)) {
    return 1;
  }
#endif   
  else  return 0; 
} 



RDF_BT
resourceTypeFromID (char* id)
{
  if (endsWith(".rdf", id) || (strstr(id, ".rdf#"))) {
    return RDF_RT;
  } else if (endsWith(".mcf", id) || (strstr(id, ".mcf#"))) {
    return RDF_RT;
  } else if (endsWith(".mco", id)) {
    return RDF_RT;
  } else if (startsWith("file:", id))  {
    return LFS_RT;
  } else if (startsWith("nes:", id)) {
    return ES_RT;
  } else if (startsWith("ftp:", id)) {
    return FTP_RT;
  } else if (startsWith("cache:", id)) {
    return CACHE_RT;
  } else return 0;
}



RDF_Resource
specialUrlResource (char* id)
{
	if (strcmp(id, "NC:PersonalToolbar") == 0)
		return RDFUtil_GetPTFolder();
	return NULL;
}



RDF_Resource
NewRDFResource (char* id)
{
	RDF_Resource	existing;
	
	if ((existing = (RDF_Resource)getMem(sizeof(struct RDF_ResourceStruct))) != NULL)
	{
		existing->url = copyString(id);
		PL_HashTableAdd(resourceHash, existing->url, existing);
	}
	return(existing);
}



RDF_Resource
QuickGetResource (char* id)
{
	RDF_Resource	existing;

	if ((existing = (RDF_Resource)PL_HashTableLookup(resourceHash, id)) == NULL)
	{
		existing = NewRDFResource(id);
	}
	return(existing);
}



PR_PUBLIC_API(RDF_Resource)
RDF_GetResource (RDF db, char* id, PRBool createp)
{
  char* nid = NULL;
  RDF_Resource existing = NULL;
  if (id == NULL) {
	if (!createp) {
		return NULL;
	} else {
		id = nid = makeNewID();
	}
  }
  existing = specialUrlResource(id);
  if (existing) return existing;
  existing = (RDF_Resource)PL_HashTableLookup(resourceHash, id);
  if (existing) return addDep(db, existing);
  if (!createp) return NULL;
  existing =  NewRDFResource(id);  
  setResourceType(existing, resourceTypeFromID(id));
  setContainerp(existing, iscontainerp(existing));
  if (nid) freeMem(nid);
  return addDep(db, existing); 
}



  
PR_PUBLIC_API (RDF_Error)
RDF_DeleteAllArcs (RDF rdf, RDF_Resource u)
{
  Assertion as, next;
  /*  if (u->locked) return -1; */
  as = u->rarg1;
  while (as != NULL) {
    next = as->next;
    remoteStoreRemove(gRemoteStore, as->u, as->s, as->value, as->type);
    as = next;
  }
  as = u->rarg2;
  while (as != NULL) {
    next = as->invNext;
    remoteStoreRemove(gRemoteStore, as->u, as->s, as->value, as->type);
    as = next;
  }
#if defined(DBMTEST) && defined(MOZILLA_CLIENT)
 nlclStoreKill(gLocalStore, u);
#endif
  return 0;
}

PR_PUBLIC_API(RDF_Error) 
RDF_Update(RDF rdf, RDF_Resource u) {
  int32 size = rdf->numTranslators;
  int32 n = 0;
  while (n < size) {
    callUpdateRoutine(n, rdf, u);
    n++;
  }
  return 0; 
}


PR_PUBLIC_API(PRBool)
RDF_HasAssertion (RDF rdf, RDF_Resource u, RDF_Resource s, 
		     void* v, RDF_ValueType type, PRBool tv)
{
  int32 size = rdf->numTranslators;
  int32 n = 0;
  XP_ASSERT( (type != RDF_STRING_TYPE ) || IsUTF8String((const char*)v));
  if (callHasAssertions(0, rdf, u, s, v, type, !tv)) return 0;
  while (n < size) {
    if (callHasAssertions(n, rdf, u, s, v, type, tv)) return 1;
    n++;
  }
  return 0; 
}



PR_PUBLIC_API(void *)
RDF_GetSlotValue (RDF rdf, RDF_Resource u, RDF_Resource s, 
		    RDF_ValueType type, PRBool inversep, PRBool tv)
{
  int32 size =  rdf->numTranslators; 
  int32 n = 0;
  if ((s == gWebData->RDF_URL) && (tv) && (!inversep) && (type == RDF_STRING_TYPE))
    return  copyString(resourceID(u));         
  while (n < size) {
	RDFT translator = rdf->translators[n];
    void*  ans = callGetSlotValue(n, rdf, u, s, type, inversep, tv);
    if ((ans != NULL) && ((n == 0)||(!callHasAssertions(0, rdf, u, s, ans, type, !tv))))
    {

    	XP_ASSERT( (type != RDF_STRING_TYPE ) || IsUTF8String((const char*)ans));

    	if (type == RDF_RESOURCE_TYPE) {
      		return addDep(rdf, ans) ;
      	} else {
		return ans;
	}
    }
    n++;
  }
  if (s == gCoreVocab->RDF_name && tv && !inversep && type == RDF_STRING_TYPE) 
    return makeResourceName(u);
  return NULL; 
}



RDFT
RDFTNamed (RDF rdf, char* name)
{
  int32 size =  rdf->numTranslators; 
  int32 n = 0;
  while (n < size) {
    char* nn = (*((RDFT*)rdf->translators + n))->url;
    if (strcmp(nn, name) == 0) return  (*((RDFT*)rdf->translators + n));
    n = n + 1;
  }
  return NULL;
}



RDF_Cursor
getSlotValues (RDF rdf, RDF_Resource u, RDF_Resource s, 
			  RDF_ValueType type, PRBool inversep, PRBool tv)
{
  RDF_Cursor c = (RDF_Cursor) getMem(sizeof(struct RDF_CursorStruct));
  if (c == NULL)	return(NULL);
  c->u = u;
  c->s = s;
  c->queryType =  RDF_GET_SLOT_VALUES_QUERY;
  c->tv = tv;
  c->inversep = inversep;
  c->type = type;
  c->count = -1;
  c->rdf = rdf;
  c->current = NULL;
  while ((c->count < rdf->numTranslators-1) && (!c->current)) {
    c->count++;
    c->current = callGetSlotValues(c->count, rdf, u, s, type, inversep, tv);
  }
  if (!c->current) {
    freeMem(c);
    return NULL;
  } else {
    return c;
  }
}



PR_PUBLIC_API(RDF_Cursor)
RDF_GetTargets (RDF rdfDB, RDF_Resource source, RDF_Resource arcLabel, 
					  RDF_ValueType targetType,  PRBool tv)
{
	return getSlotValues(rdfDB, source, arcLabel, targetType, 0, tv);
}



PR_PUBLIC_API(RDF_Cursor)
RDF_GetSources (RDF rdfDB, RDF_Resource target, RDF_Resource arcLabel, 
					  RDF_ValueType sourceType,  PRBool tv)
{
  if (sourceType != RDF_RESOURCE_TYPE) return NULL;
  return getSlotValues(rdfDB, target, arcLabel, sourceType, 1, tv);
}



PR_PUBLIC_API(RDF_ValueType)
RDF_CursorValueType(RDF_Cursor c)
{
  if (c == NULL) {
    return 0;
  } else {
    return c->type;
  }
}



PR_PUBLIC_API(char*)
RDF_ValueDataSource(RDF_Cursor c)
{
  if (c == NULL) {
    return NULL;
  } else {
    RDF rdf = c->rdf;
    RDFT rdft;
    if (rdf == NULL) return NULL;
    rdft = rdf->translators[c->count];
    if (rdft) {
      return rdft->url;
    } else {
      return NULL;
    }
  }
}



PR_PUBLIC_API(RDF_Cursor)
RDF_ArcLabelsOut (RDF rdfDB, RDF_Resource u)
{
  RDF_Cursor c = (RDF_Cursor) getMem(sizeof(struct RDF_CursorStruct));
  if (c == NULL)   return(NULL);
  c->u = u;
  c->queryType =  RDF_ARC_LABELS_OUT_QUERY;
  c->type = RDF_RESOURCE_TYPE;
  c->count = -1;
  c->rdf = rdfDB;
  c->current = NULL;
  while ((c->count < rdfDB->numTranslators-1) && (!c->current)) {
    c->count++;
    c->current = callArcLabelsOut(c->count, rdfDB, u);
  } 
  if (!c->current) {
    freeMem(c);
    return NULL;
  } else {
    return c;
  }
}



PR_PUBLIC_API(RDF_Cursor)
RDF_ArcLabelsIn (RDF rdfDB, RDF_Resource u)
{
  RDF_Cursor c = (RDF_Cursor) getMem(sizeof(struct RDF_CursorStruct));
  if (c == NULL)   return(NULL);
  c->u = u;
  c->queryType =  RDF_ARC_LABELS_IN_QUERY;
  c->type = RDF_RESOURCE_TYPE;
  c->count = -1;
  c->rdf = rdfDB;
  c->current = NULL;
  while ((c->count < rdfDB->numTranslators-1) && (!c->current)) {
    c->count++;
    c->current = callArcLabelsIn(c->count, rdfDB, u);
  }
  if (!c->current) {
    freeMem(c);
    return NULL;
  } else {
    return c;
  }
}



PR_PUBLIC_API(void *)
RDF_NextValue (RDF_Cursor c)
{
  void* ans;
  RDF rdf;  
  if (c == NULL) return NULL;
  if (c->queryType == RDF_FIND_QUERY) return nextFindValue(c);
  rdf =  c->rdf;
  ans = (*rdf->translators[c->count]->nextValue)(rdf->translators[c->count], c->current);
  while (ans != NULL) {
    if ((c->count == 0) || (c->queryType != RDF_GET_SLOT_VALUES_QUERY)) {
      if (c->type == RDF_RESOURCE_TYPE)  return addDep(c->rdf, ans);
      else				 return ans;
      }
    if (((!c->inversep) && 
         (!(callHasAssertions(0, rdf, c->u, c->s, ans, c->type, !c->tv)))) ||
        ((c->inversep) && 
         (!(callHasAssertions(0, rdf, (RDF_Resource)ans, c->s, c->u, 
                              c->type, !c->tv))))) {
      if (c->type == RDF_RESOURCE_TYPE) {
        return addDep(c->rdf, (RDF_Resource)ans);
      } else return ans;
    }
    ans = (*rdf->translators[c->count]->nextValue)(rdf->translators[c->count], c->current);
  }
  (*rdf->translators[c->count]->disposeCursor)(rdf->translators[c->count], c->current);
  c->current = NULL;
  while ((c->count < rdf->numTranslators - 1) && (!c->current)) {
    c->count++;
    switch (c->queryType) {
    case RDF_GET_SLOT_VALUES_QUERY : 
      c->current = callGetSlotValues(c->count,rdf, c->u, c->s, c->type, 
                                     c->inversep, c->tv);
      break;
    case RDF_ARC_LABELS_OUT_QUERY :
      c->current = callArcLabelsOut(c->count, rdf, c->u);
      break;
    case RDF_ARC_LABELS_IN_QUERY :
      c->current = callArcLabelsIn(c->count, rdf, c->u);
      break;
    }
    
  }
  if (c->current == NULL) return NULL;
  return RDF_NextValue(c);
}



void 
disposeResourceInt (RDF rdf, RDF_Resource u)
{
  int32 size = rdf->numTranslators;
  int32 n = 0;
  PRBool ok = 1;
  while (n < size && ok) {
    ok = (callDisposeResource(n, rdf, u));
    n++;
  } 
  if (ok && (!lockedp(u))) possiblyGCResource(u);
}
    
  

PR_PUBLIC_API(RDF_Error) 
RDF_ReleaseResource (RDF db, RDF_Resource u)
{
  if (db && u) {    
	
    u->rdf = deleteFromRDFList(u->rdf, db);
    if (u->rdf == NULL)  disposeResourceInt(db, u);
  }
  return 0;
}



PR_PUBLIC_API(RDF_Error)
RDF_DisposeCursor (RDF_Cursor c)
{
  if ((c != NULL) && (c->queryType == RDF_FIND_QUERY) && (c->pdata)) freeMem(c->pdata);
  if (c != NULL) freeMem(c);
  return 0;
}



PRIntn
findEnumerator (PLHashEntry *he, PRIntn i, void *arg)
{
  RDF_Cursor c = (RDF_Cursor) arg;
  RDF_Resource u = (RDF_Resource)he->value;
  if (itemMatchesFind(c->rdf, u, c->s, c->value, c->match, c->type)) {
    /* if (c->size <= c->count) */ {
      RDF_Resource* newBlock = getMem(c->size + sizeof(RDF_Resource *));
      if (newBlock == NULL) return  HT_ENUMERATE_STOP;
      if (c->size > 0) {
	memcpy(newBlock, c->pdata, c->size);
	freeMem(c->pdata);
      }
      c->pdata = newBlock;
      c->size += sizeof(RDF_Resource *);
    }
    *((RDF_Resource*)(((char *)c->pdata) + (c->count*sizeof(RDF_Resource *)))) = u;
    c->count++;
  }
  return  HT_ENUMERATE_NEXT;
}



PR_PUBLIC_API(RDF_Cursor)
RDF_Find (RDF_Resource s, RDF_Resource match, void* v, RDF_ValueType type)
{
  RDF_Cursor c = (RDF_Cursor) getMem(sizeof(struct RDF_CursorStruct));
  XP_ASSERT( (type != RDF_STRING_TYPE ) || IsUTF8String((const char*)v));
  c->s = s;
  c->match = match;
  c->value = v;
  c->type = type;
  c->queryType =  RDF_FIND_QUERY;
  c->rdf = gAllDBs->rdf;
  PL_HashTableEnumerateEntries(resourceHash, findEnumerator, c);
  c->count = 0;
  return c;
}



PRBool
matchStrings(RDF_Resource match, char *data, char *pattern)
{
	PRBool		ok = 0;

	if (match == NULL)	match = gCoreVocab->RDF_substring;

	if (match == gCoreVocab->RDF_substring)
	{
		ok = substring(pattern, data);
	}
	else if (match == gCoreVocab->RDF_stringEquals)
	{
		ok = (PRBool)(!compareStrings(data, pattern));
	}
	else if (match == gCoreVocab->RDF_stringNotEquals)
	{
		ok = (PRBool)compareStrings(data, pattern);
	}
	else if (match == gCoreVocab->RDF_stringStartsWith)
	{
		ok = startsWith(pattern, data);
	}
	else if (match == gCoreVocab->RDF_stringEndsWith)
	{
		ok = endsWith(pattern, data);
	}
	return(ok);
}



PRBool
itemMatchesFind (RDF r, RDF_Resource u, RDF_Resource s, void* v,
				RDF_Resource match, RDF_ValueType type)
{
	RDF_Cursor	c;
	void		*val;
    PRBool		ok = 0;

  if ((s == gWebData->RDF_URL) && (type == RDF_STRING_TYPE))
  {
	val = resourceID(u);
	ok = matchStrings(match, val, v);
	return(ok);
  }

  c = RDF_GetTargets(r, u, s, type, 1);
  if (c != NULL) {
    while ((val = RDF_NextValue(c)) != NULL) {
      if (type == RDF_RESOURCE_TYPE) {
        ok = (u == v);
      } else if (type == RDF_STRING_TYPE) {
      	if (s == gWebData->RDF_URL)
      	{
      		val = resourceID(u);
      	}
      	ok = matchStrings(match, val, v);
      }
      if (ok) {
        RDF_DisposeCursor(c);
        return 1;
      }
    }
    RDF_DisposeCursor(c);
  }
  return 0;
}



RDF_Resource
nextFindValue (RDF_Cursor c)
{
	RDF_Resource ans = NULL;

  if (((c->count*sizeof(RDF_Resource *)) < c->size) &&
  	(*(RDF_Resource *)((char *)c->pdata + (c->count*sizeof(RDF_Resource *))) != NULL))
  {
    ans = *(RDF_Resource *)((char *)c->pdata + (c->count*sizeof(RDF_Resource *)));
    c->count++;
  }
  return ans;
}



void
possiblyGCResource (RDF_Resource u)
{
  if ((nullp(u->pdata)) && (nullp(u->rarg1)) && (nullp(u->rarg2))) {
    PL_HashTableRemove(resourceHash,  resourceID(u));
    freeMem(u->url);
    freeMem(u);  
  }
}



PR_PUBLIC_API(RDF_Notification)
RDF_AddNotifiable (RDF rdfDB, RDF_NotificationProc callBack, 
					     RDF_Event ev, void* pdata)
{
  RDF_Notification ns = (RDF_Notification)getMem(sizeof(struct RDF_NotificationStruct));
  RDF_Event        nev = (RDF_Event)getMem(sizeof(struct RDF_EventStruct));
  memcpy((char*)nev, ev, sizeof(struct RDF_EventStruct));
  ns->theEvent = nev;
  ns->notifFunction = callBack;
  ns->pdata = pdata;
  ns->next = rdfDB->notifs;
  rdfDB->notifs = ns;
  ns->rdf = rdfDB;
  return ns;
}



PR_PUBLIC_API(RDF_Error)
RDF_DeleteNotifiable (RDF_Notification ns)
{
  RDF rdf = ns->rdf;
  RDF_Notification pr = rdf->notifs;
  RDF_Notification not;
  if (pr == ns) {
    rdf->notifs = pr->next; 
  } else {
    for (not = rdf->notifs; (not != NULL) ; not = not->next) {
      if (ns == not) 
	  {
		pr->next = not->next; 
		break;
      }
      pr = not;
    }
  }
  freeMem(ns->theEvent);
  freeMem(ns);
  return 0;
}



void
assertNotify (RDF rdf, RDF_Notification not, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv, char* ds)
 {
  RDF_Event nev = not->theEvent;
  RDF_AssertEvent ev = &(not->theEvent->event.assert);
  PRBool opTypeFixed = not->theEvent->eventType;
  PRBool uFixed      = (ev->u != NULL);
  PRBool sFixed		= (ev->s != NULL);
  PRBool vFixed		= (ev->v != NULL);
  RDF_EventType  oldEventType   = not->theEvent->eventType;
  if ((!opTypeFixed ||(not->theEvent->eventType & RDF_ASSERT_NOTIFY)) &&
      (!uFixed	  || (ev->u      == u))      &&
      (!vFixed    || (ev->v      == v))       &&
      (!sFixed      || (ev->s      == s))) {
    /*   if 	(callHasAssertions(0, rdf, u, s, v, type, !tv)) return; */
    nev->eventType = RDF_ASSERT_NOTIFY;
    ev->u      = u;
    ev->s      = s;
    ev->v      = v;
    ev->type   = type;
    ev->tv     = tv;
    ev->dataSource = ds;
    (*(not->notifFunction))(nev, not->pdata);
       nev->eventType = oldEventType;
    if (!uFixed)      ev->u = NULL;
    if (!sFixed)      ev->s = NULL;
    if (!vFixed)      ev->v = NULL;
    }
}



void
insertNotify (RDF rdf, RDF_Notification not, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv, char* ds)
{
  RDF_Event nev = not->theEvent;
  RDF_AssertEvent ev = &(not->theEvent->event.assert);
  PRBool opTypeFixed = not->theEvent->eventType;
  PRBool uFixed      = (ev->u != NULL);
  PRBool sFixed		= (ev->s != NULL);
  PRBool vFixed		= (ev->v != NULL);
  RDF_EventType  oldEventType   = not->theEvent->eventType;
  if ((!opTypeFixed ||(not->theEvent->eventType & RDF_INSERT_NOTIFY)) &&
      (!uFixed	  || (ev->u      == u))      &&
      (!vFixed    || (ev->v      == v))       &&
      (!sFixed      || (ev->s      == s))) {
    /*   if 	(callHasAssertions(0, rdf, u, s, v, type, !tv)) return; */
    nev->eventType = RDF_INSERT_NOTIFY;
    ev->u      = u;
    ev->s      = s;
    ev->v      = v;
    ev->type   = type;
    ev->tv     = tv;
    ev->dataSource = ds;
    (*(not->notifFunction))(nev, not->pdata);
        nev->eventType = oldEventType;
    if (!uFixed)      ev->u = NULL;
    if (!sFixed)      ev->s = NULL;
    if (!vFixed)      ev->v = NULL;
    }
}



void
unassertNotify (RDF_Notification not, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, char* ds)
{
  RDF_Event nev = not->theEvent;
  RDF_UnassertEvent ev = &(not->theEvent->event.unassert);
  PRBool opTypeFixed = not->theEvent->eventType;
  PRBool uFixed      = (ev->u != NULL);
  PRBool sFixed		= (ev->s != NULL);
  PRBool vFixed		= (ev->v != NULL);
  RDF_EventType  oldEventType   = not->theEvent->eventType;
  if ((!opTypeFixed ||(not->theEvent->eventType & RDF_DELETE_NOTIFY)) &&
      (!uFixed	  || (ev->u      == u))      &&
      (!vFixed    || (ev->v      == v))       &&
      (!sFixed      || (ev->s      == s))) {
    nev->eventType = RDF_DELETE_NOTIFY;
    ev->u      = u;
    ev->s      = s;
    ev->v      = v;
    ev->type   = type;
    ev->dataSource = ds;
    (*(not->notifFunction))(nev, not->pdata);
    nev->eventType = oldEventType;
    if (!uFixed)      ev->u = NULL;
    if (!sFixed)      ev->s = NULL;
    if (!vFixed)      ev->v = NULL;
    }
}



void
sendNotifications2 (RDFT r, RDF_EventType opType, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv)
{
  RDFL rl = r->rdf;

  if ((opType == RDF_ASSERT_NOTIFY) && gLocalStore &&
      (nlocalStoreHasAssertion(gLocalStore, u, s, v, type, !tv))) {
	  return;
  }

  while (rl) {
    sendNotifications(rl->rdf, opType, u, s, v, type, tv, r->url);
    rl = rl->next;
  }
}



void
sendNotifications (RDF rdf, RDF_EventType opType, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv, char* ds)
{
  RDF_Notification not;
  if (rdf == NULL) return;
  for (not = rdf->notifs; (not != NULL) ; not = not->next) {
    if (!not->theEvent || (not->theEvent->eventType & opType)) {
		RDF_EventType otype = not->theEvent->eventType;
      switch (opType) 
	{
	case RDF_ASSERT_NOTIFY :
	  assertNotify(rdf, not, u, s, v, type, tv, ds);
	  break;
	case RDF_INSERT_NOTIFY :
	  insertNotify(rdf, not, u, s, v, type, tv, ds);
	  break;
	case RDF_DELETE_NOTIFY :
	  unassertNotify(not, u, s, v, type, ds);
	  break;
	case RDF_KILL_NOTIFY :
	  break;
	  /*	  killNotify(not, u); */
	}
	  not->theEvent->eventType = otype;
    }
  }
}
