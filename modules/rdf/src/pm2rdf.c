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
   This file implements mail support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#ifdef SMART_MAIL

#include "net.h"
#include "rdf-int.h"
#include <stdio.h>

extern	MWContext	*FE_GetRDFContext(void);
extern char*  profileDirURL;

struct MailFolder {
  FILE *sfile;
  FILE *mfile;
  struct MailMessage* msg;
  struct MailMessage* tail;
  struct MailMessage* add;
  RDF_Resource top;
  int32 status;
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

void
Pop_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx) {
}

void 
GetPopToRDF (RDFT rdf) {
  MF folder = (MF) rdf->pdata;
  if (endsWith("/inbox", rdf->url)) {
    char* popurl = getMem(100);
    int n = 10;
    int l = strlen(rdf->url);
    URL_Struct *urls ;
    memcpy(popurl, "pop3://", 7);
    while (n < l) {
      if (rdf->url[n] == '/') break;
      popurl[n-3] = rdf->url[n];
      n++;
    }
    
    urls = NET_CreateURLStruct(popurl, NET_DONT_RELOAD);
    if (urls != NULL)  {
      urls->fe_data = rdf;
      NET_GetURL(urls, FO_PRESENT, FE_GetRDFContext(), Pop_GetUrlExitFunc);
    }
  }
}

char* stripCopy (char* str) {
  return copyString(XP_StripLine(str)); 
}

FILE *openPMFile (char* path) {
	FILE* ans = fopen(path, "r+");
	if (!ans) {
		ans = fopen(path, "w");
		fclose(ans);
		ans = fopen(path, "r+");
	}
	return ans;
}


void addMsgToFolder (MF folder, MM msg) {
	if (!folder->tail) {
		folder->msg = folder->tail = msg;
	} else {
		folder->tail->next = msg;
		folder->tail = msg;
	}
}

void 
RDF_StartMessageDelivery (RDFT rdf) {
  MF folder = (MF) rdf->pdata;
  MM msg    = (MM) getMem(sizeof(struct MailMessage));
  char* nurl = getMem(100);
  fseek(folder->mfile, 0L, SEEK_END);
  fprintf(folder->mfile, "From - \n");
  msg->offset = ftell(folder->mfile);
  sprintf(nurl, "%s?%i", rdf->url, msg->offset);
  msg->r = RDF_GetResource(NULL, nurl, 1);
  msg->r->pdata = msg;
  msg->flags = getMem(4);
  folder->add = msg;
  setResourceType(msg->r, PM_RT); 
  fseek(folder->mfile, 0L, SEEK_END);
  fputs("X-Mozilla-Status: 0000", folder->mfile);
}


char* MIW1 (const char* block, int32 len) {
  char* blk = XP_ALLOC(len +1);
  int32 n = 0;
  int32 m = 0;
  PRBool seenp = 0;
  PRBool wsendp = 0;
  memset(blk, '\0', len);
  while (n++ < len) {
    char c = block[n];
	if ((c == '\r') || (c == '\n')) break;
    if (!seenp) {
      seenp = (c == ':');
    } else {
      if (c != ' ') wsendp = 1;
      if (wsendp) {
        blk[m++] = c;
      }
    }
  }
  return blk;
}

void 
RDF_AddMessageLine (RDFT rdf, char* block, int32 length) {
  MF folder = (MF) rdf->pdata;
  MM msg    = folder->add;
  char* temp = getMem(length+1);
  memcpy(temp, block, length);
  if (!msg->from && (startsWith("From:", block))) {
    msg->from = MIW1(block, length);
  } else if (!msg->subject && (startsWith("Subject:", block))) {
    msg->subject = MIW1(block, length);
  } else if (!msg->date && (startsWith("Date:", block))) {
    msg->date = MIW1(block, length);
  }
   
  fseek(folder->mfile, 0L, SEEK_END);
  fputs(temp, folder->mfile);
  freeMem(temp);
}

