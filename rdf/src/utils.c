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
   This file implements utility routines for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/


#include "utils.h"
#include "vocabint.h"


	/* globals */
PRBool rdfDBInited = 0;
PLHashTable*  resourceHash = 0;
PLHashTable*  dataSourceHash = 0;
RDFT gRemoteStore = 0;
RDFT gSessionDB = 0;

	/* externs */
extern	char	*profileDirURL;

#define INITIAL_RESOURCE_LIST_SIZE 50
#define INITIAL_ASSERTION_LIST_SIZE 100
#define GROW_LIST_INCR 50



int
compareStrings(char *s1, char *s2)
{
   return RDF_STRCASECMP(s1, s2);
}


char *
makeRDFDBURL(char* directory, char* name)
{
  char		*ans;
  size_t		s;
  
  if (profileDirURL == NULL) return NULL;
  if ((ans = (char*) getMem(RDF_STRLEN(profileDirURL) + strlen(directory) + strlen(name) + 3)) != NULL) {
    s = RDF_STRLEN(profileDirURL);
    memcpy(ans, profileDirURL, s);
    if (ans[s-1] != '/') {
      ans[s++] = '/';
    }
    stringAppend(ans, directory);
    stringAppend(ans, "/");
    stringAppend(ans, name);
  }
  return(ans);
}


RDF_Resource
getMCFFrtop (char* furl)
{
  char* url = getBaseURL(furl);
  RDF_Resource r;
  r = RDF_GetResource(NULL, url, 1);
  freeMem(url);
  return r;
}

char*
copyString (const char* url) {
  int32 len = RDF_STRLEN(url);
  char* newStr = (char*)getMem(len+1);
  if (newStr != NULL) {
    memcpy(newStr, url, len);
  } 
  return newStr;
}



RDFFile
makeRDFFile (char* url, RDF_Resource top, PRBool localp)
{
  RDFFile ans = (RDFFile)getMem(sizeof(struct RDF_FileStruct));
  /*  ans->rdf = rdf; */
  ans->url = getBaseURL(url);
  ans->top = top;
  ans->localp = localp;
  if (!localp)  ans->refreshingp = PR_TRUE;
  initRDFFile(ans);
  return ans;
}



void
initRDFFile (RDFFile ans)
{
  char* url = ans->url;
  ans->rtop = getMCFFrtop(url);
  ans->line = (char*)getMem(RDF_BUF_SIZE);
  ans->currentSlot = (char*)getMem(100);
  ans->resourceList = (RDF_Resource*)getMem(INITIAL_RESOURCE_LIST_SIZE * sizeof(RDF_Resource));
  ans->assertionList = (Assertion*)getMem(INITIAL_ASSERTION_LIST_SIZE * sizeof(Assertion));
  ans->resourceListSize = INITIAL_RESOURCE_LIST_SIZE;
  ans->assertionListSize = INITIAL_ASSERTION_LIST_SIZE;
  ans->holdOver = (char*)getMem(RDF_BUF_SIZE);
  ans->depth = 1;
  ans->lastItem = ans->stack[0] = ans->top;
  ans->locked = ans->localp;
  ans->lineSize = LINE_SIZE;
  ans->tv = true;
  ans->resourceCount = 0;
  ans->assertionCount = 0;
}


void
addToResourceList (RDFFile f, RDF_Resource u)
{
  if (f->resourceListSize == f->resourceCount) {
    RDF_Resource* newResourceList = (RDF_Resource*)getMem(sizeof(RDF_Resource)*(f->resourceListSize + GROW_LIST_INCR));
    RDF_Resource* old = f->resourceList;
    memcpy((char*)newResourceList, (char*)f->resourceList, sizeof(RDF_Resource)*f->resourceListSize);
    f->resourceList = newResourceList;
    f->resourceListSize = f->resourceListSize + GROW_LIST_INCR;
    freeMem(old);
  }
  *(f->resourceList + f->resourceCount++) = u;
}



