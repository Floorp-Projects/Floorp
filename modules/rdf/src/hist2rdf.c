/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   This file implements History support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "hist2rdf.h"
#include "remstore.h"
#include "utils.h"

static PRBool histInFlatFilep = 0;

	/* extern declarations */
void GH_DeleteHistoryItem (char * url);
PR_PUBLIC_API(void) updateNewHistItem (DBT *key, DBT *data);	/* XXX this should be elsewhere */


	/* externs */
extern	char	*profileDirURL;
extern	char	*gGlobalHistoryURL;


	/* external string references in allxpstr */
extern	int	RDF_WEEKOF, RDF_WITHINLASTHOUR, RDF_TODAY, RDF_DAY_0;
extern	int	RDF_RELATEDLINKSNAME;


	/* globals */
PLHashTable     *hostHash = 0;
RDFT grdf = NULL;
RDFT gHistoryStore = 0;
PRBool ByDateOpened = 0;
PRBool historyInitialized = 0;
static int saveCount = 0;


char    *prefixList[] = {
	"about:", "javascript:", "livescript:", "mailbox:", "mailto:",
	"mocha:", "news:", "pop3", "snews:", "view-source", "wysiwyg:", NULL
	};

char    *suffixList[] = {
	".gif", ".jpg", ".jpeg", ".xbm", ".pfr", ".class", ".tmp", ".js", ".rdf", 
	".mcf", ".mco", NULL
	};

char   *intermediateList[] = {
        ".gif", ".GIF",  NULL
	};



void
collateHistory (RDFT r, RDF_Resource u, PRBool byDateFlag)
{
  HASHINFO hash = { 4*1024, 0, 0, 0, 0, 0};
  DBT key, data;
  time_t	last,first;
  uint32	numaccess;
  PRBool firstOne = 0;
  DB* db = CallDBOpenUsingFileURL(gGlobalHistoryURL,  O_RDONLY ,0600,
                                  DB_HASH, &hash);
  grdf = r;
  if (db != NULL)	{
    while (0 ==   (*db->seq)(db, &key, &data, (firstOne ? R_NEXT : R_FIRST))) {
      char* title =    ((char*)data.data + 16);                /* title */
      char* url =      (char*)key.data;                       /* url */
      /* int32 flag = (int32)*((char*)data.data + 3*sizeof(int32)); */
      int32 flag;
      HIST_COPY_INT32(&flag, (int8 *)data.data + 3 * sizeof(int32));
      firstOne = 1;
#ifdef XP_UNIX
      if ((/*1 == flag &&*/ displayHistoryItem((char*)key.data))) {
#else
      if (1 == flag && displayHistoryItem((char*)key.data)) {
#endif
	HIST_COPY_INT32(&last, (time_t *)((int8 *)data.data));
	HIST_COPY_INT32(&first, (time_t *)((int8 *)data.data + sizeof(int32)));
	HIST_COPY_INT32(&numaccess, (time_t *)((int8 *)data.data + 2*sizeof(int32)));

	collateOneHist(r, u,url,title, last, first, numaccess, byDateFlag);
      }
    }
    (*db->close)(db);
  }
}



void
collateOneHist (RDFT r, RDF_Resource u, char* url, char* title, time_t lastAccessDate,
		time_t firstAccessDate, uint32 numAccesses, PRBool byDateFlag)
{
  char			buffer[128];
  struct tm		*time;
  RDF_Resource		hostUnit, urlUnit;
  char* existingName = NULL;
  uint32      oldNumAccess = 0;
  if (startsWith("404", title)) return;
  urlUnit  = HistCreate(url, 1);
  existingName = nlocalStoreGetSlotValue(gLocalStore, urlUnit, gCoreVocab->RDF_name, RDF_STRING_TYPE, 0, 1);

  XP_ASSERT( IsUTF8String((const char* )existingName));

  if (existingName == NULL) {
  if (title[0] != '\0')  remoteAddName(urlUnit, title);	  
  } else freeMem(existingName);
  if (byDateFlag) {
    hostUnit = hostUnitOfDate(r, u, lastAccessDate);
  } else {
    hostUnit = hostUnitOfURL(r, u, urlUnit, title); 
  }
  
  if (hostUnit == NULL) return;

  
  if (hostUnit != urlUnit) remoteAddParent(urlUnit, hostUnit);
  
  if ((time = localtime((time_t *) &lastAccessDate)) != NULL)
  {
#ifdef	XP_MAC
	time->tm_year += 4;
	strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_MACDATE),time);
#elif	XP_UNIX
	strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_MACDATE),time);
