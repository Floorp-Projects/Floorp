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

#include "pm2rdf.h"
#include "glue.h"



char *
popmailboxesurl(void)
{
	char		*ans = NULL, *mboxFileURL;

	if ((mboxFileURL = makeDBURL("Mail")) != NULL)
	{
		if ((ans = getMem(strlen(mboxFileURL) + 8)) != NULL)
		{
			sprintf(ans, "mailbox:%s", &mboxFileURL[7]);
#ifdef	XP_WIN
			if (ans[10] == ':') ans[10] = '|';
#endif
		}
	}
	return(ans);
}



char *
imapmailboxesurl(void)
{
	char		*ans = NULL, *mboxFileURL;

	if ((mboxFileURL = makeDBURL("IMAPMail")) != NULL)
	{
		if ((ans = getMem(strlen(mboxFileURL) + 5)) != NULL)
		{
			sprintf(ans, "imap:%s", &mboxFileURL[7]);
#ifdef	XP_WIN
			if (ans[7] == ':') ans[7] = '|';
#endif
		}
	}
	return(ans);
}



void
buildMailList(RDF_Resource ms)
{
	RDF_Resource		imapmail, popmail;

	if ((imapmail = RDF_GetResource(NULL, imapmailboxesurl(), 1)) != NULL)
	{
		setContainerp(imapmail, 1);
		setResourceType(imapmail, IM_RT);
		remoteStoreAdd(gRemoteStore, imapmail, gCoreVocab->RDF_name,
				"Remote Mail", RDF_STRING_TYPE, 1);
		remoteStoreAdd(gRemoteStore, imapmail, gCoreVocab->RDF_parent,
				ms, RDF_RESOURCE_TYPE, 1);
	}
	if ((popmail = RDF_GetResource(NULL, popmailboxesurl(), 1)) != NULL)
	{
		setContainerp(popmail, 1);
		setResourceType(popmail, PM_RT);
		remoteStoreAdd(gRemoteStore, popmail, gCoreVocab->RDF_name,
				"Local Mail", RDF_STRING_TYPE, 1);
		remoteStoreAdd(gRemoteStore, popmail, gCoreVocab->RDF_parent,
				ms, RDF_RESOURCE_TYPE, 1);
	}
}



PRDir *
OpenMailDir(char *name)
{
	PRBool		nameHacked = false;
	PRDir		*dir = NULL;
	int			pathnameStart=0;

	if (startsWith("mailbox:",name))	pathnameStart=POPMAIL_URL_OFFSET;
	else if (startsWith("imap:",name))	pathnameStart=IMAPMAIL_URL_OFFSET;
	if (pathnameStart > 0)
	{
#ifdef	XP_WIN
		if (name[pathnameStart+1] == '|')
		{
			nameHacked = true;
			name[pathnameStart+1] = ':';
		}
#endif
	dir = CallPROpenDirUsingFileURL(name);

#ifdef	XP_WIN
		if (nameHacked == true)
		{
			name[pathnameStart+1] = '|';
		}
#endif
	}
/*
	else
	{
		dir = CallPROpenDirUsingFileURL(name);
	}
*/
	return(dir);
}



RDFT
MakeMailStore (char* url)
{
  RDFT ntr = (RDFT)getMem(sizeof(RDFT));
  ntr->assert = NULL;
  ntr->unassert = NULL;
  ntr->getSlotValue = pmGetSlotValue;
  ntr->getSlotValues = pmGetSlotValues;
  ntr->hasAssertion = pmHasAssertion;
  ntr->nextValue = pmNextValue;
  ntr->disposeCursor = pmDisposeCursor;
  buildMailList(gNavCenter->RDF_Mail);
  ntr->url = copyString(url);
  return ntr;
}