void
addToAssertionList (RDFFile f, Assertion as)
{
  if (f->assertionListSize == f->assertionCount) {
    Assertion* newAssertionList = (Assertion*)getMem(sizeof(RDF_Resource)*(f->assertionListSize + GROW_LIST_INCR));
    Assertion* old = f->assertionList;
    memcpy((char*)newAssertionList, (char*)f->assertionList, sizeof(RDF_Resource)*f->assertionListSize);
    f->assertionList = newAssertionList;
    f->assertionListSize = f->assertionListSize + GROW_LIST_INCR;
    freeMem(old);
  }
  *(f->assertionList + f->assertionCount++) = as;
}



void
removeFromAssertionList(RDFFile f, Assertion as)
{
	/* XXX implement */
}



void
ht_fprintf(PRFileDesc *file, const char *fmt, ...) 
{
    va_list ap;
    char *buf;
    va_start(ap, fmt);
    buf = PR_smprintf(fmt, ap);
    va_end(ap);
    if(buf) {
      	PR_Write(file, buf, RDF_STRLEN(buf));
	free(buf);
    }
}



void
ht_rjcprintf(PRFileDesc *file, const char *fmt, const char *data)
{
	char	*buf;

	buf = PR_smprintf(fmt, data);
	if(buf) {
		PR_Write(file, buf, RDF_STRLEN(buf));
		free(buf);
	}
}


char *
makeDBURL(char* name)
{
  char		*ans;
  size_t		s;
  
  if (profileDirURL == NULL) return NULL;
  if ((ans = (char*) getMem(RDF_STRLEN(profileDirURL) + strlen(name) + 3)) != NULL) {
    s = RDF_STRLEN(profileDirURL);
    memcpy(ans, profileDirURL, s);
    if (ans[s-1] != '/') {
      ans[s++] = '/';
    }
    memcpy(&ans[s], name, RDF_STRLEN(name));
    
#ifdef	XP_WIN
    if (ans[9] == '|') ans[9] = ':';
#endif
  }
  return(ans);
}


PLHashNumber
idenHash (const void *key)
{
	return (PLHashNumber)key;
}



int
idenEqual (const void *v1, const void *v2)
{
	return (v1 == v2);
}



PRBool
inverseTV (PRBool tv)
{
  if (tv == true) {
    return false;
  } else return true;
}



char *
append2Strings (const char* str1, const char* str2)
{
  int32 l1 = RDF_STRLEN(str1);
  int32 len = l1 + RDF_STRLEN(str2);
  char* ans = (char*) getMem(len+1);
  memcpy(ans, str1, l1);
  memcpy(&ans[l1], str2, len-l1);
  return ans;
}



void
stringAppendBase (char* dest, const char* addition)
{
  int32 l1 = RDF_STRLEN(dest);
  int32 l2 = RDF_STRLEN(addition);
  int32 l3 = charSearch('#', addition);
  if (l3 != -1) l2 = l3;
  memcpy(&dest[l1], addition, l2);
}



void
stringAppend (char* dest, const char* addition)
{
  int32 l1 = RDF_STRLEN(dest);
  int32 l2 = RDF_STRLEN(addition);
  memcpy(&dest[l1], addition, l2);
}



int16
charSearch (const char c, const char* data)
{
  char* ch = RDF_STRCHR(data, c);

  if (ch) {
    return (ch - data);
  } else {
    return -1;
  }
 
}



PRBool
endsWith (const char* pattern, const char* uuid)
{
  short l1 = RDF_STRLEN(pattern);
  short l2 = RDF_STRLEN(uuid);
  short index;
  
  if (l2 < l1) return false;
  
  for (index = 1; index <= l1; index++) {
    if (toupper(pattern[l1-index]) != toupper(uuid[l2-index])) return false;
  }
  
  return true;
}



PR_PUBLIC_API(PRBool)
startsWith (const char* pattern, const char* uuid)
{
  short l1 = RDF_STRLEN(pattern);
  short l2 = RDF_STRLEN(uuid);
  if (l2 < l1) return false;
  return (RDF_STRNCASECMP(pattern, uuid, l1)  == 0);
}