#else
	strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_WINDATE),time);
#endif
  	XP_ASSERT( IsUTF8String((const char* )buffer));
	remoteStoreAdd(gRemoteStore, urlUnit, gWebData->RDF_lastVisitDate,
		 (void *)copyString(buffer), RDF_STRING_TYPE, 1);
  }
  if ((time = localtime((time_t *) &firstAccessDate)) != NULL)
  {
#ifdef	XP_MAC
	time->tm_year += 4;
	strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_MACDATE),time);
#elif	XP_UNIX
	strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_MACDATE),time);
#else
	strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_WINDATE),time);
#endif
  	XP_ASSERT( IsUTF8String((const char* )buffer));
	remoteStoreAdd(gRemoteStore, urlUnit, gWebData->RDF_firstVisitDate,
		 (void *)copyString(buffer), RDF_STRING_TYPE, 1);
  }
  /* oldNumAccess = remoteStoreGetSlotValue(gHistoryStore, urlUnit, gWebData->RDF_numAccesses,
                                         RDF_INT_TYPE, 0, 1);
  if (oldNumAccess) remoteStoreRemove(gHistoryStore, urlUnit, gWebData->RDF_numAccesses, oldNumAccess,
                                      RDF_INT_TYPE);
  if (numAccesses==0)	++numAccesses;
  remoteStoreAdd(gHistoryStore, urlUnit, gWebData->RDF_numAccesses,
		 (void *)numAccesses, RDF_INT_TYPE, 1); */
  if (numAccesses > 5) 	histAddParent(urlUnit, gNavCenter->RDF_HistoryMostVisited);
}



RDF_Resource
hostUnitOfURL (RDFT r, RDF_Resource top, RDF_Resource nu, char* title)
{
  char host[100];
  char* url =  resourceID(nu);
  int16 s1, s2, s3;
  RDF_Resource hostResource, existing;
  if (strlen(url) > 100) return NULL;
  if (startsWith("file", url)) {
    return RDF_GetResource(NULL, "Local Files", 1);
  } else { 
    memset(host, '\0', 100);
    s1 = charSearch(':', url)+3;
    s2 = charSearch('/', &url[s1]);
    s3 = charSearch(':', &url[s1]);
    if (s2 == -1) s2 = strlen(url)-s1;
    if ((s3 != -1) && (s2 > s3)) s2 = s3;
    if (startsWith("www", &url[s1])) {s1 = s1+4; s2 = s2-4;}
    if (s2<1) return(NULL);
    memcpy((char*)host, &url[s1], s2);
    host[0] = toupper(host[0]);
    hostResource = RDF_GetResource(NULL, host, 1);
    setContainerp(hostResource, 1);
    setResourceType(hostResource, HISTORY_RT);
    existing = PL_HashTableLookup(hostHash, hostResource);
    if (existing != NULL) {
      if (existing == nu) {
	return existing;
      } else if (existing == top) {
	return hostResource;
      } else {
	remoteStoreRemove(gHistoryStore, existing, gCoreVocab->RDF_parent, top, RDF_RESOURCE_TYPE);
	histAddParent(existing, hostResource);
	histAddParent(hostResource, top);
	PL_HashTableAdd(hostHash, hostResource, top);
	return hostResource;
      }
    } else {
	PL_HashTableAdd(hostHash, hostResource, nu);
	histAddParent(nu, top);
	return nu;
    }
  }
}



void
hourRange(char *buffer, struct tm *theTm)
{
	char		*startHourStr="AM",*endHourStr="AM";

	if (theTm->tm_hour > 0 && theTm->tm_hour < 12) {
	  sprintf(buffer, "AM");
	} else  {
	  sprintf(buffer, "PM");
	} 
}