PRBool
pmHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
		       RDF_ValueType type, PRBool tv)
{
  if ((s == gCoreVocab->RDF_parent) && (type == RDF_RESOURCE_TYPE) &&
      (resourceType(u) == PM_RT) && (containerp(u)) && 
      (resourceType((RDF_Resource)v) == PM_RT) &&
      (startsWith( resourceID(u), resourceID((RDF_Resource)v)))) {
    PRDir* d = OpenMailDir( resourceID(u));
    char* filePathname =  resourceID((RDF_Resource)v);
    char* name = &filePathname[revCharSearch('/', filePathname)];
    PRDirEntry* de;
    int n = 0;
    PRBool ans = 0;

    while ((de = PR_ReadDir(d, (PRDirFlags)(n++))) != NULL) {
      if (strcmp(name, de->name) == 0) {
	ans = 1;
	break;
      }
    }
    PR_CloseDir(d);
    return ans;
  } else {
    return 0;
  }
}



void *
pmGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv)
{
  if ((s == gCoreVocab->RDF_parent) && (type == RDF_RESOURCE_TYPE) && (pmUnitp(u)) && tv) {
    if (inversep) {
      char* filePathname =  resourceID(u);
      size_t n = revCharSearch('/', filePathname);
      char nname[512];
      PRDir *d;
      PRBool ans = 0;
      memcpy((char*)nname, filePathname, n);
      d = OpenMailDir(nname);
      ans = (d != NULL);
      PR_CloseDir(d);
      if (ans) {
	RDF_Resource r = RDF_GetResource(NULL, nname, 1);
	setResourceType(r, PM_RT);
	setContainerp(r, 1);
	return r;
      } else return NULL;
    } else {
      PRDir* d = OpenMailDir( resourceID(u));
      PRDirEntry* de = PR_ReadDir(d, PR_SKIP_BOTH);
      if (de != NULL) {
	char nname[100];
	sprintf(nname, "%s/%s",  resourceID(u), de->name);
	PR_CloseDir(d);
	return CreatePMUnit(nname, resourceType(u), false);
      } else {
	PR_CloseDir(d);
	return NULL;
      }
    }
  } else if ((s == gCoreVocab->RDF_name) && (type == RDF_STRING_TYPE) && (tv) && (pmUnitp(u))) {
  	char *pathname, *name = NULL;
  	int len,n;
  	
  	if ((pathname = copyString( resourceID(u))) != NULL) {
  	  len = strlen(pathname);
  	  if (pathname[len-1] == '/')  pathname[--len] = '\0';
  	  if (endsWith(".sbd", pathname))	pathname[len-4] = '\0';
  	  n = revCharSearch('/', pathname);
  	  name = unescapeURL(&pathname[n+1]);
  	  freeMem(pathname);
  	}
    return(name);
  }
  else if ((s == gWebData->RDF_URL) && (type == RDF_STRING_TYPE) && (tv) && (pmUnitp(u))) {
	int		len;
	char		*pos, *name;

        if (resourceType(u) == PM_RT) return( copyString(resourceID(u)));
	if (resourceType(u) == IM_RT)
	{
		len = strlen( resourceID(u));
		if (resourceID(u)[len-1] == '/')	return(copyString(resourceID(u)));
		if ((pos=strstr(resourceID(u),"IMAPMail/")) != NULL)
		{
			if ((name = PR_smprintf("imap://%s",pos+9)) != NULL)
			{

				/* IMAP RFC specifies INBOX (all uppercase) ??? */

				if (endsWith ("/Inbox", name))
				{
					strcpy(&name[strlen(name)-5],"INBOX");
				}
			}
			return(name);
		}
	}
  }

  return NULL;
}



RDF_Cursor
pmGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, 
		     RDF_ValueType type,  PRBool inversep, PRBool tv)
{
  if ((s == gCoreVocab->RDF_parent) && (type == RDF_RESOURCE_TYPE) && (pmUnitp(u))
      && (inversep) && (tv)) {
    PRDir *d = OpenMailDir(resourceID(u));
    RDF_Cursor c;
    if (d == NULL) return NULL;
    c = (RDF_Cursor) getMem(sizeof(struct RDF_CursorStruct));
    c->u = u;
    c->count = PR_SKIP_BOTH;
    c->pdata = d;
    c->type = type;
    return c;
  } else return NULL;
}



