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
#include "nlcstore.h"


	/* globals */
PRBool rdfDBInited = 0;
PLHashTable*  resourceHash = 0;
RDFT gRemoteStore = 0;
RDFT gSessionDB = 0;

	/* externs */
extern	char	*profileDirURL;



RDF_Resource
getMCFFrtop (char* furl)
{
  char* url = getBaseURL(furl);
  RDF_Resource r;
  r = RDF_GetResource(NULL, url, 1);
  freeMem(url);
  return r;
}



RDFFile
makeRDFFile (char* url, RDF_Resource top, PRBool localp)
{
  RDFFile ans = (RDFFile)getMem(sizeof(struct RDF_FileStruct));
  /*  ans->rdf = rdf; */
  ans->url = getBaseURL(url);
  ans->top = top;
  ans->localp = localp;
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
  ans->resourceList = (RDF_Resource*)getMem(200);
  ans->assertionList = (Assertion*)getMem(400);
  ans->resourceListSize = 50;
  ans->assertionListSize = 100;
  ans->holdOver = (char*)getMem(RDF_BUF_SIZE);
  ans->depth = 1;
  ans->lastItem = ans->stack[0] = ans->top;
  ans->locked = ans->localp;
  ans->lineSize = LINE_SIZE;
  ans->tv = true;
}



void
addToResourceList (RDFFile f, RDF_Resource u)
{
  if (f->resourceListSize == f->resourceCount) {
    RDF_Resource* newResourceList = (RDF_Resource*)getMem(4*(f->resourceListSize + 50));
    RDF_Resource* old = f->resourceList;
    memcpy((char*)newResourceList, (char*)f->resourceList, 4*f->resourceListSize);
    f->resourceList = newResourceList;
    f->resourceListSize = f->resourceListSize + 50;
    freeMem(old);
  }
  *(f->resourceList + f->resourceCount++) = u;
}



void
addToAssertionList (RDFFile f, Assertion as)
{
  if (f->assertionListSize == f->assertionCount) {
    Assertion* newAssertionList = (Assertion*)getMem(4*(f->assertionListSize + 50));
    Assertion* old = f->assertionList;
    memcpy((char*)newAssertionList, (char*)f->assertionList, 4*f->assertionListSize);
    f->assertionList = newAssertionList;
    f->assertionListSize = f->assertionListSize + 50;
    freeMem(old);
  }
  *(f->assertionList + f->assertionCount++) = as;
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
      	PR_Write(file, buf, strlen(buf));
	free(buf);
    }
}



void
ht_rjcprintf(PRFileDesc *file, const char *fmt, const char *data)
{
	char	*buf;

	buf = PR_smprintf(fmt, data);
	if(buf) {
		PR_Write(file, buf, strlen(buf));
		free(buf);
	}
}



