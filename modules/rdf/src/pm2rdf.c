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
   This file implements mail support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#ifdef SMART_MAIL
#include "pm2rdf.h"

extern	char		*profileDirURL;



void
Pop_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx)
{
}



void 
GetPopToRDF (RDFT rdf)
{
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

      NET_GetURL(urls, FO_PRESENT, gRDFMWContext(rdf), Pop_GetUrlExitFunc);
    }
  }
}



void
PopGetNewMail (RDF_Resource r)
{
  if (containerp(r) && (resourceType(r) == PM_RT)) {
    MF folder = (MF) r->pdata;
    GetPopToRDF(folder->rdf);
  }
}



char *
stripCopy (char* str)
{
  return copyString(XP_StripLine(str)); 
}



PRBool
msgDeletedp (MM msg)
{
  return (msg && (msg->flags) && (msg->flags[4] == '8'));
}



FILE *
openPMFile (char* path)
{
	FILE* ans = fopen(path, "r+");
	if (!ans) {
		ans = fopen(path, "w");
		if (ans) fclose(ans);
		ans = fopen(path, "r+");
	}
	return ans;
}



void
addMsgToFolder (MF folder, MM msg)
{
  if (!folder->tail) {
    folder->msg = folder->tail = msg;
  } else {
    folder->tail->next = msg;
    folder->tail = msg;
  }
}



void 
RDF_StartMessageDelivery (RDFT rdf)
{
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
  fputs("X-Mozilla-Status: 0000\n", folder->mfile);
}



