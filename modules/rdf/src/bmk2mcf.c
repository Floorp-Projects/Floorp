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

/* Reading bookmarks.htm into rdf.
   tags in the bookmark file.
   <TITLE>
   <H1>
   <H3>
   <DL></DL>
   <DT>
   <P>

  <DT> indicates that an item is coming. 
	   If the next item is an <a then we have a url
	   If the next item is a h3, we have a folder.
  <DL> indicates that the previous item (which should have been a folder)
	    is the parent of the next set.
  </DL> indicates pop out a level
  <P> ignore this on reading, but write out one after each <DL>
  <DD> the description for the previous <DT>

  Category urls. Make it up out of the add dates. */


/* 
   This file translates netscape bookmarks into the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/


#include "bmk2mcf.h"
#include "glue.h"
#include "mcff2mcf.h"
#include "utils.h"

#ifdef MOZILLA_CLIENT

	/* extern declarations */
PR_PUBLIC_API(void) HT_WriteOutAsBookmarks (RDF r, PRFileDesc *fp, RDF_Resource u);	/* XXX this should be elsewhere */
extern	char	*gBookmarkURL;
extern	RDF	gNCDB;

	/* globals */
uint16 separatorCounter = 0;
static char* gBkFolderDate;



RDF_Resource
createSeparator(void)
{
  char url[50];
  RDF_Resource sep;
  PR_snprintf(url, 50, "separator%i", separatorCounter++);
  sep = RDF_GetResource(NULL, url, 1);
  return sep;
}

#endif

RDF_Resource
createContainer (char* id)
{
  RDF_Resource r = RDF_GetResource(NULL, id, true);
  setContainerp(r, 1);
  return r;
}



#ifdef MOZILLA_CLIENT

int16 GetBmCharSetID();
int16 GetBmCharSetID()
{
   /* we need to implement this in a better way */
   static INTLCharSetID gBmCharSetID = CS_UNKNOWN;
   if(CS_UNKNOWN == gBmCharSetID)
	gBmCharSetID = INTL_GetCharSetID(INTL_OldBookmarkCsidSel);
   return gBmCharSetID;
}



char *
resourceDescription (RDF rdf, RDF_Resource r)
{
  return (char*)RDF_GetSlotValue(rdf, r, gWebData->RDF_description, RDF_STRING_TYPE, false, true);
}



char *
resourceLastVisitDate (RDF rdf, RDF_Resource r)
{
  return (char*)RDF_GetSlotValue(rdf, r, gWebData->RDF_lastVisitDate, RDF_STRING_TYPE, false, true);
}



char *
resourceLastModifiedDate (RDF rdf, RDF_Resource r)
{
  return (char*)RDF_GetSlotValue(rdf, r, gWebData->RDF_lastModifiedDate, RDF_STRING_TYPE, false, true);
}