void 
RDF_FinishMessageDelivery (RDFT rdf) {
  MF folder = (MF) rdf->pdata;
  MM msg    = folder->add;
  folder->add = NULL;
  addMsgToFolder(folder, msg);
  setResourceType(msg->r, PM_RT);
  fseek(folder->sfile, 0L, SEEK_END);
  fprintf(folder->sfile, "From: %s\nSubject: %s\nDate: %s\n%d\n", 
          msg->from, msg->subject, msg->date, msg->offset);
   fseek(folder->mfile, 0L, SEEK_END);
  fputs("\n", folder->mfile);
  sendNotifications2(rdf, RDF_ASSERT_NOTIFY, msg->r, gCoreVocab->RDF_parent, folder->top, 
                     RDF_RESOURCE_TYPE, 1);       
}

void 
setMessageFlag (RDFT rdf, RDF_Resource r, char* newFlag) {
  MF folder = (MF) rdf->pdata;
  MM msg    = (MM)r->pdata;
  int32 offset = msg->summOffset+8;
  fseek(folder->sfile, summOffset, SEE_SET);
  fputs(newFlag, folder->sfile);
  offset = msg->offset + 17;
  fputs(newFlag, folder->mfile);
}

void 
MoveMessageToFolder (char* from, char* to, char* url) {
  int32  messageOffset;
  
        
void saveSummaryFile (RDFT rdf) {
  MF folder = (MF) rdf->pdata;
  MM msg    = folder->msg;
  fseek(folder->sfile, 0L, SEEK_SET);
  while (msg) {
    msg->summOffset = ftell(folder->sfile);
    fprintf(folder->sfile, "Status: %s\nOffset: %d\nFrom: %s\nSubject: %s\nDate: %s\n%d\n", 
            msg->summOffset, msg->from, msg->subject, msg->date, msg->offset, msg->flags);
    msg = msg->next;
  }
  fflush(folder->sfile);
}

 
#define BUFF_SIZE 100000 
          
void readSummaryFile (RDFT rdf) {
  if (startsWith("mailbox://", rdf->url)) {
    char* url = rdf->url;
    char* folderURL = &url[10];
    char* fileurl = getMem(strlen(profileDirURL) + strlen(folderURL) + 4);
    char* nurl = getMem(strlen(url) + 20);
    FILE *f; 
    char* buff = getMem(BUFF_SIZE);
    MF folder = (MF) getMem(sizeof(struct MailFolder));
    MM msg = NULL;
    FILE *mf;
     
	rdf->pdata = folder;
    sprintf(fileurl, "%s%s.ssf",  profileDirURL, folderURL);
    fileurl = MCDepFileURL(fileurl);
    f = openPMFile(fileurl);
    sprintf(fileurl, "%s%s",  profileDirURL, folderURL);
	fileurl = MCDepFileURL(fileurl);
    mf = openPMFile(fileurl);
    folder->top = RDF_GetResource(NULL, rdf->url, 1);
	setResourceType(folder->top, PM_RT);    
    setContainerp(folder->top, 1);
    folder->sfile = f;
    folder->mfile = mf;

    while (f && fgets(buff, BUFF_SIZE, f)) {
      if (startsWith("Status:", buff)) {
        msg = (MM) getMem(sizeof(struct MailMessage));
        msg->flags = stripCopy(&buff[8]);
        fgets(buff, BUFF_SIZE, f);
        sscanf(buff, "%i", &msg->summOffset);
        fgets(buff, BUFF_SIZE, f);
        msg->from = stripCopy(&buff[6]);
        fgets(buff, BUFF_SIZE, f);
        msg->subject = stripCopy(&buff[8]);
        fgets(buff, BUFF_SIZE, f);
        msg->date = stripCopy(&buff[6]);
        fgets(buff, BUFF_SIZE, f);
        msg->date = stripCopy(&buff[6]);
        fgets(buff, BUFF_SIZE, f);
        sscanf(buff, "%i", &msg->offset);
        sprintf(nurl, "%s?%i", url, msg->offset);
        msg->r = RDF_GetResource(NULL, nurl, 1);
        msg->r->pdata = msg;
        addMsgToFolder (folder, msg) ; 
        setResourceType(msg->r, PM_RT);        
      }
    }

    if (msg == NULL) {
      /* either a new mailbox or need to read BMF to recreate */
      while (mf && fgets(buff, BUFF_SIZE, mf)) {
        if (startsWith("From ", buff))  { 
          msg = (MM) getMem(sizeof(struct MailMessage));
          msg->offset = ftell(mf);
          sprintf(nurl, "%s?%i", url, msg->offset);
          msg->r = RDF_GetResource(NULL, nurl, 1); 
          msg->r->pdata = msg;
          setResourceType(msg->r, PM_RT);
		  addMsgToFolder (folder, msg) ;
        }
        if ((!msg->from) && (startsWith("From:", buff))) {
          msg->from = stripCopy(&buff[6]);
        } else if ((!msg->date) && (startsWith("Date:", buff))) {
          msg->date = stripCopy(&buff[6]);
        } else if ((!msg->subject) && (startsWith("Subject:", buff))) {
          msg->subject = stripCopy(&buff[8]);
        } else if ((!msg->flags) && (startsWith("X-Mozilla-Status:", buff))) {
          msg->flags = stripCopy(&buff[17]);
        }
      }
      saveSummaryFile(rdf);
    }
    freeMem(buff);
    freeMem(nurl);
    GetPopToRDF(rdf);
  }
}

void *
pmGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, 
                PRBool inversep,  PRBool tv) {
  if ((resourceType(u) == PM_RT) && tv && (!inversep) && (type == RDF_STRING_TYPE) && (u->pdata)) {
    MM msg = (MM) u->pdata;
    if (s == gNavCenter->from) {
      return copyString(msg->from);
    } else if (s == gNavCenter->subject) {
      return copyString(msg->subject);
    } else if (s == gNavCenter->date) {
      return copyString(msg->date);
    } else return NULL;
  } else return NULL;
}

        
RDF_Cursor
pmGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, 
                 PRBool inversep,  PRBool tv) {
  if ((resourceType(u) == PM_RT) && tv && (inversep) && (type == RDF_RESOURCE_TYPE)
      && (s == gCoreVocab->RDF_parent)) {
    MF folder = (MF)rdf->pdata;
    if (folder->top == u) {
      RDF_Cursor c = (RDF_Cursor)getMem(sizeof(struct RDF_CursorStruct));
      c->u = u;
      c->s = s;
      c->type = type;
      c->inversep = inversep;
      c->tv = tv;
      c->count = 0;
      c->pdata = folder->msg;
      return c;
    } else return NULL;
  } else return NULL;
}


void *
pmNextValue (RDFT rdf, RDF_Cursor c)
{
  MM msg = (MM) c->pdata;
  RDF_Resource ans = NULL;
  if (msg) {
	ans = msg->r;
	c->pdata = msg->next;
  }
  return ans;
}


RDF_Error
pmDisposeCursor (RDFT mcf, RDF_Cursor c)
{
  freeMem(c);
  return noRDFErr;
}

FILE *getPopMBox (RDFT db) {
  MF folder = (MF)db->pdata;
  return folder->mfile;
}

RDFT
MakePopDB (char* url)
{
  if (startsWith("mailbox://", url)) {
    RDFT		ntr;
	if ((ntr = (RDFT)getMem(sizeof(struct RDF_TranslatorStruct))) != NULL) {
		ntr->assert = NULL;
		ntr->unassert = NULL;
		ntr->getSlotValue = pmGetSlotValue;
		ntr->getSlotValues = pmGetSlotValues;
		ntr->hasAssertion = NULL;
		ntr->nextValue = pmNextValue;
		ntr->disposeCursor = pmDisposeCursor;
		ntr->url = copyString(url);
        readSummaryFile(ntr);
	}
	return(ntr);
  } else return NULL;
}

#endif    
  
    
    
    
    