char *
MIW1 (const char* block, int32 len)
{
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
RDF_AddMessageLine (RDFT rdf, char* block, int32 length)
{
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


#define TON(s) ((s == NULL) ? "" : s)  
void writeMsgSum (MF folder, MM msg) {
  if (!msg->flags) msg->flags = copyString("0000");
  if (msg->summOffset == -1) {
    fseek(folder->sfile, 0L, SEEK_END);
    msg->summOffset = ftell(folder->sfile);
  } else {
    fseek(folder->sfile, msg->summOffset, SEEK_SET);     
  }
  fprintf(folder->sfile, "Status: %s\nSOffset: %d\nFrom: %s\nSubject: %s\nDate: %s\nMOffset: %d\n", 
          msg->flags, ftell(folder->sfile), 
          TON(msg->from), TON(msg->subject), TON(msg->date), msg->offset );
}



void 
RDF_FinishMessageDelivery (RDFT rdf)
{
  MF folder = (MF) rdf->pdata;
  MM msg    = folder->add;
  folder->add = NULL;
  addMsgToFolder(folder, msg);
  setResourceType(msg->r, PM_RT);
  fseek(folder->sfile, 0L, SEEK_END);
  msg->summOffset = ftell(folder->sfile);
  writeMsgSum(folder, msg);
  fseek(folder->mfile, 0L, SEEK_END);
  fputs("\n", folder->mfile);
  sendNotifications2(rdf, RDF_ASSERT_NOTIFY, msg->r, gCoreVocab->RDF_parent, folder->top, 
                     RDF_RESOURCE_TYPE, 1);       
}



void 
setMessageFlag (RDFT rdf, RDF_Resource r, char* newFlag)
{
  MF folder = (MF) rdf->pdata;
  MM msg    = (MM)r->pdata;
  fseek(folder->sfile, msg->summOffset+8, SEEK_SET);
  fputs(newFlag, folder->sfile);
  freeMem(msg->flags);
  msg->flags = copyString(newFlag);
  /* need to mark the flag in the message file */
  fflush(folder->sfile);
}

#define BUFF_SIZE 50000


RDFT 
getBFTranslator (char* url) {
	if (startsWith("mailbox://folder/", url)) {
		char* temp = getMem(strlen(url));
		RDFT ans = NULL;
		sprintf(temp, "mailbox://%s", &url[17]);
	    ans = getTranslator(temp);
		freeMem(temp);
		return ans;
	} else return getTranslator(url);
}

PRBool
MoveMessage (char* to, char* from, MM message) {
  RDFT todb = getBFTranslator(to);
  RDFT fromdb = getBFTranslator(from);
  MF tom = todb->pdata;
  MF fom = fromdb->pdata;
  RDF_Resource r;
  MM newMsg = (MM)getMem(sizeof(struct MailMessage));
  char* buffer = getMem(BUFF_SIZE);
  if (!buffer) return 0;
  setMessageFlag(fromdb, message->r, "0008"); 
  fseek(tom->mfile, 0L, SEEK_END);
  fseek(fom->mfile, message->offset, SEEK_SET);
  fputs("From -\n", tom->mfile);
  sprintf(buffer, "mailbox://%s?%d", &to[17], ftell(tom->mfile));
  r = RDF_GetResource(NULL, buffer, 1);
  newMsg->subject = copyString(message->subject);
  newMsg->from = copyString(message->from);
  newMsg->date = copyString(message->date);
  newMsg->r = r;
  r->pdata = newMsg;
  setResourceType(r, PM_RT);        
  newMsg->summOffset = -1;
  newMsg->offset = ftell(tom->mfile);
  writeMsgSum(tom, newMsg);
  addMsgToFolder (tom, newMsg) ;
  fflush(tom->sfile);
  while (fgets(buffer, BUFF_SIZE, fom->mfile) && strncmp("From ", buffer, 5)) {
    fputs(buffer, tom->mfile);
  }
  sendNotifications2(todb, RDF_ASSERT_NOTIFY, r, gCoreVocab->RDF_parent, tom->top, 
                     RDF_RESOURCE_TYPE, 1);       
  sendNotifications2(fromdb, RDF_DELETE_NOTIFY, message->r, gCoreVocab->RDF_parent, fom->top, 
                     RDF_RESOURCE_TYPE, 1);       
  freeMem(buffer);
  return 1;
}



void
readSummaryFile (RDFT rdf)
{
  if (startsWith("mailbox://", rdf->url)) {
    char* url = rdf->url;
    char* folderURL = &url[10];
	int32 flen = strlen(profileDirURL) + strlen(folderURL) + 4;
    char* fileurl = getMem(flen);
    char* nurl = getMem(strlen(url) + 20);
    FILE *f; 
    char* buff = getMem(BUFF_SIZE);
    MF folder = (MF) getMem(sizeof(struct MailFolder));
    MM msg = NULL;
    FILE *mf;
    char* aclen;
     
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
    folder->top->pdata = folder;
    folder->rdf = rdf;
    folder->sfile = f;
    folder->mfile = mf;

    while (f && fgets(buff, BUFF_SIZE, f)) {
      if (startsWith("Status:", buff)) {
        msg = (MM) getMem(sizeof(struct MailMessage));
        msg->flags = stripCopy(&buff[8]);
        fgets(buff, BUFF_SIZE, f);
        sscanf(&buff[9], "%d", &msg->summOffset);
        fgets(buff, BUFF_SIZE, f);
        msg->from = stripCopy(&buff[6]);
        fgets(buff, BUFF_SIZE, f);
        msg->subject = stripCopy(&buff[8]);
        fgets(buff, BUFF_SIZE, f);
        msg->date = stripCopy(&buff[6]);
        fgets(buff, BUFF_SIZE, f);
        sscanf(&buff[9], "%d", &msg->offset);
        sprintf(nurl, "%s?%d", url, msg->offset);
        msg->r = RDF_GetResource(NULL, nurl, 1);
        msg->r->pdata = msg;
        addMsgToFolder (folder, msg) ; 
        setResourceType(msg->r, PM_RT);        
      }
    }

    if (msg == NULL) {
      /* either a new mailbox or need to read BMF to recreate */
      while (mf && fgets(buff, BUFF_SIZE, mf)) {
        if (strncmp("From ", buff, 5) ==0)  { 
          if (msg) writeMsgSum(folder, msg);
          msg = (MM) getMem(sizeof(struct MailMessage));
          msg->offset = ftell(mf);
          msg->summOffset = -1;
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
      if (msg) writeMsgSum(folder, msg);
      if (folder->sfile) fflush(folder->sfile);
    }
	memset(fileurl, '\0', flen);
	memcpy(fileurl, rdf->url, strlen(rdf->url));
    aclen = strchr(&fileurl[10], '/');
	fileurl[aclen-fileurl] = '\0';
	strcat(fileurl, "/trash"); 
    folder->trash = fileurl;
    freeMem(buff);
    freeMem(nurl);
    /* GetPopToRDF(rdf); */
  }
}



void *
pmGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, 
                PRBool inversep,  PRBool tv)
{
  if ((resourceType(u) == PM_RT) && tv && (!inversep) && (type == RDF_STRING_TYPE) && (u->pdata)) {
    MM msg = (MM) u->pdata;
    if (s == gNavCenter->from) {
      XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )msg->from)));
      return copyString(msg->from);
    } else if (s == gNavCenter->subject) {
      XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )msg->subject)));
      return copyString(msg->subject);
    } else if (s == gNavCenter->date) {
      return copyString(msg->date);
    } else return NULL;
  } else return NULL;
}