char *
makeDBURL(char* name)
{
  char		*ans;
  size_t		s;
  
  if (profileDirURL == NULL) return NULL;
  if ((ans = (char*) getMem(strlen(profileDirURL) + strlen(name) + 3)) != NULL) {
    s = strlen(profileDirURL);
    memcpy(ans, profileDirURL, s);
    if (ans[s-1] != '/') {
      ans[s++] = '/';
    }
    memcpy(&ans[s], name, strlen(name));
    
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
  int32 l1 = strlen(str1);
  int32 len = l1 + strlen(str2);
  char* ans = (char*) getMem(len+1);
  memcpy(ans, str1, l1);
  memcpy(&ans[l1], str2, len-l1);
  return ans;
}



void
stringAppendBase (char* dest, const char* addition)
{
  int32 l1 = strlen(dest);
  int32 l2 = strlen(addition);
  int32 l3 = charSearch('#', addition);
  if (l3 != -1) l2 = l3;
  memcpy(&dest[l1], addition, l2);
}



void
stringAppend (char* dest, const char* addition)
{
  int32 l1 = strlen(dest);
  int32 l2 = strlen(addition);
  memcpy(&dest[l1], addition, l2);
}



int16
charSearch (const char c, const char* data)
{
  char* ch = strchr(data, c);

  if (ch) {
    return (ch - data);
  } else {
    return -1;
  }
 
}



PRBool
endsWith (const char* pattern, const char* uuid)
{
  short l1 = strlen(pattern);
  short l2 = strlen(uuid);
  short index;
  
  if (l2 < l1) return false;
  
  for (index = 1; index <= l1; index++) {
    if (pattern[l1-index] != uuid[l2-index]) return false;
  }
  
  return true;
}



PR_PUBLIC_API(PRBool)
startsWith (const char* pattern, const char* uuid)
{
  short l1 = strlen(pattern);
  short l2 = strlen(uuid);
  if (l2 < l1) return false;
  return strncmp(pattern, uuid, l1)  == 0;
}



PRBool
substring (const char* pattern, const char* data)
{
  char *p = strstr(data, pattern);
  return p != NULL;
}



int16
revCharSearch (const char c, const char* data)
{
  char *p = strrchr(data, c);
  return p ? p-data : -1;
}



PRBool
urlEquals (const char* url1, const char* url2)
{
  int16 n1 = charSearch('#', url1);
  int16 n2 = charSearch('#',  url2);
  if ((n1 == -1) && (n2 == -1)) {    
    return (strcmp(url1, url2) == 0);
  } else if ((n2 == -1) && (n1 > 0)) {
    return ((strlen(url2) == (size_t)(n1)) && (strncmp(url1, url2, n1) == 0));
  } else if ((n1 == -1) && (size_t) (n2 > 0)) {
    return ((strlen(url1) == (size_t)(n2)) && (strncmp(url1, url2, n2) == 0));
  } else return 0;
}



PRBool
isSeparator (RDF_Resource r)
{
  return startsWith("separator", resourceID(r));
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
{ return r->type;
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



char *
makeResourceName (RDF_Resource node)
{
  char* name;
  name =  resourceID(node);
  if (startsWith("http:", name)) return copyString(&name[7]);
  if (startsWith("file:", name)) return copyString(&name[8]);
  return copyString(name);
}



PR_PUBLIC_API(char *)
RDF_GetResourceName(RDF rdf, RDF_Resource node)
{
  char* name = RDF_GetSlotValue(rdf, node, gCoreVocab->RDF_name, RDF_STRING_TYPE, false, true);
  if (name != NULL) return name;
  return makeResourceName(node);
}



PR_PUBLIC_API(RDF_Resource)
RDFUtil_GetFirstInstance (RDF_Resource type, char* defaultURL)
{
  RDF_Resource bmk = nlocalStoreGetSlotValue(gLocalStore, type,
					     gCoreVocab->RDF_instanceOf,
					     RDF_RESOURCE_TYPE, true, true);
  if (bmk == NULL) {
    bmk = RDF_GetResource(NULL, defaultURL, 1);
    nlocalStoreAssert(gLocalStore, bmk, gCoreVocab->RDF_instanceOf, 
		      type, RDF_RESOURCE_TYPE, 1);
  }
  return bmk;
}



PR_PUBLIC_API(void)
RDFUtil_SetFirstInstance (RDF_Resource type, RDF_Resource item)
{
  RDF_Resource bmk = nlocalStoreGetSlotValue(gLocalStore, type,
					     gCoreVocab->RDF_instanceOf,
					     RDF_RESOURCE_TYPE, true, true);
  if (bmk) {
    nlocalStoreUnassert(gLocalStore, bmk,  gCoreVocab->RDF_instanceOf, 
			type,	 RDF_RESOURCE_TYPE);
  }
  if (item) {
    nlocalStoreAssert(gLocalStore, item, gCoreVocab->RDF_instanceOf, 
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



PR_PUBLIC_API(RDF_Resource)
RDFUtil_GetPTFolder()
{
 return RDFUtil_GetFirstInstance(gNavCenter->RDF_PersonalToolbarFolderCategory, "PersonalToolbar");
}



PR_PUBLIC_API(void)
RDFUtil_SetPTFolder(RDF_Resource container)
{
  RDFUtil_SetFirstInstance( gNavCenter->RDF_PersonalToolbarFolderCategory, container);
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