RDF_Resource
hostUnitOfDate (RDFT r, RDF_Resource u, time_t lastAccessDate)
{
	RDF_Resource		node = NULL, parentNode;
/*
	RDF_Resources		std;
*/
	time_t			now, tempTime;
	struct tm		*T,nowStruct, dateStruct;
	char			hourBuffer[128], dayBuffer[128], weekBuffer[128];
	char			bigBuffer[1024];
	int			numDays;

	/* NOTE: localtime uses a single static buffer, so MUST copy contents out */

	time((time_t *)&now);
	if ((T = localtime(&now)) == NULL)	return(NULL);
	nowStruct = *T;

	if ((T = localtime(&lastAccessDate)) == NULL)	return(NULL);
	dateStruct = *T;

	bigBuffer[0] = hourBuffer[0] = dayBuffer[0] = weekBuffer[0] = 0;

	if (now < (lastAccessDate + SECS_IN_HOUR))		/* within the last hour */
	{
		strcpy(hourBuffer, XP_GetString(RDF_WITHINLASTHOUR));
	}
	else if ((nowStruct.tm_year == dateStruct.tm_year) &&	/* some time today */
		(nowStruct.tm_yday == dateStruct.tm_yday))
	{
		strcpy(dayBuffer, XP_GetString(RDF_TODAY));
		hourRange(hourBuffer, &dateStruct);
	}
	else							/* check if within last week */
	{
		numDays = 7;
		do
		{
			now -= SECS_IN_DAY;
			if ((T = localtime(&now)) == NULL)	return(NULL);
			nowStruct = *T;
			if ((nowStruct.tm_year == dateStruct.tm_year) &&
				(nowStruct.tm_yday == dateStruct.tm_yday))
			{
				sprintf(dayBuffer, "%d/%d - %s",
					(uint) dateStruct.tm_mon + 1,	(uint)dateStruct.tm_mday,
					XP_GetString(RDF_DAY_0 + nowStruct.tm_wday));
				hourRange(hourBuffer, &dateStruct);
				break;
			}
		} while (numDays-- > 0);

		if (dayBuffer[0] == '\0')				/* older than a week */
		{
			tempTime = lastAccessDate;
			numDays = dateStruct.tm_wday;
			while (numDays-- > 0)
			{
				tempTime -= SECS_IN_DAY;
			}
			if ((T = localtime(&tempTime)) == NULL)	return(NULL);
			dateStruct = *T;
#ifdef	XP_MAC
			/* Mac epoch according to localtime is 1904 */
			PR_snprintf(weekBuffer, sizeof(weekBuffer)-1, XP_GetString(RDF_WEEKOF), (uint)dateStruct.tm_mon + 1,
				(int)dateStruct.tm_mday, (uint)dateStruct.tm_year + 1904);
#else
			PR_snprintf(weekBuffer, sizeof(weekBuffer)-1, XP_GetString(RDF_WEEKOF), (uint)dateStruct.tm_mon + 1,
				(int)dateStruct.tm_mday, (uint)dateStruct.tm_year + 1900);
#endif

			if ((T = localtime(&lastAccessDate)) == NULL)	return(NULL);
			dateStruct = *T;

#ifdef	XP_MAC
			/* Mac epoch according to localtime is 1904 */
			sprintf(dayBuffer, "%s - %d/%d/%d",
				XP_GetString(RDF_DAY_0 + dateStruct.tm_wday), (uint) dateStruct.tm_mon + 1,
				(uint)dateStruct.tm_mday, (uint)dateStruct.tm_year + 1904);
#else
			sprintf(dayBuffer, "%s - %d/%d/%d",
				XP_GetString(RDF_DAY_0 + dateStruct.tm_wday), (uint) dateStruct.tm_mon + 1,
				(uint)dateStruct.tm_mday, (uint)dateStruct.tm_year + 1900);
#endif
			hourRange(hourBuffer, &dateStruct);
		}
	}
	parentNode = u;

	if (weekBuffer[0] != '\0')
	{
		if ((node = RDF_GetResource(NULL, weekBuffer, false)) == NULL)
		{
			if ((node = RDF_GetResource(NULL, weekBuffer, true)) == NULL)	return(NULL);
		}
		setContainerp(node, 1);
		setResourceType(node, HISTORY_RT);
		remoteStoreAdd(gHistoryStore, node, gCoreVocab->RDF_parent,
				parentNode, RDF_RESOURCE_TYPE, 1);
  		XP_ASSERT( IsUTF8String((const char* )weekBuffer));
		remoteStoreAdd(gHistoryStore, node, gCoreVocab->RDF_name,
				copyString(weekBuffer), RDF_STRING_TYPE, 1);
		parentNode = node;
	}
	if (dayBuffer[0] != '\0')
	{
	  sprintf(bigBuffer, "%d/%d",  (uint) dateStruct.tm_mon + 1,
		  (uint)dateStruct.tm_mday);


		if ((node = RDF_GetResource(NULL, bigBuffer, false)) == NULL)
		{
			if ((node = RDF_GetResource(NULL, bigBuffer, true)) == NULL)	return(NULL);
		}
		setContainerp(node, 1);
		setResourceType(node, HISTORY_RT);
		histAddParent(node, parentNode);
		sprintf(bigBuffer,"%s - %s",weekBuffer,dayBuffer);
  		XP_ASSERT( IsUTF8String((const char* )dayBuffer));
		remoteStoreAdd(gHistoryStore, node, gCoreVocab->RDF_name,
				copyString(dayBuffer), RDF_STRING_TYPE, 1);
		parentNode = node;
	}
	if (hourBuffer[0] == 'W')
	{
		sprintf(bigBuffer, "%s", hourBuffer);

		if ((node = RDF_GetResource(NULL, bigBuffer, false)) == NULL)
		{
			if ((node = RDF_GetResource(NULL, bigBuffer, true)) == NULL)	return(NULL);
		}
		setContainerp(node, 1);
		setResourceType(node, HISTORY_RT);
		remoteStoreAdd(gHistoryStore, node, gCoreVocab->RDF_parent,
				parentNode, RDF_RESOURCE_TYPE, 1);
  		XP_ASSERT( IsUTF8String((const char* )hourBuffer));
		remoteStoreAdd(gHistoryStore, node, gCoreVocab->RDF_name,
				copyString(hourBuffer), RDF_STRING_TYPE, 1);
		parentNode = node;
	}
	return (node);
}