void *
pmNextValue (RDFT rdf, RDF_Cursor c)
{
    PRFileInfo fn;
    int len, pathnameStart=0;
    char *encoded = NULL, *pos;

  if (c == NULL) {
    return NULL;
  } else {

    PRDirEntry* de = PR_ReadDir((PRDir*)c->pdata, (PRDirFlags)(c->count++));
    if (de == NULL) {

      PR_CloseDir((PRDir*)c->pdata);
	  c->pdata = NULL;
      return NULL;
	} else if ((resourceType((RDF_Resource)(c->u)) == IM_RT) || (endsWith(".snm", (char *)de->name)) || (endsWith(".sbd", (char *)de->name)))	{

/*
    } else if (endsWith(".snm", (char*)de->name) || (endsWith(".dat", (char*)de->name))){
*/
      char nname[512], *base;
      PRBool isDir = false, sep = (( resourceID(c->u))[strlen( resourceID(c->u))-1] == '/');
	

      base = NET_Escape(de->name, URL_PATH);
      if (base != NULL)	{
        if (sep) {
        sprintf(nname, "%s%s",  resourceID(c->u), base);
        } else
        sprintf(nname, "%s/%s",  resourceID(c->u), base);
        XP_FREE(base);
      }

       if (resourceType(c->u) == PM_RT)  {
           pathnameStart = POPMAIL_URL_OFFSET;
           encoded = unescapeURL(&nname[pathnameStart]);
         }
       else if (resourceType(c->u) == IM_RT)  {
           pathnameStart = IMAPMAIL_URL_OFFSET;
         }

        encoded = unescapeURL(&nname[pathnameStart]);
	if (encoded != NULL)	{

#ifdef  XP_WIN
            if (encoded[1] == '|') encoded[1] = ':';
#endif

	    PR_GetFileInfo(encoded, &fn);
		if (fn.type == PR_FILE_DIRECTORY)	{
			isDir = true;
 			len=strlen(nname);
			nname[len] = '/';
			nname[len+1] = '\0';
			}
		else if (resourceType(c->u) == IM_RT)	{
			if ((pos=strstr(encoded,"IMAPMail/")) != NULL)	{
				sprintf(nname,"imap://%s",pos+9);
				}
			}
		freeMem(encoded);
		}

      return CreatePMUnit(nname, resourceType(c->u), isDir);
    } else {
	return pmNextValue(rdf, c);
}
  }
}



RDF_Error
pmDisposeCursor (RDFT rdf, RDF_Cursor c)
{
  if (c != NULL) {

    if (c->pdata) PR_CloseDir((PRDir*)c->pdata);
    freeMem(c);
  }
  return 0;
}



RDF_Resource
CreatePMUnit (char* nname, RDF_BT rdfType, PRBool isDirectoryFlag)
{
  char *name;
  PRBool newName = 0;
  RDF_Resource existing;

  if (startsWith("mailbox:/", nname) || startsWith("imap:/", nname)) {
	if (endsWith(".snm", nname)) {
	  name = (char*) getMem(strlen(nname)+1);
	  memcpy(name, nname, strlen(nname));
	  name[strlen(name)-4] = '\0';
	  newName = 1;

      /* IMAP RFC specifies INBOX (all uppercase) ??? */
      if ((rdfType == IM_RT) && (endsWith ("/Inbox", name))) {
        strcpy(&name[strlen(name)-5],"INBOX");
        }

	} else  name = nname;
  } 
  else {
    if (rdfType == PM_RT)	{
      name = (char*) getMem(strlen(nname) + 9);
      memcpy(name,  "mailbox:/", 9);
      memcpy(&name[9], nname, strlen(nname));
      }
    else if (rdfType == IM_RT)	{
      name = (char*) getMem(strlen(nname) + 6);
      memcpy(name,  "imap:/", 6);
      memcpy(&name[6], nname, strlen(nname));
      }
    newName = 1;
    if (endsWith(".snm", nname)) {
      name[strlen(name)-3] = '\0';
    }
  }

  existing = RDF_GetResource(NULL, name, 0);
  if (existing != NULL) {
    if (newName) freeMem(name);
    if (isDirectoryFlag) setContainerp(existing, 1);
    return existing;
  } else {
    existing = RDF_GetResource(NULL, name, 1);
    if (isDirectoryFlag) setContainerp(existing, 1);
    setResourceType(existing, rdfType);
    if (newName) freeMem(name);
    return existing;
  }
}
