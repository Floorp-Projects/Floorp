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

#ifndef	_RDF_MCF_H_
#define	_RDF_MCF_H_


#include "rdf-int.h"
#include "prprf.h"
#include "prtime.h"



/* mcf.c data structures and defines */

struct RDF_NotificationStruct {
  RDF_Event  theEvent;
  void*      pdata;
  RDF_NotificationProc notifFunction;
  RDF        rdf;
  struct RDF_NotificationStruct* next;
};


#define ntr(r, n) (*((RDFT*)r->translators + n))
#define callAssert(n, r, u, s, v,type,tv) (ntr(r, n)->assert == NULL ? 0 : (*(ntr(r, n)->assert))(ntr(r, n), u, s, v, type, tv))
#define callUnassert(n, r, u, s, v,type) (ntr(r, n)->unassert == NULL ? 0 : (*(ntr(r, n)->unassert))(ntr(r, n), u, s, v, type))
#define callGetSlotValue(n, r, u, s, type, invp, tv) (ntr(r, n)->getSlotValue == NULL ? 0 : (*(ntr(r, n)->getSlotValue))(ntr(r, n), u, s,  type, invp, tv))
#define callGetSlotValues(n, r, u, s, type,invp, tv) (ntr(r, n)->getSlotValues == NULL ? 0 : (*(ntr(r, n)->getSlotValues))(ntr(r, n), u, s,  type,invp, tv))
#define callHasAssertions(n, r, u, s, v,type,tv) (ntr(r, n)->hasAssertion == NULL ? 0 : (*(ntr(r, n)->hasAssertion))(ntr(r, n), u, s, v, type, tv))
#define callArcLabelsOut(n, r, u) (ntr(r, n)->arcLabelsOut == NULL ? 0 : (*(ntr(r, n)->arcLabelsOut))(ntr(r, n), u))
#define callArcLabelsIn(n, r, u) (ntr(r, n)->arcLabelsIn == NULL ? 0 : (*(ntr(r, n)->arcLabelsIn))(ntr(r, n), u))
#define callDisposeResource(n, r, u) ((ntr(r, n)->disposeResource == NULL) ? 1 : (*(ntr(r, n)->disposeResource))(ntr(r, n), u))
#define callExitRoutine(n, r) ((ntr(r, n)->destroy == NULL) ? 0 : (*(ntr(r, n)->destroy))(ntr(r, n)))

#define ID_BUF_SIZE	20



/* mcf.c function prototypes */

XP_BEGIN_PROTOS

RDFT			getTranslator (char* url);
RDFL			deleteFromRDFList (RDFL xrl, RDF db);
RDF_Error		exitRDF (RDF rdf);
RDF_Resource		addDep (RDF db, RDF_Resource u);
PRBool			rdfassert(RDF rdf, RDF_Resource u, RDF_Resource  s, void* value, RDF_ValueType type,  PRBool tv);
PRBool			containerIDp(char* id);
char *			makeNewID ();
PRBool			iscontainerp (RDF_Resource u);
RDF_BT			resourceTypeFromID (char* id);
RDF_Cursor		getSlotValues (RDF rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep, PRBool tv);
void 			disposeResourceInt (RDF rdf, RDF_Resource u);
void			possiblyGCResource (RDF_Resource u);
void			assertNotify (RDF rdf, RDF_Notification not, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv, char* ds);
void			insertNotify (RDF rdf, RDF_Notification not, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv, char* ds);
void			unassertNotify (RDF_Notification not, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, char* ds);
void			sendNotifications1 (RDFL rl, RDF_EventType opType, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
void			sendNotifications (RDF rdf, RDF_EventType opType, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv, char* ds);
RDF_Resource		nextFindValue (RDF_Cursor c);
PRBool			itemMatchesFind (RDF r, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
PR_PUBLIC_API(RDF_Cursor)RDF_Find (RDF_Resource s, void* v, RDF_ValueType type);
PRIntn			findEnumerator (PLHashEntry *he, PRIntn i, void *arg);
void			disposeAllDBs ();


XP_END_PROTOS

#endif