void
saveHistory ()
{
  char* escapedPath = unescapeURL(gGlobalHistoryURL);
  char* path = WH_FilePlatformName(convertFileURLToNSPRCopaceticPath(escapedPath));
  PRFileDesc* file = PR_Open(path,  PR_WRONLY | PR_CREATE_FILE, 00200);
  char* hist;
  if (file != NULL) {
    hist = RDF_SerializeRDFStore(gHistoryStore) ;
    if (hist != NULL) {
      PR_Write(file, hist, strlen(hist));
    }
  }
  freeMem(path);
  freeMem(escapedPath);
  PR_Close(file);
}



PR_PUBLIC_API(void)
updateNewHistItem (DBT *key, DBT *data)
{
  time_t	last,first,numaccess;
  int32 flg = (int32)*((char*)data->data + 3*sizeof(int32));
  if (!displayHistoryItem((char*)key->data)) return;
  if (historyInitialized && (gHistoryStore != NULL)) {
    HIST_COPY_INT32(&last, (time_t *)((char *)data->data));
    HIST_COPY_INT32(&first, (time_t *)((char *)data->data + sizeof(int32)));
    HIST_COPY_INT32(&numaccess, (time_t *)((char *)data->data + 2*sizeof(int32)));
    
    if (hostHash) collateOneHist(grdf, gNavCenter->RDF_HistoryBySite, 
				 (char*)key->data,                        /* url */
				 ((char*)data->data + 4*sizeof(int32)),   /* title */
				 last, first, numaccess, 0);
    if (ByDateOpened) collateOneHist(grdf, gNavCenter->RDF_HistoryByDate, 
				     (char*)key->data,                        /* url */
				     ((char*)data->data + 4*sizeof(int32)),   /* title */
				     last, first, numaccess, 1);
    saveCount++;
    if (saveCount > 5) {
      if (histInFlatFilep) saveHistory();
      saveCount = 0;
    }
  }

}