RDF_Cursor
pmGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, 
                 PRBool inversep,  PRBool tv)
{
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
  while (msg && msgDeletedp(msg)) {
    msg = msg->next;
  }
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



FILE *
getPopMBox (RDFT db)
{
  MF folder = (MF)db->pdata;
  return folder->mfile;
}



PRBool
pmHasAssertion (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv)
{
  /*this is clearly wrong, but doesn't break anything now ...*/
  return 1;
}



PRBool
pmRemove (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type)
{
  XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )v)));
  if ((startsWith("mailbox://", rdf->url)) && (resourceType(u) == PM_RT) && (s == gCoreVocab->RDF_parent)
      && (type == RDF_RESOURCE_TYPE)) {
    RDF_Resource mbox = (RDF_Resource) v;
    if (!(containerp(mbox) && (resourceType(mbox) == PM_RT))) {
      return false;
    } else {
      MF folder = (MF)mbox->pdata;
      sendNotifications2(rdf, RDF_DELETE_NOTIFY, u, s, v, type, 1);
      MoveMessage(folder->trash, resourceID(mbox), (MM)u->pdata);
      return 1;
    }
  } else return false;
}



RDFT
MakePopDB (char* url)
{
  if (startsWith("mailbox://", url)) {
    RDFT		ntr;
	if ((ntr = (RDFT)getMem(sizeof(struct RDF_TranslatorStruct))) != NULL) {
      char*  fileurl = getMem(100); 
      PRDir* dir ; 
      char* aclen;
      sprintf(fileurl, "%s%s", profileDirURL, &url[10]);
      aclen = strchr(&fileurl[strlen(profileDirURL)+1], '/');
      fileurl[aclen-fileurl] = '\0';
      dir = OpenDir(fileurl);
      if (dir == NULL) {
        if ( CallPRMkDirUsingFileURL(fileurl, 00700) > -1) dir = OpenDir(fileurl);
      }
      freeMem(fileurl);
      if (dir) {
        PR_CloseDir(dir); 
        ntr->assert = NULL;
        ntr->unassert = pmRemove;
        ntr->getSlotValue = pmGetSlotValue;
        ntr->getSlotValues = pmGetSlotValues;
        ntr->hasAssertion = pmHasAssertion;
        ntr->nextValue = pmNextValue;
        ntr->disposeCursor = pmDisposeCursor;
        ntr->url = copyString(url);        
        readSummaryFile(ntr);
        return ntr;
      } else {
        freeMem(ntr);
        return NULL;
      }
    } 
    else return NULL;
  } else return NULL;
}



RDFT
MakeMailAccountDB (char* url)
{
  if (startsWith("mailaccount://", url)) {
    RDFT   ntr =   NewRemoteStore(url);
    char*  fileurl = getMem(100);
    int32 n = PR_SKIP_BOTH;
    PRDirEntry	*de;
    PRDir* dir ;
    RDF_Resource top = RDF_GetResource(NULL, url, 1);
    sprintf(fileurl, "%s%s", profileDirURL, &url[14]);
    dir = OpenDir(fileurl);
    if (dir == NULL) {
      if ( CallPRMkDirUsingFileURL(fileurl, 00700) > -1) dir = OpenDir(fileurl);
    }
    while ((dir != NULL) && ((de = PR_ReadDir(dir, (PRDirFlags)(n++))) != NULL)) {
      if ((!endsWith(".ssf", de->name)) && (!endsWith(".dat", de->name)) && 
          (!endsWith(".snm", de->name)) && (!endsWith("~", de->name))) {
        RDF_Resource r;
        sprintf(fileurl, "mailbox://folder/%s/%s", &url[14], de->name);
        r = RDF_GetResource(NULL, fileurl, 1);
        setResourceType(r, PMF_RT);
        remoteStoreAdd(ntr, r, gCoreVocab->RDF_parent, top, RDF_RESOURCE_TYPE, 1);
        remoteStoreAdd(ntr, r, gCoreVocab->RDF_name, copyString(de->name), RDF_STRING_TYPE, 1);
      }
    }
    freeMem(fileurl);
    if (dir) PR_CloseDir(dir);
    return ntr;
  } else return NULL;
}

#endif  
