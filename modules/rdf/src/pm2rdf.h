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

#ifndef	_RDF_PM2RDF_H_
#define	_RDF_PM2RDF_H_

#include "net.h"
#include "rdf.h"
#include "rdf-int.h"
#include "prio.h"
#include "glue.h"
#include "utils.h"
#include "xp_str.h"
#include <stdio.h>


/* pm2rdf.c data structures and defines */

#define TON(s) ((s == NULL) ? "" : s)  

#define pmUnitp(u) ((resourceType(u) == PM_RT) || (resourceType(u) == IM_RT))

struct MailFolder {
  FILE *sfile;
  FILE *mfile;
  struct MailMessage* msg;
  struct MailMessage* tail;
  struct MailMessage* add;
  RDF_Resource top;
  int32 status;
  RDFT rdf;
  char* trash;
};

typedef struct MailFolder* MF;

struct MailMessage {
  char* subject;
  char* from;
  char* date;
  int32 offset;
  char* flags;
  int32 summOffset;
  RDF_Resource r;
  struct MailMessage *next;
};

typedef struct MailMessage* MM;


/* pm2rdf.c function prototypes */

XP_BEGIN_PROTOS

void		Pop_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx);
void 		GetPopToRDF (RDFT rdf);
void		PopGetNewMail (RDF_Resource r);
char *		stripCopy (char* str);
PRBool		msgDeletedp (MM msg);
FILE *		openPMFile (char* path);
void		addMsgToFolder (MF folder, MM msg);
void 		RDF_StartMessageDelivery (RDFT rdf);
char *		MIW1 (const char* block, int32 len);
void 		RDF_AddMessageLine (RDFT rdf, char* block, int32 length);
void		writeMsgSum (MF folder, MM msg);
void 		RDF_FinishMessageDelivery (RDFT rdf);
void 		setMessageFlag (RDFT rdf, RDF_Resource r, char* newFlag);
PRBool		MoveMessage (char* to, char* from, MM message);
void		readSummaryFile (RDFT rdf);
void *		pmGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv);
RDF_Cursor	pmGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv);
void *		pmNextValue (RDFT rdf, RDF_Cursor c);
RDF_Error	pmDisposeCursor (RDFT mcf, RDF_Cursor c);
FILE *		getPopMBox (RDFT db);
PRBool		pmHasAssertion (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
PRBool		pmRemove (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
RDFT		MakePopDB (char* url);
RDFT		MakeMailAccountDB (char* url);




XP_END_PROTOS

#endif