/** History clustering utils **/



PRBool
displayHistoryItem (char* url)
{
  int			x;
  x=0;
  while (prefixList[x]) {
    if (startsWith(prefixList[x++], url))   return 0;
  }
  x=0;
  while (suffixList[x]) {
    if (endsWith(suffixList[x++], url))     return 0;
  }
  x=0;
  while (intermediateList[x]) {
    if (strstr(url, intermediateList[x++])) return 0;
  }
  return 1;
}



RDF_Resource
HistCreate (char* url, PRBool createp)
{
  size_t size = strlen(url);
  char* nurl = getMem(size+8);
  RDF_Resource ans;
  if (charSearch(':', url) == -1) {    
    if (url[size-1] == '/') {
      sprintf(nurl, "http://%s", url);
      nurl[strlen(nurl)-1] = '\0';
    } else {
    sprintf(nurl, "http://%s/", url);
    }
  } else {
    if (url[size-1] == '/') {
      memcpy(nurl, url, size-1);
    } else {
      sprintf(nurl, "%s/", url);
    }
  }
  ans = RDF_GetResource(NULL, nurl, 0);
  if (ans != NULL) {
    freeMem(nurl);
    return ans;
  } 
  freeMem(nurl);
  ans = RDF_GetResource(NULL, url, createp);

  if (ans != NULL) {
    return ans;
  } else {
    return NULL;
  }
}

PRBool bySite = 0;

Assertion
histAddParent (RDF_Resource child, RDF_Resource parent)
{
  Assertion nextAs, prevAs, newAs; 
  RDF_Resource s = gCoreVocab->RDF_parent;
  RDF_ValueType type = RDF_RESOURCE_TYPE;
  nextAs = prevAs = child->rarg1;
  while (nextAs != null) {
    if (asEqual(gHistoryStore, nextAs, child, s, parent, type)) return null;
    prevAs = nextAs;
    nextAs = nextAs->next;
  }
  newAs = makeNewAssertion(gHistoryStore, child, s, parent, type, 1);
  if (prevAs == null) {
    child->rarg1 = newAs;
  } else {
    prevAs->next = newAs;
  }
  nextAs = prevAs = parent->rarg2;
  if (nextAs == NULL) {
    parent->rarg2 = newAs;
  } else {
    PRBool added = 0;
    if (bySite) {
      while (nextAs && !isSeparator(nextAs->u)) {
        prevAs = nextAs;
        nextAs = nextAs->invNext;
      }
	  if (nextAs) {
		prevAs = nextAs;
		nextAs = nextAs->invNext;
	  }
    }
    while (nextAs != null) {
		char* nid =  resourceID(nextAs->u);
        if (strcmp(resourceID(child),  resourceID(nextAs->u)) > 0) {
          if (prevAs == nextAs) {
            newAs->invNext = prevAs;
            parent->rarg2  = newAs;
            added = 1;
            break;
          } else {
            newAs->invNext = nextAs;	    
            prevAs->invNext = newAs;
            added = 1;
            break;
          }
        } 
        prevAs = nextAs;
        nextAs = nextAs->invNext;
    }
    if (!added) prevAs->invNext = newAs;
  }
  sendNotifications2(gHistoryStore, RDF_ASSERT_NOTIFY, child, s, parent, type, 1);
  /* XXX have to mark the entire subtree XXX */
  /*  sendNotifications(gHistoryStore->rdf, RDF_ASSERT_NOTIFY, child, s, parent, type, 1); */
  return(newAs);
}