void
parseNextBkBlob (RDFFile f, char* blob, int32 size)
{
  int32 n, last, m;
  PRBool somethingseenp = false;
  n = last = 0;
  
  while (n < size) {
    char c = blob[n];
    m = 0;
    somethingseenp = false;
    memset(f->line, '\0', f->lineSize);
    if (f->holdOver[0] != '\0') {
      memcpy(f->line, f->holdOver, strlen(f->holdOver));
      m = strlen(f->holdOver);
      somethingseenp = true;
      memset(f->holdOver, '\0', RDF_BUF_SIZE);
    }
    while ((m < 300) && (c != '<') && (c != '>') && (n < size)) {
      f->line[m] = c;
      m++;
      somethingseenp = (somethingseenp || ((c != ' ') && (c != '\r') && (c != '\n')));
      n++;
      c = blob[n];
    }
    if (c == '>') f->line[m] = c;
    n++;
    if (m > 0) {
      if ((c == '<') || (c == '>')) {
	last = n;
	if (c == '<') f->holdOver[0] = '<'; 
	if (somethingseenp == true) parseNextBkToken(f, f->line);
      } else if (size > last) {
	memcpy(f->holdOver, f->line, m);
      }
    } else if (c == '<') f->holdOver[0] = '<';
  }
}


 
void
parseNextBkToken (RDFFile f, char* token)
{
  int16 pos;

 /*	printf(token); */
  if (token[0] == '<') {
    bkStateTransition(f, token);
  } else {
    /* ok, we have a piece of content.
       can be the title, or a description or */
    if ((f->status == IN_TITLE) || (f->status == IN_H3) || 
	(f->status == IN_ITEM_TITLE)) {
      if (IN_H3 && gBkFolderDate) {
	char *url;
	RDF_Resource newFolder;
	url = PR_smprintf("%s%s.rdf", gBkFolderDate, token);
	newFolder = createContainer(url);
	XP_FREE(url);
	addSlotValue(f,newFolder, gCoreVocab->RDF_parent, f->stack[f->depth-1], 
		     RDF_RESOURCE_TYPE, NULL);
	freeMem(gBkFolderDate);
	gBkFolderDate = NULL;
	f->lastItem = newFolder;
      }
      if ((f->db == gLocalStore) || (f->status != IN_TITLE))
	{
	      addSlotValue(f, f->lastItem, gCoreVocab->RDF_name, 
			   convertString2UTF8(GetBmCharSetID(),token), RDF_STRING_TYPE, NULL);
	}
/*
      if (startsWith("Personal Toolbar", token) && (containerp(f->lastItem)))
	nlocalStoreAssert(gLocalStore, f->lastItem, gCoreVocab->RDF_instanceOf, 
			  gNavCenter->RDF_PersonalToolbarFolderCategory, 
			  RDF_RESOURCE_TYPE, true);
*/
    } else if (f->status == IN_ITEM_DESCRIPTION) {
      if ((pos = charSearch(0x0D, token)) >=0)
      {
      	token[pos] = '\0';
      }
      addDescription(f, f->lastItem, token);
    }
  }
}



void
addDescription (RDFFile f, RDF_Resource r, char* token)
{
  char* desc = (char*)  nlocalStoreGetSlotValue(gLocalStore, r, gWebData->RDF_description, 
				       RDF_STRING_TYPE, false, true);
  if (desc == NULL) {
    addSlotValue(f, f->lastItem, gWebData->RDF_description, convertString2UTF8(GetBmCharSetID(), token),
		 RDF_STRING_TYPE, NULL);
  } else {
   addSlotValue(f, f->lastItem, gWebData->RDF_description, 
		 convertString2UTF8AndAppend(GetBmCharSetID(),desc, token), RDF_STRING_TYPE, NULL); 
    nlocalStoreUnassert(gLocalStore, f->lastItem, gWebData->RDF_description, desc, RDF_STRING_TYPE);
  }
}



void
bkStateTransition (RDFFile f, char* token)
{
  if (startsWith("<A", token)) {
    newLeafBkItem(f, token);
    f->status = IN_ITEM_TITLE;
  } else if (startsWith(OPEN_H3_STRING, token)) {
    newFolderBkItem(f, token);
    f->status = IN_H3;
  } else if (startsWith(OPEN_TITLE_STRING, token)) {
    f->status = IN_TITLE;
  } else if (startsWith(OPEN_H3_STRING, token)) {
    f->status = IN_H3;
  } else if (startsWith(DD_STRING, token)) {
    if (nlocalStoreGetSlotValue(gLocalStore, f->lastItem, gWebData->RDF_description, 
				RDF_STRING_TYPE, false, true) 
		== NULL) f->status = IN_ITEM_DESCRIPTION;
  } else if (startsWith(OPEN_DL_STRING, token)) {
    f->stack[f->depth++] = f->lastItem;
  } else if (startsWith(CLOSE_DL_STRING, token)) {
    f->depth--;
  } else if (startsWith("<HR>", token)) {
    addSlotValue(f, createSeparator(), gCoreVocab->RDF_parent, f->stack[f->depth-1], 
		 RDF_RESOURCE_TYPE, NULL);
    f->status = 0;
  } else if ((f->status == IN_ITEM_DESCRIPTION) && (startsWith("<BR>", token))) {
    addDescription(f, f->lastItem, token);
  } else f->status = 0;
}



void
newFolderBkItem(RDFFile f, char* token)
{
  int16 start, end;
  start = charSearch('"', token);
  end   = revCharSearch('"', token);
  token[end] = '\0';
  gBkFolderDate = copyString(&token[start+1]);
}