PRBool
substring (const char* pattern, const char* data)
{
  char *p = RDF_STRCASESTR(data, pattern);
  return p != NULL;
}



int16
revCharSearch (const char c, const char* data)
{
  char *p = RDF_STRRCHR(data, c);
  return p ? p-data : -1;
}



PRBool
urlEquals (const char* url1, const char* url2)
{
  int16 n1 = charSearch('#', url1);
  int16 n2 = charSearch('#',  url2);
  if ((n1 == -1) && (n2 == -1)) {    
    return (RDF_STRCMP(url1, url2) == 0);
  } else if ((n2 == -1) && (n1 > 0)) {
    return ((RDF_STRLEN(url2) == (size_t)(n1)) && (strncmp(url1, url2, n1) == 0));
  } else if ((n1 == -1) && (size_t) (n2 > 0)) {
    return ((RDF_STRLEN(url1) == (size_t)(n2)) && (strncmp(url1, url2, n2) == 0));
  } else return 0;
}



PRBool
isSeparator (RDF_Resource r)
{
  return (startsWith("separator", resourceID(r)) || startsWith("nc:separator", resourceID(r))) ;
}



char *
getBaseURL (const char* url)
{
  int n = charSearch('#' , url);
  char* ans;
  if (n == -1) return copyString(url);
  if (n == 0) return NULL;
  ans = getMem(n+1);
  memcpy(ans, url, n);
  return ans;
}



void
setContainerp (RDF_Resource r, PRBool val)
{
  if (val) {
    r->flags |= CONTAINER_FLAG;
  } else {
    r->flags &= (~CONTAINER_FLAG);
  }
}



PRBool
containerp (RDF_Resource r)
{
  return (r->flags & CONTAINER_FLAG);
}



void
setLockedp (RDF_Resource r, PRBool val)
{
  if (val) {
    r->flags |= LOCKED_FLAG;
  } else {
    r->flags &= (~LOCKED_FLAG);
  }
}



PRBool
lockedp (RDF_Resource r)
{
  return (r->flags & LOCKED_FLAG);
}



uint8
resourceType (RDF_Resource r)
{
 return r->type;
}



void
setResourceType (RDF_Resource r, uint8 val)
{
  r->type = val;
}



char *
resourceID(RDF_Resource r)
{
  return r->url;
 }

char* opTypeToString (RDF_EventType opType) {
  switch (opType) {
    case RDF_ASSERT_NOTIFY :
      return "Assert";
	case RDF_INSERT_NOTIFY :
      return "Insert";
	case RDF_DELETE_NOTIFY :
      return "Unassert";
  }
  return "Unknown Op";
}


void traceNotify (char* event, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type) {
#ifdef DEBUG_guha1    
      char* traceLine = getMem(1000);
      if (type == RDF_INT_TYPE) {
        sprintf(traceLine, "%s  %s(%s, %i)\n", 
              event, resourceID(s), resourceID(u), (int) v);
      } else if (type == RDF_STRING_TYPE){
        sprintf(traceLine, "%s %s(%s, \"%s\")\n", 
                event,  resourceID(s), resourceID(u), (char*) v);
      } else if (type == RDF_RESOURCE_TYPE) {
        sprintf(traceLine, "%s %s(%s, %s)\n", 
                event,  resourceID(s), resourceID(u), resourceID((RDF_Resource)v));
      } else {
        sprintf(traceLine, "%s <gubbish>\n", event);
      }
      FE_Trace(traceLine);
      freeMem(traceLine);    
#endif
}


char *
makeResourceName (RDF_Resource node)
{
	char		*name = NULL;

	name =  resourceID(node);
	if (startsWith("http:", resourceID(node)))
	{
      name = &name[7];
	}
	else if (startsWith("file:", resourceID(node)))
	{
      name = &name[FS_URL_OFFSET];
	}
	else
	{
      name = resourceID(node);

	}
	return ((name != NULL) ? copyString(name) : NULL);
}