PRBool
historyUnassert (RDFT hst,  RDF_Resource u, RDF_Resource s, void* v, 
		       RDF_ValueType type)
{

  XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char* )v)));
  if ((type == RDF_RESOURCE_TYPE) && (resourceType((RDF_Resource)v) == HISTORY_RT) &&
      (s == gCoreVocab->RDF_parent)) {
    RDF_Resource parents[5];
    int8 n = 0;
    Assertion as = u->rarg1;
    memset(parents, '\0', 5 * sizeof(RDF_Resource));
    while (as) {
      if ((as->type == RDF_RESOURCE_TYPE) && (as->s == gCoreVocab->RDF_parent) && 
	  (resourceType((RDF_Resource)as->value) == HISTORY_RT) && (n < 5)) {
	parents[n++] = (RDF_Resource)as->value;
      }
      as = as->next;
    }
    if (containerp(u)) {
      Assertion as = u->rarg2;
      while (as) {
        if ((as->db == gHistoryStore) && (as->s == gCoreVocab->RDF_parent)) {
          GH_DeleteHistoryItem (resourceID(as->u));          
        } 
        as = as->invNext;
      }
    } else {
      GH_DeleteHistoryItem (resourceID(u));
    }
      
    while (n > 0) {
      n = n - 1;
      if (parents[n]) {
        Assertion nas = remoteStoreRemove (gHistoryStore, u, gCoreVocab->RDF_parent, 
                                           parents[n], RDF_RESOURCE_TYPE);
        freeMem(nas);
      } 
    }
    return 1;
  }
  return 0;
}




void
HistPossiblyAccessFile (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep)
{
  if ((s ==  gCoreVocab->RDF_parent) && inversep && (rdf == gHistoryStore) &&
      ((u == gNavCenter->RDF_HistoryByDate) ||  (u == gNavCenter->RDF_HistoryBySite))) {
    if (histInFlatFilep) {
      readRDFFile(gGlobalHistoryURL, NULL, 0, gHistoryStore);
      historyInitialized = 1;
    } else {
      collateHistory(rdf, u, (u == gNavCenter->RDF_HistoryByDate)); 
      historyInitialized = 1;
    }
  } else if ((s ==  gCoreVocab->RDF_parent) && inversep && (rdf == gHistoryStore) &&
             (u == gNavCenter->RDF_HistoryMostVisited)) {
      collateHistory(rdf, gNavCenter->RDF_HistoryBySite, 0); 
      historyInitialized = 1;
  }

}

RDFT
MakeHistoryStore (char* url)
{
  if (startsWith("rdf:history", url)) {
    if (gHistoryStore == 0) {
      RDFT ntr = (RDFT)getMem(sizeof(struct RDF_TranslatorStruct));
      RDF_Resource sep = createSeparator();
      ntr->assert = NULL;
      ntr->unassert = historyUnassert;
      ntr->getSlotValue = remoteStoreGetSlotValue;
      ntr->getSlotValues = remoteStoreGetSlotValues;
      ntr->hasAssertion = remoteStoreHasAssertion;
      ntr->nextValue = remoteStoreNextValue;
      ntr->disposeCursor = remoteStoreDisposeCursor;
      ntr->possiblyAccessFile = HistPossiblyAccessFile;
      gHistoryStore = ntr;
      histInFlatFilep = endsWith(".rdf", gGlobalHistoryURL);
      ntr->url = copyString(url);
      hostHash = PL_NewHashTable(500, idenHash, PL_CompareValues,  PL_CompareValues,  null, null);
      ByDateOpened = 1;
      return ntr;
    } else return gHistoryStore;
  } else return NULL;
}



/*
void
dumpHist ()
{
  FILE *fp = fopen("history.txt", "w");
  HASHINFO hash = { 4*1024, 0, 0, 0, 0, 0};
  DBT key, data;
  PRBool firstOne = 0;
  DB* db = CallDBOpenUsingFileURL(gGlobalHistoryURL,  O_RDONLY ,0600,
                                  DB_HASH, &hash);

  if (db != NULL)	{
    while (0 ==   (*db->seq)(db, &key, &data, (firstOne ? R_NEXT : R_FIRST))) {
      firstOne = 1;
      if ((1 == (int32)*((char*)data.data + 3*sizeof(uint32)) && 
	   displayHistoryItem((char*)key.data))) {
	fprintf(fp, "%s\n", (char*)key.data);
      }
    }
  }
  fclose(fp);
  (*db->close)(db);
}

*/