void
newLeafBkItem (RDFFile f, char* token)
{
  char			buffer[128];
  struct tm		*time;
  uint32		dateVal;
  char* url = NULL;
  char* addDate = NULL;
  char* lastVisit = NULL;
  char* lastModified = NULL;
  int32 len = strlen(token); 
  int32 n = 0, cmdIndex = 0;
  char c = token[n++];
  PRBool inString = false;
  RDF_Resource newR;
  
  while (n < len) {
    if ((inString == false) && ((c == ' ') || (c == '\t'))) {
      cmdIndex = n;
    }
    if (c == '"') {
      if (inString) {
	token[n-1] = '\0';
	inString = false;
      } else {
	inString = true;
	if (startsWith("HREF=", &token[cmdIndex])) {
	  url = &token[n];
	  }
	else if (startsWith("ADD_DATE=", &token[cmdIndex])) {
	  addDate = &token[n];
	  }
	else if (startsWith("LAST_VISIT=", &token[cmdIndex])) {
	  lastVisit = &token[n];
	  }
	else if (startsWith("LAST_MODIFIED=", &token[cmdIndex])) {
	  lastModified = &token[n];
	  }
	}
      }
    c = token[n++];
  }
  if (url == NULL) return;
  newR = RDF_GetResource(NULL, url, true);
  addSlotValue(f, newR, gCoreVocab->RDF_parent, f->stack[f->depth-1],  
	       RDF_RESOURCE_TYPE, NULL);
  /* addSlotValue(f, newR, gWebData->RDF_URL, (void*)copyString(url), 
	       RDF_STRING_TYPE, true); */
  if (addDate != NULL)
    {
	dateVal = atol(addDate);
	if ((time = localtime((time_t *) &dateVal)) != NULL)
	{
#ifdef	XP_MAC
		time->tm_year += 4;
		strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_MACDATE),time);
#elif	XP_UNIX
		strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_MACDATE),time);
#else
		strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_WINDATE),time);
#endif
		addSlotValue(f, newR, gNavCenter->RDF_bookmarkAddDate,
			(void*)copyString(buffer), RDF_STRING_TYPE, NULL);
	}
    }
  if (lastVisit != NULL)
    {
	dateVal = atol(lastVisit);
	if ((time = localtime((time_t *) &dateVal)) != NULL)
	{
#ifdef	XP_MAC
		time->tm_year += 4;
		strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_MACDATE),time);
#elif	XP_UNIX
		strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_MACDATE),time);
#else
		strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_WINDATE),time);
#endif
		addSlotValue(f, newR, gWebData->RDF_lastVisitDate,
			(void*)copyString(buffer), RDF_STRING_TYPE, NULL);
	}
    }
  if (lastModified != NULL)
    {
	dateVal = atol(lastModified);
	if ((time = localtime((time_t *) &dateVal)) != NULL)
	{
#ifdef	XP_MAC
		time->tm_year += 4;
		strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_MACDATE),time);
#elif	XP_UNIX
		strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_MACDATE),time);
#else
		strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_WINDATE),time);
#endif
		addSlotValue(f, newR, gWebData->RDF_lastModifiedDate,
			(void*)copyString(buffer), RDF_STRING_TYPE, NULL);
	}
    }
  f->lastItem = newR;
}



char *
numericDate(char *url)
{
	char		*date = NULL;
	int		len=0;

	if (!url) return NULL;
	while (url[len])
	{
		if (!isdigit(url[len]))	break;
		++len;
	}
	if (len > 0)
	{
		if ((date = getMem(len+1)) != NULL)
		{
			strncpy(date, url, len);
		}
	}
	return(date);
}



PRBool
bookmarkSlotp (RDF_Resource s)
{
  return ((s == gCoreVocab->RDF_parent) || (s == gWebData->RDF_lastVisitDate) || (s == gWebData->RDF_description) ||
	  (s == gNavCenter->RDF_bookmarkAddDate) || (s == gWebData->RDF_lastModifiedDate) || 
	  (s == gCoreVocab->RDF_name));
}