PR_PUBLIC_API(char *)
RDF_GetResourceName(RDF rdf, RDF_Resource node)
{
  char* name = RDF_GetSlotValue(rdf, node, gCoreVocab->RDF_name, RDF_STRING_TYPE, false, true);
  if (name != NULL) return name;
  name = makeResourceName(node);
  return name;
}


#ifdef MOZILLA_CLIENT
PR_PUBLIC_API(RDF_Resource)
RDFUtil_GetFirstInstance (RDF_Resource type, char* defaultURL)
{

  RDF_Resource bmk = remoteStoreGetSlotValue(gLocalStore, type,
					     gCoreVocab->RDF_instanceOf,
					     RDF_RESOURCE_TYPE, true, true);
  if (bmk == NULL) {
    /* bmk = RDF_GetResource(NULL, defaultURL, 1); */
    bmk = createContainer(defaultURL);
    remoteAssert(gLocalStore, bmk, gCoreVocab->RDF_instanceOf, 
		      type, RDF_RESOURCE_TYPE, 1);
  }
  return bmk;
}


PR_PUBLIC_API(void)
RDFUtil_SetFirstInstance (RDF_Resource type, RDF_Resource item)
{
  RDF_Resource bmk = remoteStoreGetSlotValue(gLocalStore, type,
					     gCoreVocab->RDF_instanceOf,
					     RDF_RESOURCE_TYPE, true, true);
  if (bmk) {
    remoteUnassert(gLocalStore, bmk,  gCoreVocab->RDF_instanceOf, 
			type,	 RDF_RESOURCE_TYPE);
  }
  if (item) {
    remoteAssert(gLocalStore, item, gCoreVocab->RDF_instanceOf, 
			type, RDF_RESOURCE_TYPE, true);
  }
}



PR_PUBLIC_API(RDF_Resource)
RDFUtil_GetQuickFileFolder()
{
  return RDFUtil_GetFirstInstance(gNavCenter->RDF_BookmarkFolderCategory, "NC:Bookmarks");
}



PR_PUBLIC_API(void)
RDFUtil_SetQuickFileFolder(RDF_Resource container)
{
  RDFUtil_SetFirstInstance(gNavCenter->RDF_BookmarkFolderCategory, container);
}


RDF_Resource gPTFolder = NULL;
PR_PUBLIC_API(RDF_Resource)
RDFUtil_GetPTFolder()
{
  if (gPTFolder) return gPTFolder;
 return (gPTFolder =  RDFUtil_GetFirstInstance(gNavCenter->RDF_PersonalToolbarFolderCategory, "PersonalToolbar"));
}



PR_PUBLIC_API(void)
RDFUtil_SetPTFolder(RDF_Resource container)
{
  /*
  RDFUtil_SetFirstInstance( gNavCenter->RDF_PersonalToolbarFolderCategory, container);
  */
}



PR_PUBLIC_API(RDF_Resource)
RDFUtil_GetNewBookmarkFolder()
{
  return RDFUtil_GetFirstInstance(gNavCenter->RDF_NewBookmarkFolderCategory, "NC:Bookmarks");
}



PR_PUBLIC_API(void)
RDFUtil_SetNewBookmarkFolder(RDF_Resource container)
{
  RDFUtil_SetFirstInstance(gNavCenter->RDF_NewBookmarkFolderCategory, container);
}



PR_PUBLIC_API(RDF_Resource)
RDFUtil_GetDefaultSelectedView()
{
  return RDFUtil_GetFirstInstance(gNavCenter->RDF_DefaultSelectedView, "selectedView");
}



PR_PUBLIC_API(void)
RDFUtil_SetDefaultSelectedView(RDF_Resource container)
{
  RDFUtil_SetFirstInstance(gNavCenter->RDF_DefaultSelectedView, container);
}





/* Returns a new string with inURL unescaped. */
/* We return a new string because NET_UnEscape unescapes */
/* string in place */
char *
unescapeURL(char *inURL)
{
	char *escapedPath = copyString(inURL);

#ifdef MOZILLA_CLIENT
#ifdef XP_WIN
	replacePipeWithColon(escapedPath);
#endif

    /* xxx	NET_UnEscape(escapedPath); */
#endif

	return (escapedPath);
}



