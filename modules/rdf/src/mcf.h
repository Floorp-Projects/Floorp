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

#ifndef	_RDF_MCF_H_
#define	_RDF_MCF_H_


#include "rdf-int.h"
#include "prprf.h"
#include "prtime.h"
#include <string.h>


/* mcf.c data structures and defines */

struct RDF_NotificationStruct {
  RDF_Event  theEvent;
  void*      pdata;
  RDF_NotificationProc notifFunction;
  RDF        rdf;
  struct RDF_NotificationStruct* next;
};


#define ntr(r, n) (*((RDFT*)r->translators + n))
#define ntrn(r, n) (*((RDFT*)r->translators + n) == NULL)
#define callAssert(n, r, u, s, v,type,tv) (ntrn(r, n) || (ntr(r, n)->assert == NULL) ? 0 : (*(ntr(r, n)->assert))(ntr(r, n), u, s, v, type, tv))
#define callUnassert(n, r, u, s, v,type) (ntrn(r, n) || (ntr(r, n)->unassert == NULL) ? 0 : (*(ntr(r, n)->unassert))(ntr(r, n), u, s, v, type))
#define callGetSlotValue(n, r, u, s, type, invp, tv) (ntrn(r, n) || (ntr(r, n)->getSlotValue == NULL) ? 0 : (*(ntr(r, n)->getSlotValue))(ntr(r, n), u, s,  type, invp, tv))
#define callGetSlotValues(n, r, u, s, type,invp, tv) (ntrn(r, n) || (ntr(r, n)->getSlotValues == NULL) ? 0 : (*(ntr(r, n)->getSlotValues))(ntr(r, n), u, s,  type,invp, tv))
#define callHasAssertions(n, r, u, s, v,type,tv) (ntrn(r, n) || (ntr(r, n)->hasAssertion == NULL) ? 0 : (*(ntr(r, n)->hasAssertion))(ntr(r, n), u, s, v, type, tv))
#define callArcLabelsOut(n, r, u) (ntrn(r, n) || (ntr(r, n)->arcLabelsOut == NULL) ? 0 : (*(ntr(r, n)->arcLabelsOut))(ntr(r, n), u))
#define callArcLabelsIn(n, r, u) (ntrn(r, n) || (ntr(r, n)->arcLabelsIn == NULL) ? 0 : (*(ntr(r, n)->arcLabelsIn))(ntr(r, n), u))
#define callDisposeResource(n, r, u) (ntrn(r, n) || (ntr(r, n)->disposeResource == NULL) ? 1 : (*(ntr(r, n)->disposeResource))(ntr(r, n), u))
#define callExitRoutine(n, r) (ntrn(r, n) || (ntr(r, n)->destroy == NULL) ? 0 : (*(ntr(r, n)->destroy))(ntr(r, n)))
#define callUpdateRoutine(n, r, u) (ntrn(r, n) || (ntr(r, n)->update == NULL) ? 0 : (*(ntr(r, n)->update))(ntr(r, n), u))

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
RDF_Resource		specialUrlResource (char* id);
RDF_Resource		NewRDFResource (char* id);
RDF_Resource		QuickGetResource (char* id);
RDF_Cursor		getSlotValues (RDF rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep, PRBool tv);
void 			disposeResourceInt (RDF rdf, RDF_Resource u);
void			possiblyGCResource (RDF_Resource u);
RDF_Resource		NewRDFResource (char* id);
RDF_Resource		QuickGetResource (char* id);
void			assertNotify (RDF rdf, RDF_Notification not, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv, char* ds);
void			insertNotify (RDF rdf, RDF_Notification not, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv, char* ds);
void			unassertNotify (RDF_Notification not, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, char* ds);
void			sendNotifications1 (RDFL rl, RDF_EventType opType, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
void			sendNotifications (RDF rdf, RDF_EventType opType, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv, char* ds);
RDF_Resource		nextFindValue (RDF_Cursor c);
PRBool			matchStrings(RDF_Resource match, char *str1, char *str2);
PRBool			itemMatchesFind (RDF r, RDF_Resource u, RDF_Resource s, void* v, RDF_Resource match, RDF_ValueType type);
PR_PUBLIC_API(RDF_Cursor)RDF_Find (RDF_Resource s, RDF_Resource match, void* v, RDF_ValueType type);
PRIntn			findEnumerator (PLHashEntry *he, PRIntn i, void *arg);
void			disposeAllDBs ();


XP_END_PROTOS

#endif