void
HT_WriteOutAsBookmarks1 (RDF rdf, PRFileDesc *fp, RDF_Resource u, RDF_Resource top, int indent)
{
    RDF_Cursor c = RDF_GetSources(rdf, u, gCoreVocab->RDF_parent, RDF_RESOURCE_TYPE, true);
    RDF_Resource next;
    char *date, *name, *url;
    int loop;

    if (c == NULL) return;
    if (u == top) {
      name = RDF_GetResourceName(rdf, u);
      ht_rjcprintf(fp, "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n", NULL);
      ht_rjcprintf(fp, "<!-- This is an automatically generated file.\n", NULL);
      ht_rjcprintf(fp, "It will be read and overwritten.\n", NULL);
      ht_rjcprintf(fp, "Do Not Edit! -->\n", NULL);

      ht_rjcprintf(fp, "<TITLE>%s</TITLE>\n", (name) ? name:"");
      ht_rjcprintf(fp, "<H1>%s</H1>\n<DL><p>\n", (name) ? name:"");
    }
    while ((next = RDF_NextValue(c)) != NULL) {

      url = resourceID(next);
      if (containerp(next) && (!startsWith("ftp:",url)) && (!startsWith("file:",url))
	    && (!startsWith("IMAP:", url)) && (!startsWith("nes:", url))
	    && (!startsWith("mail:", url)) && (!startsWith("cache:", url))
	    && (!startsWith("ldap:", url))) {
		for (loop=0; loop<indent; loop++)	ht_rjcprintf(fp, "    ", NULL);

		date = numericDate(resourceID(next));
		ht_rjcprintf(fp, "<DT><H3 ADD_DATE=\"%s\">", (date) ? date:"");
		if (date) freeMem(date);
		name = RDF_GetResourceName(rdf, next);
		ht_rjcprintf(fp, "%s</H3>\n", name);

		for (loop=0; loop<indent; loop++)	ht_rjcprintf(fp, "    ", NULL);
		ht_rjcprintf(fp, "<DL><p>\n", NULL);
		HT_WriteOutAsBookmarks1(rdf, fp, next, top, indent+1);

		for (loop=0; loop<indent; loop++)	ht_rjcprintf(fp, "    ", NULL);

		ht_rjcprintf(fp, "</DL><p>\n", NULL);
      }
      else if (isSeparator(next)) {
	for (loop=0; loop<indent; loop++)	ht_rjcprintf(fp, "    ", NULL);
	ht_rjcprintf(fp, "<HR>\n", NULL);
      }
      else {
	char* bkAddDate = (char*)RDF_GetSlotValue(rdf, next, 
						  gNavCenter->RDF_bookmarkAddDate, 
						  RDF_STRING_TYPE, false, true);

        for (loop=0; loop<indent; loop++)	ht_rjcprintf(fp, "    ", NULL);

	ht_rjcprintf(fp, "<DT><A HREF=\"%s\" ", resourceID(next));
	date = numericDate(bkAddDate);
	ht_rjcprintf(fp, "ADD_DATE=\"%s\" ", (date) ? date: "");
	if (date) freeMem(date);
	ht_rjcprintf(fp, "LAST_VISIT=\"%s\" ", resourceLastVisitDate(rdf, next));
	ht_rjcprintf(fp, "LAST_MODIFIED=\"%s\">", resourceLastModifiedDate(rdf, next));
	ht_rjcprintf(fp, "%s</A>\n", RDF_GetResourceName(rdf, next));

	if (resourceDescription(rdf, next) != NULL) {
	  ht_rjcprintf(fp, "<DD>%s\n", resourceDescription(rdf, next));
	}
      }
    }
    RDF_DisposeCursor(c);
    if (u == top) {
      ht_rjcprintf(fp, "</DL>\n", NULL);
    }
}



PR_PUBLIC_API(void)
HT_WriteOutAsBookmarks (RDF r, PRFileDesc *fp, RDF_Resource u)
{
	HT_WriteOutAsBookmarks1 (r, fp, u, u, 1);
}



void
flushBookmarks()
{
	PRFileDesc		*bkfp;

	if (gBookmarkURL != NULL)
	{
		/*
		  delete bookmark.htm as PROpen() with PR_TRUNCATE appears broken (at least on Mac)
		*/
		CallPRDeleteFileUsingFileURL(gBookmarkURL);

		if ((bkfp = CallPROpenUsingFileURL(gBookmarkURL, (PR_WRONLY|PR_CREATE_FILE|PR_TRUNCATE),
						0644)) != NULL)
		{
			HT_WriteOutAsBookmarks(gNCDB, bkfp, gNavCenter->RDF_BookmarkFolderCategory);
			PR_Close(bkfp);
		}
	}
}

#endif