/* Given a file URL of form "file:///", return substring */
/* that can be used as a path for PR_Open. */
/* NOTE: This routine DOESN'T allocate a new string */


char *
convertFileURLToNSPRCopaceticPath(char* inURL)
{
#ifdef	XP_WIN
	if (startsWith("file://", inURL))	return (inURL + 8);
	else if (startsWith("mailbox:/", inURL))	return (inURL + 9);
	else if (startsWith("IMAP:/", inURL))	return (inURL + 6);
	else return (inURL);
#else
	/* For Mac & Unix, need preceeding '/' so that NSPR */
	/* interprets path as full path */
	if (startsWith("file://", inURL))	return (inURL + 7);
	else if (startsWith("mailbox:/", inURL))	return (inURL + 8);
	else if (startsWith("IMAP:/", inURL))	return (inURL + 5);
	else return (inURL);
#endif
}

char* MCDepFileURL (char* url) {
	char* furl;  
	int32 len;   
	char* baz = "\\";
	int32 n = 0; 
	furl = convertFileURLToNSPRCopaceticPath(unescapeURL(url));
	len = RDF_STRLEN(furl);
#ifdef XP_WIN
	while (n < len) {
		if (furl[n] == '/') furl[n] = baz[0];
		n++;
	}
#endif
	return furl;
}

PRFileDesc *
CallPROpenUsingFileURL(char *fileURL, PRIntn flags, PRIntn mode)
{
	PRFileDesc* result = NULL;
	const char *path;

	char *escapedPath = unescapeURL(fileURL);
	path = convertFileURLToNSPRCopaceticPath(escapedPath);

	if (path != NULL)	{
		result = PR_Open(path, flags, mode);
	}

	if (escapedPath != NULL)	freeMem(escapedPath);

	return result;
}



PRDir *
CallPROpenDirUsingFileURL(char *fileURL)
{
	PRDir* result = NULL;
	const char *path;
	char *escapedPath = unescapeURL(fileURL);
	path = convertFileURLToNSPRCopaceticPath(escapedPath);

	if (path != NULL)	{
		result = PR_OpenDir(path);
	}

	if (escapedPath != NULL)	freeMem(escapedPath);

	return result;
}



int32
CallPRWriteAccessFileUsingFileURL(char *fileURL)
{
	int32 result = -1;
	const char *path;
	char *escapedPath = unescapeURL(fileURL);
	path = convertFileURLToNSPRCopaceticPath(escapedPath);

	if (path != NULL)	{
		result = PR_Access(path, PR_ACCESS_WRITE_OK);
	}

	if (escapedPath != NULL)	freeMem(escapedPath);

	return result;
}



int32
CallPRDeleteFileUsingFileURL(char *fileURL)
{
	int32 result = -1;
	const char *path;
	char *escapedPath = unescapeURL(fileURL);
	path = convertFileURLToNSPRCopaceticPath(escapedPath);

	if (path != NULL)	{
		result = PR_Delete(path);
	}

	if (escapedPath != NULL)	freeMem(escapedPath);

	return result;
}



int
CallPR_RmDirUsingFileURL(char *dirURL)
{
	int32 result=-1;
	const char *path;

	char *escapedPath = unescapeURL(dirURL);
	path = convertFileURLToNSPRCopaceticPath(escapedPath);

	if (path != NULL)	{
		result = PR_RmDir(path);
	}

	if (escapedPath != NULL)	freeMem(escapedPath);

	return result;
}



int32
CallPRMkDirUsingFileURL(char *dirURL, int mode)
{
	int32 result=-1;
	const char *path;

	char *escapedPath = unescapeURL(dirURL);
	path = convertFileURLToNSPRCopaceticPath(escapedPath);

	if (path != NULL)	{
		result = PR_MkDir(path,mode);
	}

	if (escapedPath != NULL)	freeMem(escapedPath);

	return result;
}



#endif /* MOZILLA_CLIENT */
